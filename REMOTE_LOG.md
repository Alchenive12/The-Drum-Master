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
- Last known-good commit:        main (template, CI green on Linux, macOS, Windows)

## Current trip
- Pipeline phase:                2 — concept approved; prompt 01 (per-drum mix settings) out to other models
- Branch: claude/away-from-home-protocol-pc5bni (session-assigned; stands in for `remote/<feature>`)
- Goal: collect answers to prompts/01, compare vs prompts/answers/claude.md, fix per-drum controls

## Done this trip
| Commit msg | Files | Verification level (0–4) | Notes |
|---|---|---|---|
| Add away-from-home template (PR #1) | all | 3 | JUCE 9.0.3 APIs checked vs source; CI build + DSP tests + pluginval green on all 3 platforms |
| CI: add JUCE 9 Linux deps (PR #2) | build.yml | 3 | libxi-dev + libegl-dev; fixed the Linux build |
| [untested] docs: phase 1 concept draft | docs/01-concept.md, REMOTE_LOG.md | 0 | Docs only; dsp tests re-run in sandbox (level 2, pass) |
| [untested] docs: concept answers (1b, AD2-style mixing, puppet clicks) | docs/01-concept.md, REMOTE_LOG.md | 0 | Docs only |
| [untested] docs: approve concept, add prompt 01 + Claude's blind answer | docs/01-concept.md, prompts/, REMOTE_LOG.md | 0 | Docs only |
| [untested] CI: drop macOS job | build.yml, REMOTE_LOG.md | 0 | Approved by user; how to re-add is in the build.yml header |
| [untested] Rename template to The Drum Master | CMakeLists.txt, Source/*, README.md, REMOTE_LOG.md | 0 | Mechanical rename; CI to confirm (level 3) |

<!-- Levels: 0 static review · 1 API checked vs JUCE source · 2 pure C++ compiled+tested in sandbox · 3 CI green (+pluginval) · 4 tested at home in DAW -->

## Next steps
1. Paste other models' answers to prompts/01 → save to prompts/answers/<model>.md.
2. Fair comparison vs prompts/answers/claude.md → per-drum controls decided.
3. Phase 2 continues: detection algorithm prompts + docs/02-algorithm.md → parameter table (ID, range, default, unit).

## Open VERIFY markers
| Marker | File | What to check at home |
|---|---|---|
| — | — | none open (pluginval step confirmed working in CI) |

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
- Deep-edit controls for hats, cymbals, rim, clap, misc: decided from prompt 01 answers (Claude answered blind first).

## Known risks
- JUCE 9 is newer than Claude's training data (e.g. module split into juce_audio_processors_headless): verify APIs against the 9.0.3 source.
- CI minutes: private repo, macOS counts 10x, Windows 2x. CI also runs on docs-only pushes.
- Overlapping hits: per-band processing means kick/snare/hat bands must be split cleanly (crossover phase + summing must stay transparent when all settings are neutral).
- FL Studio: lookahead latency must be reported correctly or the loop drifts (VERIFY(host) at home).
- No macOS CI any more: AU and Mac builds are unverified until the job is re-added.
