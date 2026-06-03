# installer/build_installer.ps1
$iscc = "C:\Program Files (x86)\Inno Setup 6\iscc.exe"
$script = Join-Path $PSScriptRoot "eqquarmcompanion.iss"

if (-not (Test-Path $iscc)) {
    Write-Error "Inno Setup 6 introuvable : $iscc"
    exit 1
}
if (-not (Test-Path $script)) {
    Write-Error "Script .iss introuvable : $script"
    exit 1
}

& $iscc $script
if ($LASTEXITCODE -ne 0) {
    Write-Error "Compilation échouée (code $LASTEXITCODE)"
    exit $LASTEXITCODE
}
Write-Host "Installeur généré dans installer/output/"
