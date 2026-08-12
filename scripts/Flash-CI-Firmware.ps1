[CmdletBinding()]
param(
    [switch]$SelfTest,
    [switch]$List,
    [switch]$ListOnly,
    [switch]$Preflight,
    [string]$Item = '',
    [string]$Port = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$python = $null
foreach ($candidate in @('python', 'py')) {
    $command = Get-Command $candidate -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($command -and $command.Source) { $python = $command.Source; break }
}
if (-not $python) { Write-Error 'Python 3 was not found.'; exit 2 }

$arguments = @((Join-Path $PSScriptRoot 'ci_firmware.py'))
if ($SelfTest) { $arguments += '--self-test' }
if ($List -or $ListOnly) { $arguments += '--list' }
if ($Preflight) { $arguments += '--preflight' }
if ($Item) { $arguments += @('--item', $Item) }
if ($Port) { $arguments += @('--port', $Port) }
& $python @arguments
exit $LASTEXITCODE
