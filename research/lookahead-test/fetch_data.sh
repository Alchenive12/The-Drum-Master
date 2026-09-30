#!/usr/bin/env bash
# File: research/lookahead-test/fetch_data.sh
# Downloads the test data OUTSIDE the repo (audio is never committed).
#   MDB Drums    - CC BY-NC-SA 4.0 (research use), https://github.com/CarlSouthall/MDBDrums
#   Dirt-Samples - one-shots from the TidalCycles sample library, https://github.com/tidalcycles/Dirt-Samples
set -euo pipefail
dest="${1:-../../../drum-master-data}"
mkdir -p "$dest" && cd "$dest"
git clone --depth 1 --filter=blob:none --no-checkout https://github.com/CarlSouthall/MDBDrums mdb
(cd mdb && git checkout HEAD -- README.md "MDB Drums/annotations" "MDB Drums/audio/drum_only")
git clone --depth 1 --filter=blob:none --no-checkout https://github.com/tidalcycles/Dirt-Samples dirt
(cd dirt && git sparse-checkout init --no-cone \
  && printf '%s\n' /README.md /808bd/ /bd/ /clubkick/ /hardkick/ /popkick/ /kicklinn/ /808sd/ /sn/ /sd/ \
       /808hc/ /hc/ /hh/ /808oh/ /ho/ /cr/ /808cy/ /cp/ /realclaps/ /rm/ /rs/ > .git/info/sparse-checkout \
  && git checkout HEAD)
echo "Run: pip install numpy scipy scikit-learn soundfile && python3 lookahead_test.py \"$dest/mdb/MDB Drums\" \"$dest/dirt\""
