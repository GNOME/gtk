# NMake Makefile portion for code generation and
# intermediate build directory creation
# Items in here should not need to be edited unless
# one is maintaining the NMake build files.

!include config-msvc.mak

!if [echo GDK_RESOURCES = \>> resources_sources.mak ]
!endif

!if [for %f in (..\..\gdk\resources\glsl\*.glsl) do @echo %f \>> resources_sources.mak]
!endif

!if [echo.>> resources_sources.mak]
!endif

!if [del /f /q resources_sources.mak]
!endif

# Copy the pre-defined gdkconfig.h.[win32|win32_broadway]
!if "$(CFG)" == "release" || "$(CFG)" == "Release"
GDK_OLD_CFG = debug
!else
GDK_OLD_CFG = release
!endif

!ifdef BROADWAY
GDK_CONFIG = broadway
GDK_DEL_CONFIG = win32
GDK_CONFIG_TEMPLATE = ..\..\gdk\gdkconfig.h.win32_broadway
!else
GDK_CONFIG = win32
GDK_DEL_CONFIG = broadway
GDK_CONFIG_TEMPLATE = ..\..\gdk\gdkconfig.h.win32
!endif

GDK_MARSHALERS_FLAGS = --prefix=_gdk_marshal --valist-marshallers
GDK_RESOURCES_ARGS = $** --target=$@ --sourcedir=..\..\gdk --c-name _gdk --manual-register
GTK_MARSHALERS_FLAGS = --prefix=_gtk_marshal --valist-marshallers
GTK_RESOURCES_ARGS = $** --target=$@ --sourcedir=..\..\gtk --c-name _gtk --manual-register

all:	\
	..\..\config.h	\
	..\..\gdk\gdkconfig.h	\
	..\..\gdk\gdkversionmacros.h	\
	..\..\gdk\gdkmarshalers.h	\
	..\..\gdk\gdkmarshalers.c	\
	..\..\gdk\gdkresources.h	\
	..\..\gdk\gdkresources.c	\
	..\..\gtk\gtk-win32.rc	\
	..\..\gtk\libgtk3.manifest	\
	..\..\gtk\gtkdbusgenerated.h	\
	..\..\gtk\gtkdbusgenerated.c	\
	..\..\gtk\gtktypefuncs.inc	\
	..\..\gtk\gtk.gresource.xml	\
	..\..\gtk\gtkmarshalers.h	\
	..\..\gtk\gtkmarshalers.c	\
	..\..\gtk\gtkresources.h	\
	..\..\gtk\gtkresources.c	\
	..\..\demos\gtk-demo\demos.h

# Copy the pre-defined config.h.win32 and demos.h.win32
..\..\config.h: ..\..\config.h.win32
..\..\demos\gtk-demo\demos.h: ..\..\demos\gtk-demo\demos.h.win32
..\..\gtk\gtk-win32.rc: ..\..\gtk\gtk-win32.rc.body

..\..\gdk-$(CFG)-$(GDK_CONFIG)-build: $(GDK_CONFIG_TEMPLATE)
	@if exist ..\..\gdk-$(GDK_OLD_CFG)-$(GDK_DEL_CONFIG)-build del ..\..\gdk-$(GDK_OLD_CFG)-$(GDK_DEL_CONFIG)-build
	@if exist ..\..\gdk-$(GDK_OLD_CFG)-$(GDK_CONFIG)-build del ..\..\gdk-$(GDK_OLD_CFG)-$(GDK_CONFIG)-build
	@if exist ..\..\gdk-$(CFG)-$(GDK_DEL_CONFIG)-build del ..\..\gdk-$(CFG)-$(GDK_DEL_CONFIG)-build
	@copy $** $@
	
..\..\gdk\gdkconfig.h: ..\..\gdk-$(CFG)-$(GDK_CONFIG)-build

