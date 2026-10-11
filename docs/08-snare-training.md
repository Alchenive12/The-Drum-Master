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

MHA is in neither training set yet: waiting for the author's written OK.

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
