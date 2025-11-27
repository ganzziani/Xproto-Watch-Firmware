@echo off
REM Call this batch file using build.h as the parameter
REM Update build number. Read first line and extract the number at position 21, read 5 characters
set /p var= <%1
set /a var= %var:~21,5%+1
echo #define BUILD_NUMBER %var% >%1
echo Build Number: %var%
REM Get Time and Date using WMIC os GET LocalDateTime
for /F "usebackq tokens=1,2 delims==" %%i in (`wmic os get LocalDateTime /VALUE 2^>NUL`) do if '.%%i.'=='.LocalDateTime.' set ldt=%%j

echo #define BUILD_YEAR   %ldt:~2,2%>>%1

REM Remove 0 prefix 
set n=%ldt:~4,2%
set /a n=100%n% %% 100
echo #define BUILD_MONTH  %n% >>%1

set n=%ldt:~6,2%
set /a n=100%n% %% 100
echo #define BUILD_DAY    %n% >>%1

set n=%ldt:~8,2%
set /a n=100%n% %% 100
echo #define BUILD_HOUR   %n% >>%1

set n=%ldt:~10,2%
set /a n=100%n% %% 100
echo #define BUILD_MINUTE %n% >>%1

set n=%ldt:~12,2%
set /a n=100%n% %% 100
echo #define BUILD_SECOND %n% >>%1

echo // This file is generated from build_inc.bat>>%1