..\..\config.h	\
..\..\gdk\gdkconfig.h	\
..\..\gtk\gtk-win32.rc	\
..\..\demos\gtk-demo\demos.h:
	@echo Copying $@...
	@copy $** $@

..\..\gdk\gdkversionmacros.h: ..\..\gdk\gdkversionmacros.h.in
	@echo Generating $@...
	@$(PYTHON) gen-gdkversionmacros-h.py --version=$(GTK_VERSION)

..\..\gdk\gdkmarshalers.h: ..\..\gdk\gdkmarshalers.list
	@echo Generating $@...
	@$(PYTHON) $(GLIB_GENMARSHAL) $(GDK_MARSHALERS_FLAGS) --header $** > $@.tmp
	@move $@.tmp $@

..\..\gdk\gdkmarshalers.c: ..\..\gdk\gdkmarshalers.list
	@echo Generating $@...
	@$(PYTHON) $(GLIB_GENMARSHAL) $(GDK_MARSHALERS_FLAGS) --body $** > $@.tmp
	@move $@.tmp $@

..\..\gdk\gdk.gresource.xml:
	@echo Generating $@...
	@echo ^<?xml version='1.0' encoding='UTF-8'?^> >$@
	@echo ^<gresources^> >> $@
	@echo  ^<gresource prefix='/org/gtk/libgdk'^> >> $@
	@for %%f in (..\..\gdk\resources\glsl\*.glsl) do @echo     ^<file alias='glsl/%%~nxf'^>resources/glsl/%%~nxf^</file^> >> $@
	@echo   ^</gresource^> >> $@
	@echo ^</gresources^> >> $@

..\..\gdk\gdkresources.h: ..\..\gdk\gdk.gresource.xml
	@echo Generating $@...
	@if not "$(XMLLINT)" == "" set XMLLINT=$(XMLLINT)
	@if not "$(JSON_GLIB_FORMAT)" == "" set JSON_GLIB_FORMAT=$(JSON_GLIB_FORMAT)
	@if not "$(GDK_PIXBUF_PIXDATA)" == "" set GDK_PIXBUF_PIXDATA=$(GDK_PIXBUF_PIXDATA)
	@$(GLIB_COMPILE_RESOURCES) $(GDK_RESOURCES_ARGS) --generate-header

..\..\gdk\gdkresources.c: ..\..\gdk\gdk.gresource.xml $(GDK_RESOURCES)
	@echo Generating $@...
	@if not "$(XMLLINT)" == "" set XMLLINT=$(XMLLINT)
	@if not "$(JSON_GLIB_FORMAT)" == "" set JSON_GLIB_FORMAT=$(JSON_GLIB_FORMAT)
	@if not "$(GDK_PIXBUF_PIXDATA)" == "" set GDK_PIXBUF_PIXDATA=$(GDK_PIXBUF_PIXDATA)
	@$(GLIB_COMPILE_RESOURCES) $(GDK_RESOURCES_ARGS) --generate-source

..\..\gtk\libgtk3.manifest: ..\..\gtk\libgtk3.manifest.in
	@echo Generating $@...
	@$(PYTHON) replace.py	\
	--action=replace-var	\
	--input=$**	--output=$@	\
	--var=EXE_MANIFEST_ARCHITECTURE	\
	--outstring=*

..\..\gtk\gtkdbusgenerated.h ..\..\gtk\gtkdbusgenerated.c: ..\..\gtk\gtkdbusinterfaces.xml
	@echo Generating GTK DBus sources...
	@$(PYTHON) $(PREFIX)\bin\gdbus-codegen	\
	--interface-prefix org.Gtk. --c-namespace _Gtk	\
	--generate-c-code gtkdbusgenerated $**	\
	--output-directory $(@D)

..\..\gtk\gtktypefuncs.inc: ..\..\gtk\gentypefuncs.py
	@echo Generating $@...
	@echo #undef GTK_COMPILATION > $(@R).preproc.c
	@echo #include "gtkx.h" >> $(@R).preproc.c
	@cl /EP $(GTK_PREPROCESSOR_FLAGS) $(@R).preproc.c > $(@R).combined.c
	@$(PYTHON) $** $@ $(@R).combined.c
	@del $(@R).preproc.c $(@R).combined.c

