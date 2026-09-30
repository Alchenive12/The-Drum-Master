<!-- File: prompts/answers/agent2.md -->
# Answer to prompt 01 — Agent 2

> Pasted by the user on 2026-09-30. The content is unchanged; only the headings were formatted.

## Kick
| Control name | What it does (plain words) | Suggested range + unit | Default | Why this drum needs it |
|---|---|---|---|---|
| Top | Adjusts the high-frequency beater click | -12 to +12 dB | 0 dB | Helps the kick cut through a dense mix or softens a clicky kick |
| Bottom | Adjusts the chest-hitting punch (60-120Hz) | -12 to +12 dB | 0 dB | Controls the main power and presence of the kick drum |
| Sub | Adjusts ultra-low frequencies (below 60Hz) | -12 to +12 dB | 0 dB | Adds cinematic weight or removes muddy rumble |
| Tightness | Shortens or lengthens the decay/boom of the kick | -100 to +100 % | 0 % | Cleans up overly long kicks without needing a gate |
| Compression | Evens out the dynamics of the kick hits | 0 to 100 % | 0 % | Keeps the kick consistent in volume and adds punch |
| Saturation | Adds harmonic distortion and warmth | 0 to 100 % | 0 % | Helps the kick translate on small speakers |
| EQ | Standard graphical equalizer for fine-tuning | Multi-band | Flat | Fixes specific problematic resonant frequencies |

Changes from your decided list:
- Removed "Low-end boost": Redundant with "Bottom". I replaced it with "Sub" to differentiate between chest punch (Bottom) and deep rumble (Sub).
- Removed "Clipping": Hard clipping on a specific drum within a stereo mix can introduce nasty intermodulation distortion with background elements. Better kept global.
- Added "Tightness": You need an envelope/transient shaper to control the kick's tail length, which EQ and static compression cannot do.

## Snare
| Control name | What it does (plain words) | Suggested range + unit | Default | Why this drum needs it |
|---|---|---|---|---|
| Crack | Boosts or cuts the initial stick attack/transient | -100 to +100 % | 0 % | Makes the snare snap through the mix or softens a harsh hit |
| Tail / Wires | Adjusts the sustain and volume of the snare wires | -100 to +100 % | 0 % | Brings out the ghost notes/buzz or tightens up a sloppy snare |
| Saturation | Adds harmonic distortion and grit | 0 to 100 % | 0 % | Gives the snare attitude and perceived loudness |
| Compression | Evens out the hits and shapes the envelope | 0 to 100 % | 0 % | Keeps backbeats anchored and consistent |
| EQ | Standard graphical equalizer | Multi-band | Flat | Removes boxiness or adds air to the top end |

Changes from your decided list:
- Added "Crack": Snare needs dedicated transient attack control. EQ alone won't fix a weak, flat stick hit.
- Added "Tail / Wires": Controlling the sustain envelope is crucial for snares to manage the room sound and wire buzz independently of the attack.
- (No removals; your base list was solid but just needed envelope control).

## Closed hi-hat
| Control name | What it does (plain words) | Suggested range + unit | Default | Why this drum needs it |
|---|---|---|---|---|
| Brightness | Adjusts the extreme high frequencies (air) | -12 to +12 dB | 0 dB | Opens up dull hats or darkens overly bright ones |
| Harshness (optional) | Dynamic cut in the 3-5kHz region | 0 to 100 % | 0 % | Tames the ear-piercing "ice pick" frequencies automatically |
| Tick / Attack | Enhances the initial stick tip impact | -100 to +100 % | 0 % | Helps fast 16th-note patterns stay defined |
| Tightness | Shortens the decay of the closed hat | -100 to +100 % | 0 % | Removes bleed and creates a super-dry, programmed feel |

## Open hi-hat
| Control name | What it does (plain words) | Suggested range + unit | Default | Why this drum needs it |
|---|---|---|---|---|
| Sizzle | Adjusts high-frequency shelf | -12 to +12 dB | 0 dB | Highlights the splashy top-end |
| Body | Adjusts the lower mid-range (gong-like tone) | -12 to +12 dB | 0 dB | Removes the cheap, clunky mid-frequencies often found in hats |
| Length | Controls how long the open hat rings out | -100 to +100 % | 0 % | Stops loose open hats from washing out the entire groove |
| Saturation (optional) | Adds grit to the high frequencies | 0 to 100 % | 0 % | Helps the hat cut through dense guitars or synths |

## Crash
| Control name | What it does (plain words) | Suggested range + unit | Default | Why this drum needs it |
|---|---|---|---|---|
| Shimmer | Adjusts the very high frequencies (10kHz+) | -12 to +12 dB | 0 dB | Adds expensive-sounding air to the cymbal |
| Wash / Sustain | Controls the volume of the cymbal tail | -100 to +100 % | 0 % | Prevents crash cymbals from eating up all the mix headroom |
| Thickness | Adjusts the low-mid body of the cymbal | -12 to +12 dB | 0 dB | Gives a thin crash more authority and weight |
| Width (optional) | Widens the stereo image of the crash frequencies | 0 to 200 % | 100 % | Pushes the crash outside the speakers to clear the center |

