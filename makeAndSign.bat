echo off
cd source
REM STEP 1 - Compile
make

REM STEP 2 - Create Directory Structure
copy LaunchMe ..\CD /Y
echo 2 of 4: ISO template done.

REM STEP 3 - covert to Opera file system ISO
cd..
tools\opera\3doiso.exe -in CD -out optidoom.iso
echo 3 of 4: ISO file system done

REM STEP 4 - Sign it
cd tools\sign
3doEncrypt.exe genromtags ..\..\optidoom.iso
echo 4 of 4: signed optidoom.iso
echo Great Success!

pause