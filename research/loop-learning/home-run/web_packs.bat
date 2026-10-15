@echo off
rem free SampleRadar (MusicRadar) packs: royalty-free, "use in your music in any way, don't redistribute". Kept on this PC only.
mkdir D:\dm-data\web 2>nul
cd /d D:\dm-data\web
for %%U in (
 https://cdn.mos.musicradar.com/audio/samples/musicradar-drum-samples.zip
 https://cdn.mos.musicradar.com/audio/musicradar-hardware-drum-machine-samples.zip
 https://cdn.mos.musicradar.com/audio/samples/sampleradar-analogue-drums-samples.zip
 https://cdn.mos.musicradar.com/audio/samples/musicradar-essential-drumkit-samples.zip
 https://cdn.mos.musicradar.com/audio/samples/sampleradar-obscure-drum-machines-samples.zip
 https://cdn.mos.musicradar.com/audio/sampleradar-radio-ready-drums-samples.zip
 https://cdn.mos.musicradar.com/audio/samples/musicradar-808-samples.zip
 https://cdn.mos.musicradar.com/audio/samples/musicradar-trap-samples.zip
 https://cdn.mos.musicradar.com/audio/samples/musicradar-ultimate-hihat-samples.zip
 https://cdn.mos.musicradar.com/audio/samples/sampleradar-hats-cymbals-gongs-sample-pack.zip
 https://cdn.mos.musicradar.com/audio/samples/musicradar-retro-drum-machine-samples.zip
 https://cdn.mos.musicradar.com/audio/samples/musicradar-metal-drum-samples.zip
 https://cdn.mos.musicradar.com/musicradar-sampleradar-processed-808-909-samples.zip
) do (
  if not exist "%%~nU.zip" curl -sS -L --retry 5 -A "Mozilla/5.0" -o "%%~nU.zip" %%U
  if not exist "%%~nU\" ( mkdir "%%~nU" & tar -xf "%%~nU.zip" -C "%%~nU" --exclude "__MACOSX*" )
  echo %%~nU
)
dir /s /b *.wav | find /c ".wav"
echo WEB OK
