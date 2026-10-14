<!-- File: docs/03-decisions-for-review.md -->
# The Drum Master — Decisions taken alone (for review)

> The user was away and said on 2026-09-30: "continue everything leading up to the code, if you
> have to decide things on your own do it and I will go over it later".
> Every decision Claude made without asking is listed here, with the reason and how to undo it.
> **Nothing here is approved yet.** Mark each one ✅ keep / ✏️ change / ❌ revert.

| ID | Decision | Area | Status |
|---|---|---|---|
| D-C1 | Learned classifier (small neural net) replaces the hand-written rules | detection | open |
| D-C2 | Training data and licences: Groove MIDI Dataset + own synthetic loops only | legal | open |
| D-C3 | Loop adaptation of the classifier | detection | open |
| D-C4 | Drum-name mapping of the Groove MIDI Dataset | detection | open |
| D-C5 | Onset detector tuning (differs from the phase-2 design) | detection | open |
| D-C6 | Hit tracker values (tail end, fades, caps) | processing | open |
| D-C7 | Shadow Play layout details | UI | open |
| D-C8 | Test timeout raised in sanitizer builds (dsp/CMakeLists.txt) | build | open |
| D-C9 | Known limits we accept for v1 | detection | open |
| D-C10 | Per-drum processor chain (order, ranges, what each amount means) | processing | open |
| D-C11 | What every knob and snare macro moves (the hidden curves) | processing | open |
| D-C12 | How the engine splits the loop into per-drum estimates | processing | open |
| D-C13 | Plugin wiring choices (state, Link, bypass, what isn't saved) | plugin | open |
| D-C14 | The Puppet Show: prompt 04 sent to agents, DSP waits for their answers | feature | open |
| D-C15 | Puppet Show v1 targets: averaged answers, kick rows anchored to real kits, M1 redefined | feature | open |
| D-C16 | The Puppet Show engine (built from Claude's prompt-04 design; agents answered only the targets) | feature | open |
| D-C17 | The UI details the user left to Claude (layout, deep edit, pages, art, window) | UI | open |
| D-C18 | External code review (Argon, 2026-10-14): what was fixed, what wasn't, and why | review | open |
| D-C19 | MIDI mode details left to Claude (timing, fallback, merging, storage, UI) | feature | open |

---

## D-C1 — A learned classifier replaces the rules

**What was approved:** "No ML in v1: rule-based first; a small learned model only if the rules
miss their targets on real loops."

**What happened:**
- The rule-based classifier (template fit + drum physics) scored 100% on our synthetic kit.
- On real drums it failed:
  - MDB Drums (acoustic kits);
  - loops built from real drum-machine one-shots.
- Tuning the rules on the real sets didn't transfer. Decision trees trained on the rule features
  reached 61–66% within one domain, but only **14–23%** from acoustic to electronic and back. The
  rule features simply don't carry enough information.

That is exactly the "rules miss targets on real loops" exit, so Claude took it.

**Now:**
- **Features:** a 32-band log-mel picture of the hit. There are 7 time slices, from 25 ms before
  the onset to 40 ms after it. Each slice is measured relative to the hit's own peak, plus the peak
  level itself, giving 225 numbers (`dsp/include/plugin_dsp/HitFeatures.h`).
- **Network:** 225 → 48 → 9 (`TinyMlp.h`). There is one yes/no output per drum type, so a
  kick + hat hit can be both (answer 1b stays intact).
- **Cost:** about 11 k multiply-adds per hit, a few microseconds. There is no allocation and no
  library, and `dsp/` stays pure C++.
- **Unchanged:** the classifier API (weights, confidence, Misc, allowed types, auto-detect
  presence, per-type spectra for The Puppet Show). The engine and UI don't notice the swap.
- **Misc:** if no type is confident, the hit is Misc, which is neutral by default.

**Results:** see the table under "Classifier results" below (filled in after training).

**To undo:** the rule-based classifier stays in git history (commit 959d327).

## D-C2 — Training data and licences

| Data | Licence | Used for |
|---|---|---|
| Groove MIDI Dataset (Magenta / Google) | CC BY 4.0 | training |
| Expanded Groove MIDI Dataset (E-GMD): the same playing re-recorded on 43 Roland TD-17 kits, acoustic to 808/909 | CC BY 4.0 | training (30 performances per kit, fetched without the full 90 GB zip); 6 kits held out to test unseen kits |
| Our own synthetic loops (`research/classifier-train/synth_aug.py`) | ours | training (claps, 808 kicks, drum-machine hats that Groove lacks) |
| MDB Drums | CC BY-NC-SA 4.0 (non-commercial) | **evaluation only**, never training |
| Dirt-Samples one-shots | unclear | **evaluation only**, never training |

- **Why:** the trained weights ship inside the plugin. Weights trained on non-commercial or unclear
  data could put a commercial release at risk. Evaluation-only use never ships.
- **Obligation:** CC BY 4.0 requires credit. The About page (phase 4) must say:
  *"Hit classifier trained in part on the Groove MIDI Dataset (Gillick, Roberts, Engel, Tsao and
  Bamman, 2019) and the Expanded Groove MIDI Dataset (Callender, Hawthorne and Engel, 2020), both CC BY 4.0."* The generated weights header carries the same credit.
- **Please check:** whether you plan to sell the plugin. If it will only ever be free, MDB could
  also be used for training. Don't do that without deciding, though.

## D-C3 — Loop adaptation of the classifier

- **Phase-2 plan:** templates move toward the loop's confident hits, capped at 50%, saved with the
  session, with a "reset learning" button.
- **With the network, the network itself doesn't learn in the plugin.** Training inside a DAW is
  risky and hard to test.
- **What still adapts, per loop:**
  - the per-type average spectra (these feed The Puppet Show);
  - the auto-detect presence counts.

  Both are saved with the session, and "reset learning" still clears them.
- **Proposal for phase 5 (not built):** a per-loop "nearest confident example" correction on top
  of the network.

## D-C4 — Groove MIDI drum mapping

Groove labels come from the MIDI of an electronic kit (Roland TD-11), mapped to our 9 types as
follows:

| MIDI notes | Our type |
|---|---|
| 36 | kick |
| 38, 40 (snare, snare rim-shot) | snare |
| 37 (side stick) | rimshot |
| 42, 22, 44 (closed, closed edge, pedal) | closed hat |
| 46, 26 (open, open edge) | open hat |
| 49, 55, 57, 52 (crashes, splash, china) | crash |
| 51, 59, 53 (ride, ride edge, bell) | ride |
| toms | misc |

- **Why "snare rim-shot" → snare:** in a mix it sounds like a loud snare, not a click.
- **Why toms → misc:** there's no tom type in the concept, and Misc is neutral.
- **Missing: claps.** Groove has no claps, so all clap training comes from our synthetic loops. Clap
  accuracy on real claps is the weakest point to test at home.

## D-C5 — Onset detector tuning

Documented in `docs/02-algorithm.md` §3 under "As built in phase 3". In short:
- per-bin SuperFlux;
- a Hann window;
- neighbourhood level-rise check ≥ 3 dB;
- longer low-band reference (25 ms);
- relative gate 35 dB (was 45);
- δ = 2 dB;
- refractory 25 ms;
- grouping ±6 ms;
- echo merge 15 ms.

Each change was forced by a failing test.

Measured:
- synthetic kit: F1 0.97–0.99, timing ≤ 0.8 ms;
- real acoustic (MDB): onset F1 0.879;
- electronic loops: onset F1 0.975.

## D-C6 — Hit tracker values

`dsp/include/plugin_dsp/HitTracker.h`:
- **Pre-roll:** 2 ms raised-cosine fade before the onset (the "Fades" of §7).
- **Release:** 20 ms raised-cosine after the tail ends.
- **Tail end:** the range envelope stays below max(peak − R_t, floor + 6 dB) for 20 ms. R_t is as in
  §5: kick 36, snare 36, clap 36, open hat / crash / ride 40, closed hat / rim / misc 30.
- **Ranges for the envelope:** kick < 150 Hz; snare, rim, clap 150 Hz–5 kHz; hats and cymbals
  > 5 kHz; misc broadband. The noise floor may rise at most 3 dB/s.
- **Flam guard:** a re-rise above peak − 20 dB restarts the wait.
- **Caps:** crash 4 s, open hat and ride 2 s, others 1 s.
- **Retrigger (deviation):**
  - The phase-2 plan had a 5 ms crossfade. Now the old hit ends at the new onset and fades over
    its 20 ms release, while the new one fades in over 2 ms.
  - The gate takes the larger of the two, so it never dips. Per-hit envelopes (Decay, Crack)
    restart from the new onset.
- **Weak labels:** a type with weight below 0.3 in a hit gets no gate. Above that, the gate is
  scaled by the weight.
- **Measured on the synthetic kit:**
  - a 0.5 s kick ends at about 0.52 s;
  - a closed hat ends at 134 ms, an open hat at 719 ms.

## D-C7 — Shadow Play layout details

The user delegated these ("you can choose that"):
- One bar or loop is a **ring**, synced to the playhead like a clock hand.
- It sits on the **stage floor under the puppet master**. Click it to enlarge.
- **Shapes:**
  - kick: circle;
  - snare: triangle;
  - closed hat: dot;
  - open hat: hollow ring;
  - crash: starburst;
  - ride: diamond;
  - rimshot: slash;
  - clap: three small triangles;
  - misc: faint square.
- **Look:** size = level, brightness = confidence. Older hits fade like smoke.
- **Data path:** the audio thread pushes {type, time, level, confidence} into a lock-free
  single-producer / single-consumer queue. The UI thread reads it. The audio thread never waits
  (phase 4).

## D-C8 — Test timeout in sanitizer builds

- **Changed:** `dsp/CMakeLists.txt` gives tests a 300 s timeout only when `PLUGIN_DSP_SANITIZE` is
  on; normal builds and CI keep 60 s.
- **Why:** ASan/UBSan make the detector tests about 40× slower.
- **Effect on CI and the plugin build:** none.

## D-C9 — Known limits accepted for v1

- A snare with a clap layered about 6 ms later is reported as one hit (snare). The two onsets are
  closer than the 6 ms grouping.
- A sound that stops exactly at a new onset can make the old hit's tail look longer, up to the
  20 ms hangover.
- A quiet hat fully masked by a crash tail (under 1 dB above it) is missed.
- Open hat vs crash on acoustic kits keeps improving with more than 50 ms of listening (measured).
  v1 decides at 40 ms. A late-correction crossfade is parked for phase 5.

## D-C10 — Per-drum processor chain

`dsp/include/plugin_dsp/DrumChannel.h`. There is one chain per drum type. It runs on that drum's
estimate and adds only its change, scaled by the hit gate.

- **Order:** EQ (up to 8 filters) → compressor → attack/tail shaping → saturation → soft clip →
  level → width. The compressor sits before the attack shaping, so it can't flatten the attack the
  user asked for.
- **Attack** (Crack / Tick / Click / Attack / Ping / Punch):
  - +100% = +9 dB and −100% = −15 dB, over a per-drum window;
  - windows: kick 12 ms, snare 8, clap 10, rim / open hat / ride 5, closed hat 3, crash and misc 8;
  - then a same-length fade back.
- **Decay:**
  - +100% = +6 dB on the tail (the parameter-table limit);
  - −100% = the tail falls an extra 0.3 dB per ms (−30 dB after 100 ms, floor −60 dB).
- **Compression:**
  - threshold −6 → −30 dBFS and ratio 1.5 → 6 as the knob goes 0 → 100%, with a 6 dB soft knee;
  - release 80 ms; attack per drum (kick 8 ms, so the beater passes; snare 5; clap 3);
  - half of the static gain reduction comes back as make-up.
  - **Please check:** the thresholds are absolute (dBFS), so a very quiet loop compresses less.
    `VERIFY(dsp)` at home with real loops.
- **Saturation:**
  - a tanh curve with drive 1 → 5, scaled so a −6 dBFS peak stays at −6 dBFS;
  - quiet parts come up by at most +8 dB (thicker, more sustain) and loud peaks are rounded;
  - it blends in with the amount, so there's no jump when you leave 0.
- **Clip:** a cubic soft clip; its ceiling goes 0 → −12 dBFS.
- **No oversampling yet** on saturation and clip, so some aliasing at high drive. `VERIFY(dsp)`: add
  2× oversampling if it's audible.
- **Width:** mid/side, where the side gain = width (0–200%).

## D-C11 — What the knobs and macros move

`dsp/include/plugin_dsp/DrumMapping.h`. The full table is in the code; the main choices:

- **EQ slot frequencies:**
  - kick: sub shelf 50 Hz (Low-end boost), Bottom bell 90 Hz, Top bell 3.5 kHz;
  - snare Body bell 200 Hz;
  - bodies: rim 400 Hz, clap 350 Hz, open hat 900 Hz, crash 700 Hz;
  - ride Bell 1.2 kHz;
  - De-harsh = a bell at 7 kHz (8 kHz on the closed hat), up to −8 dB.
- **Tone** = tilt around 1 kHz: the top moves +tone/2 and the bottom −tone/2.
- **Ride Wash** = a static gain on everything after the ping (±12 dB).
- **Quick mix:**
  - Kick Punch: attack ±60%, plus up to 30% compression.
  - Kick Weight: ±6 dB sub shelf.
  - Snare Snap: attack ±60% and ±3 dB air. It also acts on rim and clap, at a smaller amount.
  - Snare Fat: ±5 dB body, a longer tail and some saturation.
  - Hats Tone: ±3 dB tilt.
  - Hats Tame: de-harsh plus a 40% shorter decay.
- **Global Punch:** up to +40% attack on every drum except Misc.
- **Snare macros** (each at 100%):

| Macro | Body 200 Hz | 400 Hz | Wood 700 Hz | Crack 3 kHz | Air 8 kHz | Attack | Decay | Comp | Sat | Clip |
|---|---|---|---|---|---|---|---|---|---|---|
| Popify | | −4 dB | | | +3 dB | +30% | −30% | 40% | | |
| Rockify | +3 dB | | | | | | +40% | 25% | 30% | |
| Rimify | −5 dB | | +3 dB | +4 dB | | +50% | −50% | | | |
| Dubstepify | +4 dB | | | +1.5 dB | | +40% | +60% | 70% | 60% | 50% |

- **Stacking macros:** each column is added up and then soft-limited:
  - EQ columns to ±6 dB;
  - attack and decay to ±70%;
  - comp and sat to 80%.

  All four at 100% therefore stay sane, and opposite moves cancel (Rimify cuts what Dubstepify
  adds).
- **Link** (quick mix) is handled where the knobs move (the JUCE layer), not in the DSP.

## D-C12 — How the engine splits the loop into per-drum estimates

`dsp/include/plugin_dsp/DrumEngine.h`.

- **What the phase-2 plan left open:** the estimator (D1) needs a share for each drum per frequency
  bin, but a hit is only classified about 40–48 ms after its onset. The STFT frames around an onset
  are analysed earlier than that (from about +26 ms), so time-varying shares can't know yet which
  drum is hitting.
- **Chosen:** shares are fixed per bin and change only slowly.

  share(type, bin) = presence × spectrum / Σ over the enabled types

  - **Spectrum:** each type's long-term average hit spectrum. It starts generic and is learned from
    this loop's clean, single-type hits.
  - **Presence:** 0.02 for a type never heard, rising to 1 after 4 hits containing it. Misc counts
    only when it's a hit's main type.

  **Timing comes from the gates, not the shares.** A drum's settings act only while its gate is
  open, on the part of the spectrum that drum owns.
- **Result on the synthetic loop:**
  - kick −6 dB → −5.8 dB below 120 Hz, and the hats in between move 0.00 dB;
  - every setting neutral → output = input delayed by exactly 50 ms, bit for bit, at
    44.1/48/96 kHz.
- **Limits** (`VERIFY(dsp)` at home):
  - A drum's settings also act on anything else in its bins while its gate is open. Example: the
    kick's gate also moves a bass note under 120 Hz during the kick. This is the known cost of
    answer 1b.
  - Spectral shares smear about ±10 ms. On a hard edge the change can start a few ms before the
    2 ms pre-roll.
- **CPU:** about 21% of one sandbox core for stereo 48 kHz with every drum processing. The home
  i7-11700K should be well under that. `VERIFY(host)` in FL.
  - **Possible optimisation:** skip the estimates of drums whose settings are all neutral. Not done
    yet, because switching them on mid-playback could click.

## D-C13 — Plugin wiring choices

`Source/Parameters.h`, `Source/PluginProcessor.*`.

- **One table defines all 97 parameters:** ID, name, group, range, default, unit, and where the
  value goes in the DSP. The JUCE layout and the per-block copy can't drift apart.
  - Parameters are grouped by scope (Global, Quick mix, Kick, …), so FL's automation menu stays
    readable.
  - Frequency knobs use a log-feeling range: EQ mid centred at 1 kHz, crash low cut at 150 Hz,
    HPF at 150 Hz, LPF at 5 kHz.
- **Parameters.h is header-only,** so `CMakeLists.txt` did not change.
- **Bypass:** `global_bypass` is also the host's bypass parameter. Bypassing crossfades to the
  delayed dry signal and the 50 ms latency stays reported, so FL doesn't shift the track.
- **Link:** a 30 Hz timer on the message thread watches the three group volumes. When exactly one
  of them moved, the other two move by the same dB (clamped to their range).
  - It never runs on the audio thread.
  - Moves of several at once (automation, a preset) only re-base the offsets.
  - `VERIFY(host)`: how that feels with FL automation.
- **Saved state beyond the parameters:** `expected_types`, `detect_mode` (manual / auto) and
  `ui_zoom`, plus `state_version = 1`.
- **Auto detect ends on transport stop:** when playback stops, the types heard become the expected
  types, and the mode returns to manual.
- **Not saved (deviation from docs/02-parameters.md §5):** the learned per-type spectra. They
  re-learn within about 4 hits of each drum, and saving them safely would need a lock-free snapshot
  from the audio thread. Candidate for later.
- **Checked here:** the plugin builds on Linux (JUCE 9.0.3, Release, LTO) and pluginval passes at
  strictness 5 at 44.1/48/96 kHz, blocks 64–1024.

## D-C14 — The Puppet Show phase 2

- Wrote `prompts/04-puppet-show.md` (requirements only) and Claude's blind answer,
  `prompts/answers/04-claude.md`: measurements, genre targets, correction math, timing and safety.
- **Deliberately not built yet:**
  - The protocol's phase 2 compares Claude with the agents, and the genre targets are the part
    most likely to be wrong.
  - The `ps_*` parameters exist, so sessions stay compatible, but they do nothing yet.
- **Update (user, 2026-09-30):**
  - v1 targets = the **average** of the agents' answers to prompt 05 (one fixed table, so
    averaging is mechanical, cell by cell).
  - v2 = measured from the user's genre-named sample packs.
  - Claude's blind table (`prompts/answers/05-claude.md`) is averaged in as one more answer with
    equal weight, unless the user says agents only.
