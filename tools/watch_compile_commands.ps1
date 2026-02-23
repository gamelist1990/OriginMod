param(
    [string]$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")),
    [int]$DebounceMs = 500,
    [int]$IntervalSeconds = 10    # <= 0 to disable periodic regeneration
)

$ErrorActionPreference = "Stop"

function Invoke-CompileCommands {
    param([string]$Root)

    # Use a named mutex to avoid concurrent xmake runs from different event runspaces
    $mutexName = "OriginMod_watch_compile_commands_mutex"
    $mutex = New-Object System.Threading.Mutex($false, $mutexName)
    $acquired = $false
    try {
        $acquired = $mutex.WaitOne(0)
        if (-not $acquired) {
            Write-Host "[watch_compile_commands] skipped (already running)" -ForegroundColor Yellow
            return
        }

        Write-Host "[watch_compile_commands] regenerate compile_commands.json..." -ForegroundColor Cyan
        Push-Location $Root
        try {
            # Keep output short; errors will still surface.
            xmake project -k compile_commands | Out-Host
        } finally {
            Pop-Location
        }
    } finally {
        if ($acquired) { $mutex.ReleaseMutex() }
    }
}

function New-DebouncedAction {
    param(
        [scriptblock]$Action,
        [int]$DelayMs
    )

    $timer = New-Object System.Timers.Timer
    $timer.Interval = $DelayMs
    $timer.AutoReset = $false
    Register-ObjectEvent -InputObject $timer -EventName Elapsed -Action $Action | Out-Null
    return $timer
}

$root = (Resolve-Path $ProjectRoot).Path
Write-Host "[watch_compile_commands] root: $root" -ForegroundColor DarkGray

# Initial generate (useful on first run)
Invoke-CompileCommands -Root $root

$timer = New-DebouncedAction -DelayMs $DebounceMs -Action {
    Invoke-CompileCommands -Root $root
}

# Optional periodic regeneration
if ($IntervalSeconds -gt 0) {
    Write-Host "[watch_compile_commands] periodic regeneration every ${IntervalSeconds}s" -ForegroundColor DarkGray
    $periodicTimer = New-Object System.Timers.Timer
    $periodicTimer.Interval = $IntervalSeconds * 1000
    $periodicTimer.AutoReset = $true
    Register-ObjectEvent -InputObject $periodicTimer -EventName Elapsed -Action {
        Invoke-CompileCommands -Root $using:root
    } | Out-Null
    $periodicTimer.Start()
}

function Start-Watcher {
    param(
        [string]$Path,
        [string]$Filter,
        [bool]$IncludeSubdirectories
    )

    $fsw = New-Object System.IO.FileSystemWatcher
    $fsw.Path = $Path
    $fsw.Filter = $Filter
    $fsw.IncludeSubdirectories = $IncludeSubdirectories
    $fsw.NotifyFilter = [IO.NotifyFilters]'FileName, LastWrite, Size, DirectoryName'

    $handler = {
        # restart debounce timer
        $timer.Stop()
        $timer.Start()
    }

    Register-ObjectEvent $fsw Changed -Action $handler | Out-Null
    Register-ObjectEvent $fsw Created -Action $handler | Out-Null
    Register-ObjectEvent $fsw Deleted -Action $handler | Out-Null
    Register-ObjectEvent $fsw Renamed -Action $handler | Out-Null

    $fsw.EnableRaisingEvents = $true
    return $fsw
}

$watchers = @()

# Source changes
$watchers += Start-Watcher -Path (Join-Path $root "src") -Filter "*.cpp" -IncludeSubdirectories $true
$watchers += Start-Watcher -Path (Join-Path $root "src") -Filter "*.h"   -IncludeSubdirectories $true
$watchers += Start-Watcher -Path (Join-Path $root "src") -Filter "*.hpp" -IncludeSubdirectories $true

# Build script changes
$watchers += Start-Watcher -Path $root -Filter "xmake.lua" -IncludeSubdirectories $false

Write-Host "[watch_compile_commands] watching... (Ctrl+C to stop)" -ForegroundColor Green

try {
    while ($true) {
        Wait-Event -Timeout 1 | Out-Null
    }
} finally {
    foreach ($w in $watchers) { $w.Dispose() }
    $timer.Stop(); $timer.Dispose()
    if ($periodicTimer) { $periodicTimer.Stop(); $periodicTimer.Dispose() }
    Get-EventSubscriber | Unregister-Event -Force -ErrorAction SilentlyContinue
}
