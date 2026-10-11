# Better detection without more training (plan, 12 October 2026)

Goal: raise the hit-labelling accuracy of the plugin's detector **without training the network again**.
The PC and the sample packs aren't available, and the licence check (see the PR #5 discussion)
argues against more training on commercial packs anyway. Everything here runs in the cloud sandbox,
on data the sandbox can fetch.

## Where we are

- `HitClassifier` is a per-hit network: 225 log-mel numbers from -25 to +40 ms -> 224 -> 9 sigmoids.
  Then a single confidence cut (`0.55 - 0.2 x sensitivity`) picks the primary type.
- **It has no memory of the loop when it labels a hit.** The long-term spectra it learns
  (`HitClassifier::learn`, `DrumEngine::onHit`) only feed the stems and The Puppet Show. They never
  feed back into labelling.
- The new FULL weights (PR #5) are strong on produced drums but weak on live acoustic drums (MDB
  macro F1 0.44 -> 0.33) and on synthetic EDM claps (drum_bench 83.1 -> 51.8). Open hats and cymbals
  are still the weakest types (docs/08 §9: they're the ones that need more than 40 ms).
- Older findings that are still open (docs/06): toms labelled kick (7), the music bed causing
  false hits (4), and labels drifting on repeated hits (3, measured with the old classifier, to re-check).

## The ideas, in order for tomorrow

Each idea is a small commit with a before/after measurement. The order is by expected gain per
hour of work.

### 1. Ensemble with the shipped network (quick win for acoustic drums)
The shipped 225 -> 96 -> 9 network (in `main`, 268 KB, trained on Groove + E-GMD, CC BY 4.0) is
better on acoustic drums. The new one is better on produced drums. Run both on every hit and
average the probabilities (+22k multiply-adds per hit on top of the new network's 52k, still tiny next to the STFTs). A tuned blend weight is
a later option.
- Change: `HitClassifierWeightsV1.h` (the shipped header, renamed symbols) and a second `TinyMlp`
  in `HitClassifier`.
- Expect MDB to come back towards 0.40 while produced drums lose a little. Measure both.
- Bonus: the ensemble's clean half can stay when the FULL weights are later swapped for a CLEAN model.

### 2. Shift averaging (test-time augmentation)
Run the network on the onset at -1, 0 and +1 ms and average the outputs. Live onsets are refined only
to about 1 ms (OnsetDetector), while training onsets were exact. This costs 3x the network (about 0.2 M
multiply-adds per hit with the ensemble). Keep it only if it helps.

### 3. Loop memory: repeated samples vote together (expected to be the biggest gain)
In produced music the same snare sample comes back on every backbeat. A bank of up to 32 "hit
fingerprints" stores, for each cluster, the normalised 32-band shape of the 0-20 ms segments, a
count, and the mean network probabilities.
- A new hit that is close to a fingerprint (cosine distance under a threshold, and level within
  12 dB) blends its own probabilities with the cluster's mean. The blend weight grows with the
  cluster count, up to 0.6. A hit that matches nothing starts a new cluster. The least-used cluster
  is replaced when full.
- Effect: a snare that the network gets right half the time is labelled from the majority of its
  repeats. This targets "snares inside beats 47%" directly.
- Guards: layered hits (snare + hat) are a different fingerprint, so they form their own cluster.
  The bank is forgotten with `resetLearning()` (transport stop). Arrays are fixed size: no allocation.
- Re-check docs/06 finding 3 (drift) with it on: a wrong cluster must not lock in. Measure the share
  of hits relabelled from right to wrong, not only the totals.

### 4. Cheap physical checks (the network's known blind spots)
- **Clap bursts.** `OnsetDetector` already merges a clap's later bursts within 15 ms into one event.
  Report the burst count in `OnsetEvent` and treat 2 or more bursts in the 1-5 kHz bands as strong clap
  evidence. Target: synthetic EDM claps (drum_bench EDM 51.8).
- **Snare body.** If "closed hat" wins but the rise spectrum has a clear 160-630 Hz body
  (`ClassifiedHit::rise`) and snare is second, prefer snare. If "snare" wins with no body and no
  energy below 2.5 kHz, prefer hat.
- **Toms to misc** (finding 7). A "kick" whose rise peaks at 100-400 Hz with little below 60 Hz goes
  to misc. Toms have no string, and misc is neutral.
- Each check runs only when the network is unsure (top two types within 0.2). A confident network
  is never overruled.

### 5. Late correction for open hats and cymbals (no extra latency)
The 40 ms window can't hear a tail (docs/08 §9: open hat +10 points, crash +14 at 100 ms). Instead of
adding latency, keep watching after the decision. `HitTracker` already measures each hit's envelope
(in the hit-tracker test a closed hat ends at about 134 ms and an open hat at about 719 ms).
- If a "closed hat" is still ringing above its peak - 20 dB at 150 ms, relabel it open hat. Do the
  same for clap/rim versus crash with a longer threshold.
- The type's gate crossfades over 20 ms from that point. Shadow Play updates the shape.
- This touches `HitTracker` and `DrumEngine`. It's bigger, so it's last of the core items.

### 6. Calibrate the decision (thresholds per type)
Replace the single confidence cut with one cut per type, tuned on half of each eval set and
reported on the other half. Small, but it stops rare types (ride, rim) from losing every close call.

### Parked (needs the host, or later)
- **Beat position.** Snares lean to beats 2 and 4, and hats to 8ths and 16ths. Get the host's PPQ
  from JUCE (touches `PluginProcessor`, `VERIFY(host)`) and learn a per-loop position histogram per type
  (loop memory again). It would only be used as a tie-breaker.
- **Music-bed false hits** (finding 4): reject low-band events whose spectrum is pitched and steady
  (a bass note, not a drum). That's an onset-detector change, a separate branch.

## How it's measured (all in the sandbox)

- **MDB Drums** (live acoustic, CC BY-NC-SA, evaluation only) and **Dirt-Samples electronic loops**
  via `research/lookahead-test/fetch_data.sh`, `research/classifier-eval/export_sets.py` and
  `eval.cpp`. Split by track: tune on one half, report the other.
- **drum_bench** (synthetic, deterministic): per genre plus each hit type alone.
- **DSP tests** 12/12, and `test_hit_classifier` must keep its thresholds.
- **CPU**: drum_bench performance section. Budget: at most +5% of a cloud core for everything here.
- **The owner's packs can't be used tomorrow** (the PC is off). The big pack cross-check from docs/08
  is re-run when the PC is back.

**Done means:**
- MDB macro F1 is 0.40 or higher (now 0.33), and electronic loops stay at 0.38 or higher.
- drum_bench EDM is 75 or higher (now 51.8), and the synthetic snare stays above 80%.
- No change makes a confident correct hit wrong in more than 1% of cases.

## Order of work tomorrow

1. Fetch MDB and Dirt, build `eval` and `drum_bench`, and record the baseline for both the new and
   the shipped weights (about 30 min).
2. Ensemble, then shift averaging: measure and keep or drop.
3. Loop memory, with its own test in `dsp/tests`.
4. Physical checks (clap bursts, snare body, toms).
5. Late correction, if time allows. Otherwise it's the first item next time.
6. Per-type thresholds last, because they depend on everything before them.

Every step goes on branch `remote/detection-postproc` (from `remote/snare-training`) as
`[untested]` commits with REMOTE_LOG rows. Nothing reaches `main` before the FL test.
