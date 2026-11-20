param([switch]$Mini, [switch]$Build)
$ErrorActionPreference = 'Stop'
$distribution = 'openKylin-3.0-desktop'
$mode = if ($Mini) { '--mini' } else { '--detail' }
# --exec preserves native arguments; WSL's default shell would consume Windows path backslashes.
if (-not $Build) {
    $installed = & wsl.exe -d $distribution -u openkylin --exec sh -c 'if [ -x /usr/bin/kylin-monitor ]; then printf installed; fi'
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    if (($installed -join '').Trim() -eq 'installed') {
        & wsl.exe -d $distribution -u openkylin --exec /usr/bin/kylin-monitor $mode
        exit $LASTEXITCODE
    }
}

# Explicit -Build, or no installed application: launch the incrementally rebuilt workspace.
$linuxProjectOutput = & wsl.exe -d $distribution -u openkylin --exec wslpath -a "$PSScriptRoot"
if ($LASTEXITCODE -ne 0) {
    $pathExitCode = $LASTEXITCODE
    [Console]::Error.WriteLine('无法找到 openKylin 中的项目路径。')
    exit $pathExitCode
}
$linuxProject = ($linuxProjectOutput -join "`n").Trim()
if ([string]::IsNullOrWhiteSpace($linuxProject)) {
    [Console]::Error.WriteLine('openKylin 返回了空的项目路径。')
    exit 1
}
& wsl.exe -d $distribution -u openkylin --exec bash "$linuxProject/scripts/launch.sh" $mode
exit $LASTEXITCODE
