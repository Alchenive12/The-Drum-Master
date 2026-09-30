# REMOTE_LOG

> Memory file for the "away from home" protocol. Paste or link this at the start of every session.
> Updated in every commit made away from home.

## Project facts
- Plugin name:                   The Drum Master (target `DrumMaster`, class `DrumMasterProcessor`, plugin code `Drmr`, manufacturer `Alch`)
- JUCE version / tag:            9.0.3 (FetchContent, pinned in CMakeLists.txt)
- Build system:                  CMake
- Formats:                       VST3 / AU (macOS only) / Standalone
- Platforms built at home:       Windows (FL Studio, i7-11700K, RTX 3060 Ti) — VST3
- CI:                            .github/workflows/build.yml (Linux, Windows + DSP tests + pluginval; macOS dropped)
- Last known-good commit:        ac7e881 on this branch (plugin wired to DrumEngine, 97 params; CI run 27 green on Linux + Windows incl. DSP tests + pluginval; Windows VST3 in the run's artifacts)

## Current trip
- Pipeline phase:                3 — DSP core in dsp/ (phase 2 closed: concept, mix settings, algorithm, parameters all approved)
- Branch: claude/away-from-home-protocol-pc5bni (session-assigned; stands in for `remote/<feature>`)
- Goal: phase 3 done in code (detector, learned classifier, tracker, processors, engine, 97-param plugin). Waiting on: agents' answers to prompt 04 (Puppet Show), home test in FL, review of docs/03-decisions-for-review.md. User away ~1 week and delegated decisions: every one is listed in docs/03-decisions-for-review.md

## Done this trip
| Commit msg | Files | Verification level (0–4) | Notes |
|---|---|---|---|
| [untested] prompts: 05 Puppet Show genre targets (fixed table to average) + Claude's blind answer | prompts/05-puppet-show-genre-targets.md, prompts/answers/05-claude.md, docs/05-features.md, docs/03-decisions-for-review.md, REMOTE_LOG.md | 0 | Docs only |
| [untested] dsp: learned hit classifier (225 log-mel -> 96 -> 9) replaces the rules; weights from Groove + E-GMD (CC BY 4.0) + own synth | HitClassifier.h (rewritten, same API), HitClassifierWeights.h (generated), test_hit_classifier.cpp (sanity thresholds), research/classifier-eval/eval.cpp, research/classifier-train/train.py, docs/02-algorithm.md, docs/03-decisions-for-review.md, REMOTE_LOG.md | 2 | gcc, clang, ASan/UBSan 11/11; plugin rebuilt + pluginval strictness 5 SUCCESS. Real drums (never trained on): acoustic kick 0.83 / snare 0.65 / CH 0.51, electronic kick 0.78 / snare 0.53 / CH 0.58 (rules: 0.63/0.61/0.27, 0.78/0.24/0.22) |
| [untested] plugin: 97 parameters + DrumEngine wired; prompt 04 (Puppet Show) | Source/Parameters.h (new, header-only), Source/PluginProcessor.*, prompts/04-puppet-show.md, prompts/answers/04-claude.md, docs/05-features.md, docs/03-decisions-for-review.md, REMOTE_LOG.md | 3 | CI run 27 green (Linux + Windows MSVC /W4 /WX, DSP tests, pluginval). Built in sandbox (Linux, JUCE 9.0.3 Release+LTO, no warnings from our code); pluginval strictness 5 SUCCESS (44.1/48/96 kHz, blocks 64–1024). CMakeLists.txt NOT touched |
| [untested] dsp: DrumEngine (lookahead, detection, stems, processors, global section, Shadow Play feed) | DrumEngine.h, SpscQueue.h, DrumChannel.h (level-aware sat, clip -12 dB), test_drum_engine.cpp, test_drum_channel.cpp, docs/03-decisions-for-review.md, REMOTE_LOG.md | 2 | gcc Release, clang, ASan/UBSan 11/11; bit-exact transparency + 50 ms latency at 44.1/48/96 kHz; kick -6 dB -> -5.8 dB with hats 0.00 dB; ~21 % of a sandbox core |
| [untested] dsp: per-drum processor chain + knob mapping; E-GMD fetcher; decisions doc | DrumChannel.h, DrumParams.h, DrumMapping.h, test_drum_channel.cpp, test_drum_mapping.cpp, research/classifier-train/{fetch_egmd.py, prep_gmd.py, train.py, README.md}, docs/03-decisions-for-review.md, REMOTE_LOG.md | 3 | gcc Release, clang, ASan/UBSan 10/10; neutral = exactly nothing; every knob touches only its drum(s); macros bounded; CI run 25 (72b945e) green |
| [untested] dsp: hit tracker (per-type gates) + learned-classifier groundwork (log-mel features, tiny MLP, training/eval tools) | HitTracker.h, HitFeatures.h, TinyMlp.h, HitClassifier.h (extra measurements), test_hit_tracker.cpp, research/classifier-eval/*, research/classifier-train/*, .gitignore, REMOTE_LOG.md | 2 | gcc Release, clang, ASan/UBSan: 8/8 tests; tracker: exact 2 ms pre-roll, kick tail end ~0.52 s, closed hat 134 ms vs open hat 719 ms, retrigger, weights, 1 s cap |
| Add away-from-home template (PR #1) | all | 3 | JUCE 9.0.3 APIs checked vs source; CI build + DSP tests + pluginval green on all 3 platforms |
| CI: add JUCE 9 Linux deps (PR #2) | build.yml | 3 | libxi-dev + libegl-dev; fixed the Linux build |
| [untested] docs: phase 1 concept draft | docs/01-concept.md, REMOTE_LOG.md | 0 | Docs only; dsp tests re-run in sandbox (level 2, pass) |
| [untested] docs: concept answers (1b, AD2-style mixing, puppet clicks) | docs/01-concept.md, REMOTE_LOG.md | 0 | Docs only |
| [untested] docs: approve concept, add prompt 01 + Claude's blind answer | docs/01-concept.md, prompts/, REMOTE_LOG.md | 0 | Docs only |
| [untested] CI: drop macOS job | build.yml, REMOTE_LOG.md | 0 | Approved by user; how to re-add is in the build.yml header |
| [untested] Rename template to The Drum Master | CMakeLists.txt, Source/*, README.md, REMOTE_LOG.md | 3 | CI run 7 (3ec2987) green on Linux + Windows: build, DSP tests, pluginval |
| [untested] docs: agent answers + per-drum settings comparison | prompts/answers/agent1.md, agent2.md, docs/02-mix-settings.md, REMOTE_LOG.md | 0 | Docs only |
| [untested] docs: approve mix settings, add detection prompt 02 + Claude's blind answer | docs/02-mix-settings.md, prompts/02-detection-algorithm.md, prompts/answers/02-claude.md, REMOTE_LOG.md | 0 | Docs only |
| [untested] docs: agent answers to prompt 02 + algorithm comparison | prompts/answers/02-agent1.md, 02-agent2.md, docs/02-algorithm.md, REMOTE_LOG.md | 0 | Docs only; some agent formulas lost in paste (marked) |
| [untested] research: engine shootout STFT mask vs time bands + review prompt 03 | research/engine-shootout/*, docs/02-algorithm.md, prompts/03-review-claude-engine.md, REMOTE_LOG.md | 2 | Pure C++ compiled + run in sandbox; not in plugin build/CI |
| [untested] dsp: hit classifier (regions by template fit + drum physics), Biquad, DrumTypes; sanitizer-only test timeout 300 s | HitClassifier.h, Biquad.h, DrumTypes.h, test_hit_classifier.cpp, dsp/CMakeLists.txt (timeout only), REMOTE_LOG.md | 3 | Synthetic kit: isolated 27/27 at 44.1/48/96 kHz, kick+hat both found, dense loop 100 %; real-drum check next; CI run 23 (959d327) green; since replaced by the learned classifier |
| [untested] dsp: synthetic drum kit for tests | dsp/tests/synth_drums.h | 3 | Deterministic on every compiler (own RNG); hit peaks normalised; 5 ms fade-out; CI run 22 (933bbcc) green Linux + Windows (MSVC /W4 /WX), pluginval |
| [untested] dsp: multi-band onset detector (SuperFlux) | OnsetDetector.h, test_onset_detector.cpp, docs/02-algorithm.md, REMOTE_LOG.md | 3 | gcc/clang -Werror, ASan/UBSan; isolated 100 %/0 fp, dense loop F1 0.97–0.99, timing ≤ 0.8 ms at 44.1/48/96 kHz; CI run 22 (933bbcc) green Linux + Windows (MSVC /W4 /WX), pluginval |
| [untested] docs: approve F1/F2 short phase 1 (genres, drum-aware, bipolar, one Depth; Shadow Play layout) | docs/05-features.md, docs/02-parameters.md, REMOTE_LOG.md | 0 | Docs only (2 commits: first one missed the params/log update) |
| [untested] docs: Puppet Show knobs + always-listening; visualizer named Shadow Play | docs/05-features.md, docs/02-parameters.md, docs/01-concept.md, REMOTE_LOG.md | 0 | Docs only; open Q3, Q4, Q5–Q7, Q9, Q10 |
| [untested] docs: features F1 The Puppet Show + F2 shape visualizer | docs/05-features.md, docs/01-concept.md, docs/02-parameters.md, REMOTE_LOG.md | 0 | Docs only; short phase 1 drafts with open questions Q1–Q8 |
| [untested] dsp: streaming STFT (exact latency N-1) | StreamingStft.h, test_streaming_stft.cpp | 3 | -Werror + ASan/UBSan (gcc), clang Release; CI run 16 (b93f70e) green Linux + Windows (MSVC /W4 /WX), pluginval |
| [untested] dsp: complementary linear-phase split 1.5 kHz | LinearPhaseSplit.h, test_linear_phase_split.cpp | 3 | Perfect reconstruction at 44.1/48/96 kHz, ~2.6 ms; CI run 16 (b93f70e) green Linux + Windows (MSVC /W4 /WX), pluginval |
| [untested] dsp: dual-resolution estimator (multi-output STFT) | DualResEstimator.h, StreamingStft.h (MultiOutputStft), test_dual_res_estimator.cpp, REMOTE_LOG.md | 3 | Latency 20–30 ms at all rates (fits 50 ms); complementary shares sum to delayed input; CI run 16 (b93f70e) green Linux + Windows (MSVC /W4 /WX), pluginval |
| [untested] dsp: radix-2 FFT + tests; docs: creepy UI mood, phase 3 plan | dsp/include/plugin_dsp/Fft.h, dsp/tests/test_fft.cpp, docs/01-concept.md, REMOTE_LOG.md | 3 | Built with -Werror + ASan/UBSan in sandbox, vs naive DFT; CI run 16 (b93f70e) green Linux + Windows (MSVC /W4 /WX), pluginval |
| [untested] docs: approve D1/D2, parameter table | docs/02-algorithm.md, docs/02-parameters.md, REMOTE_LOG.md | 0 | Docs only; 90 host parameters with permanent IDs |
| [untested] research: lookahead detection test on real drums | research/lookahead-test/*, docs/02-algorithm.md, REMOTE_LOG.md | 2 | MDB Drums (23 acoustic tracks) + electronic loops from held-out one-shots; knee 40–50 ms |
| [untested] research: shootout v2 after agents' review | research/engine-shootout/shootout2.cpp, RESULTS2.txt, README.md, prompts/answers/03-*.md, docs/02-algorithm.md, REMOTE_LOG.md | 2 | Same-band overlaps, dual-res STFT, spectral metric, jitter; B loses, hybrid dual-res wins |

<!-- Levels: 0 static review · 1 API checked vs JUCE source · 2 pure C++ compiled+tested in sandbox · 3 CI green (+pluginval) · 4 tested at home in DAW -->

## Next steps
1. ~~Classifier~~ done: learned network in (D-C1, results in docs/03). Weak spots to hear at home: open vs closed hat, crash vs ride, claps, very noisy snares.
2. ~~CI green on the plugin wiring~~ done: run 27 green (ac7e881); the Windows VST3 artifact can be tried in FL.
3. The Puppet Show: user pastes `prompts/05-puppet-show-genre-targets.md` to the two agents → v1 targets = cell-by-cell average of the answers (+ Claude's blind 05 table, equal weight) → then its DSP stage. v2: targets measured from the user's genre-named sample packs.
4. At home (level 4): load the VST3 in FL Studio — latency 50 ms compensated, transport stop ends auto detect, CPU, and listen to every knob on real loops (all VERIFY markers below). Review docs/03-decisions-for-review.md.
5. v2 (user, 2026-09-30): the user will give access to their own sample packs (named by genre) once v1 runs. Use them to (a) set The Puppet Show's genre targets by measurement (analysis only, lowest licence risk) and (b) train or test the classifier on real one-shots. Check each pack's EULA before training shipped weights; packs that forbid it are evaluation-only.
6. Phase 4 (UI): puppet stage, strings per drum, deep-edit zoom, Shadow Play ring — after the generic-editor version is approved.

## Open VERIFY markers
| Marker | File | What to check at home |
|---|---|---|
| VERIFY(host) | docs/02-algorithm.md §6 | How FL Studio reports transport stop to an effect (auto detect ends on it) |
| VERIFY(host) | docs/02-algorithm.md §10 | FL Studio compensates the 50 ms reported latency exactly (no flam vs other tracks) |
| VERIFY(dsp) | DrumChannel.h (D-C10) | Compressor thresholds are absolute dBFS; saturation/clip have no oversampling — listen for aliasing |
| VERIFY(dsp) | DrumEngine.h (D-C12) | Spectral shares: a bass note under the kick moves with kick settings; ±10 ms smear on hard edges |
| VERIFY(host) | DrumEngine.h (D-C12) | CPU in FL on the i7-11700K (sandbox: ~21 % of one core, stereo 48 kHz) |
| VERIFY(dsp) | HitClassifier.h (D-C1) | Open/closed hat, crash/ride, clap and noisy-snare labels on real loops (see the results table in docs/03) |
| VERIFY(host) | PluginProcessor.cpp (D-C13) | FL shows 50 ms latency (pluginval reads 0 before prepare, as with the template); Link with FL automation; transport stop ends auto detect |

## Decisions (and why)
- JUCE 9.0.3 via FetchContent: latest release; pinned so every build uses the same JUCE.
- Generic editor until phase 4: plugin testable in a DAW before any custom UI.
- dsp/ header-only for now; becomes a static lib automatically when dsp/src/*.cpp exists.
- The Drum Master is an insert effect (no MIDI); lookahead latency reported to host.
- Uncertain hits → Misc, which is neutral by default (a wrong guess must never sound bad).
- Overlaps (answer 1b): multi-label detection; each type's settings act only on its own frequency band.
- A hit's settings last through its tail, or until a new hit of the same type.
- Auto detect ends on playback stop (host transport).
- Three mixing layers: quick mix (kick/snare/hats + globals) · deep edit per drum (click string) · advanced global (click puppet master).
- Snare genre macros Dubstepify / Popify / Rockify / Rimify: blendable, each drives a hidden parameter set.
- Quick-mix groups: Kick / Snare (+rim, clap) / Hats (+ride, crash); volume + 1–2 character knobs; Link switch keeps dB offsets moving together.
- Deep-edit controls: from prompt 01 (Claude blind + 2 agents); rule = in if ≥2 of 3 sources, single-source judged on merit. Every drum has Level + Decay.
- Parked for phase 5: open-hat choke by closed hat, crash duck on kick/snare (Agent 1).
- Algorithm backbone (all 3 sources): STFT analysis, per-band flux onsets, template split + rule-based soft scores, Misc below confidence, tail = drop below peak/floor or retrigger, loop adaptation capped at 50 %.
- Shootout v2 (same-band overlaps: kick+bass, snare+clap, hat+crash): Claude's 6 bands LOSE (kick Top vs hats −8.4 vs −18.7 dB); hybrid dual-res STFT estimate (1024 low / 256 high, split 1.5 kHz, fits 30 ms) + time-domain processors best or tied on 6/7. With overlaps every engine is only ~70–90 % right (decay ~40 %).
- Engine shootout v1 (superseded): time-domain bands beat STFT masks on transient (−20 vs −11 dB), compression (−25 vs −19 dB), saturation (−40 vs impossible), tie on level; decay-shortening ~−4 dB for all (mixed-audio limit). Hybrid ≈ bands.
- APPROVED 2026-09-30: parameter table (90 host params, docs/02-parameters.md) → phase 2 closed.
- Features added to v1 (user OK with the delay; user away ~1 week): F1 The Puppet Show — genre-aware smart EQ, OFF by default, workflow "open it first, genre + depth, then tweak drums"; DECIDED: always listens actively like Guardian Angel → its own dynamic EQ stage with live band meters (does not move user knobs); knobs Genre, Depth, Bass boost, High control, Mud control, Mid presence. F2 Shadow Play (name chosen) — hits drawn as shapes (kick circle, snare triangle…) rebuilding the loop; feed = detector events via lock-free queue. Core hook: detector keeps per-type long-term spectra.
- F1 answers: drum-aware bands (bass→kick, mud→bodies, presence→snare/clap/kick click, high→hats/cymbals); knobs bipolar −100..+100 (0 = genre target); genres Rock/Pop/EDM/Metal/Hip-hop; one general Depth. Next for F1: its phase 2 (genre targets + band math).
- F2 Shadow Play (Claude's choice, user delegated): ring layout synced to playhead, stage floor under the puppet master, click to enlarge.
- Inspiration for F1: Guardian Angel (HarmonyEngine repo, read-only) — port ideas not code (dsp/ stays JUCE-free).
- UI mood: creepy, FNaF-puppet feeling — original character/art only (no copying the copyrighted design).
- APPROVED 2026-09-30: D1 hybrid dual-res engine; D2 fixed 50 ms lookahead (user trusted the measurements).
- Lookahead measured on real drums (generic classifier, first W ms only): electronic plateau at 40–50 ms (AP 0.87, same as 100 ms); acoustic kick/snare/closed hat settle by 30 ms; acoustic open hat/crash keep gaining to 100 ms+ → late-correction crossfade, not more lookahead. Recommendation 50 ms.
- ~~No ML in v1~~ REVERSED (autonomous, D-C1, for review): the rule-based classifier was perfect on the synthetic kit but failed on real drums (rule-feature trees 61–66 % in-domain, 14–23 % cross-domain), which is exactly the "rules miss targets on real loops" exit. Now: 225 log-mel segment features → 48 ReLU → 9 sigmoid (multi-label), ~11 k weights, inference ≈ 11 k MACs per hit. Train only on Groove MIDI Dataset (CC BY 4.0 → credit in About box) + own synthetic loops; MDB Drums (CC BY-NC-SA) and Dirt-Samples are evaluation only.
- Mix settings approved: kick clipping kept (off by default); Dubstepify = long tail + added ~200 Hz body; clap compression kept.

## Known risks
- JUCE 9 is newer than Claude's training data (e.g. module split into juce_audio_processors_headless): verify APIs against the 9.0.3 source.
- CI minutes: private repo, macOS counts 10x, Windows 2x. CI also runs on docs-only pushes.
- Per-band processing also moves everything else in that band (A2: hat decay ducks room/wash; snare comp pumps hats). Transition smoothing is a phase-2 decision.
- Overlapping hits: per-band processing means kick/snare/hat bands must be split cleanly (crossover phase + summing must stay transparent when all settings are neutral).
- FL Studio: lookahead latency must be reported correctly or the loop drifts (VERIFY(host) at home).
- No macOS CI any more: AU and Mac builds are unverified until the job is re-added.
