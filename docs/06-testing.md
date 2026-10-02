# Testing: method and findings

First measurements of The Drum Master's engine (`dsp/`, the code that runs inside the plugin),
made away from home on 2 October 2026 with `dsp/tools/bench.cpp`. Raw output is in
`docs/testing/results/`. The plan for testing with real sample packs at home is in
`docs/07-home-test-plan.md`.

## How it is measured

- **The real engine.** `DrumEngine` from `dsp/`, the same code the plugin runs, at 48 kHz in
  512-sample blocks, sensitivity 50, no JUCE and no DAW. Detections are read from the engine's
  Shadow Play event queue: one event per detected hit, with its time and drum type.
- **Known ground truth.** Audio is built from drum sounds placed at exact times, so every true hit's
  time and type is known. A detection counts if it lands within ±25 ms of a true onset.
- **Synthetic sounds** (`dsp/tests/synth_drums.h`). They're deterministic and the same on every
  machine, so results repeat exactly and changes to the engine can be compared run against run. But
  they're simple models of drums: **every finding below must be confirmed with real samples.**

### What the numbers mean
- **Onsets found:** the share of true onsets with any detection near them. Hits closer than 10 ms
  count as one onset (layers).
- **False:** detections with no true onset near them.
- **Label is a drum really there:** the engine gives **one label per onset**. This checks that the
  label is one of the drums actually playing at that moment. It's the fair measure for layered hits.
- **Every layered hit typed:** the share of all true hits that got their own correctly typed
  detection. It's capped well below 100% for layered beats, by design, because of the one label per
  onset.
- **Performance:** wall-clock time of `setParams()` + `process()`. The benchmark machine was a slow
  shared cloud vCPU, so absolute numbers are pessimistic, and single-block spikes there are not
  reliable.

## Results (synthetic baseline)

### 1. Detection by genre (16 bars each)

| Genre | Onsets found, drums alone | Onsets found, full mix | False hits per song, full mix | Label is a drum really there, full mix |
|---|---|---|---|---|
| Rock, 120 bpm | 96.9% | 96.9% | 5 | 76.6% |
| Pop, 100 bpm | 87.9% | 87.9% | 47 | 70.7% |
| EDM, 128 bpm | 64.8% | 64.8% | 35 | 83.1% |
| Metal, 180 bpm | 88.7% | 69.1% | 9 | 71.2% |
| Hip-hop, 88 bpm swung | 100% | 100% | 56 | 78.3% |

"Full mix" means a synthesized bass line and chords at about −12 dB under the drums. With drums
alone there were 0–4 false hits per song. Allowing only the genre's own drum types (as the strings
do in the plugin) changed the results by less than 6 points.

### 2. Each hit type alone (40 hits in a row, velocities 0.3–1.0)

| Type | Right | Mistaken for |
|---|---|---|
| Kick | 100% | |
| Closed hat | 100% | |
| Rimshot | 100% | |
| Clap | 72.5% | closed hat 22% |
| Open hat | 42.5% | closed hat 58% |
| Snare | 2.5% | closed hat 92% |
| Crash | 2.5% | clap 40%, missed 38% |
| Ride | 2.5% | rimshot 70% |
| Tom (no own string) | 0% | kick 100% (expected misc) |

The same sounds tested **one at a time in a fresh engine**
(`results/2026-10-02-synthetic-oneshots-fresh-engine.txt`):
- ride 100%, open hat 86.7%, crash 40% (else open hat)
- snare still 0% (closed hat)
- toms still kick

### 3. Performance (rock in a full mix, 34 s, stereo)

| | Share of one core (cloud vCPU) | 99% of blocks within (128 / 512 samples) |
|---|---|---|
| Detection + routing only | about 21% | 34% / 43% of budget |
| + quick mix | about 23% | 38% / 44% |
| + every drum's effects | about 24% | 53% / 35% |
| + Puppet Show | about 24% | 38% / 35% |

**The effects are cheap.** Almost all the cost is the analysis (STFT, dual-resolution estimator,
classifier), which runs whatever the settings. On a desktop CPU, expect very roughly 6–12% of one
core per instance; to be measured at home. Worst single blocks swung from 53% to 1500% of budget
between runs on the shared vCPU, which is scheduling noise there, not a conclusion.

## Issues found (to confirm with real samples)

1. **Snare is classified as closed hat.** It happened in every test, 92–100% of the time. It's the
   most important finding. It may be partly the noisy synthetic snare, so check with real snares
   first.
2. **Cymbals are weak.** Crash went to clap or was missed; ride went to rimshot when many played in a
   row.
3. **The adaptive learning drifts on repetition.** The same ride or open hat sound scores much worse
   after 40 in a row than one at a time in a fresh engine. The per-song learning may be pulling a
   repeated sound towards a neighbouring type.
4. **The music bed causes false hits:** 35–56 per song in pop, EDM and hip-hop. The plucked bass is
   taken for drums.
5. **EDM: about a third of hits are missed.** These are likely the quiet offbeat closed hats under
   claps and open hats.
6. **Metal in a full mix:** onsets found drops from 89% to 69%. The fast double kick gets lost under
   the bass.
7. **Toms are labelled kick.** There's no tom string, so they should go to misc.
8. **One label per onset (design).** When a kick and a hat hit together, one gets the label. The
   stems still separate the energy per drum, so this mainly affects Shadow Play and string jerks, not
   the mix itself. To decide whether this matters.
9. **The detector needs audio before the first hit.** A hit on the very first sample of a file isn't
   detected. Irrelevant in a DAW (continuous audio); the benchmark pads loops with 0.5 s of silence.

## Re-running

```
cmake -S dsp -B build-bench -DPLUGIN_DSP_BENCH=ON -DPLUGIN_DSP_TESTS=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build-bench --config Release --target drum_bench
build-bench/drum_bench            # (Windows: build-bench\Release\drum_bench.exe)
```

The detection numbers are deterministic. Any change to them after an engine change is a real change
in behaviour.
