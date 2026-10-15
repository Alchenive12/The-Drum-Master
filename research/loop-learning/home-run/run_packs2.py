# Drives research/loop-learning/home_packs.py with the owner's sample folders (UTF-8 paths, incl. Hebrew).
import sys
sys.path.insert(0, r"C:\Users\didi\source\repos\The-Drum-Master\train-home\src\research\loop-learning")
import home_packs
roots = [r"D:\stuff to save", r"D:\MHA", r"D:\Sickrate & SIIK Essentials I (WAV Samples)",
         r"D:\Sickrate and SIIK Essentials II Full Pack", r"D:\Splice", r"D:\Splice2",
         r"D:\CymaticsLofiToolkit-V1-m2d", r"D:\Drum Stems", r"D:\Downloads old\DABRO Music - Future Bass Vol.2",
         r"C:\Users\didi\Documents\Splice - user-2587851674",
         r"C:\Program Files\Image-Line\FL Studio 2024\Data\Patches\Packs", "D:\\תופים את היחידה", r"D:\Music folder]"]
sys.argv = ["home_packs.py", "--out", r"D:\dm-data\home2", "--roots", *roots,
            "--collections", "KSHMR", "Trap-hiphop", "Future bass-pop", "Future bounce-house", "Itai New Pc stuff",
            "--skip", r"D:\stuff to save\Midi", r"D:\stuff to save\Unison MIDI Chord Pack", r"D:\stuff to save\Stems for dumae",
            r"D:\stuff to save\goodbye mix attempt", r"D:\stuff to save\Vocals",
            "--loops-per-pack", "20", "--samples-per-loop", "2", "--max-loops", "250", "--seed", "7"]
home_packs.main()
