echo off

set dir="../CD"

REM Step 1: Create CD directory if not exist
if not exist %dir% mkdir %dir%

REM Step 2: Decompile ISO
if not exist "doom.iso" goto fail
"../tools/opera/OperaTool" -d doom.iso %dir%

REM Step 3: Copy all added extra files to the extracted CD
xcopy CDextra %dir% /E /Y

REM Success!
goto end


:fail
echo doom.iso not found
:end

pause