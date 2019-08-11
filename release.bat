for /f "tokens=1-4 delims=/ " %%i in ("%date%") do (
     set dow=%%i
     set month=%%j
     set day=%%k
     set year=%%l
   )

set datestr=%year%_%month%_%day%
set name=shade_mfm
set fname=%name%_%datestr%

xcopy /yq imgui.ini					builds\%fname%\
xcopy /yqs shaders				builds\%fname%\shaders\
xcopy /yqs projects				builds\%fname%\projects\
xcopy /yqs stdlib				builds\%fname%\stdlib\
copy Release\shade_mfm.exe			builds\%fname%\%name%.exe
cd builds
7z a %fname%.zip %fname%