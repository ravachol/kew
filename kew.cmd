@echo off

set MSYSTEM=UCRT64
set CHERE_INVOKING=1
set MSYS2_PATH_TYPE=inherit
set KEW_SIXEL=1

set TERM=xterm-256color

for /f "delims=" %%i in ('where bash.exe 2^>nul') do set BASH=%%i & goto found
set BASH=C:\msys64\usr\bin\bash.exe

:found

"%BASH%" --login -i -c "kew %*"
