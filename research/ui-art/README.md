# Realistic UI art generators

Python scripts that write the SVG masters in `assets/ui/svg/`. Run them in one folder, then
render each SVG at 2x with librsvg (it supports the noise and blur filters):

```
python3 bg.py && python3 mask.py && python3 ui.py
for f in *.svg; do rsvg-convert -z 2 "$f" -o "${f%.svg}@2x.png"; done
```

`mock.py` composes the stage preview (`docs/screenshots/realistic-stage-mockup.png`); it expects
the background render as `bg2x.png` and the mask render as `mask2x.png` in the same folder.