- **Please:** paste prompt 05 to your two agents.

## D-C15 — Puppet Show v1 genre targets

`docs/05-puppet-show-targets-v1.md`.

- **As the user decided:** the targets, tolerances and knob spans / max boost / max cut are the
  average of the three answers to prompt 05 (Claude blind, agent 1, agent 2, equal weight).
- **Claude's additions:**
  - **Real-kit check:** the answers were checked on 43 real E-GMD kits.
  - **Mud, snare presence, cymbal air:** they fit, so they're used as averaged.
  - **Kick sub and kick click:** these were physically impossible (a kick's energy is almost all
    below 90 Hz; its click is tiny in energy). So the answers' differences between genres are kept
    and the row is anchored to the real-kit median.
  - **Kick sub is redefined** as sub vs the rest, because "sub / total" saturates near 0 dB.
- **To undo:** use the plain averages in `research/puppet-show/average_targets.py`'s first table.
  Kick sub and click would then almost never be reachable, so the EQ would push every kick to
  its max cut or boost.

## D-C16 — The Puppet Show engine

`dsp/include/plugin_dsp/PuppetShow.h`, wired in `DrumEngine.h`.

Built from Claude's prompt-04 design. The agents answered only the targets (prompt 05); prompt 04
is optional.

- **Listening:** always on, even while switched off, so it acts the moment you switch it on.
  - It measures clean, single-type, confident hits from their rise spectrum.
  - A band the hit added nothing to isn't measured.
  - After 4 hits, one odd hit can't move the average by more than 12 dB.
  - The average covers about 12 hits.
