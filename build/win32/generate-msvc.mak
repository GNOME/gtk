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

all:	\
	..\..\config.h	\
	..\..\gdk\gdkconfig.h	\
	..\..\gdk\gdkversionmacros.h	\
	..\..\gdk\gdkmarshalers.h	\
	..\..\gdk\gdkmarshalers.c	\
	..\..\gdk\gdkresources.h	\
	..\..\gdk\gdkresources.c	\
	..\..\demos\gtk-demo\demos.h

# Copy the pre-defined config.h.win32 and demos.h.win32
..\..\config.h: ..\..\config.h.win32
..\..\demos\gtk-demo\demos.h: ..\..\demos\gtk-demo\demos.h.win32

..\..\gdk-$(CFG)-$(GDK_CONFIG)-build: $(GDK_CONFIG_TEMPLATE)
	@if exist ..\..\gdk-$(GDK_OLD_CFG)-$(GDK_DEL_CONFIG)-build del ..\..\gdk-$(GDK_OLD_CFG)-$(GDK_DEL_CONFIG)-build
	@if exist ..\..\gdk-$(GDK_OLD_CFG)-$(GDK_CONFIG)-build del ..\..\gdk-$(GDK_OLD_CFG)-$(GDK_CONFIG)-build
	@if exist ..\..\gdk-$(CFG)-$(GDK_DEL_CONFIG)-build del ..\..\gdk-$(CFG)-$(GDK_DEL_CONFIG)-build
	@copy $** $@
	
..\..\gdk\gdkconfig.h: ..\..\gdk-$(CFG)-$(GDK_CONFIG)-build

..\..\config.h	\
..\..\gdk\gdkconfig.h	\
..\..\demos\gtk-demo\demos.h:
	@copy $** $@

..\..\gdk\gdkversionmacros.h: ..\..\gdk\gdkversionmacros.h.in
	$(PYTHON) gen-gdkversionmacros-h.py --version=$(GTK_VERSION)

..\..\gdk\gdkmarshalers.h: ..\..\gdk\gdkmarshalers.list
	$(PYTHON) $(GLIB_GENMARSHAL) $(GDK_MARSHALERS_FLAGS) --header $** > $@.tmp
	@move $@.tmp $@

..\..\gdk\gdkmarshalers.c: ..\..\gdk\gdkmarshalers.list
	$(PYTHON) $(GLIB_GENMARSHAL) $(GDK_MARSHALERS_FLAGS) --body $** > $@.tmp
	@move $@.tmp $@

..\..\gdk\gdk.gresource.xml:
	@echo ^<?xml version='1.0' encoding='UTF-8'?^> >$@
	@echo ^<gresources^> >> $@
	@echo  ^<gresource prefix='/org/gtk/libgdk'^> >> $@
	@for %%f in (..\..\gdk\resources\glsl\*.glsl) do @echo     ^<file alias='glsl/%%~nxf'^>resources/glsl/%%~nxf^</file^> >> $@
	@echo   ^</gresource^> >> $@
	@echo ^</gresources^> >> $@

..\..\gdk\gdkresources.h: ..\..\gdk\gdk.gresource.xml
	$(GLIB_COMPILE_RESOURCES) $(GDK_RESOURCES_ARGS) --generate-header

..\..\gdk\gdkresources.c: ..\..\gdk\gdk.gresource.xml $(GDK_RESOURCES)
	$(GLIB_COMPILE_RESOURCES) $(GDK_RESOURCES_ARGS) --generate-source

# Remove the generated files
clean:
	@-del /f /q ..\..\demos\gtk-demo\demos.h
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
