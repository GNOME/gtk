# NMake Makefile portion for code generation and
# intermediate build directory creation
# Items in here should not need to be edited unless
# one is maintaining the NMake build files.

!include config-msvc.mak
!include ../demos/gtk-demo/demos-sources.mak
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

GTK_VERSION_H = .\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gtk-3\gtk\gtkversion.h

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
	.\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gtk-3\gtk\gtkresources.c	\
	$(GTK_VERSION_H)

EMOJI_GRESOURCE_XML =	\
	.\vs$(VSVER)\$(CFG)\$(PLAT)\bin\de.gresource.xml	\
	.\vs$(VSVER)\$(CFG)\$(PLAT)\bin\es.gresource.xml	\
	.\vs$(VSVER)\$(CFG)\$(PLAT)\bin\fr.gresource.xml	\
	.\vs$(VSVER)\$(CFG)\$(PLAT)\bin\zh.gresource.xml

EMOJI_GRESOURCE = $(EMOJI_GRESOURCE_XML:.gresource.xml=.gresource)

generate-base-sources:	\
	.\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gdk-3\config.h	\
	$(GDK_GENERATED_SOURCES)	\
	$(GTK_GENERATED_SOURCES)	\
	.\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gdk-3\gdk\gdk.rc	\
	.\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gtk-3\gtk\gtk-win32.rc	\
	.\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gtk-3\gtk\libgtk3.manifest	\
	.\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gtk-3\gtk\gtk.gresource.xml	\
	.\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gtk3-demo\demos.h	\
	.\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gtk3-demo\demo_resources.c	\
	.\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gtk3-icon-browser\resources.c	\
	.\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gtk3-widget-factory\widget_factory_resources.c	\
	$(EMOJI_GRESOURCE)

# Copy the pre-defined config.h.win32 and demos.h.win32
.\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gdk-3\config.h: ..\config.h.win32
.\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gtk3-demo\demos.h: ..\demos\gtk-demo\demos.h.win32

# Generate the versioned headers and resource scripts (*.rc)
.\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gdk-3\gdk\gdkversionmacros.h: ..\gdk\gdkversionmacros.h.in
.\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gtk-3\gtk\gtkversion.h: ..\gtk\gtkversion.h.in
.\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gdk-3\gdk\gdk.rc: ..\gdk\win32\rc\gdk.rc.in
.\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gtk-3\gtk\gtk-win32.rc: ..\gtk\gtk-win32.rc.body.in

..\gdk-$(CFG)-$(GDK_CONFIG)-build: $(GDK_CONFIG_TEMPLATE)
	@if exist ..\gdk-$(GDK_OLD_CFG)-$(GDK_DEL_CONFIG)-build del ..\gdk-$(GDK_OLD_CFG)-$(GDK_DEL_CONFIG)-build
	@if exist ..\gdk-$(GDK_OLD_CFG)-$(GDK_CONFIG)-build del ..\gdk-$(GDK_OLD_CFG)-$(GDK_CONFIG)-build
	@if exist ..\gdk-$(CFG)-$(GDK_DEL_CONFIG)-build del ..\gdk-$(CFG)-$(GDK_DEL_CONFIG)-build
	@copy $** $@
	
.\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gdk-3\gdk\gdkconfig.h: ..\gdk-$(CFG)-$(GDK_CONFIG)-build

.\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gdk-3\config.h	\
.\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gdk-3\gdk\gdkconfig.h	\
.\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gtk3-demo\demos.h:
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

# Generate the private headers needed for broadway-server.c
generate-broadway-items: ..\gdk\broadway\clienthtml.h ..\gdk\broadway\broadwayjs.h

..\gdk\broadway\clienthtml.h: ..\gdk\broadway\client.html
	@echo Generating $@...
	@$(PERL) ..\gdk\broadway\toarray.pl client_html $**>$@

..\gdk\broadway\broadwayjs.h:	\
..\gdk\broadway\broadway.js	\
..\gdk\broadway\rawinflate.min.js
	@echo Generating $@...
	@$(PERL) ..\gdk\broadway\toarray.pl broadway_js $**>$@

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

.\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gdk-3\gdk\gdkversionmacros.h	\
.\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gtk-3\gtk\gtkversion.h	\
.\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gdk-3\gdk\gdk.rc	\
.\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gtk-3\gtk\gtk-win32.rc:
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

