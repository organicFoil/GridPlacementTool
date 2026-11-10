<#
.SYNOPSIS
    Запускает автоматизационные тесты плагина GridPlacementTool.

.DESCRIPTION
    При необходимости собирает editor-таргет проекта, поднимает редактор headless
    с указанным фильтром тестов и печатает сводку из JSON-отчёта.

    Коды возврата:
        0 — все тесты прошли;
        1 — есть упавшие тесты;
        2 — прогон не состоялся (сборка не удалась или отчёт не появился).

.EXAMPLE
    .\RunTests.ps1
    Запустить все тесты плагина.

.EXAMPLE
    .\RunTests.ps1 GridPlacementTool.Menu
    Запустить только тесты editor-слоя.

.EXAMPLE
    .\RunTests.ps1 GridPlacementTool.Layout -Build
    Собрать проект и прогнать тесты раскладки.
#>
[CmdletBinding()]
param(
    # Префикс имени теста: запускается всё, что с него начинается.
    [Parameter(Position = 0)]
    [string]$Filter = 'GridPlacementTool',

    # Путь к .uproject. По умолчанию ищется вверх по дереву от каталога скрипта.
    [string]$Project,

    # Каталог установки движка. По умолчанию берётся из UE_ROOT, затем из EngineAssociation проекта.
    [string]$UERoot,

    # Куда сложить отчёт. По умолчанию во временный каталог.
    [string]$ReportDir,

    # Собрать editor-таргет перед прогоном.
    [switch]$Build
)

$ErrorActionPreference = 'Stop'

function Resolve-ProjectPath {
    param([string]$StartDirectory)

    $directory = Get-Item -LiteralPath $StartDirectory
    while ($directory) {
        $candidates = @(Get-ChildItem -LiteralPath $directory.FullName -Filter '*.uproject' -File -ErrorAction SilentlyContinue)
        if ($candidates.Count -eq 1) {
            return $candidates[0].FullName
        }
        if ($candidates.Count -gt 1) {
            throw "В каталоге $($directory.FullName) несколько .uproject — укажите -Project явно."
        }
        $directory = $directory.Parent
    }

    throw "Не удалось найти .uproject выше $StartDirectory — укажите -Project явно."
}

# --- Проект ---
$scriptDirectory = if ($PSScriptRoot) { $PSScriptRoot } else { (Get-Location).Path }

if (-not $Project) {
    $Project = Resolve-ProjectPath -StartDirectory $scriptDirectory
}
if (-not (Test-Path -LiteralPath $Project)) {
    throw "Не найден файл проекта: $Project"
}
$Project = (Get-Item -LiteralPath $Project).FullName

# --- Движок ---
if (-not $UERoot) {
    $UERoot = $env:UE_ROOT
}

if (-not $UERoot) {
    $association = (Get-Content -LiteralPath $Project -Raw | ConvertFrom-Json).EngineAssociation
    if ($association -match '^\d+\.\d+$') {
        $candidate = Join-Path $env:ProgramFiles "Epic Games\UE_$association"
        if (Test-Path -LiteralPath $candidate) {
            $UERoot = $candidate
        }
    }
}

if (-not $UERoot) {
    throw 'Не удалось определить каталог движка — укажите -UERoot или переменную окружения UE_ROOT.'
}

$EditorCmd = Join-Path $UERoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
if (-not (Test-Path -LiteralPath $EditorCmd)) {
    throw "Не найден исполняемый файл редактора: $EditorCmd"
}

# --- Отчёт ---
if (-not $ReportDir) {
    $ReportDir = Join-Path $env:TEMP 'grid_placement_tests'
}
$ReportDir = [IO.Path]::GetFullPath($ReportDir)
Remove-Item -LiteralPath $ReportDir -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Path $ReportDir -Force | Out-Null

# --- Сборка ---
if ($Build) {
    $TargetName = [IO.Path]::GetFileNameWithoutExtension($Project) + 'Editor'
    $BuildBat = Join-Path $UERoot 'Engine\Build\BatchFiles\Build.bat'

    Write-Host "Сборка $TargetName..."
    & $BuildBat $TargetName Win64 Development "-Project=$Project" -WaitMutex -FromMsBuild
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Сборка завершилась с кодом $LASTEXITCODE — прогон не состоялся." -ForegroundColor Red
        exit 2
    }
}

# --- Прогон ---
$consoleLog = Join-Path $ReportDir 'console.log'
$editorLog = Join-Path $ReportDir 'editor.log'

$editorArgs = @(
    $Project
    "-ExecCmds=Automation RunTests $Filter"
    '-unattended'                             # без диалогов, в том числе окна прогресса спавна ассетов
    '-nopause'
    '-nosplash'
    '-nullrhi'                                # тесты ничего не рендерят
    '-log'
    '-testexit=Automation Test Queue Empty'   # закрыть редактор, когда очередь тестов опустеет
    "-ReportExportPath=$ReportDir"
    "-abslog=$editorLog"                      # полный лог редактора: в Saved\Logs его потом не найти
)

