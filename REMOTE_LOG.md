# REMOTE_LOG

> Memory file for the "away from home" protocol. Paste or link this at the start of every session.
> Updated in every commit made away from home.

## Project facts
- Plugin name:                   Away Template (target `AwayTemplate`) — template repo
- JUCE version / tag:            9.0.3 (FetchContent, pinned in CMakeLists.txt)
- Build system:                  CMake
- Formats:                       VST3 / AU (macOS only) / Standalone
- Platforms built at home:       TBD
- CI:                            .github/workflows/build.yml (Linux, macOS, Windows + DSP tests + pluginval)
- Last known-good commit:        main after PR #2 (CI green on Linux, macOS, Windows)

## Current trip
- Pipeline phase:                template done — next plugin starts at phase 1
- Branch: none active
- Goal: —

## Done this trip
| Commit msg | Files | Verification level (0–4) | Notes |
|---|---|---|---|
| Add away-from-home template (PR #1) | all | 3 | JUCE 9.0.3 APIs checked vs source; CI build + DSP tests + pluginval green on all 3 platforms |
| CI: add JUCE 9 Linux deps (PR #2) | build.yml | 3 | libxi-dev + libegl-dev; fixed the Linux build |

<!-- Levels: 0 static review · 1 API checked vs JUCE source · 2 pure C++ compiled+tested in sandbox · 3 CI green (+pluginval) · 4 tested at home in DAW -->

## Next steps
1. Mark repo as template (Settings → Template repository).
2. First real plugin: "Use this template" → new repo → phase 1 (concept in words).
3. At home: load the CI-built plugin in a DAW once (level 4 for the template).

## Open VERIFY markers
| Marker | File | What to check at home |
|---|---|---|
| — | — | none open (pluginval step confirmed working in CI) |

## Decisions (and why)
- JUCE 9.0.3 via FetchContent: latest release; pinned so every build uses the same JUCE.
- Generic editor until phase 4: plugin testable in a DAW before any custom UI.
- dsp/ header-only for now; becomes a static lib automatically when dsp/src/*.cpp exists.

## Known risks
- JUCE 9 is newer than Claude's training data (e.g. module split into juce_audio_processors_headless): verify APIs against the 9.0.3 source.
- CI minutes: private repo, macOS counts 10x, Windows 2x.