.\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gtk-3\gtk\gtktypefuncs.inc:	\
..\gtk\gentypefuncs.py	\
.\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gtk-3\gtk\gtkversion.h	\
.\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gtk-3\gtk\gtktypebuiltins.h
	@echo Generating $@...
	@if not exist $(@D)\ md $(@D)
	@echo #undef GTK_COMPILATION > $(@R).preproc.c
	@echo #include "gtkx.h" >> $(@R).preproc.c
	@cl /EP $(GTK_PREPROCESSOR_FLAGS) $(@R).preproc.c > $(@R).combined.c
	@$(PYTHON) ..\gtk\gentypefuncs.py $@ $(@R).combined.c
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

.\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gtk3-demo\demo_resources.c: ..\demos\gtk-demo\demo.gresource.xml $(GTK_DEMO_RESOURCES)
	@echo Generating $@...
	@if not exist $(@D)\ md $(@D)
	@$(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=..\demos\gtk-demo	\
	--generate-source ..\demos\gtk-demo\demo.gresource.xml

.\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gtk3-icon-browser\resources.c: ..\demos\icon-browser\iconbrowser.gresource.xml $(ICON_BROWSER_RESOURCES)
	@echo Generating $@...
	@if not exist $(@D)\ md $(@D)
	@$(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=..\demos\icon-browser	\
	--generate-source ..\demos\icon-browser\iconbrowser.gresource.xml

.\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gtk3-widget-factory\widget_factory_resources.c:	\
..\demos\icon-browser\iconbrowser.gresource.xml $(WIDGET_FACTORY_RESOURCES)
	@echo Generating $@...
	@if not exist $(@D)\ md $(@D)
	@$(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=..\demos\widget-factory	\
	--generate-source ..\demos\widget-factory\widget-factory.gresource.xml

# (Re-) generate Visual Studio projects
# Dependencies for library projects
gdk-3.sourcefiles: $(GDK_C_SRCS:/=\)
gdk-3.vs10.sourcefiles: $(GDK_C_SRCS:/=\)
gdk-3.vs10.sourcefiles.filters: $(GDK_C_SRCS:/=\)
gdk3-win32.sourcefiles: $(GDK_WIN32_C_SRCS)
gdk3-win32.vs10.sourcefiles: $(GDK_WIN32_C_SRCS)
gdk3-win32.vs10.sourcefiles.filters: $(GDK_WIN32_C_SRCS)
gdk3-broadway.sourcefiles: $(GDK_BROADWAY_C_SRCS)
gdk3-broadway.vs10.sourcefiles: $(GDK_BROADWAY_C_SRCS)
gdk3-broadway.vs10.sourcefiles.filters: $(GDK_BROADWAY_C_SRCS)

# GTK projects-Darn the fatal error U1095...!
gtk-3.misc.sourcefiles: $(GTK_MISC_C_SRCS:/=\)
gtk-3.a-h.sourcefiles: $(GTK_C_SRCS_A_H:/=\)
gtk-3.i-w.sourcefiles: $(GTK_C_SRCS_I_W:/=\)
gtk-3.win32.sourcefiles: $(GTK_OS_WIN32_C_SRCS:/=\) $(GTK_MORE_C_SRCS)
gtk-3.misc.vs10.sourcefiles: $(GTK_MISC_C_SRCS:/=\)
gtk-3.a-h.vs10.sourcefiles: $(GTK_C_SRCS_A_H:/=\)
gtk-3.i-w.vs10.sourcefiles: $(GTK_C_SRCS_I_W:/=\)
gtk-3.win32.vs10.sourcefiles: $(GTK_OS_WIN32_C_SRCS:/=\) $(GTK_MORE_C_SRCS)
gtk-3.misc.vs10.sourcefiles.filters: $(GTK_MISC_C_SRCS:/=\)
gtk-3.a-h.vs10.sourcefiles.filters: $(GTK_C_SRCS_A_H:/=\)
gtk-3.i-w.vs10.sourcefiles.filters: $(GTK_C_SRCS_I_W:/=\)
gtk-3.win32.vs10.sourcefiles.filters: $(GTK_OS_WIN32_C_SRCS:/=\) $(GTK_MORE_C_SRCS)

gtk-3.sourcefiles:	\
	gtk-3.a-h.sourcefiles	\
	gtk-3.i-w.sourcefiles	\
	gtk-3.misc.sourcefiles	\
	gtk-3.win32.sourcefiles

gtk-3.vs10.sourcefiles:	\
	gtk-3.a-h.vs10.sourcefiles	\
	gtk-3.i-w.vs10.sourcefiles	\
	gtk-3.misc.vs10.sourcefiles	\
	gtk-3.win32.vs10.sourcefiles

gtk-3.vs10.sourcefiles.filters:	\
	gtk-3.a-h.vs10.sourcefiles.filters	\
	gtk-3.i-w.vs10.sourcefiles.filters	\
	gtk-3.misc.vs10.sourcefiles.filters	\
	gtk-3.win32.vs10.sourcefiles.filters

gtk-3.sourcefiles gtk-3.vs10.sourcefiles gtk-3.vs10.sourcefiles.filters:
	@echo Genarating the final $@ from $**...
	@for %%f in ($**) do @type %%f>>$@ & del %%f

gailutil-3.sourcefiles: $(GAILUTIL_C_SRCS)
gailutil-3.vs10.sourcefiles: $(GAILUTIL_C_SRCS)
gailutil-3.vs10.sourcefiles.filters: $(GAILUTIL_C_SRCS)

# Dependencies for executable projects
broadwayd.sourcefiles: $(BROADWAYD_C_SRCS)
broadwayd.vs10.sourcefiles: $(BROADWAYD_C_SRCS)
broadwayd.vs10.sourcefiles.filters: $(BROADWAYD_C_SRCS)
gtk3-demo.sourcefiles: $(demo_actual_sources) $(more_demo_sources)
gtk3-demo.vs10.sourcefiles: $(demo_actual_sources) $(more_demo_sources)
gtk3-demo.vs10.sourcefiles.filters: $(demo_actual_sources) $(more_demo_sources)

gdk-3.sourcefiles gdk3-win32.sourcefiles gdk3-broadway.sourcefiles	\
gailutil-3.sourcefiles	\
broadwayd.sourcefiles gtk3-demo.sourcefiles:
	@-del vs9\$(@B).vcproj
	@for %%s in ($**) do @echo.   ^<File RelativePath^="..\%%s" /^>>>$@

gtk-3.a-h.sourcefiles gtk-3.i-w.sourcefiles	\
gtk-3.misc.sourcefiles gtk-3.win32.sourcefiles:
	@echo Generating $@...
	@if exist vs9\$(GTK_VS9_PROJ) del vs9\$(GTK_VS9_PROJ)
	@for %%s in ($(**:..\gtk\=)) do @echo.   ^<File RelativePath^="..\..\gtk\%%s" /^>>>$@

gdk-3.vs10.sourcefiles	\
gdk3-win32.vs10.sourcefiles	\
gdk3-broadway.vs10.sourcefiles	\
gailutil-3.vs10.sourcefiles	\
broadwayd.vs10.sourcefiles	\
gtk3-demo.vs10.sourcefiles:
	@echo Generating $@...
	@-del vs10\$(@B:.vs10=.vcxproj)
	@for %%s in ($**) do @echo.   ^<ClCompile Include^="..\%%s" /^>>>$@

gtk-3.a-h.vs10.sourcefiles	\
gtk-3.i-w.vs10.sourcefiles	\
gtk-3.misc.vs10.sourcefiles	\
gtk-3.win32.vs10.sourcefiles:
	@echo Generating $@...
	@if exist vs10\$(GTK_VS1X_PROJ) del vs10\$(GTK_VS1X_PROJ)
	@for %%s in ($(**:..\gtk\=)) do @echo.   ^<ClCompile Include^="..\..\gtk\%%s" /^>>>$@

gdk-3.vs10.sourcefiles.filters	\
gdk3-win32.vs10.sourcefiles.filters	\
gdk3-broadway.vs10.sourcefiles.filters	\
gailutil-3.vs10.sourcefiles.filters	\
broadwayd.vs10.sourcefiles.filters	\
gtk3-demo.vs10.sourcefiles.filters:
	@-del vs10\$(@F:.vs10.sourcefiles=.vcxproj)
	@for %%s in ($**) do @echo.   ^<ClCompile Include^="..\%%s"^>^<Filter^>Source Files^</Filter^>^</ClCompile^>>>$@

gtk-3.a-h.vs10.sourcefiles.filters	\
gtk-3.i-w.vs10.sourcefiles.filters	\
gtk-3.misc.vs10.sourcefiles.filters	\
gtk-3.win32.vs10.sourcefiles.filters:
	@if exist vs10\$(GTK_VS1X_PROJ_FILTERS) del vs10\$(GTK_VS1X_PROJ_FILTERS)
	@for %%s in ($(**:..\gtk\=)) do @echo.   ^<ClCompile Include^="..\..\gtk\%%s"^>^<Filter^>Source Files^</Filter^>^</ClCompile^>>>$@

# Dependencies for GDK projects
vs9\$(GDK_VS9_PROJ): gdk-3.sourcefiles vs9\$(GDK_VS9_PROJ)in
vs9\$(GDKWIN32_VS9_PROJ).pre: gdk3-win32.sourcefiles vs9\$(GDKWIN32_VS9_PROJ)in
vs9\$(GDKBROADWAY_VS9_PROJ): gdk3-broadway.sourcefiles vs9\$(GDKBROADWAY_VS9_PROJ)in
vs9\$(GTK_VS9_PROJ): gtk-3.sourcefiles vs9\$(GTK_VS9_PROJ)in
vs9\$(GAILUTIL_VS9_PROJ): gailutil-3.sourcefiles vs9\$(GAILUTIL_VS9_PROJ)in

vs10\$(GDK_VS1X_PROJ): gdk-3.vs10.sourcefiles vs10\$(GDK_VS1X_PROJ)in
vs10\$(GDKWIN32_VS1X_PROJ).pre: gdk3-win32.vs10.sourcefiles vs10\$(GDKWIN32_VS1X_PROJ)in
vs10\$(GDKBROADWAY_VS1X_PROJ): gdk3-broadway.vs10.sourcefiles vs10\$(GDKBROADWAY_VS1X_PROJ)in
vs10\$(GTK_VS1X_PROJ): gtk-3.vs10.sourcefiles vs10\$(GTK_VS1X_PROJ)in
vs10\$(GAILUTIL_VS1X_PROJ): gailutil-3.vs10.sourcefiles vs10\$(GAILUTIL_VS1X_PROJ)in

vs10\$(GDK_VS1X_PROJ_FILTERS): gdk-3.vs10.sourcefiles.filters vs10\$(GDK_VS1X_PROJ_FILTERS)in
vs10\$(GDKWIN32_VS1X_PROJ_FILTERS): gdk3-win32.vs10.sourcefiles.filters vs10\$(GDKWIN32_VS1X_PROJ_FILTERS)in
vs10\$(GDKBROADWAY_VS1X_PROJ_FILTERS): gdk3-broadway.vs10.sourcefiles.filters vs10\$(GDKBROADWAY_VS1X_PROJ_FILTERS)in
vs10\$(GTK_VS1X_PROJ_FILTERS): gtk-3.vs10.sourcefiles.filters vs10\$(GTK_VS1X_PROJ_FILTERS)in
vs10\$(GAILUTIL_VS1X_PROJ_FILTERS): gailutil-3.vs10.sourcefiles.filters vs10\$(GAILUTIL_VS1X_PROJ_FILTERS)in

# Dependencies for tool executables
vs9\$(BROADWAYD_VS9_PROJ): broadwayd.sourcefiles vs9\$(BROADWAYD_VS9_PROJ)in
vs10\$(BROADWAYD_VS1X_PROJ): broadwayd.vs10.sourcefiles vs10\$(BROADWAYD_VS1X_PROJ)in
vs10\$(BROADWAYD_VS1X_PROJ_FILTERS): broadwayd.vs10.sourcefiles.filters vs10\$(BROADWAYD_VS1X_PROJ_FILTERS)in

# Dependencies for demos
vs9\$(DEMO_VS9_PROJ).pre: gtk3-demo.sourcefiles vs9\$(DEMO_VS9_PROJ)in
vs10\$(DEMO_VS1X_PROJ).pre: gtk3-demo.vs10.sourcefiles vs10\$(DEMO_VS1X_PROJ)in
vs10\$(DEMO_VS1X_PROJ_FILTERS): gtk3-demo.vs10.sourcefiles.filters vs10\$(DEMO_VS1X_PROJ_FILTERS)in

# Create the project files themselves without customization with options
vs9\$(GDKWIN32_VS9_PROJ).pre	\
vs9\$(GDKBROADWAY_VS9_PROJ)	\
vs9\$(GDK_VS9_PROJ)	\
vs9\$(GTK_VS9_PROJ)	\
vs9\$(GAILUTIL_VS9_PROJ)	\
vs9\$(BROADWAYD_VS9_PROJ)	\
vs9\$(DEMO_VS9_PROJ).pre	\
vs10\$(GDKWIN32_VS1X_PROJ).pre	\
vs10\$(GDKBROADWAY_VS1X_PROJ)	\
vs10\$(GDK_VS1X_PROJ)	\
vs10\$(GTK_VS1X_PROJ)	\
vs10\$(GAILUTIL_VS1X_PROJ)	\
vs10\$(BROADWAYD_VS1X_PROJ)	\
vs10\$(DEMO_VS1X_PROJ).pre	\
vs10\$(GDKWIN32_VS1X_PROJ_FILTERS)	\
vs10\$(GDKBROADWAY_VS1X_PROJ_FILTERS)	\
vs10\$(GDK_VS1X_PROJ_FILTERS)	\
vs10\$(GTK_VS1X_PROJ_FILTERS)	\
vs10\$(GAILUTIL_VS1X_PROJ_FILTERS)	\
vs10\$(BROADWAYD_VS1X_PROJ_FILTERS)	\
vs10\$(DEMO_VS1X_PROJ_FILTERS):
	@$(CPP) /nologo /EP /I. $(@:.pre=)in>$(@F:.pre=).tmp
	@for /f "usebackq tokens=* delims=" %%l in ($(@F:.pre=).tmp) do @echo %%l>>$@
	@-del $(@F:.pre=).tmp
	@-for %%f in ($**) do @if not "%%f" == "$(@:.pre=)in" del %%f

vs9\$(GDKWIN32_VS9_PROJ): vs9\$(GDKWIN32_VS9_PROJ).pre
vs10\$(GDKWIN32_VS1X_PROJ): vs10\$(GDKWIN32_VS1X_PROJ).pre

vs9\$(DEMO_VS9_PROJ): vs9\$(DEMO_VS9_PROJ).pre
vs10\$(DEMO_VS1X_PROJ): vs10\$(DEMO_VS1X_PROJ).pre

!ifdef USE_EGL
regenerate-gdk-vsproj-msg:
	@echo Regenerating GDK Visual Studio projects with EGL support...

vs9\$(GDKWIN32_VS9_PROJ):
	@echo Generating $@...
	@$(PYTHON) replace.py -a=replace-str -i=$** -o=$@	\
	--instring=";INSIDE_GDK_WIN32\""	\
	--outstring=";INSIDE_GDK_WIN32;GDK_WIN32_ENABLE_EGL\""
	@-del $**

vs10\$(GDKWIN32_VS1X_PROJ):
	@echo Generating $@...
	@$(PYTHON) replace.py -a=replace-str -i=$** -o=$@	\
	--instring=";INSIDE_GDK_WIN32;%"	\
	--outstring=";INSIDE_GDK_WIN32;GDK_WIN32_ENABLE_EGL;%"
	@-del $**
!else
regenerate-gdk-vsproj-msg:
	@echo Regenerating GDK Visual Studio projects without EGL support...

vs9\$(GDKWIN32_VS9_PROJ) vs10\$(GDKWIN32_VS1X_PROJ):
	@echo Renaming $** to $@...
	@move $** $@
!endif

!ifdef FONT_FEATURES_DEMO
!ifdef FONT_FEATURES_USE_PANGOFT2
DEMO_MSG = with font features demo using PangoFT2

vs9\$(DEMO_VS9_PROJ):
	@echo (Re-)Generating $@ $(DEMO_MSG)...
	@$(PYTHON) replace.py -a=replace-str -i=$** -o=$@	\
	--instring="AdditionalDependencies=\"\""	\
	--outstring="AdditionalDependencies=\"$(DEMO_DEP_LIBS_PANGOFT2_VS9)\""
	@-del $**

vs10\$(DEMO_VS1X_PROJ):
	@echo (Re-)Generating $@ $(DEMO_MSG)...
	@$(PYTHON) replace.py -a=replace-str -i=$** -o=$@	\
	--instring=">%(AdditionalDependencies)<"	\
	--outstring=">$(DEMO_DEP_LIBS_PANGOFT2_VS1X);%(AdditionalDependencies)<"
	@-del $**
!else
DEMO_MSG = with font features demo

vs9\$(DEMO_VS9_PROJ):
	@echo (Re-)Generating $@ $(DEMO_MSG)...
	@$(PYTHON) replace.py -a=replace-str -i=$** -o=$@	\
	--instring="AdditionalDependencies=\"\""	\
	--outstring="AdditionalDependencies=\"$(DEMO_DEP_LIBS_NEW_PANGO)\""
	@-del $**

vs10\$(DEMO_VS1X_PROJ):
	@echo (Re-)Generating $@ $(DEMO_MSG)...
	@$(PYTHON) replace.py -a=replace-str -i=$** -o=$@	\
	--instring=">%(AdditionalDependencies)<"	\
	--outstring=">$(DEMO_DEP_LIBS_NEW_PANGO);%(AdditionalDependencies)<"
	@-del $**
!endif
!else

DEMO_MSG = without font features demo

vs9\$(DEMO_VS9_PROJ) vs10\$(DEMO_VS1X_PROJ):
	@echo (Re-)Generating $@ $(DEMO_MSG)...
	@move $** $@
!endif

# VS2012+ .vcxproj: Update the toolset version as appropriate
{vs10\}.vcxproj{vs11\}.vcxproj:
	@echo Copying and updating $< for VS2012
	@$(PYTHON) replace.py -a=replace-str -i=$< -o=$@ --instring=">v100<" --outstring=">v110<"

{vs10\}.vcxproj{vs12\}.vcxproj:
	@echo Copying and updating $< for VS2013
	@$(PYTHON) replace.py -a=replace-str -i=$< -o=$@ --instring=">v100<" --outstring=">v120<"

{vs10\}.vcxproj{vs14\}.vcxproj:
	@echo Copying and updating $< for VS2015
	@$(PYTHON) replace.py -a=replace-str -i=$< -o=$@ --instring=">v100<" --outstring=">v140<"

{vs10\}.vcxproj{vs15\}.vcxproj:
	@echo Copying and updating $< for VS2017
	@$(PYTHON) replace.py -a=replace-str -i=$< -o=$@ --instring=">v100<" --outstring=">v141<"

{vs10\}.vcxproj{vs16\}.vcxproj:
	@echo Copying and updating $< for VS2019
	@$(PYTHON) replace.py -a=replace-str -i=$< -o=$@ --instring=">v100<" --outstring=">v142<"

{vs10\}.vcxproj{vs17\}.vcxproj:
	@echo Copying and updating $< for VS2022
	@$(PYTHON) replace.py -a=replace-str -i=$< -o=$@ --instring=">v100<" --outstring=">v143<"

# VS2012+ .vcxproj.filters: We simply copy the VS2010 ones
{vs10\}.filters{vs11\}.filters:
	@echo Copying $< to $@...
	@copy $< $@

{vs10\}.filters{vs12\}.filters:
	@echo Copying $< to $@...
	@copy $< $@

{vs10\}.filters{vs14\}.filters:
	@echo Copying $< to $@...
	@copy $< $@

{vs10\}.filters{vs15\}.filters:
	@echo Copying $< to $@...
	@copy $< $@

{vs10\}.filters{vs16\}.filters:
	@echo Copying $< to $@...
	@copy $< $@

{vs10\}.filters{vs17\}.filters:
	@echo Copying $< to $@...
	@copy $< $@

{vs10\}.sln{vs11\}.sln:
	@echo Copying and updating $< for VS2012...
	@$(PYTHON) replace.py -a=replace-str -i=$< -o=$@.tmp	\
	--instring="Format Version 11.00" --outstring="Format Version 12.00"
	@$(PYTHON) replace.py -a=replace-str -i=$@.tmp -o=$@	\
	--instring="# Visual Studio 2010" --outstring="# Visual Studio 2012"
	@del $@.tmp

{vs10\}.sln{vs12\}.sln:
	@echo Copying and updating $< for VS2013...
	@$(PYTHON) replace.py -a=replace-str -i=$< -o=$@.tmp	\
	--instring="Format Version 11.00" --outstring="Format Version 12.00"
	@$(PYTHON) replace.py -a=replace-str -i=$@.tmp -o=$@	\
	--instring="# Visual Studio 2010" --outstring="# Visual Studio 2013"
	@del $@.tmp

{vs10\}.sln{vs14\}.sln:
	@echo Copying and updating $< for VS2015...
	@$(PYTHON) replace.py -a=replace-str -i=$< -o=$@.tmp	\
	--instring="Format Version 11.00" --outstring="Format Version 12.00"
	@$(PYTHON) replace.py -a=replace-str -i=$@.tmp -o=$@	\
	--instring="# Visual Studio 2010" --outstring="# Visual Studio 14"
	@del $@.tmp

{vs10\}.sln{vs15\}.sln:
	@echo Copying and updating $< for VS2017...
	@$(PYTHON) replace.py -a=replace-str -i=$< -o=$@.tmp	\
	--instring="Format Version 11.00" --outstring="Format Version 12.00"
	@$(PYTHON) replace.py -a=replace-str -i=$@.tmp -o=$@	\
	--instring="# Visual Studio 2010" --outstring="# Visual Studio 15"
	@del $@.tmp

{vs10\}.sln{vs16\}.sln:
	@echo Copying and updating $< for VS2019...
	@$(PYTHON) replace.py -a=replace-str -i=$< -o=$@.tmp	\
	--instring="Format Version 11.00" --outstring="Format Version 12.00"
	@$(PYTHON) replace.py -a=replace-str -i=$@.tmp -o=$@	\
	--instring="# Visual Studio 2010" --outstring="# Visual Studio 16"
	@del $@.tmp

{vs10\}.sln{vs17\}.sln:
	@echo Copying and updating $< for VS2022...
	@$(PYTHON) replace.py -a=replace-str -i=$< -o=$@.tmp	\
	--instring="Format Version 11.00" --outstring="Format Version 12.00"
	@$(PYTHON) replace.py -a=replace-str -i=$@.tmp -o=$@	\
	--instring="# Visual Studio 2010" --outstring="# Visual Studio 17"
	@del $@.tmp

copy-update-static-projects:	\
$(GTK3_VS11_STATIC_PROJS)	\
$(GTK3_VS12_STATIC_PROJS)	\
$(GTK3_VS14_STATIC_PROJS)	\
$(GTK3_VS15_STATIC_PROJS)	\
$(GTK3_VS16_STATIC_PROJS)	\
$(GTK3_VS17_STATIC_PROJS)

.\vs$(VSVER)\$(CFG)\$(PLAT)\bin\de.gresource.xml: ..\gtk\emoji\gresource.xml.in
.\vs$(VSVER)\$(CFG)\$(PLAT)\bin\es.gresource.xml: ..\gtk\emoji\gresource.xml.in
.\vs$(VSVER)\$(CFG)\$(PLAT)\bin\fr.gresource.xml: ..\gtk\emoji\gresource.xml.in
.\vs$(VSVER)\$(CFG)\$(PLAT)\bin\zh.gresource.xml: ..\gtk\emoji\gresource.xml.in

.\vs$(VSVER)\$(CFG)\$(PLAT)\bin\de.gresource: .\vs$(VSVER)\$(CFG)\$(PLAT)\bin\de.gresource.xml ..\gtk\emoji\de.data
.\vs$(VSVER)\$(CFG)\$(PLAT)\bin\es.gresource: .\vs$(VSVER)\$(CFG)\$(PLAT)\bin\es.gresource.xml ..\gtk\emoji\es.data
.\vs$(VSVER)\$(CFG)\$(PLAT)\bin\fr.gresource: .\vs$(VSVER)\$(CFG)\$(PLAT)\bin\fr.gresource.xml ..\gtk\emoji\fr.data
.\vs$(VSVER)\$(CFG)\$(PLAT)\bin\zh.gresource: .\vs$(VSVER)\$(CFG)\$(PLAT)\bin\zh.gresource.xml ..\gtk\emoji\zh.data

$(EMOJI_GRESOURCE_XML):
	@echo Generating $@...
	@if not exist $(@D)\ mkdir $(@D)
	@$(PYTHON) replace.py -i=$** -o=$@ --action=replace-var --var=lang --outstring=$(@B:.gresource=)

$(EMOJI_GRESOURCE):
	@echo Generating $@...
	@$(GLIB_COMPILE_RESOURCES) --sourcedir=..\gtk\emoji $@.xml --target=$@

regenerate-demos-h-win32: ..\demos\gtk-demo\geninclude.py $(demo_actual_sources) regenerate-demos-h-win32-msg $(GTK3_DEMO_VCPROJS)
	@-del ..\demos\gtk-demo\demos.h.win32
	@cd ..\demos\gtk-demo
	@$(PYTHON) geninclude.py demos.h.win32 $(demo_sources)
	@cd ..\..\win32

..\po\gtk30.pot: ..\gtk\gtkbuilder.its
	$(XGETTEXT) --default-domain="$(@B)"	\
	--copyright-holder="GTK+ Team and others. See AUTHORS"	\
	--package-name="gtk+"	\
	--package-version="$(GTK_VERSION)"	\
	--msgid-bugs-address="https://gitlab.gnome.org/GNOME/gtk/-/issues/"	\
	--directory=".." \
	--add-comments=TRANSLATORS: --from-code=UTF-8 --keyword=_ --keyword=N_	\
	--keyword=C_:1c,2 --keyword=NC_:1c,2 --keyword=g_dngettext:2,3 --add-comments	\
	--files-from="$(@D:\=/)/POTFILES.in" --output=$(@F)
	@move $(@F) $@

..\po-properties\gtk30-properties.pot:
	$(XGETTEXT) --default-domain="$(@B)"	\
	--copyright-holder="GTK+ Team and others. See AUTHORS"	\
	--package-name="gtk+"	\
	--package-version="$(GTK_VERSION)"	\
	--msgid-bugs-address="https://gitlab.gnome.org/GNOME/gtk/-/issues/"	\
	--directory=".." \
	--from-code=UTF-8 --keyword --keyword=P_ --add-comments	\
	--files-from="$(@D:\=/)/POTFILES.in"
	@move $(@B).po $@

regenerate-gtk-vsproj-msg:
	@echo Regenerating GTK and gailutil projects...

regenerate-gdk-vsproj: regenerate-gdk-vsproj-msg $(GTK3_GDK_VCPROJS)
regenerate-gtk-vsproj: regenerate-gtk-vsproj-msg $(GTK3_GTK_VCPROJS) $(GTK3_GAILUTIL_VCPROJS)

regenerate-all-msvc-projs:	\
	copy-update-static-projects	\
	regenerate-gdk-vsproj	\
	regenerate-gtk-vsproj	\
	regenerate-demos-h-win32

# Remove the generated files
clean:
	@-del /f /q .\vs$(VSVER)\$(CFG)\$(PLAT)\bin\*.gresource
	@-del /f /q .\vs$(VSVER)\$(CFG)\$(PLAT)\bin\*.gresource.xml
	@-del /f /q .\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gtk3-icon-browser\resources.c
	@-del /f /q .\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gtk3-demo\demo_resources.c
	@-del /f /q .\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gtk3-demo\demos.h
	@-del /f /q .\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gtk-3\gtk\gtktypebuiltins.c
	@-del /f /q .\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gtk-3\gtk\gtktypebuiltins.h
	@-del /f /q .\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gtk-3\gtk\gtkprivatetypebuiltins.c
	@-del /f /q .\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gtk-3\gtk\gtkprivatetypebuiltins.h
	@-del /f /q .\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gtk-3\gtk\gtkversion.h
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
	@-del /f /q .\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gdk-3\gdk\gdk.rc
	@-del /f /q .\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gdk-3\gdk\gdkversionmacros.h
	@-del /f /q .\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gdk-3\gdk\gdkconfig.h
	@if exist ..\gdk-$(CFG)-$(GDK_CONFIG)-build del ..\gdk-$(CFG)-$(GDK_CONFIG)-build
	@if exist ..\gdk-$(GDK_OLD_CFG)-$(GDK_DEL_CONFIG)-build del ..\gdk-$(GDK_OLD_CFG)-$(GDK_DEL_CONFIG)-build
	@if exist ..\gdk-$(GDK_OLD_CFG)-$(GDK_CONFIG)-build del ..\gdk-$(GDK_OLD_CFG)-$(GDK_CONFIG)-build
	@if exist ..\gdk-$(CFG)-$(GDK_DEL_CONFIG)-build del ..\gdk-$(CFG)-$(GDK_DEL_CONFIG)-build
	@-del /f /q .\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gdk-3\config.h
	@-rd .\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gdk-3\gdk

.SUFFIXES: .vcxproj .filters .sln 

