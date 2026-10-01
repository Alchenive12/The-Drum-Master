# The Drum Master: realistic asset pack

Late-night horror look built on the palette in `Source/ui/Theme.h`. Original designs only.

## Use the PNGs in JUCE, keep the SVGs as masters
The realism comes from SVG filters (noise for grime, plaster, wood grain and tarnish, plus blur for
light and shadow). JUCE's SVG loader ignores filters, so load the `png/` files (rendered at 2x)
with `juce::ImageCache::getFromMemory` and draw them with `g.drawImage (img, bounds)`. Edit the
`svg/` files and re-render with `rsvg-convert -z 2 file.svg -o file@2x.png`.

| File | Size (1x) | Replaces in `Art.h` / `Theme.h` |
|---|---|---|
| `background` | 1100x688 | `room()` + `oldDrumSet()`; keep the animated dust and flicker in code on top |
| `mask` | 300x380 | `mask()`; only the chin and smile show at the top edge |
| `panel` | 1040x146 | the quick-mix box at (30, 532); labels and dividers stay in code |
| `knob_body` / `knob_pointer` | 200x200 | `drawRotarySlider`; draw the body in the inner ~78% so the red arc shows, rotate the pointer around its centre by the slider angle |
| `button_off/hover/on` | 120x34 | `drawButtonBackground`; text stays in `drawButtonText` |
| `lever_off/on` | 120x140 | the Puppet Show ON/OFF lever |
| `icon_power/help/back` | 48x48 | BYPASS, ?, back arrow |

The plaque buttons have screws at the ends, so stretching them a lot horizontally distorts the
screws. For very different widths, draw them in three slices (left end, middle, right end).

PNG total is about 4 MB, so BinaryData stays small.
