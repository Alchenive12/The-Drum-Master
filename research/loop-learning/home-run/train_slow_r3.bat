@echo off
set PY=C:\Users\didi\source\repos\The-Drum-Master\train-home\env\python.exe
set T=C:\Users\didi\source\repos\The-Drum-Master\train-home
set LL=D:\dm-data\ll
set M=D:\dm-data\models
set PYTHONUTF8=1
cd /d %T%\src\research\loop-learning
set TEST=%LL%\acoustic.csv %LL%\electronic.csv %LL%\kit.csv %LL%\electronic_kit2.csv %LL%\home_test.csv
set TRAIN=%LL%\gmd.csv %LL%\egmd.csv %LL%\synth.csv %LL%\home_train.csv@3 %LL%\home2_train.csv@2 %LL%\web_train.csv@2
%PY% -u train_slow_torch.py --train %TRAIN% --test %TEST% --egmd-subset D:\dm-data\pub\egmd\subset.csv --out %M%\r3_bal.npz --hidden 1024,512 --max-rows 1400000 --max-train-rows 1400000 --data-on-cpu --epochs 120 --cosine --patience 15 || exit /b 1
%PY% -u vote_eval.py %M%\r3_bal.npz %TEST% > %M%\r3_bal.vote.txt
%PY% -u %T%\ens.py %M%\ens4.npz %M%\r1_512.npz %M%\r2_512.npz %M%\r2_1024.npz %M%\r3_bal.npz
%PY% -u vote_eval.py %M%\ens4.npz %TEST% > %M%\ens4.vote.txt
echo R3 DONE
