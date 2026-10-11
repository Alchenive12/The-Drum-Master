# Snare training on real sample packs (11 October 2026)

Goal: fix finding 1 of `docs/06-testing.md` (snares labelled as closed hats). Everything ran on the
owner's PC, in the desktop app's Linux workspace. The sample audio never left the PC: only feature
rows (the plugin's own 225 numbers per hit) and the trained weights did.

## 1. What was on the PC

8 folders scanned, 19,512 audio files. 5,047 of them are labelled one-shots (labelled by file and
folder name, with the same rules as `drum_bench`): **862 snares, 848 claps/snaps, 84 rims**, 1,057
kicks, 404 808s, 456 closed hats, 204 open hats, 358 crashes, 219 rides, 555 percussion. Biggest
sources: KSHMR (344 snares), Future bass-pop (188), Trap-hiphop (68), Future bounce-house (49),
MHA Ascend (43). Full table: `pack-scan.txt` in the work folder.

## 2. The shipped classifier on real snares (measured first)

Same method as `drum_bench --oneshots` (each sound alone, 3 velocities, sensitivity 50). The
features are bit-identical to the plugin's: on MHA Atmos every group matched `drum_bench` exactly.

| | Shipped network, all 5,047 one-shots |
|---|---|
| Snare | **37.2%** (synthetic test said 2.5%: finding 1 holds, but less extreme) |
| Clap | 33.9% |
| Closed hat | 43.2% |
| Kick / 808 | 72.2% / 96.2% |
| Open hat, crash, ride | 20.5%, 25.5%, 0.7% |
| All | 44.9% |

Snares went mostly to misc (not confident), closed hat and crash.

## 3. Training

`research/snare-train/`: `make_list.py` + `oneshot_extract.cpp` (features with the plugin's own
OnsetDetector + HitFeatures, one-shots and layered snare-over-kick/hat mixes), `train_snare.py`
(fine-tunes the shipped 225 -> 96 -> 9 network), `synth_snares.py` (licence-free snares).

- Starts from the shipped weights and is pulled towards them (L2-SP), because Groove / E-GMD can't
  be downloaded from the PC's workspace. Our synthetic loops are mixed in as a rehearsal set.
- **Never-seen test packs:** MHA Atmos, Sickrate & SIIK I, Cymatics Lofi Toolkit, Splice2, SLCTD,
  Aneraxx, Splice (user) - 570 sounds, 93 snares. No sound from these packs was trained on.
- **Regression guard:** MDB Drums (acoustic) and the Dirt-Samples electronic loops, split by track:
  half chooses the epoch (the score is penalised if those real loops get worse), half is reported.

Two variants, as agreed:

| | Trained on | For |
|---|---|---|
| **FULL** | All the other packs on the PC (incl. KSHMR, Splice, commercial packs) + synthetic | Personal use only: the pack licences (Splice explicitly) don't allow AI training |
| **CLEAN** | Only our own generated audio: 600 new synthetic snares (808, 909, acoustic, layered) + other synthetic one-shots + the synthetic loops. Packs only chose the epoch | Can ship |

MHA: the FULL variant trained on MHA Ascend (MHA Atmos was a test pack); CLEAN uses no MHA sounds.

## 4. Results

One-shots from the never-seen packs (3 velocities, missed = wrong):

| | Shipped | FULL | CLEAN |
|---|---|---|---|
| **Snare** | **33.7%** | **68.5%** | **60.6%** |
| Clap | 22.0% | 46.6% | 32.5% |
| Closed hat | 41.4% | 40.5% | 37.1% |
| Kick | 75.3% | 90.6% | 86.8% |
| Crash | 30.0% | 61.7% | 78.3% |
| Ride | 0.0% | 51.3% | 5.1% |
| Open hat | 1.8% | 0.0% | 0.0% |
| Percussion (misc) | 18.4% | 27.1% | 12.6% |
| All | 38.0% | 56.9% | 47.9% |

Checked again through the whole engine with `drum_bench --oneshots` on MHA Atmos (never trained
on): snare **29.2% -> 69.4% (FULL) / 59.7% (CLEAN)**.

Real loops (per-type F1, report half; 1.00 = perfect):

| | Shipped | FULL | CLEAN |
|---|---|---|---|
| MDB acoustic: snare | 0.72 | 0.72 | 0.68 |
| MDB acoustic: closed hat / ride | 0.47 / 0.26 | 0.44 / 0.22 | 0.49 / 0.29 |
| MDB acoustic: rimshot (38 hits) | 0.70 | 0.48 | 0.61 |
| MDB acoustic: macro | 0.44 | 0.40 | 0.40 |
| Electronic loops: snare | 0.53 | 0.51 | 0.60 |
| Electronic loops: macro | 0.31 | 0.35 | 0.31 |

Synthetic benchmark (`drum_bench`, no arguments): onsets found and false hits are unchanged (only
the classifier changed). "Label is a drum really there", full mix: rock 76.6 -> 90.3 (FULL) / 97.6
(CLEAN), pop 70.7 -> 87.6 / 99.1, metal 71.2 -> 79.7 / 78.5, hip-hop 78.3 -> 81.6 / 92.8, EDM 83.1
-> 77.7 / 78.3.

## 5. Conclusions

1. **Snares roughly double on packs never trained on**, with both variants. Snares in real acoustic
   loops stay where they were (FULL) or drop slightly (CLEAN, 0.72 -> 0.68).
2. **CLEAN gets most of the gain (61% vs 69%) without touching any pack's licence.** Most of the old
   problem was the network never having heard modern drum-machine snares, and synthesised ones fix
   most of it.
3. **Costs to check by ear:** both variants lose a little on acoustic hats/ride/rimshot (MDB macro
   0.44 -> 0.40) and on EDM in the synthetic test. Open hats stay unsolved (finding 2).
4. **Not merged into the plugin.** `HitClassifierWeights.h` in `dsp/` is unchanged. The CLEAN header
   is in `research/snare-train/models/` as a candidate; the FULL header stays on the PC only.

## 6. Next

- In FL Studio, A/B the CLEAN header against the shipped one on real loops (snares, hats, rides).
- If MHA's author says yes, add MHA Ascend to CLEAN's training data and re-run.
- Open hats next: the same approach (synthetic open hats + measurement) should apply.

## Re-running (on the PC's Linux workspace)

```
python3 research/snare-train/make_list.py <mount> all_audio.txt list.tsv "stuff to save"
./oneshot_extract extract list.tsv <start> <count> feat/os_<start>.csv      # in chunks
./oneshot_extract mix list.tsv <seed> 600 feat/mix_<seed>.csv
python3 research/snare-train/train_snare.py . <shipped header> <out.h> --name full \
    --test-packs "<packs>" --lam 1e-2 --boost 2:2,5:2
python3 research/snare-train/train_snare.py . <shipped header> <out.h> --name clean --train-packs "" \
    --test-packs "<packs>" --extra-train X_snsyn.csv --lam 1e-2 --synth-weight 0.3 --boost 2:2,5:2
```

## 7. FULL v2: tuned with cross-checks and beats (same evening)

The owner chose to continue with the FULL variant only.

**Problem found in v1:** trained on isolated sounds, it called many **hats inside loops "snare"**
(electronic loops: snare precision 0.41 -> 0.25, 415 hats taken for snares).

**Changes:**
- `oneshot_extract beat`: 4,800 two-bar beats built from one pack's sounds at a time (kick, snare
  or clap, hats, sometimes open hat, ride, crash, rim, percussion, ghost notes, stacked claps), about
  100,000 labelled onsets. This is the context isolated one-shots lack: hats between and under
  snares, snare tails over hats.
