<!-- File: docs/04-ui-concept.md -->
# The Drum Master — Phase 4: UI concept

> Status: **built (2026-09-30)**, for review. Screenshots below.
> - **User's answers:** marked [user].
> - **Everything else:** decided by Claude from the context of the earlier answers ("decide by
>   context of my earlier answers"), marked [Claude], and listed in
>   `docs/03-decisions-for-review.md` as D-C17.
> - **Screenshots:** in `docs/screenshots/`.
> - **Background:** `docs/01-concept.md` (look and feel) and `docs/05-features.md` (F1, F2).

## Art
- **Everything is drawn in code as vector graphics** [Claude]. This environment has no AI image
  generator, and vector art stays sharp at every window size.
- **Swapping in art later:** every piece (stage, hands, mask, drums, curtain, knobs) is one drawing
  function in `Source/ui/Art.h`. Replacing one with an image later changes only that function.
- **Mood** [user]: creepy, FNaF-puppet feeling, original design only.
  - **Palette:** near-black stage, deep maroon velvet, bone/ivory for the puppet master and
    knobs, dim gold for text, a sickly green glow for "alive" things (meters, the playhead).
  - **Font:** the system serif (theatre-poster feel).

## Window
- **Size** [Claude]: resizable, fixed 16:10 shape, 900×563 up to 1600×1000. Default 1100×688.
- **Pages:** stage (main), deep edit (one drum), The Puppet Show, the master's hands (advanced),
  about. The open page is saved in `ui_zoom`.

## Main page: the stage
From top to bottom:

1. **Top bar** [Claude]:
   - left: the title, "THE DRUM MASTER";
   - middle: **LISTEN** (auto detect: glows green while armed), **+ STRING** [user] and
     **THE PUPPET SHOW** [user];
   - right: **BYPASS** and **?** (about).
2. **The puppet master** [concept]:
   - He's taller than the window: only the bottom of his pale mask shows at the top edge, a thin
     cracked smile, never the eyes.
   - Two long bony hands hold the wooden control bar. They sway slowly while idle, and the mask
     "breathes".
   - Clicking him (mask, hands or bar) opens **the master's hands** (advanced page) [concept].
3. **Strings** [user]:
   - One string per drum type, hung **side by side and always centred**. The bar grows with the
     number of strings.
   - **Auto:** a new string drops in when a new type is recognised [user], with a small drop and
     bounce animation.
   - **Manual:** **+ STRING** adds an empty string with a "?" tag and opens its deep edit, where you
     choose its type [user].
   - When a drum is heard, its string **jerks up** and settles, and the jerk size follows the hit
     level [concept]. Two drums at once: both jerk.
   - Each string ends in that drum, drawn small. Misc hangs a **wooden tom block**, because toms
     are Misc [Claude].
   - Click a string or its drum → **deep edit** (zoom in) [concept].
4. **Shadow Play** [F2, decided]:
   - The ring on the stage floor under the strings, drawn in perspective: a clock face synced to
     the playhead, where hits stack up and fade like smoke.
   - Shapes: kick circle, snare triangle, closed hat dot, open hat hollow ring, crash starburst,
     ride diamond, rimshot slash, clap three triangles, misc faint square.
   - Click → it fills the window; click again → back.
5. **Quick mix** [concept]:
   - At the bottom, three groups (**KICK:** volume, punch, weight · **SNARE:** volume, snap, fat ·
     **HATS:** volume, tone, tame), plus the **LINK** switch between them.
   - On the right: **GLOBAL** (dry/wet, output, sensitivity, punch, clip).

When the stage has no strings yet (a fresh instance listening), it says "Play your loop… the
master is listening" [Claude].

## Deep edit (one drum) [Claude]
- **The zoom:** clicking a string zooms from that drum into its page (~0.3 s), and the back arrow
  zooms out.
- **Left third:** the drum, large, under a spotlight, still on its string (it still jerks on hits),
  with its name.
  - **"This string is a: [type ▾]"**: the type chooser [user]. For a new "?" string it's
    the first thing shown. Changing it moves the string to the new type.
  - **CUT STRING:** removes the string (manual mode).
- **Right two thirds:** that drum's knobs in labelled groups.
  - **Kick:** WEIGHT (low-end boost, bottom, top) · DYNAMICS (decay, compression, saturation,
    clipping) · EQ (low, mid, mid freq, high) · LEVEL.
  - **Snare:** CHARACTER (the four blendable macros: Dubstepify, Popify, Rockify, Rimify) ·
    BODY & CRACK (crack, body, decay) · DYNAMICS (compression, saturation) · EQ · LEVEL.
  - **Other drums:** one group with their controls (docs/02-parameters.md), then LEVEL.
- **Knobs:**
  - Every knob shows its value while you move it.
  - Double-click returns a knob to its default.
  - Bipolar knobs light their arc from the centre.

## The Puppet Show [user + F1]
- **Opening** [user]: the **THE PUPPET SHOW** button closes red velvet curtains over the stage
  (~0.4 s), then they open again (~0.5 s) on the show page.
- **The show page** [Claude]:
  - a small theatre with a lit marquee, "THE PUPPET SHOW";
  - an **ON/OFF** lever (off by default);
  - the **genre** as five posters (Rock, Pop, EDM, Metal, Hip-hop);
  - **Depth** as the big centre knob;
  - the four bipolar knobs (Bass boost, Mud control, Mid presence, High control);
  - five **live band meters** (kick sub, mud, presence, kick click, air) that move up for boosts
    and down for cuts, with ±8 dB marks.
- **Closing:** the back arrow closes the curtains and reopens them on the stage.

## The master's hands (advanced) [concept + Claude]
- Drive, saturation, high-pass, low-pass.
- **FORGET THIS LOOP:** resets what was learned (presence and spectra).

## About [Claude]
- Version and credits. The Groove MIDI and Expanded Groove MIDI credit (CC BY 4.0) is **required**
  (D-C2).

## Technical [Claude]
- **No CMakeLists.txt change:** all UI code is header-only under `Source/ui/`, included by the
  processor.
- **Timer and threads:**
  - A 60 Hz editor timer drives the animations.
  - It reads hits from the lock-free Shadow Play queue (the UI thread is the only reader) and the
    band meters (atomics).
  - It reads the transport position (atomics written by processBlock) for the ring.
- **UI actions never touch the DSP directly.** Adding or removing a string, re-arming auto detect
  and "forget" set atomics or flags, which the audio thread applies at the start of the next
  block.
- **Screenshots:**
  - The standalone app runs under a virtual display in the sandbox, with `DRUM_MASTER_DEMO=<page>`.
  - That environment variable lets the editor play fake hits, so the strings, jerks and Shadow Play
    show without audio.
  - It never affects the sound, and the plugin ignores it unless it's set.

## Screenshots (sandbox, `DRUM_MASTER_DEMO`, made-up hits)
| Page | File |
|---|---|
| Stage: puppet master, 6 strings, Shadow Play ring, quick mix | `screenshots/stage.png` |
| Deep edit: snare (four macros on top) | `screenshots/snare.png` |
| Deep edit: kick | `screenshots/kick.png` |
| A new string: choose its type | `screenshots/new.png` |
| The Puppet Show's curtain closing | `screenshots/curtain.png` |
| The Puppet Show page (meters show made-up values) | `screenshots/show.png` |
| The master's hands (advanced) | `screenshots/advanced.png` |
| Shadow Play enlarged | `screenshots/shadow.png` |
| About and credits | `screenshots/about.png` |

**Checked in the sandbox:**
- The plugin builds (Linux, Release, LTO) with no warnings from our code.
- pluginval passes at strictness 5 **including its editor tests**: opening, and opening the
  editor while audio is processing.
