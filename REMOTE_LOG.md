# REMOTE_LOG

> Memory file for the "away from home" protocol. Paste or link this at the start of every session.
> Updated in every commit made away from home.

## Project facts
- Plugin name:                   The Drum Master (code still says `AwayTemplate` — rename after concept approval)
- JUCE version / tag:            9.0.3 (FetchContent, pinned in CMakeLists.txt)
- Build system:                  CMake
- Formats:                       VST3 / AU (macOS only) / Standalone
- Platforms built at home:       Windows (FL Studio, i7-11700K, RTX 3060 Ti) — VST3
- CI:                            .github/workflows/build.yml (Linux, macOS, Windows + DSP tests + pluginval)
- Last known-good commit:        main after PR #2 (CI green on Linux, macOS, Windows)

## Current trip
- Pipeline phase:                1 — answers integrated into concept, waiting for final approval + 6 small confirms
- Branch: claude/away-from-home-protocol-pc5bni (session-assigned; stands in for `remote/<feature>`)
- Goal: approve docs/01-concept.md, then rename template in CMakeLists.txt and start phase 2

## Done this trip
| Commit msg | Files | Verification level (0–4) | Notes |
|---|---|---|---|
| Add away-from-home template (PR #1) | all | 3 | JUCE 9.0.3 APIs checked vs source; CI build + DSP tests + pluginval green on all 3 platforms |
| CI: add JUCE 9 Linux deps (PR #2) | build.yml | 3 | libxi-dev + libegl-dev; fixed the Linux build |
| [untested] docs: phase 1 concept draft | docs/01-concept.md, REMOTE_LOG.md | 0 | Docs only; dsp tests re-run in sandbox (level 2, pass) |
| [untested] docs: concept answers (1b, AD2-style mixing, puppet clicks) | docs/01-concept.md, REMOTE_LOG.md | 0 | Docs only |

<!-- Levels: 0 static review · 1 API checked vs JUCE source · 2 pure C++ compiled+tested in sandbox · 3 CI green (+pluginval) · 4 tested at home in DAW -->

## Next steps
1. Confirm the *(to confirm)* items in docs/01-concept.md and approve the concept.
2. Rename `AwayTemplate` / `PRODUCT_NAME` / `PLUGIN_CODE` in CMakeLists.txt (announced CMake change).
3. Phase 2: write prompts/ for other models from the concept only, then Claude's plan + comparison → parameter table.

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

## Known risks
- JUCE 9 is newer than Claude's training data (e.g. module split into juce_audio_processors_headless): verify APIs against the 9.0.3 source.
- CI minutes: private repo, macOS counts 10x, Windows 2x. CI also runs on docs-only pushes.
- Overlapping hits: per-band processing means kick/snare/hat bands must be split cleanly (crossover phase + summing must stay transparent when all settings are neutral).
- FL Studio: lookahead latency must be reported correctly or the loop drifts (VERIFY(host) at home).
- AU/macOS not needed for home testing; macOS CI job costs 10x minutes — consider dropping (CI change, ask first).