- **Correcting:**
  - aim = target + span × knob. For Mud it's minus, so + means cut more.
  - Soft dead zone: the genre tolerance.
  - gain = Depth × 0.8 × error, clamped to the knob's max boost / cut.
  - Nothing before 4 hits; boosts at half size before 12 hits.
- **Glide:**
  - towards a stronger correction: τ 1.5 s (cuts 1 s);
  - back: τ 3 s;
  - Off or Depth 0: to exactly 0 in about 0.2 s.
- **Where it acts:** 3 reserved EQ slots in each drum's own chain (the user's knobs use the other
  8), so it acts only while that drum plays.

  | Knob | Drums | Band |
  |---|---|---|
  | Bass boost | kick | low shelf 70 Hz |
  | Mud control | kick / snare / toms (misc) | bells at 250 / 400 / 300 Hz |
  | Mid presence | snare, clap | bell 3 kHz |
  | Mid presence (click) | kick | bell 3.5 kHz |
  | High control | hats, cymbals | high shelf 8 kHz |

  It measures the raw hits, before the user's own settings, so it never fights the user's EQ.
- **Meters:** `meterDb()` is safe to read from the UI thread (phase 4).
- **Not built (v1):** the "harsh peak" bell from the prompt-04 design. The genre profiles
  (808 / acoustic kick …) suggested by agent 1 are parked for later.
