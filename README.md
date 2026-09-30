# Away From Home — JUCE plugin template

Template repo for developing JUCE audio plugins with the **"away from home" protocol**: planning and
writing code from a phone, with real compiles and validation happening in GitHub Actions.

- `REMOTE_PROTOCOL.md` — the protocol (Hebrew)
- `REMOTE_LOG.md` — memory file; paste it at the start of every session
- `dsp/` — pure C++ DSP core + tests (no JUCE), builds standalone or as part of the plugin
- `Source/` — JUCE glue (processor, generic editor until the UI phase)
- `.github/workflows/build.yml` — builds Linux / macOS / Windows, runs DSP tests and pluginval
- `docs/`, `prompts/` — pipeline phase documents

New plugin: **Use this template** → rename `AwayTemplate`, `PRODUCT_NAME`, `PLUGIN_CODE` in `CMakeLists.txt`.

Build locally:
```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```
