@echo off
setlocal enabledelayedexpansion
title Compilar Multistream Chat

echo ========================================================
echo         MULTISTREAM CHAT - AUTO COMPILADOR
echo ========================================================
echo.

:: Cambiar al directorio donde se encuentra este script
cd /d "%~dp0"

:: Asegurar que las herramientas de MinGW / MSYS2 esten en el PATH si estan instaladas
if exist "C:\msys64\mingw64\bin" (
    set "PATH=C:\msys64\mingw64\bin;!PATH!"
)

:: Verificar si cmake esta disponible
where cmake >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo [ERROR] No se encontro 'cmake' en el PATH.
    echo Asegurate de tener CMake y MinGW64 instalados y configurados.
    echo.
    pause
    exit /b 1
)

:: Si la carpeta build no existe o no tiene CMakeCache, configurar el proyecto
if not exist "build\CMakeCache.txt" (
    echo [1/2] Configurando proyecto con CMake y Ninja...
    where ninja >nul 2>&1
    if %ERRORLEVEL% equ 0 (
        cmake -B build -G Ninja
    ) else (
        cmake -B build
    )
    if %ERRORLEVEL% neq 0 (
        echo.
        echo [ERROR] Fallo la configuracion con CMake.
        echo.
        pause
        exit /b 1
    )
    echo.
)

:: Compilar el proyecto
echo [2/2] Compilando MultistreamChat...
cmake --build build
if %ERRORLEVEL% neq 0 (
    echo.
    echo [ERROR] La compilacion ha fallado. Revisa los errores anteriores.
    echo.
    pause
    exit /b 1
)

echo.
echo ========================================================
echo         COMPILACION COMPLETADA CON EXITO!
echo ========================================================
echo Ejecutable generado: build\MultistreamChat.exe
echo.

:: Preguntar si desea iniciar la aplicacion
set /p RUN_APP="Deseas iniciar MultistreamChat ahora? (S/N) [Por defecto S]: "
if /i "!RUN_APP!"=="" set RUN_APP=S
if /i "!RUN_APP!"=="S" (
    echo Iniciando MultistreamChat...
    start "" "build\MultistreamChat.exe"
)

exit /b 0