- **Checked:**
  - 12/12 tests on gcc, Clang and ASan/UBSan: measurement math, no action before 4 hits, the
    correction formula, limits, the knob signs, off → 0.
  - With The Puppet Show on and nothing measured, the engine is still bit-exact transparent.
  - Plugin rebuilt; pluginval passes at strictness 5.
  - On real MDB loops the corrections stay within their limits.

## D-C17 — UI details decided by Claude

The full spec is in `docs/04-ui-concept.md`; screenshots are in `docs/screenshots/`. The user
decided the curtain button, strings added by themselves, "+ STRING" with the type chosen in deep
edit, and strings side by side and centred. Claude decided the rest:

- **Art is drawn in code (vector).** There's no AI image generator in this environment. Every
  piece is one function in `Source/ui/Art.h`, so it can be swapped for an image later.
- **The master:**
  - an original porcelain mask with a stitched smile and a hairline crack, cut off by the top
    edge;
  - long bony hands with dark nails and frayed gold cuffs, arms reaching down from the body;
  - a wooden control bar.
- **Colours:** black, maroon velvet, bone, dim gold, and a sickly green for "alive" things.
- **Update (user):** the background is now a late-night grey-blue room with an old drum set, and the master is very tall, with his skinny legs standing in the room. The velvet stays only for The Puppet Show.
- **Update (user, v2 look):** "looks like a 2016 plugin; make it a modern 2026 look, modern but
  creepy and eerie; change only the UI, decide the specifics". Claude redesigned every page (full
  spec: `docs/04-ui-concept.md`, "Look (v2)"): embedded fonts (Instrument Serif, Geist, Geist Mono,
  all SIL OFL 1.1) instead of the system serif; moonlit room with god rays, fog, grain and frosted
  glass panels; one colour per drum type; a glowing crack in the mask that flares on kicks;
  skeletal hands; drums that light up on hits; glass knobs and pills; genre tickets; a level fader
  per drum; strings from the master's fingers to the advanced knobs. No parameter, page or
  behaviour was removed. `VERIFY(ui)`: how it all looks under FL Studio's Direct2D renderer on
  Windows, at HiDPI, and its CPU at 60 fps.
