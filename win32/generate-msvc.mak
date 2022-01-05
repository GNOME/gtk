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
GDK_RESOURCES_ARGS = ..\gdk\gdk.gresource.xml --target=$@ --sourcedir=..\gdk --c-name _gdk --manual-register
GTK_MARSHALERS_FLAGS = --prefix=_gtk_marshal --valist-marshallers
GTK_RESOURCES_ARGS = ..\gtk\gtk.gresource.xml --target=$@ --sourcedir=..\gtk --c-name _gtk --manual-register

all:	\
	..\config.h	\
	..\gdk\gdkconfig.h	\
	..\gdk\gdkversionmacros.h	\
	..\gdk\gdkmarshalers.h	\
	..\gdk\gdkmarshalers.c	\
	..\gdk\gdkresources.h	\
	..\gdk\gdkresources.c	\
	..\gtk\gtk-win32.rc	\
	..\gtk\libgtk3.manifest	\
	..\gtk\gtkdbusgenerated.h	\
	..\gtk\gtkdbusgenerated.c	\
	..\gtk\gtktypefuncs.inc	\
	..\gtk\gtk.gresource.xml	\
	..\gtk\gtkmarshalers.h	\
	..\gtk\gtkmarshalers.c	\
	..\gtk\gtkresources.h	\
	..\gtk\gtkresources.c	\
	..\demos\gtk-demo\demos.h	\
	..\demos\gtk-demo\demo_resources.c	\
	..\demos\icon-browser\resources.c

# Copy the pre-defined config.h.win32 and demos.h.win32
..\config.h: ..\config.h.win32
..\demos\gtk-demo\demos.h: ..\demos\gtk-demo\demos.h.win32
..\gtk\gtk-win32.rc: ..\gtk\gtk-win32.rc.body

..\gdk-$(CFG)-$(GDK_CONFIG)-build: $(GDK_CONFIG_TEMPLATE)
	@if exist ..\gdk-$(GDK_OLD_CFG)-$(GDK_DEL_CONFIG)-build del ..\gdk-$(GDK_OLD_CFG)-$(GDK_DEL_CONFIG)-build
	@if exist ..\gdk-$(GDK_OLD_CFG)-$(GDK_CONFIG)-build del ..\gdk-$(GDK_OLD_CFG)-$(GDK_CONFIG)-build
	@if exist ..\gdk-$(CFG)-$(GDK_DEL_CONFIG)-build del ..\gdk-$(CFG)-$(GDK_DEL_CONFIG)-build
	@copy $** $@
	
..\gdk\gdkconfig.h: ..\gdk-$(CFG)-$(GDK_CONFIG)-build

..\config.h	\
..\gdk\gdkconfig.h	\
..\gtk\gtk-win32.rc	\
..\demos\gtk-demo\demos.h:
	@echo Copying $@...
	@copy $** $@

..\gdk\gdkversionmacros.h: ..\gdk\gdkversionmacros.h.in
	@echo Generating $@...
	@$(PYTHON) gen-gdkversionmacros-h.py --version=$(GTK_VERSION)

..\gdk\gdkmarshalers.h: ..\gdk\gdkmarshalers.list
	@echo Generating $@...
	@$(PYTHON) $(GLIB_GENMARSHAL) $(GDK_MARSHALERS_FLAGS) --header $** > $@.tmp
	@move $@.tmp $@

..\gdk\gdkmarshalers.c: ..\gdk\gdkmarshalers.list
	@echo Generating $@...
	@$(PYTHON) $(GLIB_GENMARSHAL) $(GDK_MARSHALERS_FLAGS) --body $** > $@.tmp
	@move $@.tmp $@

..\gdk\gdk.gresource.xml: $(GDK_RESOURCES)
	@echo Generating $@...
	@echo ^<?xml version='1.0' encoding='UTF-8'?^> >$@
	@echo ^<gresources^> >> $@
	@echo  ^<gresource prefix='/org/gtk/libgdk'^> >> $@
	@for %%f in (..\gdk\resources\glsl\*.glsl) do @echo     ^<file alias='glsl/%%~nxf'^>resources/glsl/%%~nxf^</file^> >> $@
	@echo   ^</gresource^> >> $@
	@echo ^</gresources^> >> $@

