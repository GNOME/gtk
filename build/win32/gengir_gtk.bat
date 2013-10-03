@echo off

setlocal EnableDelayedExpansion

rem Needed environmental variables:
rem PLAT: Windows platform-Win32 (i.e. x86) or x64 (i.e. x86-64)
rem CONF: Configuration Type, Release or Debug
rem VSVER: Visual C++ version used [9, 10 or 11]
rem BASEDIR: Where the dependent libraries/headers are located
rem PKG_CONFIG_PATH: Where the GLib/ATK/Pango/GDK-Pixbuf and their dependent pkg-config .pc files can be found
rem MINGWDIR: Installation path of MINGW GCC, so gcc.exe can be found in %MINGWDIR%\bin.

rem Note that the Python executable/installation and all the runtime dependencies of the
rem library/libraries need to be in your PATH or %BASEBIN%\bin.

rem Check the environemental variables...
if /i "%PLAT%" == "Win32" goto PLAT_OK
if /i "%PLAT%" == "x64" goto PLAT_OK
if /i "%PLAT%" == "x86" (
   set PLAT=Win32
   goto PLAT_OK
)
if /i "%PLAT%" == "x86-64" (
   set PLAT=x64
   goto PLAT_OK
)
goto ERR_PLAT

:PLAT_OK
if "%VSVER%" == "9" goto VSVER_OK
if "%VSVER%" == "10" goto VSVER_OK
if "%VSVER%" == "11" goto VSVER_OK
goto ERR_VSVER
:VSVER_OK
if /i "%CONF%" == "Release" goto CONF_OK
if /i "%CONF%" == "Debug" goto CONF_OK
goto ERR_CONF
:CONF_OK
if "%BASEDIR%" == "" goto ERR_BASEDIR
if not exist %BASEDIR% goto ERR_BASEDIR

if "%PKG_CONFIG_PATH%" == "" goto ERR_PKGCONFIG
if not exist %PKG_CONFIG_PATH%\gobject-2.0.pc goto ERR_PKGCONFIG

if "%MINGWDIR%" == "" goto ERR_MINGWDIR
if not exist %MINGWDIR%\bin\gcc.exe goto ERR_MINGWDIR

set CC=cl
set BINDIR=%CD%\vs%VSVER%\%CONF%\%PLAT%\bin
set INCLUDE=%BASEDIR%\include\glib-2.0;%BASEDIR%\lib\glib-2.0\include;%INCLUDE%
set LIB=%BINDIR%;%BASEDIR%\lib;%LIB%
set PATH=%BINDIR%;%BASEDIR%\bin;%PATH%;%MINGWDIR%\bin
set PYTHONPATH=%BASEDIR%\lib\gobject-introspection;%BINDIR%

echo Creating filelist files for generating GDK3/GTK3 .gir's...
call python gen-file-list-gtk.py

echo Setup .bat for generating GDK3/GTK3 .gir's...

rem ===============================================================================
rem Begin setup of gtk_gir.bat to create Gdk-3.0.gir
rem (The ^^ is necessary to span the command to multiple lines on Windows cmd.exe!)
rem ===============================================================================

echo echo Generating Gdk-3.0.gir...> gtk_gir.bat
echo @echo off>> gtk_gir.bat
echo.>> gtk_gir.bat
rem ===============================================================
rem Setup the command line flags to g-ir-scanner for Gdk-3.0.gir...
rem ===============================================================
echo python %BASEDIR%\bin\g-ir-scanner --verbose -I..\.. -I..\..\gdk ^^>> gtk_gir.bat
echo -I%BASEDIR%\include\glib-2.0 -I%BASEDIR%\lib\glib-2.0\include ^^>> gtk_gir.bat
echo -I%BASEDIR%\include\pango-1.0 -I%BASEDIR%\include\atk-1.0 ^^>> gtk_gir.bat
echo -I%BASEDIR%\include\gdk-pixbuf-2.0 -I%BASEDIR%\include ^^>> gtk_gir.bat
if "%PLAT%" == "x64" echo -D__int64=long long ^^>> gtk_gir.bat
if "%PLAT%" == "Win32" echo -Dtime_t=long ^^>> gtk_gir.bat
echo --namespace=Gdk --nsversion=3.0 ^^>> gtk_gir.bat
echo --include=Gio-2.0 --include=GdkPixbuf-2.0 ^^>> gtk_gir.bat
echo --include=Pango-1.0 --include=cairo-1.0 ^^>> gtk_gir.bat
echo --no-libtool --library=gdk-3-vs%VSVER% ^^>> gtk_gir.bat
echo --reparse-validate --add-include-path=%BASEDIR%\share\gir-1.0 --add-include-path=. ^^>> gtk_gir.bat
echo --pkg-export gdk-3.0 --warn-all --c-include="gdk/gdk.h" ^^>> gtk_gir.bat
echo -I..\.. -DG_LOG_DOMAIN=\"Gdk\" -DGDK_COMPILATION ^^>> gtk_gir.bat
echo --filelist=gdk_list ^^>> gtk_gir.bat
echo -o Gdk-3.0.gir>> gtk_gir.bat
echo.>> gtk_gir.bat

echo Completed setup of .bat for generating Gdk-3.0.gir.
echo.>> gtk_gir.bat

rem =================================================
rem Finish setup of gtk_gir.bat to create Gtk-3.0.gir
rem =================================================

rem ===============================================================================
rem Begin setup of gtk_gir.bat to create Gtk-3.0.gir
rem (The ^^ is necessary to span the command to multiple lines on Windows cmd.exe!)
rem ===============================================================================

