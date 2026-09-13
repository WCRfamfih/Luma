param([ValidateSet('x64','x86')][string]$Architecture='x64')
$ErrorActionPreference = 'Stop'
Set-Location -LiteralPath $PSScriptRoot
New-Item -ItemType Directory -Force -Path build,dist | Out-Null
Add-Type -AssemblyName System.Drawing
# The same white ring / mint arc as the tray icon, rasterized for each Windows
# icon size. PNG-backed ICO frames keep Explorer's large icons crisp and small.
$iconSizes = @(16,20,24,32,48,64,128,256)
$iconFrames = [System.Collections.Generic.List[byte[]]]::new()
foreach ($iconSize in $iconSizes) {
    $large = [System.Drawing.Bitmap]::new($iconSize*4,$iconSize*4)
    $graphics = [System.Drawing.Graphics]::FromImage($large)
    $graphics.SmoothingMode = 'AntiAlias'
    $graphics.Clear([System.Drawing.Color]::Transparent)
    $graphics.ScaleTransform($iconSize/16.0,$iconSize/16.0)
    $pen = [System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(244,250,255),4)
    $graphics.DrawEllipse($pen,8,8,48,48)
    $pen.Color = [System.Drawing.Color]::FromArgb(157,229,204)
    $pen.Width = 3
    $graphics.DrawArc($pen,14,12,38,40,220,200)
    $pen.Dispose()
    $graphics.Dispose()
    $bitmap = [System.Drawing.Bitmap]::new($iconSize,$iconSize)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    $graphics.InterpolationMode = 'HighQualityBicubic'
    $graphics.DrawImage($large,0,0,$iconSize,$iconSize)
    $graphics.Dispose()
    $large.Dispose()
    $png = [System.IO.MemoryStream]::new()
    $bitmap.Save($png,[System.Drawing.Imaging.ImageFormat]::Png)
    $iconFrames.Add($png.ToArray())
    if ($iconSize -eq 256) { $bitmap.Save((Join-Path $PSScriptRoot 'build/luma-icon-preview.png'),[System.Drawing.Imaging.ImageFormat]::Png) }
    $png.Dispose()
    $bitmap.Dispose()
}
$stream = [System.IO.File]::Create((Join-Path $PSScriptRoot 'build/luma.ico'))
$writer = [System.IO.BinaryWriter]::new($stream)
$writer.Write([uint16]0); $writer.Write([uint16]1); $writer.Write([uint16]$iconSizes.Count)
$iconOffset = 6 + 16*$iconSizes.Count
for ($i=0; $i -lt $iconSizes.Count; $i++) {
    $sizeByte = if ($iconSizes[$i] -eq 256) {0} else {$iconSizes[$i]}
    $writer.Write([byte]$sizeByte); $writer.Write([byte]$sizeByte)
    $writer.Write([byte]0); $writer.Write([byte]0)
    $writer.Write([uint16]1); $writer.Write([uint16]32)
    $writer.Write([uint32]$iconFrames[$i].Length); $writer.Write([uint32]$iconOffset)
    $iconOffset += $iconFrames[$i].Length
}
foreach ($frame in $iconFrames) { $writer.Write([byte[]]$frame) }
$writer.Dispose()
$vswhere = @('C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe','D:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe') | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
$install = if ($vswhere) { & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath }
if (-not $install) { $cl = (Get-Command cl.exe -ErrorAction Stop).Source; $install = $cl.Substring(0,$cl.IndexOf('\VC\Tools\')) }
$vcvars = Join-Path $install 'VC\Auxiliary\Build\vcvarsall.bat'
$target = if ($Architecture -eq 'x64') {'x64'} else {'x64_x86'}
$commands = @"
@echo off
call "$vcvars" $target >nul
if errorlevel 1 exit /b 1
cd /d "$PSScriptRoot\src"
rc /nologo /fo ..\build\resources.res resources.rc
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /O2 /GL /MT /EHsc /utf-8 /W4 /DUNICODE /D_UNICODE /D_WIN32_WINNT=0x0A00 main.cpp /Fo..\build\main-$Architecture.obj /Fe..\dist\Luma-$Architecture.exe ..\build\resources.res /link /LTCG /OPT:REF /OPT:ICF /SUBSYSTEM:WINDOWS /MANIFEST:EMBED /MANIFESTINPUT:app.manifest d3d11.lib dxgi.lib dcomp.lib d3dcompiler.lib ole32.lib shell32.lib user32.lib gdi32.lib ws2_32.lib uuid.lib psapi.lib bcrypt.lib windowscodecs.lib
exit /b %errorlevel%
"@
$batch = Join-Path $PSScriptRoot 'build/compile.cmd'
[System.IO.File]::WriteAllText($batch,$commands,[System.Text.Encoding]::Default)
& cmd.exe /d /c $batch
if ($LASTEXITCODE -ne 0) { throw "Build failed ($LASTEXITCODE)" }
$exe = Get-Item -LiteralPath "dist/Luma-$Architecture.exe"
if ($exe.Length -gt 50MB) { throw 'Release exceeds 50MB' }
Write-Output "Built $($exe.FullName) ($([Math]::Round($exe.Length/1MB,3)) MB)"
if ($Architecture -eq 'x64') {
    Copy-Item -LiteralPath $exe.FullName -Destination (Join-Path $PSScriptRoot 'dist/Luma.exe') -Force
    Write-Output 'Product entry point: dist/Luma.exe'
}
