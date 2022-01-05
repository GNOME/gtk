# Convert the source listing to object (.obj) listing in
# another NMake Makefile module, include it, and clean it up.
# This is a "fact-of-life" regarding NMake Makefiles...
# This file does not need to be changed unless one is maintaining the NMake Makefiles

# For those wanting to add things here:
# To add a list, do the following:
# # $(description_of_list)
# if [call create-lists.bat header $(makefile_snippet_file) $(variable_name)]
# endif
#
# if [call create-lists.bat file $(makefile_snippet_file) $(file_name)]
# endif
#
# if [call create-lists.bat footer $(makefile_snippet_file)]
# endif
# ... (repeat the if [call ...] lines in the above order if needed)
# !include $(makefile_snippet_file)
#
# (add the following after checking the entries in $(makefile_snippet_file) is correct)
# (the batch script appends to $(makefile_snippet_file), you will need to clear the file unless the following line is added)
#!if [del /f /q $(makefile_snippet_file)]
#!endif

# In order to obtain the .obj filename that is needed for NMake Makefiles to build DLLs/static LIBs or EXEs, do the following
# instead when doing 'if [call create-lists.bat file $(makefile_snippet_file) $(file_name)]'
# (repeat if there are multiple $(srcext)'s in $(source_list), ignore any headers):
# !if [for %c in ($(source_list)) do @if "%~xc" == ".$(srcext)" @call create-lists.bat file $(makefile_snippet_file) $(intdir)\%~nc.obj]
#
# $(intdir)\%~nc.obj needs to correspond to the rules added in build-rules-msvc.mak
# %~xc gives the file extension of a given file, %c in this case, so if %c is a.cc, %~xc means .cc
# %~nc gives the file name of a given file without extension, %c in this case, so if %c is a.cc, %~nc means a

NULL=

# For GDK enumeration sources
!include ..\gdk\gdk-pub-headers.mak

!if [call create-lists.bat header gdk_headers.mak GDK_PUBLIC_HEADERS]
!endif

!if [for %f in ($(gdk_public_h_sources) $(gdk_deprecated_h_sources)) do @call create-lists.bat file gdk_headers.mak ../gdk/%f]
!endif

!if [call create-lists.bat footer gdk_headers.mak]
!endif

!include gdk_headers.mak

!if [del /f /q gdk_headers.mak]
!endif

# For GDK resources

!if [call create-lists.bat header resources_sources.mak GDK_RESOURCES]
!endif

!if [for %f in (..\gdk\resources\glsl\*.glsl) do @call create-lists.bat file resources_sources.mak %f]
!endif

!if [call create-lists.bat footer resources_sources.mak]
!endif

!if [call create-lists.bat header resources_sources.mak GTK_RESOURCES]
!endif

# For GTK enumeration sources
!include ..\gtk\gtk-mkenums-headers.mak
!include ..\gtk\a11y\Makefile.inc
!include ..\gtk\deprecated\Makefile.inc

!if [call create-lists.bat header gtk_headers.mak GTK_PUBLIC_ENUM_HEADERS]
!endif

!if [for %f in ($(GTK_PUB_HDRS:.h=)) do @call create-lists.bat file gtk_headers.mak ../gtk/%f.h]
!endif

!if [for %f in ($(a11y_h_sources) $(gtk_deprecated_h_sources)) do @call create-lists.bat file gtk_headers.mak ../gtk/%f]
!endif

!if [call create-lists.bat footer gtk_headers.mak]
!endif

!if [call create-lists.bat header gtk_headers.mak GTK_PRIVATE_ENUM_HEADERS]
!endif

!if [for %f in ($(GTK_PRIVATE_TYPE_HDRS)) do @call create-lists.bat file gtk_headers.mak ../gtk/%f]
!endif

!if [call create-lists.bat footer gtk_headers.mak]
!endif

!include gtk_headers.mak

!if [del /f /q gtk_headers.mak]
!endif

# For GTK resources

!if [for %f in (..\gtk\theme\Adwaita\gtk.css ..\gtk\theme\Adwaita\gtk-dark.css ..\gtk\theme\Adwaita\gtk-contained.css ..\gtk\theme\Adwaita\gtk-contained-dark.css) do @call create-lists.bat file resources_sources.mak %f]
!endif

!if [for %x in (png svg) do @(for %f in (..\gtk\theme\Adwaita\assets\*.%x) do @call create-lists.bat file resources_sources.mak %f)]
!endif

!if [for %f in (..\gtk\theme\HighContrast\gtk.css ..\gtk\theme\HighContrast\gtk-inverse.css ..\gtk\theme\HighContrast\gtk-contained.css ..\gtk\theme\HighContrast\gtk-contained-inverse.css) do @call create-lists.bat file resources_sources.mak %f]
!endif

!if [for %x in (png svg) do @(for %f in (..\gtk\theme\HighContrast\assets\*.%x) do @call create-lists.bat file resources_sources.mak %f)]
!endif

!if [for %f in (..\gtk\theme\win32\gtk-win32-base.css ..\gtk\theme\win32\gtk.css) do @call create-lists.bat file resources_sources.mak %f]
!endif

!if [for %f in (..\gtk\cursor\*.png ..\gtk\gesture\*.symbolic.png ..\gtk\ui\*.ui) do @call create-lists.bat file resources_sources.mak %f]
!endif

!if [for %s in (16 22 24 32 48) do @(for %c in (actions status categories) do @(for %f in (..\gtk\icons\%sx%s\%c\*.png) do @call create-lists.bat file resources_sources.mak %f))]
!endif

!if [for %s in (scalable) do @(for %c in (status) do @(for %f in (..\gtk\icons\%s\%c\*.svg) do @call create-lists.bat file resources_sources.mak %f))]
!endif

!if [for %f in (..\gtk\inspector\*.ui ..\gtk\inspector\logo.png ..\gtk\emoji\*.data) do @call create-lists.bat file resources_sources.mak %f]
!endif

!if [call create-lists.bat footer resources_sources.mak]
!endif

# For gtk demo program resources

!if [call create-lists.bat header resources_sources.mak GTK_DEMO_RESOURCES]
!endif

!if [for /f %f in ('$(GLIB_COMPILE_RESOURCES) --generate-dependencies --sourcedir=..\demos\gtk-demo ..\demos\gtk-demo\demo.gresource.xml') do @call create-lists.bat file resources_sources.mak %f]
!endif

!if [call create-lists.bat footer resources_sources.mak]
!endif

!if [call create-lists.bat header resources_sources.mak ICON_BROWSER_RESOURCES]
!endif

!if [for /f %f in ('$(GLIB_COMPILE_RESOURCES) --sourcedir=..\demos\icon-browser --generate-dependencies ..\demos\icon-browser\iconbrowser.gresource.xml') do @call create-lists.bat file resources_sources.mak %f]
!endif

!if [call create-lists.bat footer resources_sources.mak]
!endif

!include resources_sources.mak

!if [del /f /q resources_sources.mak]
!endif

!if [call create-lists.bat header demo_sources.mak demo_actual_sources]
!endif

!if [for %f in ($(demo_sources)) do @call create-lists.bat file demo_sources.mak ..\demos\gtk-demo\%f]
!endif

!if [call create-lists.bat footer demo_sources.mak]
!endif

!include demo_sources.mak

!if [del /f /q demo_sources.mak]
!endif