## Ride
| Control name | What it does (plain words) | Suggested range + unit | Default | Why this drum needs it |
|---|---|---|---|---|
| Ping | Boosts the transient attack in the upper mids | -100 to +100 % | 0 % | Accentuates the stick definition so the pattern is readable |
| Bell Focus | Targeted EQ boost around the bell frequencies | -12 to +12 dB | 0 dB | Brings out the musical "ding" of the ride bell |
| Wash | Controls the sustain and underlying roar | -100 to +100 % | 0 % | Keeps the cymbal build-up from burying the snare and kick |
| Air | High frequency shelf | -12 to +12 dB | 0 dB | Adds breath and space to the cymbal top |

## Rimshot
| Control name | What it does (plain words) | Suggested range + unit | Default | Why this drum needs it |
|---|---|---|---|---|
| Wood | Adjusts the low-mid "knock" frequency (500-800Hz) | -12 to +12 dB | 0 dB | Accentuates the physical shell sound |
| Attack | Controls the initial sharp transient | -100 to +100 % | 0 % | Makes the rimshot poke through or pushes it back |
| Ring | Controls the resonant tail | -100 to +100 % | 0 % | Tames annoying metallic overtones or enhances natural resonance |
| Saturation (optional) | Adds harmonic density | 0 to 100 % | 0 % | Fattens up a weak, thin rimshot |

## Clap
| Control name | What it does (plain words) | Suggested range + unit | Default | Why this drum needs it |
|---|---|---|---|---|
| Splash | Adjusts high-frequency energy | -12 to +12 dB | 0 dB | Makes the clap sound more modern and airy |
| Thickness | Adjusts the 200-400Hz range | -12 to +12 dB | 0 dB | Gives the clap a group feel rather than a single hand |
| Width | Applies mid/side widening to the hit | 0 to 200 % | 100 % | Pushes claps wide, keeping them out of the snare's way |
| Decay | Controls the length of the clap's room sound | -100 to +100 % | 0 % | Tightens a sloppy clap or extends a short one |
| Smack | Adjusts the initial transient spike | -100 to +100 % | 0 % | Softens harsh peaks often found in layered claps |

## Misc
| Control name | What it does (plain words) | Suggested range + unit | Default | Why this drum needs it |
|---|---|---|---|---|
| Presence | Broad high-mid EQ shelf | -12 to +12 dB | 0 dB | Safely brightens unidentified percussion |
| Body | Broad low-mid EQ band | -12 to +12 dB | 0 dB | Safely adds weight to unidentified percussion |
| Attack | Transient shaper (attack only) | -100 to +100 % | 0 % | Adds punch to bongos, toms, or weird Foley hits |
| Sustain | Transient shaper (sustain only) | -100 to +100 % | 0 % | Controls the ring of unidentified elements safely |

## Snare macros
- **Dubstepify:** Turns a natural snare into a massive, heavy-hitting EDM snare. Underlying settings moved: Boosts "Bottom/Body" heavily around 200Hz, aggressively boosts the "Crack" transient, pushes heavy clipping/saturation, and heavily compresses the tail to bring up a wall of white noise.
- **Popify:** Makes the snare tight, clean, and snappy, sitting perfectly out of the way of vocals. Underlying settings moved: Cuts muddy low-mids (~400Hz), boosts top-end air/EQ, shortens the "Tail/Wires" for a very dry decay, and applies fast, snappy compression.
- **Rockify:** Creates a loud, aggressive, natural-sounding snare with plenty of bleed and ring. Underlying settings moved: Boosts lower-mid body, increases the "Tail / Wires" to emphasize shell resonance and room sound, applies moderate warm saturation, and utilizes parallel compression for density.
- **Rimify:** Re-shapes a full snare hit to mimic the pokey, woody sound of a rimshot. Underlying settings moved: Drastically cuts the low-end sub/body, heavily boosts a narrow EQ band around 600-800Hz (the "Wood" frequency), shortens the tail entirely, and slightly boosts the attack transient.

## Quick-mix groups
- **Kick:** Weight (macro controlling Sub/Bottom EQ for instant fatness) · Punch (macro controlling transient Attack + Compression)
- **Snare:** Snap (macro controlling top-end EQ + transient Crack) · Fatness (macro controlling Saturation + low-mid body EQ)
- **Hats:** Brightness (macro controlling high-shelf EQ across all hat/cymbal types) · Tame (macro that simultaneously lowers Harshness and shortens Decay/Length to clean up sloppy top-end)

## Risks and pitfalls
- **Phase shift artifacts:** Applying aggressive, sudden EQ changes to a specific frequency band only during a detected hit can cause audible phase shifts and "tearing" against the continuous background audio (like cymbal wash or room tone).
- **Misclassification glitches:** If the lookahead AI misidentifies a tom as a snare, or a crash as an open hat, the plugin will suddenly apply the wrong EQ/transient settings for a split second, causing jarring, unmusical jumps in the audio.
- **Transient masking on simultaneous hits:** When a kick and crash happen simultaneously, applying different dynamic envelopes to overlapping frequency ranges might create unnatural comb-filtering or cause the processing of one drum to swallow the transient of the other.
- **Unnatural tail truncation:** Using transient tools to shorten the "decay" of an open hat will effectively duck the high frequencies of the entire drum bus for that brief moment, audibly sucking the life out of any cymbals or room noise playing in the background.
- **Pumping/breathing:** Because it works on a mixed stereo file, applying heavy compression or saturation to just the "snare" hit will cause any background elements (like hi-hats playing at the exact same millisecond) to distort or pump in volume alongside the snare.
