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
- **Knob** (`scripts/knob_test.py`): round 2 approved as a direction; the user wants the
  **abandoned** look (cracked yellowed bone, heavier grime and verdigris, moonlight). Final
  filmstrip later.
- **Stage** (`scripts/stage.py`): first draft of the room (wallpaper, floorboards, cracked window with
  moonlight and dust, old drum set) and the master (legs, hands with elongated fingers on the control
  bar, cracked porcelain mask with a stitched smile). Original design, not FNaF's Puppet: no rosy
  cheeks, tear streaks or striped cuffs.
- `render_list.txt` picks which scripts CI renders, to save minutes.
- Shared helpers: `scripts/lib3d.py`.
