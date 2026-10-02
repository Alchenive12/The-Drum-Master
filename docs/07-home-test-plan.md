# Home test plan: real samples, Claude Code on the PC

Goal: confirm or disprove the findings in `docs/06-testing.md` with real drums, then measure CPU and
check everything in a DAW. Everything here runs locally with Claude Code (native, in the repo). The
sample packs never leave the PC.

**Never commit sample audio** (licences). Commit only the text results: summaries and CSVs.

## 0. Setup (once)

1. Pull the latest `main` and open a terminal in the repo.
2. Start Claude Code in the repo folder (`claude`). First prompt:

   > Read docs/06-testing.md and docs/07-home-test-plan.md. Build drum_bench in Release (section 0)
   > and run the synthetic baseline. Compare its detection numbers with
   > docs/testing/results/2026-10-02-synthetic-baseline.txt: they must match exactly. Then tell me
   > how fast this PC is compared with the baseline's performance section.

3. The build commands Claude Code will use:
   ```
   cmake -S dsp -B build-bench -DPLUGIN_DSP_BENCH=ON -DPLUGIN_DSP_TESTS=OFF -DCMAKE_BUILD_TYPE=Release
   cmake --build build-bench --config Release --target drum_bench
   ```
   The tool is `build-bench\Release\drum_bench.exe` on Windows (Visual Studio) or
   `build-bench/drum_bench` with Ninja / macOS.

**Pass for step 0:** the baseline's detection numbers are identical to the saved file. If they
aren't, stop: something differs in the build or the code.

## 1. The packs and what each one tests

| Pack(s) | Genre run | Main question |
|---|---|---|
| The big hip-hop pack, Cymatics, and the small hip-hop packs | `--genre hip-hop` | 808s vs kicks, claps and snaps, rims, trap hats |
| Siik, Sickrate 1 + 2 (house) | `--genre house` | four-on-the-floor kicks, offbeat open hats (finding 5), claps |
| KSHMR 1–3 | `--genre edm`, plus `--genre hip-hop` for their trap kits | big-room claps and snares, crashes and risers |
| MHA packs | `--genre pop` and `--genre rock` | general acoustic and hybrid kits |
| Splice metal: one-shots and loops | `--genre metal` (one-shots), `--loops` | double kick under a full mix (finding 6), ride and crash (finding 2) |
| The other small packs | `--oneshots` | whatever they contain |

Metal: the loops have no labels, so they can only be checked by ear. For real numbers, also download
**one metal drum one-shot kit from Splice** (kick, snare, toms, hats, ride, crash). The kits mode can
then build metal beats with exact ground truth.

## 2. One-shots: is the classifier right on real sounds?

Run it per pack folder, or per genre folder holding several packs. `<...>` is the path on the PC:
```
drum_bench --oneshots "<hip-hop packs>" --csv results-oneshots-hiphop.csv
drum_bench --oneshots "<house packs>"   --csv results-oneshots-house.csv
drum_bench --oneshots "<KSHMR packs>"   --csv results-oneshots-kshmr.csv
drum_bench --oneshots "<MHA packs>"     --csv results-oneshots-mha.csv
drum_bench --oneshots "<metal kit>"     --csv results-oneshots-metal.csv
```

How it works:
- Labels come from the file name, then the folder names: Kick, Snare, OH / Open Hat, Hat / HH, Rim,
  Clap / Snap, Crash, Ride, 808, Perc / Tom / Shaker…
- Files without a drum keyword, loop folders, and files longer than 4 s are skipped, and counted at
  the top of the output.
- Each sound plays alone, at 3 velocities, in a fresh engine.

Prompt for Claude Code after each run:
> Summarise this one-shot run against docs/06-testing.md: which findings are confirmed, which are
> not, which packs or sound groups are worst. Open the CSV and list the 15 most misclassified files
> with what they were taken for. Check that the skip counts are plausible, and spot-check 10 labels
> from file names to make sure the labelling isn't wrong.

**Proposed targets** (adjust after the first run):
- kick, snare, clap, closed hat **≥ 90%**
- open hat, rimshot **≥ 80%**
- crash, ride **≥ 70%**
- 808s should be **kick**, toms and percussion **misc**

## 3. Kits: real samples in real beats

```
drum_bench --kits "<hip-hop packs>" --genre hip-hop --kitsets 10 --csv results-kits-hiphop.csv
drum_bench --kits "<house packs>"   --genre house   --kitsets 10 --csv results-kits-house.csv
drum_bench --kits "<KSHMR packs>"   --genre edm     --kitsets 10 --csv results-kits-kshmr.csv
drum_bench --kits "<MHA packs>"     --genre pop     --kitsets 10 --csv results-kits-mha.csv
drum_bench --kits "<metal kit>"     --genre metal   --kitsets 10 --csv results-kits-metal.csv
```

Each kit set picks one random sound per drum type from the folder (like a real kit) and plays the
genre's 16-bar pattern. It's scored twice: drums alone, and in a full mix with only that kit's types
allowed, as with the plugin's strings.

**Proposed targets:**
- **Onsets found:** at least 95% drums alone and at least 90% in a full mix.
- **False hits:** at most one per 4 bars in a full mix.
- **Labels:** the label is a drum really there at least 90% of the time.

Prompt:
> Compare the kits results across genres and against the synthetic baseline. Where are the gaps
> biggest? Do findings 4 (false hits from the bass), 5 (EDM offbeat hats) and 6 (metal double kick)
> hold with real samples?

## 4. Loops: by ear, and as a sanity check

```
drum_bench --loops "<Splice metal loops>" --csv results-loops-metal.csv
drum_bench --loops "<a few loops from each pack>" --csv results-loops-mixed.csv
```

Then check a few loops in the DAW:
1. Drop the loop on a track with The Drum Master and watch the Shadow Play while it loops.
2. Compare with the CSV: the per-event times and types.
3. Anything with clearly too many or too few hits per second goes on a list for Claude Code.

## 5. Performance

1. **Offline:** the baseline's performance section, run on the PC, gives the real share of one core.
   That replaces the cloud estimate in `docs/06-testing.md`.
2. **In the DAW, at 48 kHz, with buffers of 64, 128 and 512 samples:**
   - 1, 4 and 8 instances on drum loops: note the DAW's CPU meter and listen for any crackles;
   - every effect on, plus the Puppet Show, with the same instance counts;
   - the plugin window open and closed, to see the UI cost (the 3D background and the animations).

**Targets:**
- No crackles at a 128-sample buffer with 4 instances.
- Each instance well under 10% of one core with the window closed.

## 6. The v1 checks in the DAW (from `REMOTE_LOG.md`)

1. **Sound:** detection, per-drum mixing, quick-mix knobs, bypass, Puppet Show genres.
2. **UI:**
   - the look on this screen and the knob size (`bodyInFrame` in `Source/ui/Art3D.h`)
   - the flicker
   - adding a string: the drop, the landing and the shiver
   - drums swinging on their knots on hits
   - resizing the window
3. **Saving:** save the session, reopen it, and check that every setting is back.
4. **The `VERIFY` markers:** clear every open marker in the code.

## 7. Reporting back

Have Claude Code write `docs/testing/results/<date>-<what>.md`, with the summary tables, the targets
met or missed, and the conclusions. Commit it together with the CSVs on a branch named
`remote/home-test-results`, then open a PR. Fixes to the classifier come after that, each one
measured with `drum_bench` against these results.

Prompt to finish:
> Write the results file for today's runs (section 7), with the targets met or missed and which
> findings are confirmed. Commit it with the CSVs, but no audio, to remote/home-test-results, then
> propose an order for fixing the confirmed issues.
