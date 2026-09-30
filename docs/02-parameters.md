<!-- File: docs/02-parameters.md -->
# The Drum Master — Phase 2c: Parameter table

> Status: **proposal, waiting for approval**. This closes phase 2.
> Sources: `docs/01-concept.md`, `docs/02-mix-settings.md` (the controls),
> `prompts/answers/*` (ranges), `docs/02-algorithm.md` (the engine).

## Rules
- **IDs are permanent.** Once a session is saved with them, they can never be renamed.
  - Format: `<scope>_<control>` in snake_case, for example `kick_level`, `snare_crack`.
  - Each one is registered with JUCE parameter version hint 1.
- **Neutral defaults.** Every per-drum and quick-mix control defaults to "no change", so a fresh
  instance is transparent. The only exceptions are the global mix (100% wet) and sensitivity (50%).
- **Level stacking:** a drum's gain = its own **Level** + its quick-mix **group volume** (in dB).
  - With **Link** on, moving one group volume moves the other two by the same number of dB.
- **Decay** ranges are two-sided, but the engine test (`research/engine-shootout`) showed that
  shortening a tail on a mixed loop is only about 40–60% accurate. The positive side (a longer tail)
  adds at most +6 dB of tail gain.
- **Not host parameters** (saved in the plugin state, not automatable): the expected drum types, the
  auto-detect state and the learned templates. Automating them mid-song would make detection jump.
- **Internal constants** (thresholds, time constants) are not parameters. They live in
  `docs/02-algorithm.md` §8.

## 1. Global (main screen)
| ID | Name | Range | Default | Unit | Notes |
|---|---|---|---|---|---|
| `global_mix` | Dry/Wet | 0 – 100 | 100 | % | |
| `global_output` | Output | −24 – +12 | 0 | dB | |
| `global_sensitivity` | Detection sensitivity | 0 – 100 | 50 | % | Scales the onset threshold α and the confidence cut-off θ_conf |
| `global_punch` | Punch | 0 – 100 | 0 | % | Transient emphasis on the whole bus |
| `global_clip` | Clip | 0 – 100 | 0 | % | Soft clip on the whole bus, 0 = off |
| `global_bypass` | Bypass | off / on | off | — | Latency stays reported when bypassed, so FL doesn't jump |

## 2. Quick mix (main screen)
| ID | Name | Range | Default | Unit | Covers |
|---|---|---|---|---|---|
| `qm_kick_volume` | Kick volume | −24 – +12 | 0 | dB | kick |
| `qm_kick_punch` | Kick Punch | −100 – +100 | 0 | % | |
| `qm_kick_weight` | Kick Weight | −100 – +100 | 0 | % | |
| `qm_snare_volume` | Snare volume | −24 – +12 | 0 | dB | snare, rimshot, clap |
| `qm_snare_snap` | Snare Snap | −100 – +100 | 0 | % | |
| `qm_snare_fat` | Snare Fat | −100 – +100 | 0 | % | |
| `qm_hats_volume` | Hats volume | −24 – +12 | 0 | dB | closed hat, open hat, ride, crash |
| `qm_hats_tone` | Hats Tone | −100 – +100 | 0 | % | Dark ↔ bright |
| `qm_hats_tame` | Hats Tame | 0 – 100 | 0 | % | De-harsh plus a shorter decay |
| `qm_link` | Link | off / on | off | — | Keeps the dB offsets between the three group volumes |

## 3. Deep edit per drum (click a string)
Shared definitions, so every page behaves the same way:
- **Level:** −24 – +12 dB, default 0.
- **Tone:** −6 – +6 dB tilt around 1 kHz, default 0.
- **Band gains** (Body, Top, Bottom, Ping, Bell, Wash): −12 – +12 dB, default 0.
- **Transient controls** (Crack, Tick, Click, Attack): −100 – +100 %, default 0.
- **Decay:** −100 – +100 %, default 0.
- **Amounts** (Compression, Saturation, Clipping, De-harsh): 0 – 100 %, default 0.
- **Width:** 0 – 200 %, default 100.

