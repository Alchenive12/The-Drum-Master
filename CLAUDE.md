# CLAUDE.md

The Drum Master, a JUCE 9 audio plugin (VST3, Windows / FL Studio at home). Repo: `Alchenive12/The-Drum-Master`.
All plugin work stays in the **Alchenive12** GitHub org. Do not use the old personal-account repo.

Read these before doing anything:

1. **`REMOTE_PROTOCOL.md`**: the "away from home" rules (written in Hebrew). It covers branches
   (`remote/<feature>`, nothing reaches `main` before a home test), commit prefixes (`[untested]`, then `[ci-green]`),
   `VERIFY(...)` markers, the verification ladder (levels 0-4), and the CI loop.
2. **`REMOTE_LOG.md`**: the project's memory. It holds project facts, the current trip, a table of every
   commit with its verification level, open `VERIFY` markers, and decisions with their reasons.
   **Update it in the same commit as the code.**
3. **`docs/`**: concept (01), algorithm and parameters (02), decisions for review (03), UI (04),
   features (05), testing (06, 07), snare training (08), and the detection plan without training (09).

Quick facts:
- Pure DSP lives in `dsp/` (standard C++, no JUCE, has its own CMake). Its tests can be built and run
  in the sandbox: `cmake -S dsp -B build-dsp && cmake --build build-dsp && ctest --test-dir build-dsp`.
- CI: `.github/workflows/build.yml` runs a Linux and Windows build, DSP tests, and pluginval on every push.
- `processBlock`: no allocations, locks or I/O. Don't touch CMakeLists or CI without saying so.
- `dsp/include/plugin_dsp/HitClassifierWeights.h` is generated (now `research/loop-learning/export_mlp2.py`,
  225 -> 512 -> 256 -> 9, `TinyMlp2`; it also sets the network type and whether the V1 second network runs).
  Trained on commercial sample packs: **personal use only**. Don't ship it (docs/08).
- `HitClassifierWeightsV1.h` is the first network (Groove + E-GMD, CC BY 4.0, credit in the About box).
  Off by default since the 2026-10-15 home run (`Settings::secondNet`; docs/05 F4).
- Home GPU training: `research/loop-learning/home-run/` (runner + jobs on the owner's PC, data in `D:\dm-data`).
- Real-drum eval in the sandbox: `research/lookahead-test/fetch_data.sh`, then `research/classifier-eval/export_sets.py`,
  then `eval <set> --half guard|report` (tune on guard, report on report).
  `export_kit_loops.py` adds `electronic_kit` (one sample per type per loop, like a produced beat).
- Sample audio stays on the owner's PC. Only feature rows and trained weights leave it.
