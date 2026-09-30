<!-- File: docs/03-decisions-for-review.md -->
# The Drum Master — Decisions taken alone (for review)

> The user was away and said on 2026-09-30: "continue everything leading up to the code, if you
> have to decide things on your own do it and I will go over it later".
> Every decision Claude made without asking is listed here, with the reason and how to undo it.
> **Nothing here is approved yet.** Mark each one ✅ keep / ✏️ change / ❌ revert.

| ID | Decision | Area | Status |
|---|---|---|---|
| D-C1 | Learned classifier (small neural net) replaces the hand-written rules | detection | open |
| D-C2 | Training data and licences: Groove MIDI Dataset + own synthetic loops only | legal | open |
| D-C3 | Loop adaptation of the classifier | detection | open |
| D-C4 | Drum-name mapping of the Groove MIDI Dataset | detection | open |
| D-C5 | Onset detector tuning (differs from the phase-2 design) | detection | open |
| D-C6 | Hit tracker values (tail end, fades, caps) | processing | open |
| D-C7 | Shadow Play layout details | UI | open |
| D-C8 | Test timeout raised in sanitizer builds (dsp/CMakeLists.txt) | build | open |
| D-C9 | Known limits we accept for v1 | detection | open |
| D-C10 | Per-drum processor chain (order, ranges, what each amount means) | processing | open |
| D-C11 | What every knob and snare macro moves (the hidden curves) | processing | open |
| D-C12 | How the engine splits the loop into per-drum estimates | processing | open |
| D-C13 | Plugin wiring choices (state, Link, bypass, what isn't saved) | plugin | open |
| D-C14 | The Puppet Show: prompt 04 sent to agents, DSP waits for their answers | feature | open |
| D-C15 | Puppet Show v1 targets: averaged answers, kick rows anchored to real kits, M1 redefined | feature | open |
| D-C16 | The Puppet Show engine (built from Claude's prompt-04 design; agents answered only the targets) | feature | open |

---

## D-C1 — A learned classifier replaces the rules

**What was approved:** "No ML in v1: rule-based first; a small learned model only if the rules
miss their targets on real loops."

**What happened:**
- The rule-based classifier (template fit + drum physics) scored 100% on our synthetic kit.
- On real drums it failed:
  - MDB Drums (acoustic kits);
  - loops built from real drum-machine one-shots.
- Tuning the rules on the real sets didn't transfer. Decision trees trained on the rule features
  reached 61–66% within one domain, but only **14–23%** from acoustic to electronic and back. The
  rule features simply don't carry enough information.

That is exactly the "rules miss targets on real loops" exit, so Claude took it.

**Now:**
- **Features:** a 32-band log-mel picture of the hit. There are 7 time slices, from 25 ms before
  the onset to 40 ms after it. Each slice is measured relative to the hit's own peak, plus the peak
  level itself, giving 225 numbers (`dsp/include/plugin_dsp/HitFeatures.h`).
- **Network:** 225 → 48 → 9 (`TinyMlp.h`). There is one yes/no output per drum type, so a
  kick + hat hit can be both (answer 1b stays intact).
- **Cost:** about 11 k multiply-adds per hit, a few microseconds. There is no allocation and no
  library, and `dsp/` stays pure C++.
- **Unchanged:** the classifier API (weights, confidence, Misc, allowed types, auto-detect
  presence, per-type spectra for The Puppet Show). The engine and UI don't notice the swap.
- **Misc:** if no type is confident, the hit is Misc, which is neutral by default.

**Results:** see the table under "Classifier results" below (filled in after training).

**To undo:** the rule-based classifier stays in git history (commit 959d327).

## D-C2 — Training data and licences

| Data | Licence | Used for |
|---|---|---|
| Groove MIDI Dataset (Magenta / Google) | CC BY 4.0 | training |
| Expanded Groove MIDI Dataset (E-GMD): the same playing re-recorded on 43 Roland TD-17 kits, acoustic to 808/909 | CC BY 4.0 | training (30 performances per kit, fetched without the full 90 GB zip); 6 kits held out to test unseen kits |
| Our own synthetic loops (`research/classifier-train/synth_aug.py`) | ours | training (claps, 808 kicks, drum-machine hats that Groove lacks) |
| MDB Drums | CC BY-NC-SA 4.0 (non-commercial) | **evaluation only**, never training |
| Dirt-Samples one-shots | unclear | **evaluation only**, never training |

- **Why:** the trained weights ship inside the plugin. Weights trained on non-commercial or unclear
  data could put a commercial release at risk. Evaluation-only use never ships.
- **Obligation:** CC BY 4.0 requires credit. The About page (phase 4) must say:
  *"Hit classifier trained in part on the Groove MIDI Dataset (Gillick, Roberts, Engel, Tsao and
  Bamman, 2019) and the Expanded Groove MIDI Dataset (Callender, Hawthorne and Engel, 2020), both CC BY 4.0."* The generated weights header carries the same credit.
- **Please check:** whether you plan to sell the plugin. If it will only ever be free, MDB could
  also be used for training. Don't do that without deciding, though.

## D-C3 — Loop adaptation of the classifier

- **Phase-2 plan:** templates move toward the loop's confident hits, capped at 50%, saved with the
  session, with a "reset learning" button.
- **With the network, the network itself doesn't learn in the plugin.** Training inside a DAW is
  risky and hard to test.
- **What still adapts, per loop:**
  - the per-type average spectra (these feed The Puppet Show);
  - the auto-detect presence counts.

  Both are saved with the session, and "reset learning" still clears them.
- **Proposal for phase 5 (not built):** a per-loop "nearest confident example" correction on top
  of the network.

## D-C4 — Groove MIDI drum mapping

Groove labels come from the MIDI of an electronic kit (Roland TD-11), mapped to our 9 types as
follows:

| MIDI notes | Our type |
|---|---|
| 36 | kick |
| 38, 40 (snare, snare rim-shot) | snare |
| 37 (side stick) | rimshot |
| 42, 22, 44 (closed, closed edge, pedal) | closed hat |
| 46, 26 (open, open edge) | open hat |
| 49, 55, 57, 52 (crashes, splash, china) | crash |
| 51, 59, 53 (ride, ride edge, bell) | ride |
| toms | misc |

- **Why "snare rim-shot" → snare:** in a mix it sounds like a loud snare, not a click.
- **Why toms → misc:** there's no tom type in the concept, and Misc is neutral.
- **Missing: claps.** Groove has no claps, so all clap training comes from our synthetic loops. Clap
  accuracy on real claps is the weakest point to test at home.

## D-C5 — Onset detector tuning

Documented in `docs/02-algorithm.md` §3 under "As built in phase 3". In short:
- per-bin SuperFlux;
- a Hann window;
- neighbourhood level-rise check ≥ 3 dB;
- longer low-band reference (25 ms);
- relative gate 35 dB (was 45);
- δ = 2 dB;
- refractory 25 ms;
- grouping ±6 ms;
- echo merge 15 ms.

Each change was forced by a failing test.

Measured:
- synthetic kit: F1 0.97–0.99, timing ≤ 0.8 ms;
- real acoustic (MDB): onset F1 0.879;
- electronic loops: onset F1 0.975.

## D-C6 — Hit tracker values

`dsp/include/plugin_dsp/HitTracker.h`:
- **Pre-roll:** 2 ms raised-cosine fade before the onset (the "Fades" of §7).
- **Release:** 20 ms raised-cosine after the tail ends.
- **Tail end:** the range envelope stays below max(peak − R_t, floor + 6 dB) for 20 ms. R_t is as in
  §5: kick 36, snare 36, clap 36, open hat / crash / ride 40, closed hat / rim / misc 30.
- **Ranges for the envelope:** kick < 150 Hz; snare, rim, clap 150 Hz–5 kHz; hats and cymbals
  > 5 kHz; misc broadband. The noise floor may rise at most 3 dB/s.
- **Flam guard:** a re-rise above peak − 20 dB restarts the wait.
- **Caps:** crash 4 s, open hat and ride 2 s, others 1 s.
- **Retrigger (deviation):**
  - The phase-2 plan had a 5 ms crossfade. Now the old hit ends at the new onset and fades over
    its 20 ms release, while the new one fades in over 2 ms.
  - The gate takes the larger of the two, so it never dips. Per-hit envelopes (Decay, Crack)
    restart from the new onset.
- **Weak labels:** a type with weight below 0.3 in a hit gets no gate. Above that, the gate is
  scaled by the weight.
- **Measured on the synthetic kit:**
  - a 0.5 s kick ends at about 0.52 s;
  - a closed hat ends at 134 ms, an open hat at 719 ms.

## D-C7 — Shadow Play layout details

The user delegated these ("you can choose that"):
- One bar or loop is a **ring**, synced to the playhead like a clock hand.
- It sits on the **stage floor under the puppet master**. Click it to enlarge.
- **Shapes:**
  - kick: circle;
  - snare: triangle;
  - closed hat: dot;
  - open hat: hollow ring;
  - crash: starburst;
  - ride: diamond;
  - rimshot: slash;
  - clap: three small triangles;
  - misc: faint square.
- **Look:** size = level, brightness = confidence. Older hits fade like smoke.
- **Data path:** the audio thread pushes {type, time, level, confidence} into a lock-free
  single-producer / single-consumer queue. The UI thread reads it. The audio thread never waits
  (phase 4).

## D-C8 — Test timeout in sanitizer builds

- **Changed:** `dsp/CMakeLists.txt` gives tests a 300 s timeout only when `PLUGIN_DSP_SANITIZE` is
  on; normal builds and CI keep 60 s.
- **Why:** ASan/UBSan make the detector tests about 40× slower.
- **Effect on CI and the plugin build:** none.

## D-C9 — Known limits accepted for v1

- A snare with a clap layered about 6 ms later is reported as one hit (snare). The two onsets are
  closer than the 6 ms grouping.
- A sound that stops exactly at a new onset can make the old hit's tail look longer, up to the
  20 ms hangover.
- A quiet hat fully masked by a crash tail (under 1 dB above it) is missed.
- Open hat vs crash on acoustic kits keeps improving with more than 50 ms of listening (measured).
  v1 decides at 40 ms. A late-correction crossfade is parked for phase 5.

## D-C10 — Per-drum processor chain

`dsp/include/plugin_dsp/DrumChannel.h`. There is one chain per drum type. It runs on that drum's
estimate and adds only its change, scaled by the hit gate.

- **Order:** EQ (up to 8 filters) → compressor → attack/tail shaping → saturation → soft clip →
  level → width. The compressor sits before the attack shaping, so it can't flatten the attack the
  user asked for.
- **Attack** (Crack / Tick / Click / Attack / Ping / Punch):
  - +100% = +9 dB and −100% = −15 dB, over a per-drum window;
  - windows: kick 12 ms, snare 8, clap 10, rim / open hat / ride 5, closed hat 3, crash and misc 8;
  - then a same-length fade back.
- **Decay:**
  - +100% = +6 dB on the tail (the parameter-table limit);
  - −100% = the tail falls an extra 0.3 dB per ms (−30 dB after 100 ms, floor −60 dB).
- **Compression:**
  - threshold −6 → −30 dBFS and ratio 1.5 → 6 as the knob goes 0 → 100%, with a 6 dB soft knee;
  - release 80 ms; attack per drum (kick 8 ms, so the beater passes; snare 5; clap 3);
  - half of the static gain reduction comes back as make-up.
  - **Please check:** the thresholds are absolute (dBFS), so a very quiet loop compresses less.
    `VERIFY(dsp)` at home with real loops.
- **Saturation:**
  - a tanh curve with drive 1 → 5, scaled so a −6 dBFS peak stays at −6 dBFS;
  - quiet parts come up by at most +8 dB (thicker, more sustain) and loud peaks are rounded;
  - it blends in with the amount, so there's no jump when you leave 0.
- **Clip:** a cubic soft clip; its ceiling goes 0 → −12 dBFS.
- **No oversampling yet** on saturation and clip, so some aliasing at high drive. `VERIFY(dsp)`: add
  2× oversampling if it's audible.
- **Width:** mid/side, where the side gain = width (0–200%).

## D-C11 — What the knobs and macros move

`dsp/include/plugin_dsp/DrumMapping.h`. The full table is in the code; the main choices:

- **EQ slot frequencies:**
  - kick: sub shelf 50 Hz (Low-end boost), Bottom bell 90 Hz, Top bell 3.5 kHz;
  - snare Body bell 200 Hz;
  - bodies: rim 400 Hz, clap 350 Hz, open hat 900 Hz, crash 700 Hz;
  - ride Bell 1.2 kHz;
  - De-harsh = a bell at 7 kHz (8 kHz on the closed hat), up to −8 dB.
- **Tone** = tilt around 1 kHz: the top moves +tone/2 and the bottom −tone/2.
- **Ride Wash** = a static gain on everything after the ping (±12 dB).
- **Quick mix:**
  - Kick Punch: attack ±60%, plus up to 30% compression.
  - Kick Weight: ±6 dB sub shelf.
  - Snare Snap: attack ±60% and ±3 dB air. It also acts on rim and clap, at a smaller amount.
  - Snare Fat: ±5 dB body, a longer tail and some saturation.
  - Hats Tone: ±3 dB tilt.
  - Hats Tame: de-harsh plus a 40% shorter decay.
- **Global Punch:** up to +40% attack on every drum except Misc.
- **Snare macros** (each at 100%):

| Macro | Body 200 Hz | 400 Hz | Wood 700 Hz | Crack 3 kHz | Air 8 kHz | Attack | Decay | Comp | Sat | Clip |
|---|---|---|---|---|---|---|---|---|---|---|
| Popify | | −4 dB | | | +3 dB | +30% | −30% | 40% | | |
| Rockify | +3 dB | | | | | | +40% | 25% | 30% | |
| Rimify | −5 dB | | +3 dB | +4 dB | | +50% | −50% | | | |
| Dubstepify | +4 dB | | | +1.5 dB | | +40% | +60% | 70% | 60% | 50% |

- **Stacking macros:** each column is added up and then soft-limited:
  - EQ columns to ±6 dB;
  - attack and decay to ±70%;
  - comp and sat to 80%.

  All four at 100% therefore stay sane, and opposite moves cancel (Rimify cuts what Dubstepify
  adds).
- **Link** (quick mix) is handled where the knobs move (the JUCE layer), not in the DSP.

## D-C12 — How the engine splits the loop into per-drum estimates

`dsp/include/plugin_dsp/DrumEngine.h`.

- **What the phase-2 plan left open:** the estimator (D1) needs a share for each drum per frequency
  bin, but a hit is only classified about 40–48 ms after its onset. The STFT frames around an onset
  are analysed earlier than that (from about +26 ms), so time-varying shares can't know yet which
  drum is hitting.
- **Chosen:** shares are fixed per bin and change only slowly.

  share(type, bin) = presence × spectrum / Σ over the enabled types

  - **Spectrum:** each type's long-term average hit spectrum. It starts generic and is learned from
    this loop's clean, single-type hits.
  - **Presence:** 0.02 for a type never heard, rising to 1 after 4 hits containing it. Misc counts
    only when it's a hit's main type.

  **Timing comes from the gates, not the shares.** A drum's settings act only while its gate is
  open, on the part of the spectrum that drum owns.
- **Result on the synthetic loop:**
  - kick −6 dB → −5.8 dB below 120 Hz, and the hats in between move 0.00 dB;
  - every setting neutral → output = input delayed by exactly 50 ms, bit for bit, at
    44.1/48/96 kHz.
- **Limits** (`VERIFY(dsp)` at home):
  - A drum's settings also act on anything else in its bins while its gate is open. Example: the
    kick's gate also moves a bass note under 120 Hz during the kick. This is the known cost of
    answer 1b.
  - Spectral shares smear about ±10 ms. On a hard edge the change can start a few ms before the
    2 ms pre-roll.
- **CPU:** about 21% of one sandbox core for stereo 48 kHz with every drum processing. The home
  i7-11700K should be well under that. `VERIFY(host)` in FL.
  - **Possible optimisation:** skip the estimates of drums whose settings are all neutral. Not done
    yet, because switching them on mid-playback could click.

## D-C13 — Plugin wiring choices

`Source/Parameters.h`, `Source/PluginProcessor.*`.

- **One table defines all 97 parameters:** ID, name, group, range, default, unit, and where the
  value goes in the DSP. The JUCE layout and the per-block copy can't drift apart.
  - Parameters are grouped by scope (Global, Quick mix, Kick, …), so FL's automation menu stays
    readable.
  - Frequency knobs use a log-feeling range: EQ mid centred at 1 kHz, crash low cut at 150 Hz,
    HPF at 150 Hz, LPF at 5 kHz.
- **Parameters.h is header-only,** so `CMakeLists.txt` did not change.
- **Bypass:** `global_bypass` is also the host's bypass parameter. Bypassing crossfades to the
  delayed dry signal and the 50 ms latency stays reported, so FL doesn't shift the track.
- **Link:** a 30 Hz timer on the message thread watches the three group volumes. When exactly one
  of them moved, the other two move by the same dB (clamped to their range).
  - It never runs on the audio thread.
  - Moves of several at once (automation, a preset) only re-base the offsets.
  - `VERIFY(host)`: how that feels with FL automation.
- **Saved state beyond the parameters:** `expected_types`, `detect_mode` (manual / auto) and
  `ui_zoom`, plus `state_version = 1`.
- **Auto detect ends on transport stop:** when playback stops, the types heard become the expected
  types, and the mode returns to manual.
- **Not saved (deviation from docs/02-parameters.md §5):** the learned per-type spectra. They
  re-learn within about 4 hits of each drum, and saving them safely would need a lock-free snapshot
  from the audio thread. Candidate for later.
- **Checked here:** the plugin builds on Linux (JUCE 9.0.3, Release, LTO) and pluginval passes at
  strictness 5 at 44.1/48/96 kHz, blocks 64–1024.

## D-C14 — The Puppet Show phase 2

- Wrote `prompts/04-puppet-show.md` (requirements only) and Claude's blind answer,
  `prompts/answers/04-claude.md`: measurements, genre targets, correction math, timing and safety.
- **Deliberately not built yet:**
  - The protocol's phase 2 compares Claude with the agents, and the genre targets are the part
    most likely to be wrong.
  - The `ps_*` parameters exist, so sessions stay compatible, but they do nothing yet.
- **Update (user, 2026-09-30):**
  - v1 targets = the **average** of the agents' answers to prompt 05 (one fixed table, so
    averaging is mechanical, cell by cell).
  - v2 = measured from the user's genre-named sample packs.
  - Claude's blind table (`prompts/answers/05-claude.md`) is averaged in as one more answer with
    equal weight, unless the user says agents only.
