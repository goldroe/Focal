@echo off

IF NOT EXIST build MKDIR build
SET warning_flags=/WX /W4 /wd4100 /wd4189 /wd4505
SET includes=/Iext\
SET compiler_flags=%warning_flags% %includes% /nologo /FC /Zi /Fdbuild\ /Fobuild\ /FeFocal.exe

SET libs=user32.lib kernel32.lib winmm.lib
SET linker_flags=%libs% /INCREMENTAL:NO /OPT:REF

CL src\win32_focal.cpp %compiler_flags% /link %linker_flags%
