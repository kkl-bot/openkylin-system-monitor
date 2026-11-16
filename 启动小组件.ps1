param([switch]$Mini, [switch]$Build)
$ErrorActionPreference = 'Stop'
$distribution = 'openKylin-3.0-desktop'
$linuxProject = (& wsl.exe -d $distribution -- wslpath -a "$PSScriptRoot").Trim()
if ($LASTEXITCODE -ne 0) { throw '无法找到 openKylin 中的项目路径。' }
if ($Build) {
    & wsl.exe -d $distribution -u openkylin -- bash "$linuxProject/scripts/build.sh"
    if ($LASTEXITCODE -ne 0) { throw '编译或测试未通过。' }
}
$mode = if ($Mini) { '--mini' } else { '--detail' }
& wsl.exe -d $distribution -u openkylin -- bash "$linuxProject/scripts/launch.sh" $mode
exit $LASTEXITCODE