..\..\gtk\gtk.gresource.xml:
	@echo Generating $@...
	@echo ^<?xml version='1.0' encoding='UTF-8'?^>> $@
	@echo ^<gresources^>>> $@
	@echo   ^<gresource prefix='/org/gtk/libgtk'^>>> $@
	@echo     ^<file^>theme/Adwaita/gtk.css^</file^>>> $@
	@echo     ^<file^>theme/Adwaita/gtk-dark.css^</file^>>> $@
	@echo     ^<file^>theme/Adwaita/gtk-contained.css^</file^>>> $@
	@echo     ^<file^>theme/Adwaita/gtk-contained-dark.css^</file^>>> $@
	@for %%f in (..\..\gtk\theme\Adwaita\assets\*.png) do @echo     ^<file preprocess='to-pixdata'^>theme/Adwaita/assets/%%~nxf^</file^>>> $@
	@for %%f in (..\..\gtk\theme\Adwaita\assets\*.svg) do @echo     ^<file^>theme/Adwaita/assets/%%~nxf^</file^>>> $@
	@echo     ^<file^>theme/HighContrast/gtk.css^</file^>>> $@
	@echo     ^<file alias='theme/HighContrastInverse/gtk.css'^>theme/HighContrast/gtk-inverse.css^</file^>>> $@
	@echo     ^<file^>theme/HighContrast/gtk-contained.css^</file^>>> $@
	@echo     ^<file^>theme/HighContrast/gtk-contained-inverse.css^</file^>>> $@
	@for %%f in (..\..\gtk\theme\HighContrast\assets\*.png) do @echo     ^<file preprocess='to-pixdata'^>theme/HighContrast/assets/%%~nxf^</file^>>> $@
	@for %%f in (..\..\gtk\theme\HighContrast\assets\*.svg) do @echo     ^<file^>theme/HighContrast/assets/%%~nxf^</file^>>> $@
	@echo     ^<file^>theme/win32/gtk-win32-base.css^</file^>>> $@
	@echo     ^<file^>theme/win32/gtk.css^</file^>>> $@
	@for %%f in (..\..\gtk\cursor\*.png) do @echo     ^<file^>cursor/%%~nxf^</file^>>> $@
	@for %%f in (..\..\gtk\gesture\*.symbolic.png) do @echo     ^<file alias='icons/64x64/actions/%%~nxf'^>gesture/%%~nxf^</file^>>> $@
	@for %%f in (..\..\gtk\ui\*.ui) do @echo     ^<file preprocess='xml-stripblanks'^>ui/%%~nxf^</file^>>> $@
	@for %%s in (16 22 24 32 48) do @(for %%c in (actions status categories) do @(for %%f in (..\..\gtk\icons\%%sx%%s\%%c\*.png) do @echo     ^<file^>icons/%%sx%%s/%%c/%%~nxf^</file^>>> $@))
	@for %%s in (scalable) do @(for %%c in (status) do @(for %%f in (..\..\gtk\icons\%%s\%%c\*.svg) do @echo     ^<file^>icons/%%s/%%c/%%~nxf^</file^>>> $@))
	@for %%f in (..\..\gtk\inspector\*.ui) do @echo     ^<file compressed='true' preprocess='xml-stripblanks'^>inspector/%%~nxf^</file^>>> $@
	@echo     ^<file^>inspector/logo.png^</file^>>> $@
	@echo     ^<file^>emoji/emoji.data^</file^>>> $@
	@echo   ^</gresource^>>> $@
	@echo ^</gresources^>>> $@