- **Update (user, 2026-10-01):** The Puppet Show shows a live, reactive EQ graph like Gullfoss /
  Guardian Angel instead of five dB bars: spectrum before/after plus one EQ curve per drum, drawn
  from the engine's own band table (moved into `PuppetShow::bands`; the engine's output is
  bit-identical, checked on a 32 s loop with the show correcting). Needed one processor addition:
  a lock-free before/after copy of the bus for the spectrum (`copyScope()`).
- **Font:** the system serif (v1; v2 embeds its own, see below).
- **Top bar:** LISTEN (auto detect: clears the strings and forgets the loop), + STRING,
  PUPPET SHOW, bypass (power symbol), ? (about).
- **Quick mix** at the bottom in 3 groups plus LINK, with GLOBAL on the right.
- **A fresh instance listens (auto detect armed).** Before, docs/02-parameters.md §5 said
  "manual". That fits "strings added by themselves".
- **Misc hangs a wooden tom block.**
- **The deep edit page:**
  - left: the big drum on its string, the type chooser, CUT STRING;
  - right: knob groups (kick: weight / dynamics / EQ / level; snare: the four macros first).
- **Knobs:** they show their value while hovered; double-click resets; bipolar arcs light from
  the centre.
- **The Puppet Show page:** marquee, ON/OFF lever, five genre posters, the big Depth knob,
  four bipolar knobs, five live meters (±8 dB, up = boost, down = cut).