..\gdk\gdkresources.h: ..\gdk\gdk.gresource.xml
	@echo Generating $@...
	@if not "$(XMLLINT)" == "" set XMLLINT=$(XMLLINT)
	@if not "$(JSON_GLIB_FORMAT)" == "" set JSON_GLIB_FORMAT=$(JSON_GLIB_FORMAT)
	@if not "$(GDK_PIXBUF_PIXDATA)" == "" set GDK_PIXBUF_PIXDATA=$(GDK_PIXBUF_PIXDATA)
	@start /min $(GLIB_COMPILE_RESOURCES) $(GDK_RESOURCES_ARGS) --generate-header

..\gdk\gdkresources.c: ..\gdk\gdk.gresource.xml $(GDK_RESOURCES)
	@echo Generating $@...
	@if not "$(XMLLINT)" == "" set XMLLINT=$(XMLLINT)
	@if not "$(JSON_GLIB_FORMAT)" == "" set JSON_GLIB_FORMAT=$(JSON_GLIB_FORMAT)
	@if not "$(GDK_PIXBUF_PIXDATA)" == "" set GDK_PIXBUF_PIXDATA=$(GDK_PIXBUF_PIXDATA)
	@start /min $(GLIB_COMPILE_RESOURCES) $(GDK_RESOURCES_ARGS) --generate-source

..\gtk\libgtk3.manifest: ..\gtk\libgtk3.manifest.in
	@echo Generating $@...
	@$(PYTHON) replace.py	\
	--action=replace-var	\
	--input=$**	--output=$@	\
	--var=EXE_MANIFEST_ARCHITECTURE	\
	--outstring=*

..\gtk\gtkdbusgenerated.h ..\gtk\gtkdbusgenerated.c: ..\gtk\gtkdbusinterfaces.xml
	@echo Generating GTK DBus sources...
	@$(PYTHON) $(GDBUS_CODEGEN)	\
	--interface-prefix org.Gtk. --c-namespace _Gtk	\
	--generate-c-code gtkdbusgenerated $**	\
	--output-directory $(@D)

..\gtk\gtktypefuncs.inc: ..\gtk\gentypefuncs.py
	@echo Generating $@...
	@echo #undef GTK_COMPILATION > $(@R).preproc.c
	@echo #include "gtkx.h" >> $(@R).preproc.c
	@cl /EP $(GTK_PREPROCESSOR_FLAGS) $(@R).preproc.c > $(@R).combined.c
	@$(PYTHON) $** $@ $(@R).combined.c
	@del $(@R).preproc.c $(@R).combined.c

..\gtk\gtk.gresource.xml: $(GTK_RESOURCES)
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

..\gtk\gtkresources.h: ..\gtk\gtk.gresource.xml
	@echo Generating $@...
	@if not "$(XMLLINT)" == "" set XMLLINT=$(XMLLINT)
	@if not "$(JSON_GLIB_FORMAT)" == "" set JSON_GLIB_FORMAT=$(JSON_GLIB_FORMAT)
	@if not "$(GDK_PIXBUF_PIXDATA)" == "" set GDK_PIXBUF_PIXDATA=$(GDK_PIXBUF_PIXDATA)
	@start /min $(GLIB_COMPILE_RESOURCES) $(GTK_RESOURCES_ARGS) --generate-header

..\gtk\gtkresources.c: ..\gtk\gtk.gresource.xml $(GTK_RESOURCES)
	@echo Generating $@...
	@if not "$(XMLLINT)" == "" set XMLLINT=$(XMLLINT)
	@if not "$(JSON_GLIB_FORMAT)" == "" set JSON_GLIB_FORMAT=$(JSON_GLIB_FORMAT)
	@if not "$(GDK_PIXBUF_PIXDATA)" == "" set GDK_PIXBUF_PIXDATA=$(GDK_PIXBUF_PIXDATA)
	@start /min $(GLIB_COMPILE_RESOURCES) $(GTK_RESOURCES_ARGS) --generate-source

