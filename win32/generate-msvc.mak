# NMake Makefile portion for code generation and
# intermediate build directory creation
# Items in here should not need to be edited unless
# one is maintaining the NMake build files.

!include ../demos/gtk-demo/demos-sources.mak
!include config-msvc.mak
!include create-lists-msvc.mak

# Copy the pre-defined gdkconfig.h.[win32|win32_broadway]
!if "$(CFG)" == "release" || "$(CFG)" == "Release"
GDK_OLD_CFG = debug
!else
GDK_OLD_CFG = release
!endif

!ifdef BROADWAY
GDK_CONFIG = broadway
GDK_DEL_CONFIG = win32
GDK_CONFIG_TEMPLATE = ..\gdk\gdkconfig.h.win32_broadway
!else
GDK_CONFIG = win32
GDK_DEL_CONFIG = broadway
GDK_CONFIG_TEMPLATE = ..\gdk\gdkconfig.h.win32
!endif

GDK_MARSHALERS_FLAGS = --prefix=_gdk_marshal --valist-marshallers
GDK_RESOURCES_ARGS = --target=$@ --sourcedir=..\gdk --c-name _gdk --manual-register
GTK_MARSHALERS_FLAGS = --prefix=_gtk_marshal --valist-marshallers
GTK_RESOURCES_ARGS = --target=$@ --sourcedir=..\gtk --c-name _gtk --manual-register

GDK_GENERATED_SOURCES =	\
	.\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gdk-3\gdk\gdkconfig.h	\
	.\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gdk-3\gdk\gdkenumtypes.h	\
	.\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gdk-3\gdk\gdkenumtypes.c	\
	.\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gdk-3\gdk\gdkmarshalers.h	\
	.\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gdk-3\gdk\gdkmarshalers.c	\
	.\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gdk-3\gdk\gdkresources.h	\
	.\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gdk-3\gdk\gdkresources.c	\
	.\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gdk-3\gdk\gdkversionmacros.h

GTK_TYPEBUILTIN_SOURCES =	\
	.\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gtk-3\gtk\gtktypebuiltins.h	\
	.\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gtk-3\gtk\gtktypebuiltins.c

GTK_GENERATED_SOURCES =	\
	.\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gtk-3\gtk\gtktypefuncs.inc	\
	.\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gtk-3\gtk\gtkprivatetypebuiltins.h	\
	.\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gtk-3\gtk\gtkprivatetypebuiltins.c	\
	$(GTK_TYPEBUILTIN_SOURCES)	\
	.\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gtk-3\gtk\gtkdbusgenerated.h	\
	.\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gtk-3\gtk\gtkdbusgenerated.c	\
	.\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gtk-3\gtk\gtkmarshalers.h	\
	.\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gtk-3\gtk\gtkmarshalers.c	\
	.\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gtk-3\gtk\gtkresources.h	\
	.\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gtk-3\gtk\gtkresources.c

generate-base-sources:	\
	.\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gdk-3\config.h	\
	$(GDK_GENERATED_SOURCES)	\
	$(GTK_GENERATED_SOURCES)	\
	.\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gtk-3\gtk\gtk-win32.rc	\
	.\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gtk-3\gtk\libgtk3.manifest	\
	.\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gtk-3\gtk\gtk.gresource.xml	\
	..\demos\gtk-demo\demos.h	\
	..\demos\gtk-demo\demo_resources.c	\
	..\demos\icon-browser\resources.c

# Copy the pre-defined config.h.win32 and demos.h.win32
.\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gdk-3\config.h: ..\config.h.win32
..\demos\gtk-demo\demos.h: ..\demos\gtk-demo\demos.h.win32
.\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gtk-3\gtk\gtk-win32.rc: ..\gtk\gtk-win32.rc.body

..\gdk-$(CFG)-$(GDK_CONFIG)-build: $(GDK_CONFIG_TEMPLATE)
	@if exist ..\gdk-$(GDK_OLD_CFG)-$(GDK_DEL_CONFIG)-build del ..\gdk-$(GDK_OLD_CFG)-$(GDK_DEL_CONFIG)-build
	@if exist ..\gdk-$(GDK_OLD_CFG)-$(GDK_CONFIG)-build del ..\gdk-$(GDK_OLD_CFG)-$(GDK_CONFIG)-build
	@if exist ..\gdk-$(CFG)-$(GDK_DEL_CONFIG)-build del ..\gdk-$(CFG)-$(GDK_DEL_CONFIG)-build
	@copy $** $@
	
