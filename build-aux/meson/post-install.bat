@echo off

set gtk_api_version=%1
set gtk_abi_version=%2
set gtk_libdir=%3
set gtk_datadir=%4

rem Convert '/' in paths to '\', so that mkdir runs properly
set gtk_libdir_win=%gtk_libdir:/=\%
set gtk_datadir_win=%gtk_datadir:/=\%

rem Package managers set this so we don't need to run

if not "%DESTDIR%" == "" goto :end

echo Compiling GSettings schemas...
glib-compile-schemas %gtk_datadir_win%\glib-2.0\schemas

echo Updating icon cache...
gtk-update-icon-cache -q -t -f %gtk_datadir_win%\icons\hicolor

echo Updating module cache for print backends...
if not exist %gtk_libdir_win%\gtk-4.0\4.0.0\printbackends\ mkdir %gtk_libdir_win%\gtk-4.0\4.0.0\printbackends
gio-querymodules %gtk_libdir_win%\gtk-4.0\4.0.0\printbackends

echo Updating module cache for input methods...
if not exist %gtk_libdir_win%\gtk-4.0\4.0.0\immodules\ mkdir %gtk_libdir_win%\gtk-4.0\4.0.0\immodules
gio-querymodules %gtk_libdir_win%\gtk-4.0\4.0.0\immodules

:end