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
- **Step 1, now:** one test knob (`scripts/knob_test.py`) at min / mid / max, plus the mid position at
  -1 and +1 stop, to judge the look and the lighting before building everything.
- **Next, if the look is approved:**
  - knob filmstrips (every position, transparent background with a contact shadow);
  - buttons, lever and LEDs in each state;
  - the room background;
  - then wiring them into `Source/ui/Art.h` in a separate PR.