- **Please:** paste prompt 05 to your two agents.

## D-C15 — Puppet Show v1 genre targets

`docs/05-puppet-show-targets-v1.md`.

- **As the user decided:** the targets, tolerances and knob spans / max boost / max cut are the
  average of the three answers to prompt 05 (Claude blind, agent 1, agent 2, equal weight).
- **Claude's additions:**
  - **Real-kit check:** the answers were checked on 43 real E-GMD kits.
  - **Mud, snare presence, cymbal air:** they fit, so they're used as averaged.
  - **Kick sub and kick click:** these were physically impossible (a kick's energy is almost all
    below 90 Hz; its click is tiny in energy). So the answers' differences between genres are kept
    and the row is anchored to the real-kit median.
  - **Kick sub is redefined** as sub vs the rest, because "sub / total" saturates near 0 dB.
- **To undo:** use the plain averages in `research/puppet-show/average_targets.py`'s first table.
  Kick sub and click would then almost never be reachable, so the EQ would push every kick to
  its max cut or boost.

## D-C16 — The Puppet Show engine

`dsp/include/plugin_dsp/PuppetShow.h`, wired in `DrumEngine.h`.

Built from Claude's prompt-04 design. The agents answered only the targets (prompt 05); prompt 04
is optional.

- **Listening:** always on, even while switched off, so it acts the moment you switch it on.
  - It measures clean, single-type, confident hits from their rise spectrum.
  - A band the hit added nothing to isn't measured.
  - After 4 hits, one odd hit can't move the average by more than 12 dB.
  - The average covers about 12 hits.