..\gtk\gtkmarshalers.h: ..\gtk\gtkmarshalers.list
	@echo Generating $@...
	@$(PYTHON) $(GLIB_GENMARSHAL) $(GTK_MARSHALERS_FLAGS) --header $** > $@.tmp
	@move $@.tmp $@

..\gtk\gtkmarshalers.c: ..\gtk\gtkmarshalers.list
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
	@cd ..\..\win32
	@echo Regenerating gtk3-demo VS project files...
	@-del vs9\$(DEMO_VS9_PROJ) vs10\$(DEMO_VS10_PROJ) vs10\$(DEMO_VS10_PROJ_FILTERS)
	@for %%s in ($(demo_sources) gtkfishbowl.c demo_resources.c main.c) do	\
	@echo.   ^<File RelativePath^="..\..\demos\gtk-demo\%%s" /^>>>gtk3-demo.sourcefiles & \
	@echo.   ^<ClCompile Include^="..\..\demos\gtk-demo\%%s" /^>>>gtk3-demo.vs10.sourcefiles & \
	@echo.   ^<ClCompile Include^="..\..\demos\gtk-demo\%%s"^>^<Filter^>Source Files^</Filter^>^</ClCompile^>>>gtk3-demo.vs10.sourcefiles.filters
	@$(CPP) /nologo /EP /I. vs9\$(DEMO_VS9_PROJ)in>$(DEMO_VS9_PROJ).tmp
	@for /f "usebackq tokens=* delims=" %%l in ($(DEMO_VS9_PROJ).tmp) do @echo %%l>>$(DEMO_VS9_PROJ).tmp1
	@$(CPP) /nologo /EP /I. vs10\$(DEMO_VS10_PROJ)in>$(DEMO_VS10_PROJ).tmp
	@for /f "usebackq tokens=* delims=" %%l in ($(DEMO_VS10_PROJ).tmp) do @echo %%l>>$(DEMO_VS10_PROJ).tmp1
	@$(CPP) /nologo /EP /I. vs10\$(DEMO_VS10_PROJ_FILTERS)in> $(DEMO_VS10_PROJ_FILTERS).tmp
	@for /f "usebackq tokens=* delims=" %%l in ($(DEMO_VS10_PROJ_FILTERS).tmp) do @ echo %%l>>vs10\$(DEMO_VS10_PROJ_FILTERS)
	@if not "$(FONT_FEATURES_DEMO)" == ""	\
	 if not "$(FONT_FEATURES_USE_PANGOFT2)" == ""	\
	 ($(PYTHON) replace.py -a=replace-str -i=$(DEMO_VS9_PROJ).tmp1 -o=vs9\$(DEMO_VS9_PROJ) --instring="AdditionalDependencies=\"\"" --outstring="AdditionalDependencies=\"$(DEMO_DEP_LIBS_PANGOFT2_VS9)\"") & \
	 ($(PYTHON) replace.py -a=replace-str -i=$(DEMO_VS10_PROJ).tmp1 -o=vs10\$(DEMO_VS10_PROJ) --instring=">%(AdditionalDependencies)<" --outstring=">$(DEMO_DEP_LIBS_PANGOFT2_VS1X);%(AdditionalDependencies)<")
	@if not "$(FONT_FEATURES_DEMO)" == ""	\
	 if "$(FONT_FEATURES_USE_PANGOFT2)" == ""	\
	 ($(PYTHON) replace.py -a=replace-str -i=$(DEMO_VS9_PROJ).tmp1 -o=vs9\$(DEMO_VS9_PROJ) --instring="AdditionalDependencies=\"\"" --outstring="AdditionalDependencies=\"$(DEMO_DEP_LIBS_NEW_PANGO)\"") & \
	 ($(PYTHON) replace.py -a=replace-str -i=$(DEMO_VS10_PROJ).tmp1 -o=vs10\$(DEMO_VS10_PROJ) --instring=">%(AdditionalDependencies)<" --outstring=">$(DEMO_DEP_LIBS_NEW_PANGO);%(AdditionalDependencies)<")
	@if "$(FONT_FEATURES_DEMO)" == "" copy $(DEMO_VS9_PROJ).tmp1 vs9\$(DEMO_VS9_PROJ) & copy $(DEMO_VS10_PROJ).tmp1 vs10\$(DEMO_VS10_PROJ)
	@del *vc*proj*.tmp* gtk3-demo.*sourcefiles*
	@for %%v in (11 12 14 15 16 17) do @(copy /y vs10\$(DEMO_VS10_PROJ_FILTERS) vs%v\ & del vs%v\gtk3-demo.vcxproj)
	@$(PYTHON) replace.py -a=replace-str -i=vs10\$(DEMO_VS10_PROJ) -o=vs11\$(DEMO_VS10_PROJ) --instring=">v100<" --outstring=">v110<"
	@$(PYTHON) replace.py -a=replace-str -i=vs10\$(DEMO_VS10_PROJ) -o=vs12\$(DEMO_VS10_PROJ) --instring=">v100<" --outstring=">v120<"
	@$(PYTHON) replace.py -a=replace-str -i=vs10\$(DEMO_VS10_PROJ) -o=vs14\$(DEMO_VS10_PROJ) --instring=">v100<" --outstring=">v140<"
	@$(PYTHON) replace.py -a=replace-str -i=vs10\$(DEMO_VS10_PROJ) -o=vs15\$(DEMO_VS10_PROJ) --instring=">v100<" --outstring=">v141<"
	@$(PYTHON) replace.py -a=replace-str -i=vs10\$(DEMO_VS10_PROJ) -o=vs16\$(DEMO_VS10_PROJ) --instring=">v100<" --outstring=">v142<"
	@$(PYTHON) replace.py -a=replace-str -i=vs10\$(DEMO_VS10_PROJ) -o=vs17\$(DEMO_VS10_PROJ) --instring=">v100<" --outstring=">v143<"

