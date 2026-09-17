param([ValidatePattern('^\d+\.\d+\.\d+$')][string]$Version='1.1.0')
$ErrorActionPreference = 'Stop'
Set-Location -LiteralPath $PSScriptRoot
$executables = @('Luma.exe','Luma-x86.exe')
foreach ($name in $executables) {
    $file = Get-Item -LiteralPath "dist/$name"
    if ($file.Length -ge 50000000) { throw "$name exceeds the 50 MB limit" }
}
$hashes = foreach ($name in $executables) {
    $hash = (Get-FileHash -LiteralPath "dist/$name" -Algorithm SHA256).Hash.ToLowerInvariant()
    "$hash  $name"
}
$utf8 = [System.Text.UTF8Encoding]::new($false)
[System.IO.File]::WriteAllText((Join-Path $PSScriptRoot 'dist/SHA256SUMS.txt'),($hashes -join "`n")+"`n",$utf8)
Copy-Item -LiteralPath README.md,LICENSE -Destination dist -Force
$zip = "dist/Luma-$Version-Windows.zip"
Compress-Archive -LiteralPath dist/Luma.exe,dist/Luma-x86.exe,dist/README.md,dist/LICENSE,dist/SHA256SUMS.txt -DestinationPath $zip -Force
if ((Get-Item -LiteralPath $zip).Length -ge 50000000) { throw 'Release ZIP exceeds 50 MB' }
Get-Item -LiteralPath $zip | Select-Object Name,Length