- **Advanced page:** "the master's hands", with FORGET THIS LOOP.
- **Window:** resizable at 16:10, 900 to 1600 px wide. Everything scales, nothing reflows.
- **Screenshot demo mode:** `DRUM_MASTER_DEMO=<page>` shows made-up hits so screenshots work
  without audio. It never touches the sound.
- `VERIFY(host)`: Shadow Play's ring position with FL's latency compensation, and editor CPU at
  60 fps in FL.

---

## D-C18 — External code review (Argon, 2026-10-14)

Argon reviewed the DSP core and plugin wiring and reported 8 defects. Claude reproduced each one
with a test before fixing it. Every new test fails on main and passes after the fix.

| ID | Claim | Verdict | What changed |
|---|---|---|---|
| D1 | The hit tracker never measures a hit's peak (the hit arrives 40 ms late, after the 20 ms peak window), so the per-type tail drop and the flam guard are dead. Silence pins the noise floor at −140 dB. | **Real, worse than reported.** A closed hat over a −60 dB noise bed stayed open until the 1 s cap. | The tracker keeps 100 ms of level history and replays a hit's start when the hit arrives; the floor can't sink below −80 dB. Closed hat now ends at 72 ms (it was 134 ms in silence and 1000 ms over noise). |
| D2 | Misc as "the unexplained rest" opens misc gates on ordinary hits, blocks the engine's spectrum learning, and lets switched-off types steal the kick's sub share. | **Real, theft worse than reported.** In manual mode with hats off, misc took 55 % of the kick's 47 Hz bin (Argon estimated 25 %). | A misc gate opens only for hits whose main type is misc. The engine's learning ignores the misc rest. Misc learns its own spectrum like every other type. The kick keeps 100 % of its sub. |
| D3 | A retrigger skips the new hit's 2 ms pre-roll, and the attack/tail gain jumps up to 69 dB in one sample. | **Real.** | The pre-roll counts as the new hit (age −1). The shape gain glides over about 0.5 ms while the gate is open. |
| D4 | `reset()` keeps the classifier's presence data; `prepare()` resets before `setParams()`; LISTEN leaves old strings visible. | **1st and 3rd real; 2nd not a bug.** The targets already hold the latest parameters when `prepare()` runs. | `reset()` also forgets presence. LISTEN clears the heard strings at once. |
| D5 | The onset detector waits up to ~12 ms longer than needed before reporting a layered event, past its promised maximum delay. | **Real.** The synthetic kit never showed it, but real drums did (measured 2026-10-14): before the fix, 78 of 7,201 MDB Drums events and 7 of 1,761 electronic ones were reported after the promised 37.9 ms, at worst 43.5 ms. All still met the engine's 48 ms deadline, so nothing was audible. | It now closes the event as soon as no band can join. Events are identical, only reported sooner: on real drums the mean is 22 ms instead of 27 ms, the worst case 34.8 ms, and none are late. Tests now check every event's report time. Results: `docs/testing/results/2026-10-14-start-of-week-vs-now.txt`. |
| D6 | The rise spectrum's two lowest bands (30–60 and 60–100 Hz) read the same FFT bin. | **Real, but not fixed yet.** | Fixing it changes The Puppet Show's kick-sub measurement, whose targets were anchored to 43 real kits measured with the current bins. The fix and a re-measure go together (a heavy download, so it needs your OK). |
| D7 | Switching the compressor off and on again ducks from a stale envelope. | **Real, small.** | The envelope restarts when the compressor switches on. |
| D8 | Quick-mix Link loses the dB offsets when a linked volume hits its rail. | **Real.** | Link keeps each volume's unclamped position, so a volume pushed against its rail comes back to its offset. Argon's fix also pulled back the knob you are dragging, so Claude didn't use it. |

