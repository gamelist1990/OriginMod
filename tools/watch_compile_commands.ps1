param(
    [string]$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot ".."))
)

$ErrorActionPreference = "Stop"

function Invoke-CompileCommands {
    param([string]$Root)

    Write-Host "[auto_compile_commands] regenerate compile_commands.json..." -ForegroundColor Cyan
    Push-Location $Root
    try {
        xmake project -k compile_commands | Out-Host
    } finally {
        Pop-Location
    }
}

$root = (Resolve-Path $ProjectRoot).Path
Write-Host "[auto_compile_commands] root: $root" -ForegroundColor DarkGray

Write-Host "[auto_compile_commands] running every 5 seconds..." -ForegroundColor Green

while ($true) {
    Invoke-CompileCommands -Root $root
    Start-Sleep -Seconds 20
}