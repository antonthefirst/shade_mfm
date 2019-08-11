for /f "tokens=1-4 delims=/ " %%i in ("%date%") do (
     set dow=%%i
     set month=%%j
     set day=%%k
     set year=%%l
   )
for /f "tokens=1-2 delims=: " %%i in ("%time%") do (
     set hour=%%i
     set minute=%%j
   )
set datestr=%year%_%month%_%day%_at_%hour%_%minute%
set name=shade_mfm
set fname=%name%_%datestr%

xcopy /yq imgui.ini			builds\%fname%\
xcopy /yq %name%.sln			builds\%fname%\dev\
xcopy /yq %name%.vcxproj		builds\%fname%\dev\
xcopy /yq %name%.vcxproj.filters	builds\%fname%\dev\
xcopy /yq Makefile			builds\%fname%\dev\
xcopy /yqs src                  	builds\%fname%\dev\src\
xcopy /yqs wrap                 	builds\%fname%\dev\wrap\
xcopy /yqs libs                 	builds\%fname%\dev\libs\
xcopy /yqs core                 	builds\%fname%\dev\core\
xcopy /yqs notes                	builds\%fname%\dev\notes\
xcopy /yqs snips                	builds\%fname%\snips\
xcopy /yqs shaders			builds\%fname%\shaders\
xcopy /yqs projects			builds\%fname%\projects\
xcopy /yqs stdlib			builds\%fname%\stdlib\
copy Release\shade_mfm.exe		builds\%fname%\%name%.exe
copy Debug\shade_mfm.exe		builds\%fname%\%name%_debug.exe
cd builds
7z a -r %fname%.zip %fname%