**Not done:**
- **Per-block cost.** Argon noted the channel filters are designed twice per block. That costs
  well under 1 % of a block, so it was left as is.
- **Weak spots.** Argon's hypotheses for the weak spots are good research leads, not bugs: clap
  burst features, open/closed-hat revision at 90 ms, high-band flatness for crash vs ride, a
  background term in the shares, ADAA instead of oversampling, and level-relative compressor
  thresholds. They are parked for phase 5.

`VERIFY(dsp)`: listen at home for retriggered hits with Attack/Decay turned up (D3), and check
that hat and snare tails now end sooner with Decay settings (D1).

---

## D-C19 — MIDI mode: the details Claude decided

You chose: MIDI in, General MIDI with several notes per drum, presets with the project and as files.
Claude decided the rest:

| Decision | Why | Alternative |
|---|---|---|
| A note goes into the classifier as a labelled hit (the network is skipped, everything else runs) | The rise spectrum, peak level, tail tracking, shares and Puppet Show all keep working unchanged | Bypass the classifier entirely: simpler, but the engine and Puppet Show would lose their spectra |
| Notes within 6 ms are one hit | Same window as audio grouping. Kick + hat as two hits would teach the kick the hat's spectrum | Every note separate |
| While notes keep coming, audio onsets without a note are ignored | The notes are the truth. A crash with no note shouldn't be guessed at | Classify those too |
| No note for 4 s: audio detection takes over | A pattern with gaps, or MIDI that stops, still gets processed | Stay MIDI-only |
| Hit time = note time (no snapping to the audio onset) | With FL's latency compensation they should line up; the 2 ms pre-roll covers small offsets | Snap to the nearest detected onset (more code, and onsets are reported ~40 ms late) |
| MIDI mode is saved state, not a host parameter | Automating it makes no sense, and the 97-parameter table stays as it is | An automatable toggle |
| Velocity is ignored | The hit's level comes from the audio | Scale the gate by velocity |
| Presets are XML files in `Documents/The Drum Master/MIDI maps/` | Easy to find, back up and share | AppData (hidden from users) |
| The page shows FL's note names (middle C = C5) | You use FL Studio | Standard names (C4) |
| CMakeLists.txt: `NEEDS_MIDI_INPUT TRUE` | Required for a plugin to receive MIDI | None |
| **MIDI write (F3b):** a take ends at the first jump of the song position | FL loops patterns; without this, every pass would stack on the last | Keep recording and remove duplicates (guesswork) |
| MIDI write: drums are written on their usual GM note, else their lowest mapped note | Kick on 36, not 35, which most kits expect | Always the lowest note |
| MIDI write: velocity relative to the loudest hit of the same drum (30 dB range) | Works at any input level | Absolute dBFS |
| MIDI write: misc goes to note 34, or the next free note below | Just below the GM drums, so no default kit uses it | A note in the 80s |
| MIDI write: the file starts at the bar where the take began | Dropped on a bar line in FL, the notes land where they were played | Start at the first note |
| MIDI write: files are always saved to `Documents/The Drum Master/Written MIDI/` | Dragging needs a real file, and you keep every take | Temp folder |