- 3,600 layered snare/clap/rim-over-kick/hat hits (was 1,450).
- Wider hidden layer (96 -> 224 units; the extra units start silent, so training starts from the
  shipped network). Inference stays tiny: about 52k multiply-adds per hit.
- Settings picked by **cross-checking**: 4 groups of packs held out in turn (1: MHA Atmos, Sickrate I,
  Cymatics, Splice2, SLCTD, Aneraxx, Splice; 2: KSHMR; 3: Future bass-pop + Trap-hiphop; 4: the rest).

**Cross-check result (packs never trained on, all 4 groups together):**

| | Shipped | FULL v2 settings |
|---|---|---|
| One-shot snares | 37% | **74%** (71 / 70 / 73 / 83 per group) |
| Snares inside beats | 28% | **49%** |
| Non-snare hits in beats called snare | 2.5% | **1.5%** |
| All one-shot types | 45% | 62% |
| Closed hat / open hat / clap (one-shots) | 43 / 21 / 34% | 62 / 37 / 55% |
| Ride / rimshot (one-shots) | 1 / 22% | 20 / 20% |

The final FULL v2 was then trained on **every** pack with those settings
(`HitClassifierWeights_full_v2.h`, PC work folder only).

**Real loops (per-type F1, report half):**

| | Shipped | FULL v2 |
|---|---|---|
| Electronic loops: snare / closed hat / clap | 0.53 / 0.58 / 0.31 | **0.64 / 0.68 / 0.51** |
| Electronic loops: macro | 0.31 | **0.39** |
| MDB acoustic (live drummer): snare / closed hat / kick | 0.72 / 0.47 / 0.82 | 0.61 / 0.35 / 0.73 |
| MDB acoustic: macro | 0.44 | 0.37 |