echo echo Generating Gtk-3.0.gir...>> gtk_gir.bat
echo.>> gtk_gir.bat
rem ===============================================================
rem Setup the command line flags to g-ir-scanner for Gtk-3.0.gir...
rem ===============================================================
echo python %BASEDIR%\bin\g-ir-scanner --verbose -I..\.. -I..\..\gtk -I..\..\gdk ^^>> gtk_gir.bat
echo -I%BASEDIR%\include\glib-2.0 -I%BASEDIR%\lib\glib-2.0\include ^^>> gtk_gir.bat
echo -I%BASEDIR%\include\pango-1.0 -I%BASEDIR%\include\atk-1.0 ^^>> gtk_gir.bat
echo -I%BASEDIR%\include\gdk-pixbuf-2.0 -I%BASEDIR%\include ^^>> gtk_gir.bat
echo --namespace=Gtk --nsversion=3.0 ^^>> gtk_gir.bat
echo --include=Atk-1.0 ^^>> gtk_gir.bat
echo --include-uninstalled=./Gdk-3.0.gir ^^>> gtk_gir.bat
echo --no-libtool --library=gtk-3-vs%VSVER% ^^>> gtk_gir.bat
echo --reparse-validate --add-include-path=%BASEDIR%\share\gir-1.0 --add-include-path=. ^^>> gtk_gir.bat
echo --pkg-export gtk+-3.0 --warn-all --c-include="gtk/gtkx.h" ^^>> gtk_gir.bat
echo -I..\.. -DG_LOG_DOMAIN=\"Gtk\" -DGTK_LIBDIR=\"/dummy/lib\" ^^>> gtk_gir.bat
if "%PLAT%" == "x64" echo -D__int64=long long ^^>> gtk_gir.bat
if "%PLAT%" == "Win32" echo -Dtime_t=long ^^>> gtk_gir.bat
echo -DGTK_DATADIR=\"/dummy/share\" -DGTK_DATA_PREFIX=\"/dummy\" ^^>> gtk_gir.bat
echo -DGTK_SYSCONFDIR=\"/dummy/etc\" -DGTK_VERSION=\"3.6.2\" ^^>> gtk_gir.bat
echo -DGTK_BINARY_VERSION=\"3.0.0\" -DGTK_HOST=\"i686-pc-vs%VSVER%\" ^^>> gtk_gir.bat
echo -DGTK_COMPILATION -DGTK_PRINT_BACKENDS=\"file\" ^^>> gtk_gir.bat
echo -DGTK_PRINT_PREVIEW_COMMAND=\"undefined-gtk-print-preview-command\" ^^>> gtk_gir.bat
echo -DGTK_FILE_SYSTEM_ENABLE_UNSUPPORTED -DGTK_PRINT_BACKEND_ENABLE_UNSUPPORTED ^^>> gtk_gir.bat
echo -DINCLUDE_IM_am_et -DINCLUDE_IM_cedilla -DINCLUDE_IM_cyrillic_translit ^^>> gtk_gir.bat
echo -DINCLUDE_IM_ime -DINCLUDE_IM_inuktitut -DINCLUDE_IM_ipa ^^>> gtk_gir.bat
echo -DINCLUDE_IM_multipress -DINCLUDE_IM_thai -DINCLUDE_IM_ti_er ^^>> gtk_gir.bat
echo -DINCLUDE_IM_ti_et -DINCLUDE_IM_viqr --filelist=gtk_list ^^>> gtk_gir.bat
echo -o Gtk-3.0.gir>> gtk_gir.bat
echo.>> gtk_gir.bat

echo Completed setup of .bat for generating Gtk-3.0.gir.
echo.>> gtk_gir.bat

rem =================================================
rem Finish setup of gtk_gir.bat to create Gtk-3.0.gir
rem =================================================

rem =======================
rem Now generate the .gir's
rem =======================
CALL gtk_gir.bat

rem Clean up the .bat/filelists for generating the .gir files...
del gtk_gir.bat
del gdk_list
del gtk_list

rem Now compile the generated .gir files
%BASEDIR%\bin\g-ir-compiler --includedir=. --debug --verbose Gdk-3.0.gir -o Gdk-3.0.typelib
%BASEDIR%\bin\g-ir-compiler --includedir=. --debug --verbose Gtk-3.0.gir -o Gtk-3.0.typelib
rem Copy the generated .girs and .typelibs to their appropriate places

mkdir ..\..\build\win32\vs%VSVER%\%CONF%\%PLAT%\share\gir-1.0
move /y *.gir %BASEDIR%\share\gir-1.0\

mkdir ..\..\build\win32\vs%VSVER%\%CONF%\%PLAT%\lib\girepository-1.0
move /y *.typelib %BASEDIR%\lib\girepository-1.0\

goto DONE

:ERR_PLAT
echo You need to specify a valid Platform [set PLAT=Win32 or PLAT=x64]
goto DONE
:ERR_VSVER
echo You need to specify your Visual Studio version [set VSVER=9 or VSVER=10 or VSVER=11]
goto DONE
:ERR_CONF
echo You need to specify a valid Configuration [set CONF=Release or CONF=Debug]
goto DONE
:ERR_BASEDIR
echo You need to specify a valid BASEDIR.
goto DONE
:ERR_PKGCONFIG
echo You need to specify a valid PKG_CONFIG_PATH
goto DONE
:ERR_MINGWDIR
echo You need to specify a valid MINGWDIR, where a valid gcc installation can be found.
goto DONE
:DONE

