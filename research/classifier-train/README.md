# Hit classifier training

The learned classifier (docs/03-decisions-for-review.md, D-C1): 225 log-mel segment features
(dsp/include/plugin_dsp/HitFeatures.h) → 48 ReLU → 9 sigmoid (TinyMlp.h). Not part of the plugin build.

Data (licences decide what may train the shipped weights):

| Set | Licence | Use |
|---|---|---|
| Groove MIDI Dataset (Magenta) | CC BY 4.0 | training + held-out validation — credit required in the plugin's About box |
| Expanded Groove MIDI Dataset (E-GMD, Magenta) | CC BY 4.0 | training (43 kits) + 6 held-out kits — same credit |
| Own synthetic loops (`synth_aug.py`) | ours | training (claps, 808s, drum-machine hats Groove lacks) |
| MDB Drums | CC BY-NC-SA 4.0 | evaluation only |
| Dirt-Samples loops (`../classifier-eval/export_sets.py`) | unclear | evaluation only |

Steps (S = a scratch directory):

    python3 prep_gmd.py <groove_dir> $S/evalsets/gmd          # WAV links + "<sample> <type>" labels
    python3 fetch_egmd.py $S/egmd 30                           # E-GMD subset: 30 performances x 43 kits (~11 GB)
    python3 prep_gmd.py $S/egmd $S/evalsets/egmd subset.csv
    python3 synth_aug.py $S/evalsets/synth 150
    python3 ../classifier-eval/export_sets.py <mdb> <dirt> $S/evalsets
    g++ -O2 -std=c++17 -I../../dsp/include -I../classifier-eval extract.cpp -o $S/extract
    for s in gmd egmd synth acoustic electronic; do $S/extract $S/evalsets/$s $S/X_$s.csv; done
    # (train.py expects X_gmd, X_syn, X_ac, X_el: rename synth/acoustic/electronic accordingly)
    python3 train.py $S ../../dsp/include/plugin_dsp/HitClassifierWeights.h --hidden 48 --aug 1

`extract` runs the plugin's own OnsetDetector + HitFeatures, so training sees exactly the onsets
and features the plugin sees; detections with no label within 25 ms get all-zero labels (the
model learns to say "nothing confident" → misc).
