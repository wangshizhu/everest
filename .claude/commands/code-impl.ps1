<#
.SYNOPSIS
    code-impl mechanical checks.
.DESCRIPTION
    Reports violations only; never modifies files.
    Default thresholds come from doc/standard/cpp_style_guide.md and code_standard.md.
    code-impl calls this script without overriding them; the parameters remain optional overrides.
.NOTES
    Default thresholds (code_standard.md style 1/2/4/7):
      line width 180, parameters 3, nesting 3, function lines 100.
    Covered:
      - file: UTF-8 without BOM, LF, no tabs, no #pragma once, header guard, file name
      - line: line width, trailing whitespace, keyword spacing, ")const", line-end comments,
              NULL, using namespace, C++20 modules and <format>
      - structure: parameter count, nesting depth, function length (heuristic)
      - optional: clang-format --dry-run --Werror
    Items that cannot be judged mechanically must be checked manually against the standards.
#>
param(
    [Parameter(Mandatory = $true)][string[]]$Files,
    [string]$RepoRoot = (Get-Location).Path,
    [int]$MaxLineWidth = 180,
    [int]$MaxParams = 3,
    [int]$MaxNesting = 3,
    [int]$MaxFunctionLines = 100,
    [switch]$AllLines,
    [switch]$UseClangFormat
)

$violations = New-Object System.Collections.Generic.List[string]
$notices = New-Object System.Collections.Generic.List[string]

function Add-Violation {
    param(
        [string]$Clause,
        [string]$Path,
        [int]$Line,
        [string]$Message,
        [switch]$Heuristic
    )

    $tail = ''
    if ($Heuristic) {
        $tail = ' (heuristic, verify manually)'
    }

    $violations.Add(('[{0}] {1}:{2}: {3}{4}' -f $Clause, $Path, $Line, $Message, $tail))
}

function Add-Notice {
    param([string]$Message)
    $notices.Add($Message)
}

