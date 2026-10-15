param([switch]$IncludeFFmpeg)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$toolsDir = Join-Path $projectRoot 'tools'
New-Item -ItemType Directory -Path $toolsDir -Force | Out-Null
$headers = @{ 'User-Agent' = 'mdownXTwitter-Setup'; 'Accept' = 'application/vnd.github+json' }
function Get-VerifiedAsset($Asset, [string]$Target, [string]$FallbackHash = '') {
    $expected = $FallbackHash
    if ($Asset.digest -match '^sha256:([a-fA-F0-9]{64})$') { $expected = $Matches[1] }
    if ($expected -notmatch '^[a-fA-F0-9]{64}$') { throw "No SHA256 digest available for $($Asset.name)" }
    $partial = "$Target.partial"
    Invoke-WebRequest -Uri $Asset.browser_download_url -Headers $headers -OutFile $partial
    $actual = (Get-FileHash -LiteralPath $partial -Algorithm SHA256).Hash
    if ($actual -ne $expected) { throw "Checksum mismatch: $($Asset.name)" }
    Move-Item -LiteralPath $partial -Destination $Target -Force
}
$release = Invoke-RestMethod -Uri 'https://api.github.com/repos/yt-dlp/yt-dlp/releases/latest' -Headers $headers
$asset = $release.assets | Where-Object name -EQ 'yt-dlp.exe' | Select-Object -First 1
if (!$asset) { throw 'yt-dlp.exe release asset not found' }
$hash = ''
if (!$asset.digest) {
    $sums = $release.assets | Where-Object name -EQ 'SHA2-256SUMS' | Select-Object -First 1
    $text = (Invoke-WebRequest -Uri $sums.browser_download_url -Headers $headers).Content
    if ($text -is [byte[]]) { $text = [Text.Encoding]::UTF8.GetString($text) }
    foreach ($line in ($text -split "`n")) { if ($line -match '^([a-fA-F0-9]{64})\s+\*?yt-dlp\.exe\s*$') { $hash = $Matches[1] } }
}
Get-VerifiedAsset $asset (Join-Path $toolsDir 'yt-dlp.exe') $hash
$release.tag_name | Set-Content -LiteralPath (Join-Path $toolsDir 'yt-dlp-version.txt')
if ($IncludeFFmpeg) {
    $ff = Invoke-RestMethod -Uri 'https://api.github.com/repos/yt-dlp/FFmpeg-Builds/releases/latest' -Headers $headers
    $ffAsset = $ff.assets | Where-Object name -EQ 'ffmpeg-master-latest-win64-gpl.zip' | Select-Object -First 1
    if (!$ffAsset) { throw 'FFmpeg Windows release asset not found' }
    $archive = Join-Path $toolsDir 'ffmpeg.zip'
    Get-VerifiedAsset $ffAsset $archive
    $unpack = Join-Path $toolsDir ('ffmpeg-unpack-' + [guid]::NewGuid().ToString('N'))
    Expand-Archive -LiteralPath $archive -DestinationPath $unpack
    foreach ($name in 'ffmpeg.exe', 'ffprobe.exe') {
        $binary = Get-ChildItem -LiteralPath $unpack -Recurse -File -Filter $name | Select-Object -First 1
        if (!$binary) { throw "Archive missing $name" }
        Copy-Item -LiteralPath $binary.FullName -Destination (Join-Path $toolsDir $name) -Force
    }
    # Retain source archive and upstream license materials for release preparation.
    $ff.tag_name | Set-Content -LiteralPath (Join-Path $toolsDir 'ffmpeg-version.txt')
}
Write-Host "Dependencies prepared in $toolsDir"