.\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gdk-3\gdk\gdkconfig.h: ..\gdk-$(CFG)-$(GDK_CONFIG)-build

.\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gdk-3\config.h	\
.\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gdk-3\gdk\gdkconfig.h	\
.\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gtk-3\gtk\gtk-win32.rc	\
..\demos\gtk-demo\demos.h:
	@echo Copying $@...
	@if not exist $(@D)\ md $(@D)
	@copy $** $@

.\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gdk-3\gdk\gdkenumtypes.c: ..\gdk\gdkenumtypes.c.template $(GDK_PUBLIC_HEADERS)
.\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gdk-3\gdk\gdkenumtypes.h: ..\gdk\gdkenumtypes.h.template $(GDK_PUBLIC_HEADERS)
.\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gtk-3\gtk\gtktypebuiltins.c: ..\gtk\gtktypebuiltins.c.template $(GTK_PUBLIC_ENUM_HEADERS)
.\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gtk-3\gtk\gtktypebuiltins.h: ..\gtk\gtktypebuiltins.h.template $(GTK_PUBLIC_ENUM_HEADERS)
.\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gtk-3\gtk\gtkprivatetypebuiltins.c: ..\gtk\gtkprivatetypebuiltins.c.template $(GTK_PRIVATE_ENUM_HEADERS)
.\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gtk-3\gtk\gtkprivatetypebuiltins.h: ..\gtk\gtkprivatetypebuiltins.h.template $(GTK_PRIVATE_ENUM_HEADERS)

.\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gdk-3\gdk\gdkenumtypes.c	\
.\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gdk-3\gdk\gdkenumtypes.h:
	@echo Generating $@...
	@if not exist $(@D)\ md $(@D)
	@cd ..\gdk
	@$(PYTHON) $(GLIB_MKENUMS) --template $(@F).template $(gdk_public_h_sources) $(gdk_deprecated_h_sources) > ..\win32\$@
	@cd ..\win32

.\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gtk-3\gtk\gtktypebuiltins.h	\
.\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gtk-3\gtk\gtktypebuiltins.c:
	@echo Generating $@...
	@if not exist $(@D)\ md $(@D)
	@cd ..\gtk
	@$(PYTHON) $(GLIB_MKENUMS) --template $(@F).template $(GTK_PUB_HDRS) $(a11y_h_sources) $(gtk_deprecated_h_sources) > ..\win32\$@
	@cd ..\win32

.\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gtk-3\gtk\gtkprivatetypebuiltins.c	\
.\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gtk-3\gtk\gtkprivatetypebuiltins.h:
	@echo Generating $@...
	@if not exist $(@D)\ md $(@D)
	@cd ..\gtk
	@$(PYTHON) $(GLIB_MKENUMS) --template $(@F).template $(GTK_PRIVATE_TYPE_HDRS) > ..\win32\$@
	@cd ..\win32

.\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gdk-3\gdk\gdkversionmacros.h: ..\gdk\gdkversionmacros.h.in
	@echo Generating $@...
	@if not exist $(@D)\ md $(@D)
	@$(PYTHON) gen-version-items.py --version=$(GTK_VERSION) --interface-age=$(GTK_INTERFACE_AGE) --source=$** --output=$@

.\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gdk-3\gdk\gdkmarshalers.h: ..\gdk\gdkmarshalers.list
	@echo Generating $@...
	@if not exist $(@D)\ md $(@D)
	@$(PYTHON) $(GLIB_GENMARSHAL) $(GDK_MARSHALERS_FLAGS) --header $** > $@.tmp
	@move $@.tmp $@

.\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gdk-3\gdk\gdkmarshalers.c: ..\gdk\gdkmarshalers.list
	@echo Generating $@...
	@if not exist $(@D)\ md $(@D)
	@$(PYTHON) $(GLIB_GENMARSHAL) $(GDK_MARSHALERS_FLAGS) --body $** > $@.tmp
	@move $@.tmp $@

.\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gdk-3\gdk\gdk.gresource.xml: $(GDK_RESOURCES)
	@echo Generating $@...
	@if not exist $(@D)\ md $(@D)
	@echo ^<?xml version='1.0' encoding='UTF-8'?^> >$@
	@echo ^<gresources^> >> $@
	@echo  ^<gresource prefix='/org/gtk/libgdk'^> >> $@
	@for %%f in (..\gdk\resources\glsl\*.glsl) do @echo     ^<file alias='glsl/%%~nxf'^>resources/glsl/%%~nxf^</file^> >> $@
	@echo   ^</gresource^> >> $@
	@echo ^</gresources^> >> $@

