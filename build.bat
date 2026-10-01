@echo off
rem NanoWeb — сборка на Windows.
rem Требуется: fasm.exe в PATH (или переменная FASM), переменная INCLUDE = каталог INCLUDE из пакета FASM,
rem Python 3 с модулем pefile (pip install pefile) для проверки заголовка.
setlocal
if "%FASM%"=="" set FASM=fasm
if "%PYTHON%"=="" set PYTHON=python
if not exist build mkdir build

pushd src
%FASM% -d CONSOLE=0 main.asm ..\build\nanoweb.exe || goto fail
%FASM% -d CONSOLE=1 main.asm ..\build\nanoweb-con.exe || goto fail
popd

%PYTHON% tools\patch_pe.py build\nanoweb.exe build\nanoweb-con.exe || goto end_fail
%PYTHON% tools\check_pe.py build\nanoweb.exe build\nanoweb-con.exe || goto end_fail
exit /b 0

:fail
popd
:end_fail
exit /b 1