$printedArgs = ($editorArgs | ForEach-Object { if ($_ -match '\s') { '"' + $_ + '"' } else { $_ } }) -join ' '
Write-Host "Фильтр:     $Filter"
Write-Host "Проект:     $Project"
Write-Host "Команда:    `"$EditorCmd`" $printedArgs"
Write-Host 'Идёт прогон — основное время занимает старт редактора...'

$stopwatch = [Diagnostics.Stopwatch]::StartNew()
$previousErrorAction = $ErrorActionPreference
$processExitCode = $null

try {
    # Вывод редактора идёт в файл: на консоль он всё равно не помещается, а при
    # неудачном старте именно он объясняет причину. stderr не должен ронять скрипт.
    $ErrorActionPreference = 'Continue'
    & $EditorCmd @editorArgs *>&1 | Out-File -LiteralPath $consoleLog -Encoding utf8
    $processExitCode = $LASTEXITCODE
}
finally {
    $ErrorActionPreference = $previousErrorAction
    $stopwatch.Stop()
}

# --- Результат ---
$reportFile = Join-Path $ReportDir 'index.json'
if (-not (Test-Path -LiteralPath $reportFile)) {
    $exitDescription = if ($null -eq $processExitCode) { 'редактор не запустился' } else { "код процесса $processExitCode" }
    Write-Host ''
    Write-Host "Отчёт не появился — прогон не состоялся ($exitDescription)." -ForegroundColor Red

    # Редактор пишет причину в свой лог; консольный вывод остаётся запасным вариантом.
    $diagnosticLog = if (Test-Path -LiteralPath $editorLog) { $editorLog } else { $consoleLog }
    $logLines = @()
    if (Test-Path -LiteralPath $diagnosticLog) {
        $logLines = @(Get-Content -LiteralPath $diagnosticLog -ErrorAction SilentlyContinue)
    }

    # Хвост лога — почти всегда шум завершения, поэтому сначала показываем найденные ошибки.
    $errorLines = @($logLines | Where-Object { $_ -match '\[error\]' -or $_ -match '\b(Error|Fatal error):' } | Select-Object -Last 10)
    if ($errorLines.Count -gt 0) {
        Write-Host 'Ошибки из лога:'
        $errorLines | ForEach-Object { Write-Host "    $_" -ForegroundColor Red }
        Write-Host 'Последние строки лога:'
    }
    if ($logLines.Count -gt 0) {
        $logLines | Select-Object -Last 15 | Write-Host
    }

    # Самая частая причина: плагин не собран, и редактор выходит сразу после старта.
    $logText = $logLines -join "`n"
    if ($logText -match "failed to load because module '([^']+)' could not be found") {
        Write-Host ''
        Write-Host "Модуль '$($Matches[1])' не найден: плагин не собран под этот движок." -ForegroundColor Yellow
        Write-Host 'Соберите проект и повторите: .\RunTests.ps1 -Build' -ForegroundColor Yellow
    }
    elseif ($logText -match 'missing or out-of-date modules') {
        Write-Host ''
        Write-Host 'Модули проекта отсутствуют или устарели: их нужно пересобрать.' -ForegroundColor Yellow
        Write-Host 'Пересоберите проект и повторите: .\RunTests.ps1 -Build' -ForegroundColor Yellow
    }

    Write-Host "Лог:        $diagnosticLog"
    exit 2
}

$report = Get-Content -LiteralPath $reportFile -Raw | ConvertFrom-Json

Write-Host ''
Write-Host ('Прогон {0:N1} с.  Тестов: {1}  Успешно: {2}  Упало: {3}  С предупреждениями: {4}' -f `
        $stopwatch.Elapsed.TotalSeconds, @($report.tests).Count, $report.succeeded, $report.failed, $report.succeededWithWarnings)

@($report.tests) |
    Select-Object @{ n = 'Тест'; e = { $_.testDisplayName } },
        @{ n = 'Статус'; e = { $_.state } },
        @{ n = 'Ошибки'; e = { $_.errors } },
        @{ n = 'Предупр.'; e = { $_.warnings } },
        @{ n = 'Сек.'; e = { [Math]::Round($_.duration, 3) } } |
    Format-Table -AutoSize |
    Out-String |
    Write-Host

foreach ($test in @($report.tests) | Where-Object { $_.state -ne 'Success' }) {
    Write-Host ('--- {0} [{1}]' -f $test.fullTestPath, $test.state) -ForegroundColor Red
    foreach ($entry in @($test.entries)) {
        $firstLine = ($entry.event.message -split "\r?\n" | Where-Object { $_ } | Select-Object -First 1)
        Write-Host ('    [{0}] {1}' -f $entry.event.type, $firstLine)
    }
}

Write-Host "Отчёт:      $reportFile"
Write-Host "Лог:        $editorLog"
Write-Host "Вывод:      $consoleLog"

if ($report.failed -gt 0) {
    exit 1
}

Write-Host ''
Write-Host 'Все тесты прошли.' -ForegroundColor Green
exit 0