.\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gdk-3\gdk\gdkresources.h: .\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gdk-3\gdk\gdk.gresource.xml
	@echo Generating $@...
	@if not "$(XMLLINT)" == "" set XMLLINT=$(XMLLINT)
	@if not "$(JSON_GLIB_FORMAT)" == "" set JSON_GLIB_FORMAT=$(JSON_GLIB_FORMAT)
	@if not "$(GDK_PIXBUF_PIXDATA)" == "" set GDK_PIXBUF_PIXDATA=$(GDK_PIXBUF_PIXDATA)
	@start /min $(GLIB_COMPILE_RESOURCES) $** $(GDK_RESOURCES_ARGS) --generate-header

.\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gdk-3\gdk\gdkresources.c: .\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gdk-3\gdk\gdk.gresource.xml
	@echo Generating $@...
	@if not "$(XMLLINT)" == "" set XMLLINT=$(XMLLINT)
	@if not "$(JSON_GLIB_FORMAT)" == "" set JSON_GLIB_FORMAT=$(JSON_GLIB_FORMAT)
	@if not "$(GDK_PIXBUF_PIXDATA)" == "" set GDK_PIXBUF_PIXDATA=$(GDK_PIXBUF_PIXDATA)
	@start /min $(GLIB_COMPILE_RESOURCES) $** $(GDK_RESOURCES_ARGS) --generate-source

.\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gtk-3\gtk\libgtk3.manifest: ..\gtk\libgtk3.manifest.in
	@echo Generating $@...
	@if not exist $(@D)\ md $(@D)
	@$(PYTHON) replace.py	\
	--action=replace-var	\
	--input=$**	--output=$@	\
	--var=EXE_MANIFEST_ARCHITECTURE	\
	--outstring=*

.\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gtk-3\gtk\gtkdbusgenerated.h: ..\gtk\gtkdbusinterfaces.xml
	@echo Generating GTK DBus sources...
	@if not exist $(@D)\ md $(@D)
	@$(PYTHON) $(GDBUS_CODEGEN)	\
	--interface-prefix org.Gtk. --c-namespace _Gtk	\
	--generate-c-code gtkdbusgenerated $**	\
	--output-directory $(@D)

.\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gtk-3\gtk\gtkdbusgenerated.c: .\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gtk-3\gtk\gtkdbusgenerated.h

.\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gtk-3\gtk\gtktypefuncs.inc: ..\gtk\gentypefuncs.py
	@echo Generating $@...
	@if not exist $(@D)\ md $(@D)
	@if not exist $(@D)\gtktypebuiltins.h $(MAKE) /f generate-msvc.mak CFG=$(CFG) $(@D)\gtktypebuiltins.h
	@if not exist $(@D)\gtkversion.h $(MAKE) /f generate-msvc.mak CFG=$(CFG) $(@D)\gtkversion.h
	@echo #undef GTK_COMPILATION > $(@R).preproc.c
	@echo #include "gtkx.h" >> $(@R).preproc.c
	@cl /EP $(GTK_PREPROCESSOR_FLAGS) $(@R).preproc.c > $(@R).combined.c
	@$(PYTHON) $** $@ $(@R).combined.c
	@del $(@R).preproc.c $(@R).combined.c