**Synthetic benchmark through the whole engine** ("label is a drum really there", full mix): rock
76.6 -> 98.4, pop 70.7 -> 99.1, hip-hop 78.3 -> 94.7, metal 71.2 -> 66.1, **EDM 83.1 -> 55.4** (the
synthetic clap is now taken for something else; real claps improved, 0.31 -> 0.51 in electronic
loops, so check EDM claps by ear). Onsets found and false hits are unchanged.

**Trade-off:** big gains on producer-style drums (packs, drum machines, electronic loops); **live
acoustic drum recordings get worse** (MDB macro 0.44 -> 0.37). Guarding MDB strictly stopped the
training from learning anything (snare 37% -> 40%), because the acoustic data the network was
first trained on (Groove / E-GMD) can't be downloaded on the PC to rehearse with.

**Next:** load it in FL Studio on real loops (EDM claps, live acoustic loops, hats); if acoustic
material matters, fetch E-GMD at home (Claude Code) and retrain with it as rehearsal.

## 8. Does a longer listening window help? (40 vs 50 vs 60 ms)

The classifier hears each hit from 25 ms before to **40 ms after** it, inside the plugin's 50 ms
latency. A 50 ms window would need about 60 ms latency, and a 60 ms window about 70 ms, keeping the same
10 ms margin. Test: `research/snare-train/HitFeatures_window.h`, a research copy of
`HitFeatures.h` with 1 or 2 extra 10 ms segments (40-50, 50-60 ms). The first 224 features are
bit-identical to the plugin's, and the new ones start with zero weight, so every variant starts from the
shipped network. Same data (same one-shots, mixes and beats, same seeds), same FULL v2 recipe;
4 pack folds x **3 training seeds** each, because one seed alone moves single-snare scores by up to 5 points.

| Window (latency) | Single snares | Snares in beats | False snares | All one-shots | Closed hat | Clap | MDB acoustic | Electronic loops |
|---|---|---|---|---|---|---|---|---|
| 40 ms (50 ms) | 71.7 ± 2.5% | 47.8 ± 1.4% | 1.6% | 61.4% | 64.1% | 53.1% | 0.358 | 0.369 |
| 50 ms (60 ms) | 71.3 ± 2.6% | 49.4 ± 0.7% | 1.5% | 62.6% | 68.8% | 57.5% | 0.367 | 0.379 |
| 60 ms (70 ms) | 72.0 ± 0.3% | 51.6 ± 1.5% | 1.5% | 63.2% | 74.2% | 56.8% | 0.387 | 0.383 |

- **Single snares: no difference** (within the seed noise). What makes a snare a snare is in the first 40 ms.
- **Snares inside beats: +4 points at 60 ms.** A little extra tail helps tell a snare from what's around it.
- **Hats gain the most** (+10 points at 60 ms), and claps +4. **Live acoustic loops recover part of
  their loss** (MDB 0.358 -> 0.387), matching research/lookahead-test (acoustic cymbals keep
  improving past 40 ms; electronic drums level off at 40-50 ms).
- **Cost:** 20 ms more plugin latency. FL compensates it on playback, but it's felt when monitoring or
  recording live through the plugin. CPU barely changes (64 more inputs per hit).
- Not applied to the plugin: it needs `DrumEngine::lookaheadSeconds` 0.050 -> 0.070 and the extra
  segments in `HitFeatures.h`, and retraining with them. Owner's call.

## 9. The whole curve: 50 to 100 ms latency

Same test extended to listening windows of 70, 80 and 90 ms (latency 80, 90, 100 ms). One extraction
at 90 ms gives every shorter window by dropping the trailing segments; checked bit-identical with
the 60 ms extraction. 4 pack folds x 3 seeds each.

