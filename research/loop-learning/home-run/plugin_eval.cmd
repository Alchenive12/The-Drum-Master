@echo off
rem plugin_eval.cmd <model name in D:\dm-data\models, without .npz>: builds the plugin's classifier with that
rem network (export_mlp2.py), re-runs extract_long (= the plugin's own detector + classifier) on the test sets,
rem and scores plugin-today vs new exactly as eval.cpp does.
setlocal
set N=%1
set T=C:\Users\didi\source\repos\The-Drum-Master\train-home
set PY=%T%\env\python.exe
set D=%T%\srcnew_%N%
set PYTHONUTF8=1
xcopy /e /i /y /q %T%\src\dsp %D%\dsp >nul
xcopy /e /i /y /q %T%\src\research %D%\research >nul
copy /y %T%\src2\TinyMlp.h %D%\dsp\include\plugin_dsp\ >nul
copy /y %T%\src2\HitClassifier.h %D%\dsp\include\plugin_dsp\ >nul
%PY% -u %T%\src\research\loop-learning\export_mlp2.py D:\dm-data\models\%N%.npz %D%\dsp\include\plugin_dsp\HitClassifierWeights.h --note "FULL: trained on the owner's commercial packs - personal use only (docs/08)" || exit /b 1
call "C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 2
cd /d D:\dm-data\bin
cl /nologo /O2 /EHsc /std:c++17 /bigobj /D_CRT_SECURE_NO_WARNINGS /I%D%\dsp\include /I%D%\research\classifier-eval /Feextract_long_%N%.exe %D%\research\loop-learning\extract_long.cpp || exit /b 3
mkdir D:\dm-data\ll_%N% 2>nul
for %%s in (acoustic electronic kit) do %PY% -u %T%\src\research\loop-learning\par_extract.py D:\dm-data\bin\extract_long_%N%.exe D:\dm-data\sets\%%s D:\dm-data\ll_%N%\%%s.csv 8
%PY% -u %T%\src\research\loop-learning\par_extract.py D:\dm-data\bin\extract_long_%N%.exe D:\dm-data\sets\electronic_kit\electronic_kit D:\dm-data\ll_%N%\electronic_kit2.csv 8
%PY% -u %T%\src\research\loop-learning\par_extract.py D:\dm-data\bin\extract_long_%N%.exe D:\dm-data\home\test D:\dm-data\ll_%N%\home_test.csv 12
%PY% -u %T%\plugin_score.py D:\dm-data\ll D:\dm-data\ll_%N% acoustic electronic kit electronic_kit2 home_test
