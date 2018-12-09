echo off
REM STEP 4 - Sign it
cd sign
3doEncrypt.exe genromtags ..\optidoom.iso
3doEncrypt.exe ..\optidoom.iso
echo 4 of 4: signed optidoom.iso
echo Great Success!

pause