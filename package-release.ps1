$ErrorActionPreference = 'Stop'

$project = $PSScriptRoot
$workRoot = Split-Path -Parent $project
$releaseRoot = Split-Path -Parent $workRoot
$out = Join-Path $releaseRoot 'outputs'
$version = '0.1.29'
$releaseLabel = 'ScreenshotLightingAssistant-0.1.29'

$installStage = Join-Path $out ($releaseLabel + '-MO2')
$sourceStage = Join-Path $out ($releaseLabel + '-source')
$symbolsStage = Join-Path $out ($releaseLabel + '-symbols')
$installZip = $installStage + '.zip'
$sourceZip = $sourceStage + '.zip'
$symbolsZip = $symbolsStage + '.zip'

foreach ($path in @($installStage, $sourceStage, $symbolsStage, $installZip, $sourceZip, $symbolsZip)) {
    if (Test-Path -LiteralPath $path) {
        throw ('Refusing to overwrite existing release: ' + $path)
    }
}

$oldArchive = Join-Path $out 'ScreenshotLightingAssistant-0.1.28-MO2-test.zip'
$oldHash = '553FC2E3BAA58D92268EB3666D622E20E82C7EC4A1BC5305303F6DB5EEEB56D3'
if ((Get-FileHash -LiteralPath $oldArchive).Hash -ne $oldHash) {
    throw 'Previous release hash mismatch'
}

$dll = Join-Path $project ('build/artifacts/' + $version + '/ScreenshotLightingAssistant.dll')
$pdb = Join-Path $project ('build/artifacts/' + $version + '/ScreenshotLightingAssistant.pdb')
if ((Get-Item -LiteralPath $dll).VersionInfo.FileVersion -ne '0.1.29.0') {
    throw 'Wrong DLL version'
}

# The installer root is Data-shaped. PDB and source files are deliberately excluded.
$installFiles = [ordered]@{
    'SKSE/Plugins/ScreenshotLightingAssistant.dll' = $dll
    'docs/ScreenshotLightingAssistant/README_JA.md' = (Join-Path $project 'README_JA.md')
    'docs/ScreenshotLightingAssistant/README_EN.md' = (Join-Path $project 'README_EN.md')
    'docs/ScreenshotLightingAssistant/RELEASE_CHECKLIST_JA.md' = (Join-Path $project 'RELEASE_CHECKLIST_JA.md')
    'docs/ScreenshotLightingAssistant/DEPENDENCIES.md' = (Join-Path $project 'DEPENDENCIES.md')
    'docs/ScreenshotLightingAssistant/LICENSE' = (Join-Path $project 'LICENSE')
    'docs/ScreenshotLightingAssistant/THIRD_PARTY_NOTICES.md' = (Join-Path $project 'THIRD_PARTY_NOTICES.md')
    'docs/ScreenshotLightingAssistant/CommonLibSSE-NG-COPYING' = (Join-Path $project 'LICENSE')
    'docs/ScreenshotLightingAssistant/CommonLibSSE-NG-EXCEPTIONS.md' = (Join-Path $project 'third_party/CommonLibSSE-NG/EXCEPTIONS.md')
    'docs/ScreenshotLightingAssistant/CommonLibSSE-NG-MIT-LICENSE' = (Join-Path $project 'third_party/CommonLibSSE-NG/MIT-LICENSE')
    'docs/ScreenshotLightingAssistant/SKSEMenuFramework-LICENSE' = (Join-Path $project 'third_party/SKSEMenuFramework/LICENSE')
}

$symbolsFiles = [ordered]@{
    'symbols/ScreenshotLightingAssistant.pdb' = $pdb
    'SYMBOLS_README.md' = (Join-Path $project 'SYMBOLS_README.md')
}