### Kick (12)
| ID | Name | Range | Default | Unit |
|---|---|---|---|---|
| `kick_level` | Level | −24 – +12 | 0 | dB |
| `kick_lowboost` | Low-end boost (sub, < ~60 Hz) | 0 – +12 | 0 | dB |
| `kick_top` | Top (beater click) | −12 – +12 | 0 | dB |
| `kick_bottom` | Bottom (body, ~60–150 Hz) | −12 – +12 | 0 | dB |
| `kick_decay` | Decay | −100 – +100 | 0 | % |
| `kick_comp` | Compression | 0 – 100 | 0 | % |
| `kick_sat` | Saturation | 0 – 100 | 0 | % |
| `kick_clip` | Clipping (soft, kick band only) | 0 – 100 | **0 = off** | % |
| `kick_eq_low` | EQ low shelf (100 Hz) | −12 – +12 | 0 | dB |
| `kick_eq_mid` | EQ mid bell | −12 – +12 | 0 | dB |
| `kick_eq_midfreq` | EQ mid frequency | 200 – 5000 | 1000 | Hz |
| `kick_eq_high` | EQ high shelf (8 kHz) | −12 – +12 | 0 | dB |

### Snare (14)
| ID | Name | Range | Default | Unit |
|---|---|---|---|---|
| `snare_level` | Level | −24 – +12 | 0 | dB |
| `snare_crack` | Crack (attack) | −100 – +100 | 0 | % |
| `snare_body` | Body | −12 – +12 | 0 | dB |
| `snare_decay` | Decay | −100 – +100 | 0 | % |
| `snare_comp` | Compression | 0 – 100 | 0 | % |
| `snare_sat` | Saturation | 0 – 100 | 0 | % |
| `snare_eq_low` / `_mid` / `_midfreq` / `_high` | EQ (as on the kick) | as kick | 0 / 0 / 1000 / 0 | dB / Hz |
| `snare_dubstepify` | Dubstepify | 0 – 100 | 0 | % |
| `snare_popify` | Popify | 0 – 100 | 0 | % |
| `snare_rockify` | Rockify | 0 – 100 | 0 | % |
| `snare_rimify` | Rimify | 0 – 100 | 0 | % |

The macros move hidden settings, and several macros can be blended. What each one does is described in
`docs/02-mix-settings.md`. Their exact curves are set in phase 3.

### Closed hat (6)
`chat_level` · `chat_tone` · `chat_tick` · `chat_decay` · `chat_deharsh` · `chat_width`

### Open hat (7)
`ohat_level` · `ohat_tone` · `ohat_body` · `ohat_decay` · `ohat_width` · `ohat_sat` · `ohat_deharsh`

### Crash (7)
`crash_level` · `crash_tone` · `crash_body` · `crash_decay` · `crash_width` · `crash_deharsh` ·
`crash_lowcut` (20 – 800 Hz, default **20 = off**)

### Ride (6)
`ride_level` · `ride_ping` · `ride_bell` · `ride_wash` · `ride_tone` · `ride_width`

### Rimshot (6)
`rim_level` · `rim_click` · `rim_body` · `rim_tone` · `rim_decay` · `rim_sat`

### Clap (8)
`clap_level` · `clap_attack` · `clap_body` · `clap_tone` · `clap_decay` · `clap_width` · `clap_comp` ·
`clap_sat`

### Misc (4)
`misc_level` · `misc_tone` · `misc_attack` · `misc_decay`

All of them use the shared ranges and defaults above.

## 4. Advanced global (click the puppet master)
The concept says "global distortion, saturation, filter, etc.". Version 1 has only what was named.
Anything more is added in phase 5.

| ID | Name | Range | Default | Unit |
|---|---|---|---|---|
| `adv_drive` | Distortion drive | 0 – 100 | 0 | % |
| `adv_sat` | Saturation | 0 – 100 | 0 | % |
| `adv_hpf` | High-pass filter | 20 – 1000 | **20 = off** | Hz |
| `adv_lpf` | Low-pass filter | 1000 – 20000 | **20000 = off** | Hz |

## 5. Plugin state (saved, not automatable)
| Key | What | Default |
|---|---|---|
| `expected_types` | Which of the 9 types are enabled (manual mode, or the auto-detect result) | all 9 on |
| `detect_mode` | manual / auto-detect-armed | manual |
| `learned_templates` | Per-type templates adapted to this loop | generic templates |
| `ui_zoom` | Which puppet or page is open | main |

## Totals
- **Host parameters:** 6 global + 10 quick mix + 70 per drum + 4 advanced = **90**.
  - The per-drum 70: 12 kick + 14 snare + 6 closed hat + 7 open hat + 7 crash + 6 ride + 6 rim + 8
    clap + 4 misc.
  - FL Studio handles this comfortably. They're grouped by scope so its automation menu stays
    readable.
- **Latency reported:** 50 ms, fixed (D2), at every sample rate.