# Remove the generated files
clean:
	@-del /f /q ..\demos\icon-browser\resources.c
	@-del /f /q ..\demos\gtk-demo\demo_resources.c
	@-del /f /q ..\demos\gtk-demo\demos.h
	@-del /f /q ..\gtk\gtkresources.c
	@-del /f /q ..\gtk\gtkresources.h
	@-del /f /q ..\gtk\gtkmarshalers.c
	@-del /f /q ..\gtk\gtkmarshalers.h
	@-del /f /q ..\gtk\gtk.gresource.xml
	@-del /f /q ..\gtk\gtktypefuncs.inc
	@-del /f /q ..\gtk\gtkdbusgenerated.c
	@-del /f /q ..\gtk\gtkdbusgenerated.h
	@-del /f /q ..\gtk\libgtk3.manifest
	@-del /f /q ..\gtk\gtk-win32.rc
	@-del /f /q ..\gdk\gdkresources.c
	@-del /f /q ..\gdk\gdkresources.h
	@-del /f /q ..\gdk\gdk.gresource.xml
	@-del /f /q ..\gdk\gdkmarshalers.c
	@-del /f /q ..\gdk\gdkmarshalers.h
	@-del /f /q ..\gdk\gdkversionmacros.h
	@-del /f /q ..\gdk\gdkconfig.h
	@if exist ..\gdk-$(CFG)-$(GDK_CONFIG)-build del ..\gdk-$(CFG)-$(GDK_CONFIG)-build
	@if exist ..\gdk-$(GDK_OLD_CFG)-$(GDK_DEL_CONFIG)-build del ..\gdk-$(GDK_OLD_CFG)-$(GDK_DEL_CONFIG)-build
	@if exist ..\gdk-$(GDK_OLD_CFG)-$(GDK_CONFIG)-build del ..\gdk-$(GDK_OLD_CFG)-$(GDK_CONFIG)-build
	@if exist ..\gdk-$(CFG)-$(GDK_DEL_CONFIG)-build del ..\gdk-$(CFG)-$(GDK_DEL_CONFIG)-build
	@-del /f /q ..\config.h
