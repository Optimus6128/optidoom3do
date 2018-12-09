echo off
cd source
REM STEP 1 - Compile
amake

REM STEP 2 - Create Directory Structure
copy LaunchMe ..\takeme /Y
echo 2 of 4: ISO template done.

REM STEP 3 - covert to Opera file system ISO
cd..
operaFS.ahk
echo 3 of 4: ISO file system done

pause