- **Correcting:**
  - aim = target + span × knob. For Mud it's minus, so + means cut more.
  - Soft dead zone: the genre tolerance.
  - gain = Depth × 0.8 × error, clamped to the knob's max boost / cut.
  - Nothing before 4 hits; boosts at half size before 12 hits.
- **Glide:**
  - towards a stronger correction: τ 1.5 s (cuts 1 s);
  - back: τ 3 s;
  - Off or Depth 0: to exactly 0 in about 0.2 s.
- **Where it acts:** 3 reserved EQ slots in each drum's own chain (the user's knobs use the other
  8), so it acts only while that drum plays.

  | Knob | Drums | Band |
  |---|---|---|
  | Bass boost | kick | low shelf 70 Hz |
  | Mud control | kick / snare / toms (misc) | bells at 250 / 400 / 300 Hz |
  | Mid presence | snare, clap | bell 3 kHz |
  | Mid presence (click) | kick | bell 3.5 kHz |
  | High control | hats, cymbals | high shelf 8 kHz |

  It measures the raw hits, before the user's own settings, so it never fights the user's EQ.
- **Meters:** `meterDb()` is safe to read from the UI thread (phase 4).
- **Not built (v1):** the "harsh peak" bell from the prompt-04 design. The genre profiles
  (808 / acoustic kick …) suggested by agent 1 are parked for later.