.\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gtk-3\gtk\gtk.gresource.xml: $(GTK_RESOURCES)
	@echo Generating $@...
	@echo ^<?xml version='1.0' encoding='UTF-8'?^>> $@
	@echo ^<gresources^>>> $@
	@echo   ^<gresource prefix='/org/gtk/libgtk'^>>> $@
	@echo     ^<file^>theme/Adwaita/gtk.css^</file^>>> $@
	@echo     ^<file^>theme/Adwaita/gtk-dark.css^</file^>>> $@
	@echo     ^<file^>theme/Adwaita/gtk-contained.css^</file^>>> $@
	@echo     ^<file^>theme/Adwaita/gtk-contained-dark.css^</file^>>> $@
	@for %%f in (..\gtk\theme\Adwaita\assets\*.png) do @echo     ^<file preprocess='to-pixdata'^>theme/Adwaita/assets/%%~nxf^</file^>>> $@
	@for %%f in (..\gtk\theme\Adwaita\assets\*.svg) do @echo     ^<file^>theme/Adwaita/assets/%%~nxf^</file^>>> $@
	@echo     ^<file^>theme/HighContrast/gtk.css^</file^>>> $@
	@echo     ^<file alias='theme/HighContrastInverse/gtk.css'^>theme/HighContrast/gtk-inverse.css^</file^>>> $@
	@echo     ^<file^>theme/HighContrast/gtk-contained.css^</file^>>> $@
	@echo     ^<file^>theme/HighContrast/gtk-contained-inverse.css^</file^>>> $@
	@for %%f in (..\gtk\theme\HighContrast\assets\*.png) do @echo     ^<file preprocess='to-pixdata'^>theme/HighContrast/assets/%%~nxf^</file^>>> $@
	@for %%f in (..\gtk\theme\HighContrast\assets\*.svg) do @echo     ^<file^>theme/HighContrast/assets/%%~nxf^</file^>>> $@
	@echo     ^<file^>theme/win32/gtk-win32-base.css^</file^>>> $@
	@echo     ^<file^>theme/win32/gtk.css^</file^>>> $@
	@for %%f in (..\gtk\cursor\*.png) do @echo     ^<file^>cursor/%%~nxf^</file^>>> $@
	@for %%f in (..\gtk\gesture\*.symbolic.png) do @echo     ^<file alias='icons/64x64/actions/%%~nxf'^>gesture/%%~nxf^</file^>>> $@
	@for %%f in (..\gtk\ui\*.ui) do @echo     ^<file preprocess='xml-stripblanks'^>ui/%%~nxf^</file^>>> $@
	@for %%s in (16 22 24 32 48) do @(for %%c in (actions status categories) do @(for %%f in (..\gtk\icons\%%sx%%s\%%c\*.png) do @echo     ^<file^>icons/%%sx%%s/%%c/%%~nxf^</file^>>> $@))
	@for %%s in (scalable) do @(for %%c in (status) do @(for %%f in (..\gtk\icons\%%s\%%c\*.svg) do @echo     ^<file^>icons/%%s/%%c/%%~nxf^</file^>>> $@))
	@for %%f in (..\gtk\inspector\*.ui) do @echo     ^<file compressed='true' preprocess='xml-stripblanks'^>inspector/%%~nxf^</file^>>> $@
	@echo     ^<file^>inspector/logo.png^</file^>>> $@
	@for %%f in (..\gtk\emoji\*.data) do @echo     ^<file^>emoji/%%~nxf^</file^>>> $@
	@echo   ^</gresource^>>> $@
	@echo ^</gresources^>>> $@

.\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gtk-3\gtk\gtkresources.h: .\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gtk-3\gtk\gtk.gresource.xml
	@echo Generating $@...
	@if not "$(XMLLINT)" == "" set XMLLINT=$(XMLLINT)
	@if not "$(JSON_GLIB_FORMAT)" == "" set JSON_GLIB_FORMAT=$(JSON_GLIB_FORMAT)
	@if not "$(GDK_PIXBUF_PIXDATA)" == "" set GDK_PIXBUF_PIXDATA=$(GDK_PIXBUF_PIXDATA)
	@start /min $(GLIB_COMPILE_RESOURCES) $(GTK_RESOURCES_ARGS) $** --generate-header

.\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gtk-3\gtk\gtkresources.c: .\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gtk-3\gtk\gtk.gresource.xml
	@echo Generating $@...
	@if not "$(XMLLINT)" == "" set XMLLINT=$(XMLLINT)
	@if not "$(JSON_GLIB_FORMAT)" == "" set JSON_GLIB_FORMAT=$(JSON_GLIB_FORMAT)
	@if not "$(GDK_PIXBUF_PIXDATA)" == "" set GDK_PIXBUF_PIXDATA=$(GDK_PIXBUF_PIXDATA)
	@start /min $(GLIB_COMPILE_RESOURCES) $(GTK_RESOURCES_ARGS) $** --generate-source

.\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gtk-3\gtk\gtkmarshalers.h: ..\gtk\gtkmarshalers.list
	@echo Generating $@...
	@$(PYTHON) $(GLIB_GENMARSHAL) $(GTK_MARSHALERS_FLAGS) --header $** > $@.tmp
	@move $@.tmp $@

.\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gtk-3\gtk\gtkmarshalers.c: ..\gtk\gtkmarshalers.list
	@echo Generating $@...
	@echo #undef G_ENABLE_DEBUG> $@.tmp
	@$(PYTHON) $(GLIB_GENMARSHAL) $(GTK_MARSHALERS_FLAGS) --body $** >> $@.tmp
	@move $@.tmp $@

