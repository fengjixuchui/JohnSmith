param(
    [switch]$RequireDoxygen
)

$repoRoot = Split-Path -Parent $PSScriptRoot
$errors = [System.Collections.Generic.List[string]]::new()
$excluded = '\\(build|tasks|\.git|\.codegraph)\\'

Get-ChildItem -LiteralPath $repoRoot -Recurse -File -Filter '*.md' |
    Where-Object { $_.FullName -notmatch $excluded } |
    ForEach-Object {
        $document = $_
        $content = Get-Content -Raw -LiteralPath $document.FullName
        $matches = [regex]::Matches(
            $content,
            '\[[^\]]*\]\((?!https?://|mailto:|#)([^)#]+)(?:#[^)]*)?\)')

        foreach ($match in $matches) {
            $target = $match.Groups[1].Value
            $resolved = [System.IO.Path]::GetFullPath(
                (Join-Path $document.DirectoryName $target))
            if (-not (Test-Path -LiteralPath $resolved)) {
                $relative = [System.IO.Path]::GetRelativePath(
                    $repoRoot, $document.FullName)
                $errors.Add("$relative -> $target")
            }
        }
    }

if ($errors.Count -ne 0) {
    Write-Error ("Broken local documentation links:`n  " +
        ($errors -join "`n  "))
    exit 1
}

$doxygen = Get-Command 'doxygen' -ErrorAction SilentlyContinue
if ($null -eq $doxygen) {
    if ($RequireDoxygen) {
        Write-Error 'Doxygen is required but was not found on PATH.'
        exit 1
    }
    Write-Warning 'Doxygen was not found; local-link validation passed.'
    exit 0
}

& $doxygen.Source (Join-Path $repoRoot 'Doxyfile')
exit $LASTEXITCODE
