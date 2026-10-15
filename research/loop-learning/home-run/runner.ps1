# The Drum Master home-training job runner (Claude). Runs every jobs\*.bat it finds, each in its own
# minimized window; output goes to logs\<name>.log, exit code to logs\<name>.exit.
# Stop it by closing this window, or by creating jobs\STOP.
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$jobs = Join-Path $root 'jobs'; $logs = Join-Path $root 'logs'
New-Item -ItemType Directory -Force $jobs, $logs | Out-Null
Add-Type -Namespace W -Name P -MemberDefinition '[DllImport("kernel32.dll")] public static extern uint SetThreadExecutionState(uint f);'
[W.P]::SetThreadExecutionState([uint32]2147483649) | Out-Null   # keep the PC awake while this runs
$host.UI.RawUI.WindowTitle = 'Drum Master training runner (Claude) - close to stop'
"runner started $(Get-Date -Format s)" | Out-File (Join-Path $logs 'runner.txt') -Append -Encoding ascii
Write-Host 'Drum Master training runner is waiting for jobs. Close this window to stop it.'
while (-not (Test-Path (Join-Path $jobs 'STOP'))) {
  Get-ChildItem $jobs -Filter '*.bat' | ForEach-Object {
    $n = $_.BaseName
    Rename-Item $_.FullName ($n + '.run') -Force
    "$(Get-Date -Format s) start $n" | Out-File (Join-Path $logs 'runner.txt') -Append -Encoding ascii
    Write-Host "$(Get-Date -Format T) started $n"
    Start-Process cmd.exe -ArgumentList '/c', (Join-Path $jobs 'run_one.cmd'), $n -WindowStyle Minimized
  }
  [W.P]::SetThreadExecutionState([uint32]2147483649) | Out-Null
  "$(Get-Date -Format s)" | Out-File (Join-Path $logs 'heartbeat.txt') -Encoding ascii
  Start-Sleep -Seconds 3
}
Remove-Item (Join-Path $jobs 'STOP')
"runner stopped $(Get-Date -Format s)" | Out-File (Join-Path $logs 'runner.txt') -Append -Encoding ascii
