<!-- File: prompts/06-argon-training-plan.md -->
# Prompt 06: Argon reviews the training plan for the drum detectors

**How to use:** copy everything inside the box and paste it to Argon (he can read the GitHub repo).
Paste his reply back to Claude → `prompts/answers/06-argon.md`.

```text
You reviewed the DSP code of my drum plugin "The Drum Master" on 2026-10-14 (your 8 findings are in
docs/03-decisions-for-review.md, D-C18). This time I want your critical review of how we TRAIN its
drum detectors, and your own plan for what to do next. Don't just agree with the plan below: look for
mistakes in the method, the measurements and the priorities, and say what you would do instead.

REPO
GitHub: Alchenive12/The-Drum-Master. Branch: home/full-length-training (latest work; CI green on
Linux + Windows). Start with:
- CLAUDE.md, REMOTE_LOG.md (project memory, every commit with its verification level)
- docs/05-features.md, section "F4 - Loop learning" (plan, history, all results so far, incl.
  "Home GPU run (2026-10-15)")
- docs/testing/results/2026-10-15-home-gpu-training.txt (raw numbers of the latest run)
- docs/08-snare-training.md (earlier fine-tuning on my packs), docs/09-detection-without-training.md
  (post-processing ideas that were measured and mostly dropped)
- research/loop-learning/: home_packs.py (my packs -> labelled kit loops), extract_long.cpp (the
  plugin's own detector + classifier over a labelled set; writes per-hit features), train_slow_torch.py
  (PyTorch trainer, used for both models), train_fast_torch.py, vote_eval.py, export_mlp2.py,
  par_extract.py, home-run/ (the exact jobs that ran on my PC)
- dsp/include/plugin_dsp/: OnsetDetector.h, HitFeatures.h (the live picture), HitClassifier.h,
  TinyMlp.h (TinyMlp + TinyMlp2), HitClassifierWeights.h (generated)

WHAT THE PLUGIN DOES
An insert effect (VST3, FL Studio on Windows) on a mixed drum loop or beat. It detects every hit
and labels it kick / snare / closed hat / open hat / crash / ride / rimshot / clap / misc
(multi-label: kick + hat at once is both), then applies that drum's own mix settings only to its
frequency range and only for the hit's duration. 50 ms lookahead (reported as latency). The label
has to be decided 40 ms after the onset. Pure C++ in dsp/ (no JUCE, no ML runtime, no allocations
in processBlock). Uncertain hits go to "misc", which is neutral, so a wrong guess must never sound bad.

THE TWO MODELS
1. Live ("fast") classifier - runs in the plugin today.
   Input: HitFeatures, 225 numbers = 32 log-mel bands x 7 time segments from -25 to +40 ms around the
   onset (dB relative to the hit's post-onset peak) + the peak level.
   Now: 225 -> 512 -> 256 -> 9 sigmoid (TinyMlp2, ~250k multiply-adds per hit), trained from scratch
   on the GPU. Before: 225 -> 224 -> 9, fine-tuned on my packs (docs/08), plus the first CC-BY network
   (225 -> 96 -> 9) running next to it with a per-type max (docs/09). That second network is now OFF:
   with the new model it re-added false snares (electronic snare 0.71 -> 0.53).
2. Full-length ("slow") model - research only, not in the plugin yet. Meant for F4 "loop learning":
   the plugin studies a loop with no time limit, groups repeats of the same sample, labels each group,
   and hangs a string (fingerprint) per sound.
   Input: 643 numbers = 40 mel bands x 16 time segments from -30 ms to +1100 ms + peak + time to
   the previous / next onset. Best single model: 643 -> 1024 -> 512 -> 9 (slow_r3_bal.npz).

DATA (audio never leaves my PC; only feature rows and weights do)
- My own sample packs: 6,984 one-shots labelled by file/folder name (keyword rules shared with
  dsp/tools/sample_packs.h) in 63 "packs" (KSHMR, Trap-hiphop, Future bass/bounce, MHA, SLCTD,
  Sickrate & SIIK, Splice, Cymatics, DABRO, FL Studio's own packs, ...). home_packs.py builds kit loops
  from them: each loop uses ONE sound per drum type from ONE pack, a random groove at 80-170 bpm, small
  timing/pitch jitter, labels from the placement. Split BY PACK (hash, ~20 % of packs never trained on
  = "home_test"). Note: Splice downloads count as many 1-sample "packs".
  home_train 1,298 loops (34,610 hits) + home2_train 2,005 more loops (other seed, 104,864 hits);
  home_test 378 loops (9,654 hits).
- 13 free MusicRadar SampleRadar packs: 3,511 labelled one-shots -> 1,217 loops (63,386 hits).
- Groove MIDI (CC BY 4.0, 235k hits; 15 % of files held out) and an E-GMD subset (CC BY 4.0,
  30 performances x 43 kits, 765k hits; 6 kits held out).
- 600 synthetic loops (27.5k hits).
- Training is "personal use": the pack licences (Splice explicitly) don't allow shipping models
  trained on them. A separate shippable model would use only Groove / E-GMD / synthetic.

TEST SETS (never trained on; scoring = research/classifier-eval/eval.cpp: a type is "said" when its
weight >= 0.5, misc weight = max(p_misc, 1 - best other p); F1 per type, misses of the onset
detector count as false negatives)
- acoustic: MDB Drums, 23 live acoustic tracks
- electronic: 40 loops from held-out Dirt-Samples one-shots, new sample per hit
- kit / electronic_kit: 40 loops each, one Dirt sample per drum reused (like a produced beat)
- home_test: my never-trained packs (see above)
- Leak check: of 22,460 training sounds, 1 is audio-identical to a Dirt test sound.

RESULTS (F1, plugin before this run -> now; full tables in the results file)
Full-length model alone (slow_r3_bal):
| F1         | my unheard packs | electronic  | kit loops   | acoustic (MDB) |
| kick       | 0.88 -> 0.91     | 0.81 -> 0.89 | 0.77 -> 0.89 | 0.82 -> 0.85   |
| snare      | 0.62 -> 0.76     | 0.50 -> 0.77 | 0.45 -> 0.81 | 0.70 -> 0.72   |
| closed hat | 0.85 -> 0.90     | 0.72 -> 0.75 | 0.72 -> 0.79 | 0.57 -> 0.70   |
| open hat   | 0.22 -> 0.48     | 0.39 -> 0.72 | 0.45 -> 0.76 | 0.11 -> 0.44   |
| crash      | 0.12 -> 0.65     | 0.12 -> 0.60 | 0.09 -> 0.51 | 0.14 -> 0.31   |
| ride       | 0.08 -> 0.65     | 0.21 -> 0.79 | 0.27 -> 0.74 | 0.37 -> 0.47   |
| clap       | 0.39 -> 0.65     | 0.45 -> 0.62 | 0.41 -> 0.65 | -              |
Averaging 4 trained slow models adds a bit (electronic snare 0.84, my packs snare 0.78). Rimshot stays
weak everywhere (0.13-0.46). Voting with the live model no longer helps.

Live classifier now in the plugin (exact: plugin's detector + classifier re-run on each set):
my packs snare 0.62 -> 0.65, clap 0.39 -> 0.56, crash 0.12 -> 0.24, ride 0.08 -> 0.25;
electronic snare 0.50 -> 0.68, open hat 0.39 -> 0.59, clap 0.45 -> 0.67;
acoustic snare 0.70 -> 0.65 (WORSE), closed hat 0.57 -> 0.63.
Fine-tuning the old 225-224-9 net instead gave small, mixed changes. 225-1024-512-9 was no better.

Target for v1: kick / snare / closed hat F1 >= 0.85 with >= 85 % precision; every other type >= 70 %
precision when it fires.

THINGS CLAUDE ALREADY SUSPECTS ARE WRONG OR WEAK
- Validation leak: train_slow_torch.py picks its validation rows AFTER oversampling + augmentation, so
  copies of training rows are in it. Early stopping and "val loss" are optimistic. Bigger runs were
  still improving at their last epoch (undertrained?).
- Noisy test: one pack split, one seed. Some types have few test hits (home_test crash 100, rimshot
  206), so differences of ~0.03 may be noise.
- Labels come from file names ("stick" -> rimshot, "oh" -> open hat, ...). Label noise is unmeasured.
- Kit loops are clean: drums only, no bass / synths / vocals underneath, no bus processing, no reverb,
  even though the plugin may sit on a full beat.
- The live model got slightly worse on live acoustic drums.
- The oversampling of rare labels was multiplied by the per-file weights, so rows could repeat up to
  24x (now capped). r1/r2 were trained before the fix; slow_r3_bal and the live model after it.

CLAUDE'S PROPOSED PLAN (critique it)
1. Fix the measurement: validation by whole files / loops; longer cosine-decay runs; 5-fold
   rotation of held-out packs x 3 seeds, so every number has an error bar.
2. Better data where it is weak (rimshot, open hat, crash, ride):
   - audit labels (per pack, list the most-confused sounds for me to check by ear);
   - put my packs' melodic / bass / FX loops underneath the kit loops + random reverb, compression,
     sidechain pumping, pitch shift, so the models learn drums inside a full mix;
   - rebuild beats from my own FL Studio projects (.flp parsed with PyFLP: real patterns, real kit
     choices, exact labels);
   - targeted downloads only for the weak classes.
3. Distillation: run the slow-model ensemble over all the UNLABELLED drum loops on my PC, keep only
   very confident hits, and train the live model on them (lots of real, mixed hits for free).
4. Ship the slow model in the plugin: compute the 643-number picture in C++, run it with TinyMlp2, and
   build F4 loop learning on it; real use in FL then produces new training material.
5. My "misheard" folder: when I hear a wrong label in FL, I save that audio; those clips become the
   most valuable training examples.
Two decisions for me: (a) a latency selector (60-80 ms instead of 50) - earlier measurements (docs/08
§8-9) show hats / cymbals / claps gain the most from a longer window; (b) whether to keep protecting
live acoustic drums, or let the live model specialise in producer drums.

Practical limits: one RTX 3060 Ti (8 GB), i7-11700K, 32 GB RAM. One training run is 5-20 min. Features
are cached, so new experiments don't need re-extraction. Claude drives the PC remotely through a job
runner, so sessions should be planned (each one takes an evening). Inference must stay in plain C++
inside a real-time plugin (no ONNX if avoidable), cheap enough for many hits per second.

WHAT I WANT FROM YOU
1. Problems: anything wrong or risky in how we train, split, validate, score or compare (with file
   and line where possible). Which of the numbers above do you NOT trust, and why?
2. Your ranked plan for the next 3 sessions at home, with the expected gain of each step and how we'll
   know it worked (which number, on which set, by how much beyond the noise).
3. Model: is the MLP on a mel picture the right family? Would a small CNN / temporal conv / other
   input (e.g. different time resolution, onset-aligned spectrogram, harmonic features) be worth it,
   given that inference must be hand-written C++? Rough CPU cost per hit for what you suggest.
4. Data: what would you add, generate or fix first? Is the kit-loop generator a good proxy for real
   beats, and how would you make it closer? Is distillation from the slow model sound here, or will it
   just copy the slow model's mistakes?
5. Rimshot and open hat specifically: why are they so bad, and what would fix them?
6. The live model vs. latency: is a 60-80 ms option worth it, or is there a smarter way (e.g. a quick
   label at 40 ms corrected a few tens of ms later)?
7. Anything we are not looking at that we should.
Please be concrete (file names, numbers, experiments), and say where you are guessing.
```