function Find-ClangFormat {
    $cmd = Get-Command clang-format -ErrorAction SilentlyContinue
    if ($cmd) {
        return $cmd.Source
    }

    foreach ($edition in @('Community', 'Professional', 'Enterprise', 'BuildTools')) {
        foreach ($arch in @('x64', '')) {
            $candidate = Join-Path $env:ProgramFiles ('Microsoft Visual Studio\2022\' + $edition + '\VC\Tools\Llvm\' + $arch + '\bin\clang-format.exe')
            if (Test-Path -LiteralPath $candidate) {
                return $candidate
            }
            $candidate = Join-Path ${env:ProgramFiles(x86)} ('Microsoft Visual Studio\2022\' + $edition + '\VC\Tools\Llvm\' + $arch + '\bin\clang-format.exe')
            if (Test-Path -LiteralPath $candidate) {
                return $candidate
            }
        }
    }

    $llvm = Join-Path $env:ProgramFiles 'LLVM\bin\clang-format.exe'
    if (Test-Path -LiteralPath $llvm) {
        return $llvm
    }

    return $null
}

function ConvertFrom-UnifiedDiff {
    param([object[]]$Lines)

    $set = New-Object 'System.Collections.Generic.HashSet[int]'
    foreach ($line in $Lines) {
        $text = [string]$line
        if ($text -match '^@@ -\d+(?:,\d+)? \+(\d+)(?:,(\d+))? @@') {
            $start = [int]$matches[1]
            $count = 1
            if ($matches[2] -ne '') {
                $count = [int]$matches[2]
            }

            for ($i = 0; $i -lt $count; $i++) {
                [void]$set.Add($start + $i)
            }
        }
    }

    return $set
}

function Get-ChangedLineSet {
    param([string]$Path)

    if ($AllLines) {
        return $null
    }

    $git = Get-Command git -ErrorAction SilentlyContinue
    if ($git) {
        $diff = & $git.Source -C $RepoRoot diff -U0 -- $Path 2>$null
        if ($LASTEXITCODE -eq 0 -and $diff) {
            return (ConvertFrom-UnifiedDiff -Lines $diff)
        }
    }

    return $null
}

function Get-CodeText {
    param([string]$Text)

    $result = $Text -replace '"[^"]*"', '""'
    $result = $result -replace "'[^']*'", "''"
    $result = $result -replace '//.*$', ''

    return $result
}

function Get-BraceDelta {
    param([string]$Text)

    $open = ([regex]::Matches($Text, '\{')).Count
    $close = ([regex]::Matches($Text, '\}')).Count

    return ($open - $close)
}

function Test-FunctionStart {
    param([string]$Line)

    if ($Line -match '^\s*#') { return $false }
    if ($Line -match '^\s*(if|for|while|switch|catch|return|sizeof|alignof|decltype|static_assert|assert)\b') { return $false }
    if ($Line -match '^\s*[A-Z_][A-Z0-9_]*\s*\(') { return $false }
    if ($Line -match ';\s*$') { return $false }

    if ($Line -match '^\s*(?:template\s*<.*>\s*)?[\w:~<>*&\s]+?\s+\w+\s*\(') { return $true }
    if ($Line -match '^\s*(?:explicit\s+)?~?[\w:]+\s*\(') { return $true }

    return $false
}

function Get-SignatureText {
    param([string[]]$Lines, [int]$Start)

    $builder = New-Object System.Text.StringBuilder
    $depth = 0
    $seenParen = $false

    for ($i = $Start; $i -lt [Math]::Min($Lines.Count, $Start + 20); $i++) {
        $text = Get-CodeText -Text $Lines[$i]
        [void]$builder.Append(' ').Append($text)

        foreach ($ch in $text.ToCharArray()) {
            if ($ch -eq '(') {
                $depth++
                $seenParen = $true
            }
            elseif ($ch -eq ')') {
                $depth--
            }
        }

        if ($seenParen -and $depth -le 0) {
            break
        }
    }

    return $builder.ToString()
}

function Get-ParameterCount {
    param([string]$Signature)

    $start = $Signature.IndexOf('(')
    if ($start -lt 0) { return -1 }

    $depthParen = 0
    $depthAngle = 0
    $depthBrace = 0
    $hasContent = $false
    $commas = 0

    for ($i = $start; $i -lt $Signature.Length; $i++) {
        $c = $Signature[$i]

        if ($c -eq '(') {
            $depthParen++
        }
        elseif ($c -eq ')') {
            $depthParen--
            if ($depthParen -eq 0) { break }
        }
        elseif ($c -eq '<') {
            $depthAngle++
        }
        elseif ($c -eq '>') {
            if ($depthAngle -gt 0) { $depthAngle-- }
        }
        elseif ($c -eq '{') {
            $depthBrace++
        }
        elseif ($c -eq '}') {
            if ($depthBrace -gt 0) { $depthBrace-- }
        }
        elseif ($depthParen -eq 1) {
            if (($c -eq ',') -and ($depthAngle -eq 0) -and ($depthBrace -eq 0)) {
                $commas++
            }
            if (-not [char]::IsWhiteSpace($c)) {
                $hasContent = $true
            }
        }
    }

    if (-not $hasContent) { return 0 }

    return ($commas + 1)
}

function Analyze-Functions {
    param(
        [string[]]$Lines,
        $Changed,
        [string]$Display
    )

    $i = 0
    while ($i -lt $Lines.Count) {
        $isStart = Test-FunctionStart -Line $Lines[$i]
        $startInScope = ($null -eq $Changed) -or $Changed.Contains($i + 1)

        if ($isStart -and $startInScope) {
            $signature = Get-SignatureText -Lines $Lines -Start $i
            $paramCount = Get-ParameterCount -Signature $signature
            if (($MaxParams -gt 0) -and ($paramCount -gt $MaxParams)) {
                Add-Violation 'code_standard 1' $Display ($i + 1) ('function has {0} parameters, limit is {1}' -f $paramCount, $MaxParams)
            }
        }

        if ($isStart) {
            $braceLine = -1
            for ($j = $i; $j -lt [Math]::Min($Lines.Count, $i + 5); $j++) {
                $text = Get-CodeText -Text $Lines[$j]
                if ($text -match '\{') {
                    $braceLine = $j
                    break
                }
                if ($Lines[$j] -match ';\s*$') {
                    break
                }
            }

            if ($braceLine -ge 0) {
                $depth = 0
                $endLine = -1

                for ($k = $braceLine; $k -lt $Lines.Count; $k++) {
                    $text = Get-CodeText -Text $Lines[$k]
                    $depth += (Get-BraceDelta -Text $text)

                    $lineInScope = ($null -eq $Changed) -or $Changed.Contains($k + 1)
                    if (($k -gt $braceLine) -and ($lineInScope) -and (($depth - 1) -gt $MaxNesting)) {
                        Add-Violation 'code_standard 2' $Display ($k + 1) ('nesting depth {0} exceeds {1}' -f ($depth - 1), $MaxNesting) -Heuristic
                    }

                    if (($depth -le 0) -and ($k -ge $braceLine)) {
                        $endLine = $k
                        break
                    }
                }

                if ($endLine -ge 0) {
                    $length = $endLine - $i + 1
                    if (($MaxFunctionLines -gt 0) -and ($length -gt $MaxFunctionLines) -and $startInScope) {
                        Add-Violation 'code_standard 4' $Display ($i + 1) ('function length {0} lines exceeds {1}' -f $length, $MaxFunctionLines) -Heuristic
                    }

                    $i = $endLine
                }
            }
        }

        $i++
    }
}

Write-Output '== code-impl mechanical checks =='

foreach ($file in $Files) {
    $path = $file
    if (-not [System.IO.Path]::IsPathRooted($path)) {
        $path = [System.IO.Path]::GetFullPath((Join-Path (Get-Location).Path $file))
    }

    if (-not (Test-Path -LiteralPath $path)) {
        Add-Notice ('file not found, skipped: {0}' -f $file)
        continue
    }

    $display = $path
    $relative = Resolve-Path -LiteralPath $path -Relative
    if (($null -ne $relative) -and ($relative.ToString() -notmatch ':')) {
        $display = $relative.ToString()
    }

    $extension = [System.IO.Path]::GetExtension($path).ToLowerInvariant()
    if (($extension -ne '.h') -and ($extension -ne '.hpp') -and ($extension -ne '.cpp') -and ($extension -ne '.cc') -and ($extension -ne '.cxx')) {
        Add-Notice ('{0}: not a C++ source file, skipped' -f $display)
        continue
    }

    $fileName = [System.IO.Path]::GetFileName($path)
    if ($fileName -notmatch '^[a-z0-9_]+\.(h|hpp|cpp|cc|cxx)$') {
        Add-Violation 'cpp_style_guide naming' $display 1 ('file name {0} is not lower_snake_case' -f $fileName)
    }

    $bytes = [System.IO.File]::ReadAllBytes($path)

    $hasBom = $false
    if (($bytes.Length -ge 3) -and ($bytes[0] -eq 0xEF) -and ($bytes[1] -eq 0xBB) -and ($bytes[2] -eq 0xBF)) {
        $hasBom = $true
    }

    $strictUtf8 = New-Object System.Text.UTF8Encoding($false, $true)
    try {
        $text = $strictUtf8.GetString($bytes)
    }
    catch {
        Add-Violation 'code_standard 11' $display 1 'file is not valid UTF-8'
        $text = [System.Text.Encoding]::UTF8.GetString($bytes)
    }

    if ($hasBom) {
        Add-Violation 'code_standard 11' $display 1 'file has UTF-8 BOM, remove it'
        if ($text.Length -gt 0) {
            $text = $text.Substring(1)
        }
    }

    if ($text -match "`r") {
        Add-Violation 'code_standard 11' $display 1 'file contains CR; rewritten files must use LF'
    }

    $lines = $text -split "`r`n|`r|`n"
    $changed = Get-ChangedLineSet -Path $path
    if ($null -eq $changed) {
        Add-Notice ('{0}: no version-control diff, scanning the whole file' -f $display)
    }

    # ===== file-level checks =====
    $hasPragmaOnce = $false
    $guardLine = -1
    $guardName = ''
    $hasDefine = $false

    for ($i = 0; $i -lt $lines.Count; $i++) {
        $line = $lines[$i]

        if ($line -match '^\s*#\s*pragma\s+once\b') {
            $hasPragmaOnce = $true
        }

        if (($guardLine -lt 0) -and ($i -lt 40) -and ($line -match '^\s*#\s*ifndef\s+(\w+)')) {
            $guardLine = $i
            $guardName = $matches[1]
        }

        if (($guardName -ne '') -and ($line -match ('^\s*#\s*define\s+' + [regex]::Escape($guardName) + '\b'))) {
            $hasDefine = $true
        }
    }

    if ($hasPragmaOnce) {
        Add-Violation 'cpp_style_guide header' $display 1 'do not use #pragma once in rewritten files'
    }

    if (($extension -eq '.h') -or ($extension -eq '.hpp')) {
        $baseName = [System.IO.Path]::GetFileNameWithoutExtension($path).ToUpperInvariant()
        $baseName = $baseName -replace '[^A-Z0-9]', '_'
        $expectedGuard = $baseName + '_H_'

        if ($guardLine -lt 0) {
            Add-Violation 'cpp_style_guide header' $display 1 ('missing #ifndef guard, expected {0}' -f $expectedGuard)
        }
        elseif ($guardName -ne $expectedGuard) {
            Add-Violation 'cpp_style_guide header' $display ($guardLine + 1) ('guard {0} does not match expected {1}' -f $guardName, $expectedGuard)
        }
        elseif (-not $hasDefine) {
            Add-Violation 'cpp_style_guide header' $display ($guardLine + 1) ('missing #define {0}' -f $expectedGuard)
        }
    }

    # ===== line-level checks =====
    for ($i = 0; $i -lt $lines.Count; $i++) {
        $line = $lines[$i]
        $lineNo = $i + 1
        $inScope = ($null -eq $changed) -or $changed.Contains($lineNo)

        if (-not $inScope) {
            continue
        }

        if ($line -match '^[ ]*\t') {
            Add-Violation 'cpp_style_guide format' $display $lineNo 'leading tab; use 4 spaces'
        }

        if (($MaxLineWidth -gt 0) -and ($line.Length -gt $MaxLineWidth)) {
            Add-Violation 'code_standard 7' $display $lineNo ('line has {0} characters, limit is {1}' -f $line.Length, $MaxLineWidth)
        }

        if (($line -ne '') -and ($line -match '\s+$')) {
            Add-Violation 'cpp_style_guide format' $display $lineNo 'trailing whitespace'
        }

        $isCommentLine = ($line -match '^\s*//') -or ($line -match '^\s*\*')

        if ($line -match '\busing\s+namespace\b') {
            Add-Violation 'cpp_style_guide scope' $display $lineNo 'do not use using namespace'
        }

        if ($line -match '\bstd\s*::\s*auto_ptr\b') {
            Add-Violation 'cpp_style_guide ownership' $display $lineNo 'std::auto_ptr is forbidden'
        }

        if (($line -match '^\s*#\s*include\s*[<"]format[>"]') -or ($line -match '\bstd\s*::\s*format\b')) {
            Add-Violation 'cpp_style_guide toolchain' $display $lineNo 'do not use <format>; use fmt'
        }

        if ($line -match '^\s*(export\s+module|import\s+[\w:])') {
            Add-Violation 'cpp_style_guide toolchain' $display $lineNo 'do not use C++20 modules'
            $isCommentLine = $true
        }

        if ((-not $isCommentLine) -and ($line -match '\bNULL\b')) {
            Add-Violation 'cpp_style_guide null' $display $lineNo 'use nullptr instead of NULL' -Heuristic
        }

        if ((-not $isCommentLine) -and ($line -match '\b(if|for|while|switch)\(')) {
            Add-Violation 'cpp_style_guide format' $display $lineNo 'add a space between keyword and "("' -Heuristic
        }

        if ((-not $isCommentLine) -and ($line -match '\)const\b')) {
            Add-Violation 'cpp_style_guide format' $display $lineNo 'write ") const" with a space' -Heuristic
        }

        if ((-not $isCommentLine) -and ($line -match '\bvirtual\b[^;{]*\boverride\b')) {
            Add-Violation 'cpp_style_guide class' $display $lineNo 'do not repeat virtual on an override' -Heuristic
        }

        if ((-not $isCommentLine) -and ($line -match '\S\s+//') -and ($line -notmatch '://')) {
            Add-Violation 'cpp_style_guide comments' $display $lineNo 'comment should be above the code, not at line end' -Heuristic
        }

        if ($line -cmatch '\bNONCOPYABLE\b|\bONLY_MOVE_COPYABLE\b') {
            Add-Violation 'code_standard 3' $display $lineNo 'use NonCopyable or = delete instead of a macro'
        }

        if ($line -match '^\s*#\s*include\s*[<"]\.\.?/') {
            Add-Violation 'cpp_style_guide header' $display $lineNo 'include path must be relative to the source root, not "." or ".."'
        }

        if ($line -match '^\s*#\s*pragma\s+execution_character_set') {
            Add-Violation 'cpp_style_guide toolchain' $display $lineNo 'do not use #pragma execution_character_set; use /utf-8'
        }
    }

    Analyze-Functions -Lines $lines -Changed $changed -Display $display

    if ($UseClangFormat) {
        $clangFormat = Find-ClangFormat
        if (-not $clangFormat) {
            Add-Notice ('{0}: clang-format not found, formatting check skipped' -f $display)
        }
        else {
            $formatOutput = & $clangFormat --dry-run --Werror --style=file $path 2>&1
            if ($LASTEXITCODE -ne 0) {
                $first = ($formatOutput | Select-Object -First 1)
                Add-Violation 'cpp_style_guide format' $display 1 ('clang-format check failed: {0}' -f $first)
            }
        }
    }
}

if ($notices.Count -gt 0) {
    Write-Output '-- notices --'
    foreach ($notice in $notices) {
        Write-Output (' - {0}' -f $notice)
    }
}

if ($violations.Count -eq 0) {
    Write-Output 'No mechanically decidable violations found.'
}
else {
    Write-Output '-- violations --'
    foreach ($violation in $violations) {
        Write-Output (' - {0}' -f $violation)
    }
}

Write-Output 'Items that cannot be judged mechanically must be checked manually against the standards.'
