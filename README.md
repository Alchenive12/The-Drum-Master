# The Drum Master

A drum-bus / drum-loop insert effect that recognises each hit (kick, snare, hats, cymbals, rim, clap)
with lookahead and mixes it with that drum's own settings — see `docs/01-concept.md`.

Developed with the **"away from home" protocol**: planning and writing code from a phone, with real
compiles and validation happening in GitHub Actions (Linux + Windows).

- `REMOTE_PROTOCOL.md` — the protocol (Hebrew)
- `REMOTE_LOG.md` — memory file; paste it at the start of every session
- `dsp/` — pure C++ DSP core + tests (no JUCE), builds standalone or as part of the plugin
- `Source/` — JUCE glue (processor, generic editor until the UI phase)
- `.github/workflows/build.yml` — builds Linux / Windows, runs DSP tests and pluginval
- `docs/`, `prompts/` — pipeline phase documents

Build locally:
```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```
