@echo off

set mode=%1
if [%1]==[] (
  set mode=debug
)

IF NOT EXIST build MKDIR build
SET warning_flags=/WX /W4 /wd4100 /wd4189 /wd4505
SET includes=/Iext\ /Iext\freetype\
SET compiler_flags=%warning_flags% %includes% /nologo /FC /Fdbuild\ /Fobuild\ /FeFocal.exe

if %mode%==release (
  SET compiler_flags=/O2 /D %compiler_flags%
) else if %mode%==debug (
  SET compiler_flags=/Zi /Od %compiler_flags%
) else (
  echo Unkown build mode
  exit /B 2
)

SET libs=user32.lib kernel32.lib winmm.lib freetype.lib
SET linker_flags=%libs% /LIBPATH:ext\freetype\ /INCREMENTAL:NO /OPT:REF

CL src\win32_focal.cpp %compiler_flags% /link %linker_flags%