`VERIFY(host)`: FL routing of MIDI into an effect; MIDI vs audio alignment; the AU type change on macOS;
dragging the .mid from the plugin into FL's playlist and piano roll; FL's song position with pattern
looping (the take should end after one pass).

---

## Classifier results

- **Measured with:** the plugin's own onset detector and classifier (`research/classifier-eval`)
  on real drums the network never trained on.
- **Scoring:** per-type F1. A type counts as "said" when its weight ≥ 0.5, and a detection matches
  a labelled hit within 25 ms.
- **Onset detection is unchanged:** acoustic F1 0.879, electronic 0.975.

| Type | Acoustic (MDB Drums, 23 tracks): rules → **learned** | Electronic loops (40): rules → **learned** |
|---|---|---|
| kick | 0.63 → **0.83** | 0.78 → **0.78** |
| snare | 0.61 → **0.65** (precision 0.48 → 0.75) | 0.24 → **0.53** |
| closed hat | 0.27 → **0.51** | 0.22 → **0.58** |
| open hat | 0.00 → 0.06 | 0.32 → 0.23 |
| crash | 0.03 → 0.14 | 0.06 → 0.10 |
| ride | 0.01 → **0.35** | 0.00 → 0.08 |
| rimshot | 0.56 → 0.42 | 0.00 → 0.14 |
| clap | none in the set; false claps 2205 → 283 | 0.50 → 0.33 |

**Also measured:**
- On 6 E-GMD kits held out of training, including 808, lo-fi, metal and jazz kits: macro F1 0.59,
  kick 0.80, snare 0.75, closed hat 0.64, ride 0.66.
- On Groove performances held out of training: macro F1 0.85.

**What this means:**
- **Kick, snare and closed hat** — most of every loop — are clearly better and usable.
- **Open hat vs closed hat, crash vs ride, and claps** are still weak. When unsure these fall to
  Misc, which is neutral, or to the neighbouring type.
  - A wrong neighbour mostly means the hi-hat or cymbal settings of the wrong page get used.
  - `VERIFY(dsp)`: listen at home.
- **The synthetic test kit's snare** (almost pure bright noise) is read as a closed hat. A very
  noisy electronic snare could get hat settings. `VERIFY(dsp)` with real 909-style snares.
- **Best next data:** your genre-named sample packs (v2), for claps, open hats and cymbals.

**Choices made while training (all in `research/classifier-train/`):**
- **Network:** 96 hidden units, not 48. Slightly better, and still only about 22 k
  multiply-adds per hit.
- **E-GMD:** 30 performances per kit, 350 k events sampled.
- **Synthetic loops:** 1000, for about 6 000 claps.
- **EQ augmentation:** ±6 dB tilt and ±6 dB bumps.
- **Level augmentation** (−24…+6 dB on every row): quiet loops were being read as hats.
- **Kit hold-out:** Acoustic Kit, 808 Simple, Heavy Metal, Nu RNB, Cassette (Lo-Fi), Jazz.
- **The unit test** (`test_hit_classifier.cpp`) is now a sanity check on the synthetic kit: every
  kick right, isolated ≥ 60%, dense loop ≥ 85%, multi-label, mask and presence. The
  accuracy target is the table above.
