<!-- File: docs/04-ui-concept.md -->
# The Drum Master — Phase 4: UI concept

> Status: **built (2026-09-30)**, then **redesigned the same day ("v2 look")** at the user's request:
> "it looks like a 2016 plugin, I want a 2026 modern look, modern but creepy and eerie; change only
> the UI, decide the specifics yourself". Every page, control and behaviour stayed; only how they
> look (and a few ways they respond to the mouse) changed. See "Look (v2)" below. Screenshots below.
> - **User's answers:** marked [user].
> - **Everything else:** decided by Claude from the context of the earlier answers ("decide by
>   context of my earlier answers"), marked [Claude], and listed in
>   `docs/03-decisions-for-review.md` as D-C17.
> - **Screenshots:** in `docs/screenshots/`.
> - **Background:** `docs/01-concept.md` (look and feel) and `docs/05-features.md` (F1, F2).

## The room and the very tall master [user, 2026-09-30]
- **Background:** an eerie, dark, grey room with a little blue in it, late at night.
  - Faded striped wallpaper with damp stains.
  - A tall cracked window high on the left, with cold moonlight falling in a shaft across the
    floorboards and dust drifting in the light.
  - An **old dusty drum set** in the dark back corner: a split bass-drum head, a floor tom under a
    dust sheet, tarnished cymbals.
  - The Puppet Show keeps its velvet theatre, because it's "the show".
- **The master is very tall:** his bowed head (the mask) hangs in at the top, and **his legs stand
  in the room behind the strings**.
  - Very long, skinny and bowed outwards, with knobbly knees.
  - Charcoal pinstripe trousers with a cold moonlit edge, and long curled pointed shoes on the
    floor.
  - His sleeves are the same charcoal suit.

## Art
- **Everything is drawn in code as vector graphics** [Claude]. This environment has no AI image
  generator, and vector art stays sharp at every window size.
- **Swapping in art later:** every piece (room, hands, mask, legs, drums, curtain, theatre) is one
  drawing function in `Source/ui/Art.h`. Replacing one with an image later changes only that function.
- **Mood** [user]: creepy, FNaF-puppet feeling, original design only.

## Look (v2) [Claude, 2026-09-30, user's brief: "modern but creepy and eerie"]
The idea: a horror film still, not a skeuomorphic hardware panel. Moonlight, fog, dust and film
grain carry the dread; the controls on top are quiet, modern glass.

- **Type** (embedded, identical on every machine; all SIL Open Font License 1.1, credited on the
  About page, licences in `Source/ui/fonts/`):
  - **Instrument Serif** (and its italic) for names and titles: "Snare", "The Puppet Show". The
    italic is the master's whisper ("the master is listening", "a crack, like a bone").
  - **Geist** for the interface, as small tracked capitals (labels, buttons).
  - **Geist Mono** for numbers (values, dB, "STRING 02 / 06").
- **Colour:** blue-black ink, cold moonlight and porcelain bone. Light means something:
  - **ectoplasm green:** alive (LISTEN, the playhead, the glowing crack in the mask);
  - **one hue per drum:** kick blood, snare bone, closed hat ectoplasm, open hat cold cyan, crash
    bruise violet, ride midnight blue, rimshot candle amber, clap rose, misc ash. A drum's hue
    lights its string, its knobs' arcs, its deep edit and its Shadow Play shape;
  - **blood red:** danger (bypass, CUT STRING);
  - **limelight** (warm) for The Puppet Show; its meters are warm for boosts, cold for cuts.
- **Light and surfaces:** a cracked arched window with a clouded moon behind dead branches,
  god rays and a pool of moonlight on the boards, fog along the skirting, dust turning in the
  light, an empty oval portrait that is looking back, cobwebs, vignette and film grain. Panels are
  frosted dark glass (the room is really blurred behind them) with hairline edges.
- **The master:** a porcelain mask whose crack **glows from inside** and flares with every kick;
  long skeletal hands (knobbly three-joint fingers, dark nails, frayed yellowed cuffs) wrapped over
  an ebony bar with brass caps and a screw eye per string; pinstriped legs lit from the window side,
  casting long shadows across the floor.
- **Drums** are moonlit objects: the kick's head carries the master's stitched smile as a logo,
  the crash is cracked, the clap is two bony hands. On a hit a drum **lights up in its hue**, its
  string twitches and glows, and the boards under it catch the light. A new "?" string hangs an
  old **toe tag**.
- **Controls:**
  - knobs: a dark lathe-turned cap in its own shadow, a glowing value arc (from the centre for
    bipolar ones), a bone needle; big knobs get an LED ring of ticks. Hover shows the value in mono.
  - buttons: glass pills with line icons; a lit button glows (LISTEN breathes while armed).
  - The Puppet Show: genre **tickets** (notched stubs with icons), an ON/OFF **switch**.
  - the drum's **level is a tall fader** with a ghost meter of its hits (deep edit).
- **Bypass:** the strings go slack and the drums sink a little, and the status line says BYPASSED.

## Window
- **Size** [Claude]: resizable, fixed 16:10 shape, 900×563 up to 1600×1000. Default 1100×688.
- **Pages:** stage (main), deep edit (one drum), The Puppet Show, the master's hands (advanced),
  about. The open page is saved in `ui_zoom`.

## Main page: the stage
From top to bottom:

1. **Top bar** [Claude]:
   - left: the wordmark "*The* Drum Master" and a status line (LISTENING, "6 STRINGS", BYPASSED);
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
   - Click a string or its drum → **deep edit** (zoom in) [concept]. Hovering a string lights it;
     hovering the master flares the crack in his mask and names what a click opens.
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
   - v2: a frosted glass dock; group names with their drum's colour dot; chain links between the
     groups light up while LINK is on.

When the stage has no strings yet (a fresh instance listening), it says "Play your loop… the
master is listening" [Claude].

## Deep edit (one drum) [Claude]
- **The zoom:** clicking a string zooms from that drum into its page (~0.3 s), and the back arrow
  zooms out.
- **Left third:** the drum, large, under a spotlight, still on its string (it still jerks on hits),
  with its name (v2: large serif, a whispered line under it, "STRING 02 / 06" at the top).
  - **"This string is a: [type ▾]"**: the type chooser [user]. For a new "?" string it's
    the first thing shown. Changing it moves the string to the new type.
  - **CUT STRING:** removes the string (manual mode).
- **Right two thirds:** that drum's knobs in labelled groups (v2: glass cards packed into rows of
  up to five knobs; the drum's LEVEL moved out of the grid into a tall fader card on the right).
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
  - ~~five live band meters~~ → since 2026-10-01 a **live EQ graph** in the manner of Gullfoss and
    Guardian Angel [user: "more a reactive EQ curve, less just bands of dB"]:
    - behind: the bus **spectrum** before the plugin (a ghost line) and after it (filled), 20 Hz to
      20 kHz, ±12 dB scale for the EQ;
    - in front: the show's **EQ curve for each drum** (kick, snare, clap, toms, hats & cymbals) in
      the drum's colour, drawn from the same band table the engine uses
      (`plugin_dsp::PuppetShow::bands`) with the live gains, so it is exactly what is applied;
      a curve flares when its drum is hit;
    - a **handle per measure** (kick sub, mud, presence, kick click, air) with its dB: warm when it
      boosts, cold when it cuts;
    - "CORRECTING" while on; "OFF - LISTENING ONLY" (and the hint) while off.
- **Closing:** the back arrow closes the curtains and reopens them on the stage.
- v2: a black stage under a limelight spotlight with haze, velvet wings, a valance and footlights;
  the marquee's bulbs chase; genres are tickets; the live EQ graph sits in the middle, the knobs
  (Depth in the centre) under it.
- **Technical (the graph):** the processor keeps a mono copy of the bus before and after the plugin
  in two lock-free rings (relaxed atomics, no allocation, no locks; it never touches the sound).
  The editor reads the latest 4096 samples, windows them and runs the dsp/ core's own FFT
  (`Source/ui/Spectrum.h`), with a +4.5 dB/octave display tilt and a fast rise / slow fall. The
  graph refreshes at 30 fps (sandbox: 15 % of a core, as much as the old bar meters).

## The master's hands (advanced) [concept + Claude]
- Drive, saturation, high-pass, low-pass.
- v2: the master's two hands hang open above the four knobs, and a string runs from a fingertip to
  each knob's pointer: when a knob turns, its string follows. He is working them.
- **FORGET THIS LOOP:** resets what was learned (presence and spectra).

## About [Claude]
- Version and credits. The Groove MIDI and Expanded Groove MIDI credit (CC BY 4.0) is **required**
  (D-C2). v2 adds the fonts' credit (SIL OFL 1.1).

## Technical [Claude]
- **No CMakeLists.txt change:** all UI code is header-only under `Source/ui/`, included by the
  processor: `Theme.h` (colours, type), `Fx.h` (glow, glass, blur, grain, cached layers, icons),
  `LookAndFeel.h`, `Widgets.h`, `Art.h`, and `FontData.h` (the fonts as byte arrays, generated by
  `research/ui-fonts/make_font_header.py`; no BinaryData needed).
- **Cached scenery (v2):** each page's static background (room, blur, fog, glass, grain) is rendered
  once into an image at the screen's real pixel density and then only blitted; it re-renders when
  the window's scale changes (never during a zoom). The 60 Hz timer only redraws what moves: on the
  stage, the scene above the dock (the dock's knobs repaint themselves); the master's hands page at
  30 fps. Sandbox CPU (software renderer, one core, 60 fps): stage 48 % (v1: 53 %), The Puppet Show
  12 % (v1: 15 %), the master's hands 25 % (v1: static, 3 %).
- **Fonts lifetime:** the typefaces are a `DeletedAtShutdown` singleton, released with JUCE's own
  shutdown (never after it, when a host unloads the plugin).
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
| A fresh instance: empty stage, the master listening (`DRUM_MASTER_DEMO=listen`) | `screenshots/listen.png` |
| Deep edit: snare (four macros on top) | `screenshots/snare.png` |
| Deep edit: kick | `screenshots/kick.png` |
| A new string: choose its type | `screenshots/new.png` |
| The Puppet Show's curtain closing | `screenshots/curtain.png` |
| The Puppet Show page: live EQ graph (made-up spectrum and gains) | `screenshots/show.png` |
| The master's hands (advanced) | `screenshots/advanced.png` |
| Shadow Play enlarged | `screenshots/shadow.png` |
| About and credits | `screenshots/about.png` |

**Checked in the sandbox (v1 and again for v2):**
- The plugin builds (Linux, Release, LTO) with no warnings from our code (v2 also checked with
  clang `-Wconversion -Wshadow-all -Wdouble-promotion`).
- pluginval passes at strictness 5 **including its editor tests**: opening, opening the editor
  while audio is processing, and editor automation.
- v2: resizing to 1600 x 1000 re-renders the scenery sharp; clicks and hovers checked by driving the
  Standalone with xdotool.
