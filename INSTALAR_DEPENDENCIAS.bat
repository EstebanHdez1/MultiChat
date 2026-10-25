@echo off
chcp 65001 >nul
title MultiChat - Instalador de Dependencias
color 0B

echo ================================================================
echo           MultiChat - Instalador de Dependencias
echo ================================================================
echo.

if exist "C:\msys64\usr\bin\bash.exe" (
    echo [OK] MSYS2 detectado en C:\msys64
    echo Instalando herramientas de compilación, Qt6 y librerías...
    echo.
    C:\msys64\usr\bin\bash.exe -l -c "pacman -S --needed --noconfirm mingw-w64-x86_64-toolchain mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja mingw-w64-x86_64-qt6-base mingw-w64-x86_64-qt6-websockets mingw-w64-x86_64-qt6-svg mingw-w64-x86_64-nlohmann-json mingw-w64-x86_64-zlib git"
    if %errorlevel% equ 0 (
        echo.
        echo ================================================================
        echo [EXITO] Todas las dependencias han sido instaladas correctamente.
        echo ================================================================
    ) else (
        echo.
        echo [AVISO] Ocurrió un detalle durante pacman. Revisa la consola de MSYS2.
    )
) else (
    echo [!] MSYS2 no se encontró en C:\msys64
    echo.
    echo Por favor descarga e instala MSYS2 desde:
    echo https://www.msys2.org/
    echo.
    echo Luego ejecuta este archivo nuevamente o consulta DEPENDENCIAS.txt.
    start https://www.msys2.org/
)

echo.
pause