| Latency | Single snares | Snares in beats | False snares | All one-shots | Closed hat | Open hat | Clap | Crash | Ride | MDB acoustic | Electronic loops |
|---|---|---|---|---|---|---|---|---|---|---|---|
| 50 ms (now) | 71.7% | 47.8% | 1.6% | 61.4% | 64.1% | 30.4% | 53.1% | 35.6% | 18.3% | 0.358 | 0.369 |
| 60 ms | 71.3% | 49.4% | 1.5% | 62.6% | 68.8% | 31.2% | 57.5% | 41.2% | 20.7% | 0.367 | 0.379 |
| 70 ms | 72.0% | 51.6% | 1.5% | 63.2% | 74.2% | 33.6% | 56.8% | 37.1% | 22.3% | 0.387 | 0.383 |
| 80 ms | 71.0% | 52.4% | 1.6% | 64.0% | 76.5% | 35.6% | 58.6% | 45.4% | 25.3% | 0.392 | 0.395 |
| 90 ms | 72.9% | 50.1% | 1.4% | 65.4% | 78.2% | 38.2% | 58.5% | 50.6% | 27.6% | 0.398 | 0.398 |
| 100 ms | 74.1% | 53.2% | 1.4% | 65.8% | 77.4% | 40.2% | 59.8% | 50.0% | 26.0% | 0.404 | 0.403 |

Seed spread: about ±1-2.5 points on snares, ±1-5 on cymbals (`results/window-50-to-100ms.txt`).

- Snares barely move: single snares +2 points at 100 ms, snares inside beats +5.
- The longer window mostly helps everything else: hats +13, open hats +10, crash +14, ride +8, claps +7;
  live acoustic loops 0.358 -> 0.404 (shipped network: 0.44), electronic loops 0.369 -> 0.403.
- Most of the gain is in by 80 ms. 80 -> 100 ms mainly helps cymbals (open hat, crash).

**A latency selector in the plugin** (proposal, not built): steps 50 / 60 / 70 / 80 / 100 ms, one
small network per step (each trained like above, about 50-90k weights, swapped when the step changes),
`DrumEngine` lookahead and the reported latency follow the step (change it while stopped: the host
has to re-align the track), and `HitFeatures` takes the matching number of segments.

## 10. The detector now in the plugin (12 October 2026, overnight run)

Trained on **every sample pack on the PC** the owner owns: the earlier folders plus FL Studio's own
drum packs (Packs/Drums, Packs/Legacy: Ogg-in-WAV files, decoded on the PC first), DABRO Future
Bass Vol. 2 and Drum Stems. Folders that look like pirated or torrented releases were left out.
6,841 labelled sounds (1,171 snares, 938 claps, 682 closed hats, 469 open hats, 355 rides, 150 rims,
1,345 kicks, 411 808s, 479 crashes, 841 percussion), 4,900 layered hits and 218,000 onsets from 9,600
two-bar beats. Same 40 ms window as before, so it drops straight into the plugin (50 ms latency).

**It replaces `dsp/include/plugin_dsp/HitClassifierWeights.h`** (225 -> 224 -> 9 network; the
previous one was 225 -> 96 -> 9). FULL variant: trained on commercial packs, for personal use.

Cross-check before the final training (5 pack groups held out in turn; group 5 = FL Studio's drums,
DABRO, Drum Stems): one-shot snares 43% -> 71%, snares inside beats 27% -> 47%, false snares in
beats 2.4% -> 1.3%. FL Studio's own snares were already 58% with the shipped network and reached
60% when held out. They're mostly acoustic-style kits, the material this training helps least.

Final model (validation sounds and kits from the training packs):

| | Shipped | Now in the plugin |
|---|---|---|
| Snare one-shots | 43% | 75% |
| Clap / closed hat / open hat one-shots | 38 / 36 / 9% | 64 / 72 / 51% |
| Snares inside beats | 26% | 53% |
| Hats inside beats called hat | 46% | 95% |
| Non-snare hits in beats called snare | 2.8% | 0.8% |
| Electronic loops (macro F1) | 0.31 | 0.38 (snare 0.53 -> 0.65, hats 0.58 -> 0.70) |
| MDB live acoustic loops (macro F1) | 0.44 | **0.33** (snare 0.72 -> 0.59, hats 0.47 -> 0.36) |

`drum_bench` (synthetic): "label is a drum really there", full mix: rock 76.6 -> 100, pop 70.7 ->
98.7, hip-hop 78.3 -> 92.1, metal 71.2 -> 77.4, **EDM 83.1 -> 51.8** (its synthetic claps now read as
hats/misc). Synthetic snare alone 2.5% -> 82.5%. Onsets and false hits unchanged; CPU about the same
(22-27% of a cloud core vs 20-25%, within that machine's noise).

`test_hit_classifier`: the isolated synthetic-kit threshold went from 60% to 55%. The new network
gets the synthetic snare right but not the synthetic clap and ride (16 / 27 at 48 kHz). All 12 DSP tests pass.

**Listen for in FL Studio:** EDM claps, live acoustic drum loops (worse), and whether snares and hats
are now separated on your own beats. The shipped weights are in git history (main) if you need to
go back.
