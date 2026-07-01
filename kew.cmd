@echo off

if defined KEW_BASH (
    set BASH=%KEW_BASH%
) else (
    for /f "delims=" %%i in ('where bash.exe 2^>nul') do set BASH=%%i & goto found
    set BASH=C:\msys64\usr\bin\bash.exe
)

:found
set MSYSTEM=UCRT64
set CHERE_INVOKING=1
set MSYS2_PATH_TYPE=inherit

"%BASH%" --login -i -c "kew %*"