$sourceFiles = [ordered]@{}
foreach ($directory in @('src', 'tests', 'third_party')) {
    foreach ($file in Get-ChildItem -LiteralPath (Join-Path $project $directory) -File -Recurse) {
        $relative = [IO.Path]::GetRelativePath($project, $file.FullName).Replace('\', '/')
        $sourceFiles[$relative] = $file.FullName
    }
}
foreach ($file in @(
    '.gitignore',
    'xmake.lua',
    'README.md',
    'README_EN.md',
    'README_JA.md',
    'README_TEST.md',
    'RELEASE_CHECKLIST_JA.md',
    'ENGINE_NOTES.md',
    'DEPENDENCIES.md',
    'SYMBOLS_README.md',
    'LICENSE',
    'THIRD_PARTY_NOTICES.md',
    'package-release.ps1'
)) {
    $sourceFiles[$file] = Join-Path $project $file
}
if ($sourceFiles.Count -ne 38) {
    throw ('Unexpected source file set; review before packaging. Count=' + $sourceFiles.Count)
}

Add-Type -AssemblyName System.IO.Compression.FileSystem

function New-VerifiedArchive($stage, $archive, $files, [string]$kind) {
    foreach ($relative in $files.Keys) {
        $destination = Join-Path $stage $relative
        $null = New-Item -ItemType Directory -Path (Split-Path -Parent $destination) -Force
        Copy-Item -LiteralPath $files[$relative] -Destination $destination
    }

    [IO.Compression.ZipFile]::CreateFromDirectory($stage, $archive)
    $zip = [IO.Compression.ZipFile]::OpenRead($archive)
    try {
        $entries = @($zip.Entries | Where-Object { $_.Name })
        if ($entries.Count -ne $files.Count) {
            throw ('Archive manifest count mismatch: ' + $kind)
        }

        $seen = @{}
        foreach ($entry in $entries) {
            $relative = $entry.FullName.Replace('\', '/')
            if (-not $files.Contains($relative) -or $seen.ContainsKey($relative)) {
                throw ('Unexpected/duplicate ZIP entry: ' + $relative)
            }
            $seen[$relative] = $true
            if ($relative.StartsWith('/') -or $relative.Contains('../') -or $relative.Contains(':')) {
                throw ('Unsafe ZIP path: ' + $relative)
            }
            if ($kind -eq 'installer' -and $relative -notmatch '^(SKSE/Plugins/|docs/ScreenshotLightingAssistant/)') {
                throw ('Invalid MO2 root: ' + $relative)
            }

            $stream = $entry.Open()
            $sha = [Security.Cryptography.SHA256]::Create()
            try {
                $hash = [BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-', '')
            } finally {
                $sha.Dispose()
                $stream.Dispose()
            }
            if ($hash -ne (Get-FileHash -LiteralPath $files[$relative]).Hash) {
                throw ('ZIP/source hash mismatch: ' + $relative)
            }
        }

        if ($kind -eq 'installer') {
            if (-not $seen.ContainsKey('SKSE/Plugins/ScreenshotLightingAssistant.dll')) {
                throw 'DLL not directly under SKSE/Plugins'
            }
            if (@($seen.Keys | Where-Object { $_ -match '\.pdb$' }).Count -ne 0) {
                throw 'PDB leaked into MO2 archive'
            }
        }

        [ordered]@{
            archive = $archive
            bytes = (Get-Item -LiteralPath $archive).Length
            sha256 = (Get-FileHash -LiteralPath $archive).Hash
            verifiedFiles = $entries.Count
            paths = @($seen.Keys | Sort-Object)
        }
    } finally {
        $zip.Dispose()
    }
}

$installer = New-VerifiedArchive $installStage $installZip $installFiles 'installer'
$source = New-VerifiedArchive $sourceStage $sourceZip $sourceFiles 'source'
$symbols = New-VerifiedArchive $symbolsStage $symbolsZip $symbolsFiles 'symbols'

if ((Get-FileHash -LiteralPath $oldArchive).Hash -ne $oldHash) {
    throw 'Previous release was changed'
}

[ordered]@{
    installer = $installer
    source = $source
    symbols = $symbols
    dllVersion = (Get-Item -LiteralPath $dll).VersionInfo.FileVersion
    dllSha256 = (Get-FileHash -LiteralPath $dll).Hash
    previousReleaseUnchanged = $oldHash
} | ConvertTo-Json -Depth 5