..\..\gtk\gtkresources.h: ..\..\gtk\gtk.gresource.xml
	@echo Generating $@...
	@if not "$(XMLLINT)" == "" set XMLLINT=$(XMLLINT)
	@if not "$(JSON_GLIB_FORMAT)" == "" set JSON_GLIB_FORMAT=$(JSON_GLIB_FORMAT)
	@if not "$(GDK_PIXBUF_PIXDATA)" == "" set GDK_PIXBUF_PIXDATA=$(GDK_PIXBUF_PIXDATA)
	@$(GLIB_COMPILE_RESOURCES) $(GTK_RESOURCES_ARGS) --generate-header

..\..\gtk\gtkresources.c: ..\..\gtk\gtk.gresource.xml
	@echo Generating $@...
	@if not "$(XMLLINT)" == "" set XMLLINT=$(XMLLINT)
	@if not "$(JSON_GLIB_FORMAT)" == "" set JSON_GLIB_FORMAT=$(JSON_GLIB_FORMAT)
	@if not "$(GDK_PIXBUF_PIXDATA)" == "" set GDK_PIXBUF_PIXDATA=$(GDK_PIXBUF_PIXDATA)
	@$(GLIB_COMPILE_RESOURCES) $(GTK_RESOURCES_ARGS) --generate-source

..\..\gtk\gtkmarshalers.h: ..\..\gtk\gtkmarshalers.list
	@echo Generating $@...
	@$(PYTHON) $(GLIB_GENMARSHAL) $(GTK_MARSHALERS_FLAGS) --header $** > $@.tmp
	@move $@.tmp $@

..\..\gtk\gtkmarshalers.c: ..\..\gtk\gtkmarshalers.list
	@echo Generating $@...
	@echo #undef G_ENABLE_DEBUG> $@.tmp
	@$(PYTHON) $(GLIB_GENMARSHAL) $(GTK_MARSHALERS_FLAGS) --body $** >> $@.tmp
	@move $@.tmp $@

# Remove the generated files
clean:
	@-del /f /q ..\..\demos\gtk-demo\demos.h
	@-del /f /q ..\..\gtk\gtkresources.c
	@-del /f /q ..\..\gtk\gtkresources.h
	@-del /f /q ..\..\gtk\gtkmarshalers.c
	@-del /f /q ..\..\gtk\gtkmarshalers.h
	@-del /f /q ..\..\gtk\gtk.gresource.xml
	@-del /f /q ..\..\gtk\gtktypefuncs.inc
	@-del /f /q ..\..\gtk\gtkdbusgenerated.c
	@-del /f /q ..\..\gtk\gtkdbusgenerated.h
	@-del /f /q ..\..\gtk\libgtk3.manifest
	@-del /f /q ..\..\gtk\gtk-win32.rc
	@-del /f /q ..\..\gdk\gdkresources.c
	@-del /f /q ..\..\gdk\gdkresources.h
	@-del /f /q ..\..\gdk\gdk.gresource.xml
	@-del /f /q ..\..\gdk\gdkmarshalers.c
	@-del /f /q ..\..\gdk\gdkmarshalers.h
	@-del /f /q ..\..\gdk\gdkversionmacros.h
	@-del /f /q ..\..\gdk\gdkconfig.h
	@if exist ..\..\gdk-$(CFG)-$(GDK_CONFIG)-build del ..\..\gdk-$(CFG)-$(GDK_CONFIG)-build
	@if exist ..\..\gdk-$(GDK_OLD_CFG)-$(GDK_DEL_CONFIG)-build del ..\..\gdk-$(GDK_OLD_CFG)-$(GDK_DEL_CONFIG)-build
	@if exist ..\..\gdk-$(GDK_OLD_CFG)-$(GDK_CONFIG)-build del ..\..\gdk-$(GDK_OLD_CFG)-$(GDK_CONFIG)-build
	@if exist ..\..\gdk-$(CFG)-$(GDK_DEL_CONFIG)-build del ..\..\gdk-$(CFG)-$(GDK_DEL_CONFIG)-build
	@-del /f /q ..\..\config.h