- **Checked:**
  - 12/12 tests on gcc, Clang and ASan/UBSan: measurement math, no action before 4 hits, the
    correction formula, limits, the knob signs, off → 0.
  - With The Puppet Show on and nothing measured, the engine is still bit-exact transparent.
  - Plugin rebuilt; pluginval passes at strictness 5.
  - On real MDB loops the corrections stay within their limits.

---

## Classifier results

- **Measured with:** the plugin's own onset detector and classifier (`research/classifier-eval`)
  on real drums the network never trained on.
- **Scoring:** per-type F1. A type counts as "said" when its weight ≥ 0.5, and a detection matches
  a labelled hit within 25 ms.
- **Onset detection is unchanged:** acoustic F1 0.879, electronic 0.975.

| Type | Acoustic (MDB Drums, 23 tracks): rules → **learned** | Electronic loops (40): rules → **learned** |
|---|---|---|
| kick | 0.63 → **0.83** | 0.78 → **0.78** |
| snare | 0.61 → **0.65** (precision 0.48 → 0.75) | 0.24 → **0.53** |
| closed hat | 0.27 → **0.51** | 0.22 → **0.58** |
| open hat | 0.00 → 0.06 | 0.32 → 0.23 |
| crash | 0.03 → 0.14 | 0.06 → 0.10 |
| ride | 0.01 → **0.35** | 0.00 → 0.08 |
| rimshot | 0.56 → 0.42 | 0.00 → 0.14 |
| clap | none in the set; false claps 2205 → 283 | 0.50 → 0.33 |

