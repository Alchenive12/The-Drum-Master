# 3D UI assets

Ultra-realistic UI art for The Drum Master, rendered in Blender instead of drawn as SVG.
Same designs and layout as `assets/ui/` (the SVG pack); only the rendering changes.

## Pipeline
1. `scripts/*.py` build each asset procedurally (models, materials, lights, camera) and render it.
2. `.github/workflows/render-assets.yml` runs them headless in GitHub Actions (Blender from apt,
   Cycles on CPU, Linux only) on pushes to `remote/3d-*` branches, and commits the PNGs to
   `renders/` on the same branch.
3. Review the renders in the PR, adjust, push again.

## Status
- **Stage, body: final (round 9).** The master is seen up to the shoulders: arms raised out of frame,
  holding the control bar above the plugin; only the strings fall in, drawn by the plugin (any number,
  sway, jerk on hits). No mask, no hands in frame. Original design, not FNaF's Puppet.
  Known small issues, to tackle later if at all: jacket reads light grey under the warm lamp; the pale
  neck stub between the arms draws the eye; leg folds are subtle.
- **Stage backgrounds: final renders** at 2x (2200x1376), no preview strings, in three lamp levels:
  `stage_background.png` (100%), `stage_background_lamp050.png`, `stage_background_lamp015.png`.
  The plugin crossfades between them at random moments for an occasional flicker (the moon stays).
- **Knob:** abandoned pass in progress (dusty lacquer, yellowed cracked bone, more verdigris, stronger
  moonlight), drafted at low quality. Then the filmstrip: every position, transparent background with
  its own shadow.
- **Next:** name plates (normal / hover / active), lever switches (up / down), LEDs (off / on) in 3D;
  icons probably stay 2D. Optional: a realistic string texture for the plugin to stretch.
- **Then (separate PR):** wire the renders into the UI code, update `docs/04-ui-concept.md` (no mask /
  hands layers; strings from the top edge).

## Minutes
Drafts run at low samples / resolution; only finals get full quality. `render_list.txt` picks what
renders on each push. Renders are committed to the branch (no artifact upload).
