@echo off
REM Call this batch file using build.h as the parameter
REM Update build number. Read first line and extract the number at position 21, read 5 characters
set /p var= <%1
set /a var= %var:~21,5%+1
echo #define BUILD_NUMBER %var% >%1
echo Build Number: %var%

REM Get Time and Date using PowerShell instead of WMIC
for /f "tokens=1-6 delims=/:" %%a in ('powershell -command "Get-Date -Format 'yy/MM/dd/HH/mm/ss'"') do (
    set year=%%a
    set month=%%b
    set day=%%c
    set hour=%%d
    set minute=%%e
    set second=%%f
)

echo #define BUILD_YEAR   %year%>>%1

REM Remove 0 prefix 
set /a month=100%month% %% 100
echo #define BUILD_MONTH  %month% >>%1

set /a day=100%day% %% 100
echo #define BUILD_DAY    %day% >>%1

set /a hour=100%hour% %% 100
echo #define BUILD_HOUR   %hour% >>%1

set /a minute=100%minute% %% 100
echo #define BUILD_MINUTE %minute% >>%1

set /a second=100%second% %% 100
echo #define BUILD_SECOND %second% >>%1

echo // This file is generated from build_inc.bat>>%1