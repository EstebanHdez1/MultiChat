param(
    [string]$OutputDir = "release\MultiChat-v1.3.0-Portable",
    [string]$ZipName = "release\MultiChat-v1.3.0-Windows-Portable.zip"
)

$ErrorActionPreference = "Stop"

Write-Host "==> Preparando empaquetado portable de MultiChat..." -ForegroundColor Cyan

# 1. Asegurar build actualizado
Write-Host "==> Compilando versión final..." -ForegroundColor Yellow
cmake --build build --config Release
if ($LASTEXITCODE -ne 0) {
    Write-Error "Fallo en la compilación de MultiChat."
    exit 1
}

# 2. Limpiar y crear directorio de release
if (Test-Path $OutputDir) {
    Remove-Item -Recurse -Force $OutputDir
}
New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null

# 3. Copiar ejecutable principal
Write-Host "==> Copiando ejecutable..." -ForegroundColor Yellow
Copy-Item "build\MultistreamChat.exe" -Destination "$OutputDir\MultiChat.exe"

# 4. Ejecutar windeployqt para librerías Qt
Write-Host "==> Desplegando dependencias de Qt..." -ForegroundColor Yellow
$windeploy = "C:\msys64\mingw64\bin\windeployqt6.exe"
if (-not (Test-Path $windeploy)) {
    $windeploy = "windeployqt6.exe"
}

& $windeploy --no-translations --compiler-runtime "$OutputDir\MultiChat.exe"

# 5. Copiar DLLs esenciales de MinGW y C++ runtime si faltan
$mingwBin = "C:\msys64\mingw64\bin"
$extraDlls = @(
    "libgcc_s_seh-1.dll",
    "libstdc++-6.dll",
    "libwinpthread-1.dll",
    "zlib1.dll"
)

foreach ($dll in $extraDlls) {
    $srcDll = Join-Path $mingwBin $dll
    $dstDll = Join-Path $OutputDir $dll
    if ((Test-Path $srcDll) -and (-not (Test-Path $dstDll))) {
        Copy-Item $srcDll -Destination $dstDll
        Write-Host "   + Copiado: $dll" -ForegroundColor Green
    }
}

# 6. Crear archivo LEEME en el directorio portable
$readmeContent = @"
=====================================================
MultiChat - Unified Live Stream Chat (Versión Portable)
=====================================================

Esta es la versión portable de MultiChat.
No requiere instalación. Solo haz doble clic en MultiChat.exe para iniciar.

Plataformas soportadas:
- Twitch (IRC tiempo real + Emotes nativos y de terceros)
- YouTube (InnerTube live chat poller)
- Kick (Centrifugo WebSocket nativo en tiempo real)
- TikTok (Monitoreo de directo con soporte de likes y donaciones)

Características:
- Servidor web local integrado para móvil (Wi-Fi) y OBS Studio (Overlay)
- Modo oscuro moderno, interfaz minimalista y ultraligera
- Filtros por plataforma y tipo de mensaje
- Ranking de donaciones y estadísticas en vivo
"@

Set-Content -Path "$OutputDir\LEEME.txt" -Value $readmeContent -Encoding UTF8

# 7. Comprimir en archivo ZIP
Write-Host "==> Comprimiendo versión portable en $ZipName..." -ForegroundColor Yellow
if (Test-Path $ZipName) {
    Remove-Item -Force $ZipName
}
Compress-Archive -Path "$OutputDir\*" -DestinationPath $ZipName -Force

Write-Host "==> Paquete portable completado exitosamente!" -ForegroundColor Green
Write-Host "    Ubicación: $ZipName" -ForegroundColor Cyan