**Also measured:**
- On 6 E-GMD kits held out of training, including 808, lo-fi, metal and jazz kits: macro F1 0.59,
  kick 0.80, snare 0.75, closed hat 0.64, ride 0.66.
- On Groove performances held out of training: macro F1 0.85.

**What this means:**
- **Kick, snare and closed hat** — most of every loop — are clearly better and usable.
- **Open hat vs closed hat, crash vs ride, and claps** are still weak. When unsure these fall to
  Misc, which is neutral, or to the neighbouring type.
  - A wrong neighbour mostly means the hi-hat or cymbal settings of the wrong page get used.
  - `VERIFY(dsp)`: listen at home.
- **The synthetic test kit's snare** (almost pure bright noise) is read as a closed hat. A very
  noisy electronic snare could get hat settings. `VERIFY(dsp)` with real 909-style snares.
- **Best next data:** your genre-named sample packs (v2), for claps, open hats and cymbals.

**Choices made while training (all in `research/classifier-train/`):**
- **Network:** 96 hidden units, not 48. Slightly better, and still only about 22 k
  multiply-adds per hit.
- **E-GMD:** 30 performances per kit, 350 k events sampled.
- **Synthetic loops:** 1000, for about 6 000 claps.
- **EQ augmentation:** ±6 dB tilt and ±6 dB bumps.
- **Level augmentation** (−24…+6 dB on every row): quiet loops were being read as hats.
- **Kit hold-out:** Acoustic Kit, 808 Simple, Heavy Metal, Nu RNB, Cassette (Lo-Fi), Jazz.
- **The unit test** (`test_hit_classifier.cpp`) is now a sanity check on the synthetic kit: every
  kick right, isolated ≥ 60%, dense loop ≥ 85%, multi-label, mask and presence. The
  accuracy target is the table above.
