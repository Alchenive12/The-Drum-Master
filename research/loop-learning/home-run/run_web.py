import sys
sys.path.insert(0, r"C:\Users\didi\source\repos\The-Drum-Master\train-home\src\research\loop-learning")
import home_packs
sys.argv = ["home_packs.py", "--out", r"D:\dm-data\webpk", "--roots", r"D:\dm-data\web",
            "--loops-per-pack", "20", "--samples-per-loop", "2", "--max-loops", "200", "--seed", "11"]
home_packs.main()