..\demos\gtk-demo\demo_resources.c: ..\demos\gtk-demo\demo.gresource.xml $(GTK_DEMO_RESOURCES)
	@echo Generating $@...
	@$(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(@D) --generate-source $(@D)\demo.gresource.xml

..\demos\icon-browser\resources.c: ..\demos\icon-browser\iconbrowser.gresource.xml $(ICON_BROWSER_RESOURCES)
	@echo Generating $@...
	@$(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(@D) --generate-source $(@D)\iconbrowser.gresource.xml

regenerate-demos-h-win32: ..\demos\gtk-demo\geninclude.py $(demo_actual_sources)
	@echo Regenerating demos.h.win32...
	@-del ..\demos\gtk-demo\demos.h.win32
	@cd ..\demos\gtk-demo
	@$(PYTHON) geninclude.py demos.h.win32 $(demo_sources)

Gdk_3_0_gir_list_final: Gdk_3_0_gir_list $(GDK_GENERATED_SOURCES)
	@echo Generating $@...
	@type Gdk_3_0_gir_list>$@
	@for %%s in ($(GDK_GENERATED_SOURCES)) do echo %%s>>$@

Gtk_3_0_gir_list_final: Gtk_3_0_gir_list $(GTK_TYPEBUILTIN_SOURCES)
	@echo Generating $@...
	@type Gtk_3_0_gir_list>$@
	@for %%s in ($(GTK_TYPEBUILTIN_SOURCES)) do echo %%s>>$@

GdkWin32_3_0_gir_list_final: GdkWin32_3_0_gir_list
	@echo Copying $@...
	@copy $** $@

# Remove the generated files
clean:
	@-del /f /q ..\demos\icon-browser\resources.c
	@-del /f /q ..\demos\gtk-demo\demo_resources.c
	@-del /f /q ..\demos\gtk-demo\demos.h
	@-del /f /q .\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gtk-3\gtk\gtktypebuiltins.c
	@-del /f /q .\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gtk-3\gtk\gtktypebuiltins.h
	@-del /f /q .\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gtk-3\gtk\gtkprivatetypebuiltins.c
	@-del /f /q .\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gtk-3\gtk\gtkprivatetypebuiltins.h
	@-del /f /q .\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gtk-3\gtk\gtkresources.c
	@-del /f /q .\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gtk-3\gtk\gtkresources.h
	@-del /f /q .\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gtk-3\gtk\gtkmarshalers.c
	@-del /f /q .\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gtk-3\gtk\gtkmarshalers.h
	@-del /f /q .\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gtk-3\gtk\gtk.gresource.xml
	@-del /f /q .\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gtk-3\gtk\gtktypefuncs.inc
	@-del /f /q .\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gtk-3\gtk\gtkdbusgenerated.c
	@-del /f /q .\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gtk-3\gtk\gtkdbusgenerated.h
	@-del /f /q .\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gtk-3\gtk\libgtk3.manifest
	@-del /f /q .\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gtk-3\gtk\gtk-win32.rc
	@-rd .\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gtk-3\gtk
	@-del /f /q .\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gdk-3\gdk\gdkenumtypes.c
	@-del /f /q .\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gdk-3\gdk\gdkenumtypes.h
	@-del /f /q .\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gdk-3\gdk\gdkresources.c
	@-del /f /q .\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gdk-3\gdk\gdkresources.h
	@-del /f /q .\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gdk-3\gdk\gdk.gresource.xml
	@-del /f /q .\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gdk-3\gdk\gdkmarshalers.c
	@-del /f /q .\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gdk-3\gdk\gdkmarshalers.h
	@-del /f /q .\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gdk-3\gdk\gdkversionmacros.h
	@-del /f /q .\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gdk-3\gdk\gdkconfig.h
	@if exist ..\gdk-$(CFG)-$(GDK_CONFIG)-build del ..\gdk-$(CFG)-$(GDK_CONFIG)-build
	@if exist ..\gdk-$(GDK_OLD_CFG)-$(GDK_DEL_CONFIG)-build del ..\gdk-$(GDK_OLD_CFG)-$(GDK_DEL_CONFIG)-build
	@if exist ..\gdk-$(GDK_OLD_CFG)-$(GDK_CONFIG)-build del ..\gdk-$(GDK_OLD_CFG)-$(GDK_CONFIG)-build
	@if exist ..\gdk-$(CFG)-$(GDK_DEL_CONFIG)-build del ..\gdk-$(CFG)-$(GDK_DEL_CONFIG)-build
	@-del /f /q .\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gdk-3\config.h
	@-rd .\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gdk-3\gdk
