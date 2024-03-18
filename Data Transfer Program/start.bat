@echo off
setlocal enabledelayedexpansion 
:: Clear old logs from previous runs
del *.log
:: Save current directory
set "STARTING_DIR=%cd%"
echo Working Directory: %STARTING_DIR%
cd C:\Program Files (x86)\teraterm
c:
for /f "tokens=2 delims=()" %%a in ('wmic path win32_pnpentity get caption /format:list ^| findstr /i "STMicroelectronics STLink"') do (
    set "port=%%a"
    set "port=!port:~3!"
    echo !port!
)
:: Run tera term, save log to original directory
echo Attempting to collect logs from COM port #%port%
ttermpro /C=%port% /BAUD=115200 /M="%STARTING_DIR%\clear_buffers.ttl"
ttermpro /C=%port% /BAUD=115200 /L="%STARTING_DIR%/temperature-data.log" /M="%STARTING_DIR%\receive_data.ttl"