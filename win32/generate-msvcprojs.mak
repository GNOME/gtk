# NMake Makefile portion for (re-)generating Visual Studio
# projects. Items in here should not need to be edited unless
# one is maintaining the NMake build files.


GTK3_VS10_STATIC_PROJS =	\
	vs10\gtk+.sln	\
	vs10\gtk-builder-tool.vcxproj	\
	vs10\gtk-encode-symbolic-svg.vcxproj	\
	vs10\gtk-query-settings.vcxproj	\
	vs10\gtk-update-icon-cache.vcxproj	\
	vs10\gtk3-demo-application.vcxproj	\
	vs10\gtk3-icon-browser.vcxproj	\
	vs10\gtk3-install.vcxproj	\
	vs10\gtk3-introspect.vcxproj	\
	vs10\gtk3-prebuild.vcxproj	\
	vs10\gtk3-widget-factory.vcxproj	\
	vs10\gtk-builder-tool.vcxproj.filters	\
	vs10\gtk-encode-symbolic-svg.vcxproj.filters	\
	vs10\gtk-query-settings.vcxproj.filters	\
	vs10\gtk-update-icon-cache.vcxproj.filters	\
	vs10\gtk3-demo-application.vcxproj.filters	\
	vs10\gtk3-icon-browser.vcxproj.filters	\
	vs10\gtk3-widget-factory.vcxproj.filters

GTK3_VS11_STATIC_PROJS = $(GTK3_VS10_STATIC_PROJS:vs10\=vs11\)
GTK3_VS12_STATIC_PROJS = $(GTK3_VS10_STATIC_PROJS:vs10\=vs12\)
GTK3_VS14_STATIC_PROJS = $(GTK3_VS10_STATIC_PROJS:vs10\=vs14\)
GTK3_VS15_STATIC_PROJS = $(GTK3_VS10_STATIC_PROJS:vs10\=vs15\)
GTK3_VS16_STATIC_PROJS = $(GTK3_VS10_STATIC_PROJS:vs10\=vs16\)
GTK3_VS17_STATIC_PROJS = $(GTK3_VS10_STATIC_PROJS:vs10\=vs17\)

GDK_VS9_PROJ = gdk-3.vcproj
GDKWIN32_VS9_PROJ = gdk3-win32.vcproj
GDKBROADWAY_VS9_PROJ = $(GDKWIN32_VS9_PROJ:-win32=-broadway)
GTK_VS9_PROJ = gtk-3.vcproj
GAILUTIL_VS9_PROJ = gailutil-3.vcproj
BROADWAYD_VS9_PROJ = broadwayd.vcproj
DEMO_VS9_PROJ = gtk3-demo.vcproj

GDK_VS1X_PROJ = $(GDK_VS9_PROJ:.vcproj=.vcxproj)
GDKWIN32_VS1X_PROJ = $(GDKWIN32_VS9_PROJ:.vcproj=.vcxproj)
GDKBROADWAY_VS1X_PROJ = $(GDKBROADWAY_VS9_PROJ:.vcproj=.vcxproj)
GTK_VS1X_PROJ = $(GTK_VS9_PROJ:.vcproj=.vcxproj)
GAILUTIL_VS1X_PROJ = $(GAILUTIL_VS9_PROJ:.vcproj=.vcxproj)
BROADWAYD_VS1X_PROJ = $(BROADWAYD_VS9_PROJ:.vcproj=.vcxproj)
DEMO_VS1X_PROJ = $(DEMO_VS9_PROJ:.vcproj=.vcxproj)

GDK_VS1X_PROJ_FILTERS = $(GDK_VS9_PROJ:.vcproj=.vcxproj.filters)
GDKWIN32_VS1X_PROJ_FILTERS = $(GDKWIN32_VS9_PROJ:.vcproj=.vcxproj.filters)
GDKBROADWAY_VS1X_PROJ_FILTERS = $(GDKBROADWAY_VS9_PROJ:.vcproj=.vcxproj.filters)
GTK_VS1X_PROJ_FILTERS = $(GTK_VS9_PROJ:.vcproj=.vcxproj.filters)
GAILUTIL_VS1X_PROJ_FILTERS = $(GAILUTIL_VS9_PROJ:.vcproj=.vcxproj.filters)
BROADWAYD_VS1X_PROJ_FILTERS = $(BROADWAYD_VS9_PROJ:.vcproj=.vcxproj.filters)
DEMO_VS1X_PROJ_FILTERS = $(DEMO_VS9_PROJ:.vcproj=.vcxproj.filters)

GTK3_GDK_VC1X_PROJS =	\
	vs10\$(GDK_VS1X_PROJ)	\
	vs11\$(GDK_VS1X_PROJ)	\
	vs12\$(GDK_VS1X_PROJ)	\
	vs14\$(GDK_VS1X_PROJ)	\
	vs15\$(GDK_VS1X_PROJ)	\
	vs16\$(GDK_VS1X_PROJ)	\
	vs17\$(GDK_VS1X_PROJ)

GTK3_GDK_VC1X_PROJ_FILTERS =	\
	vs11\$(GDK_VS1X_PROJ_FILTERS)	\
	vs12\$(GDK_VS1X_PROJ_FILTERS)	\
	vs14\$(GDK_VS1X_PROJ_FILTERS)	\
	vs15\$(GDK_VS1X_PROJ_FILTERS)	\
	vs16\$(GDK_VS1X_PROJ_FILTERS)	\
	vs17\$(GDK_VS1X_PROJ_FILTERS)

GTK3_GDK_WIN32_VC1X_PROJS =	\
	vs10\$(GDKWIN32_VS1X_PROJ)	\
	vs11\$(GDKWIN32_VS1X_PROJ)	\
	vs12\$(GDKWIN32_VS1X_PROJ)	\
	vs14\$(GDKWIN32_VS1X_PROJ)	\
	vs15\$(GDKWIN32_VS1X_PROJ)	\
	vs16\$(GDKWIN32_VS1X_PROJ)	\
	vs17\$(GDKWIN32_VS1X_PROJ)

GTK3_GDK_WIN32_VC1X_PROJ_FILTERS =	\
	vs11\$(GDKWIN32_VS1X_PROJ_FILTERS)	\
	vs12\$(GDKWIN32_VS1X_PROJ_FILTERS)	\
	vs14\$(GDKWIN32_VS1X_PROJ_FILTERS)	\
	vs15\$(GDKWIN32_VS1X_PROJ_FILTERS)	\
	vs16\$(GDKWIN32_VS1X_PROJ_FILTERS)	\
	vs17\$(GDKWIN32_VS1X_PROJ_FILTERS)

GTK3_GDK_BROADWAY_VC1X_PROJS =	\
	vs10\$(GDKBROADWAY_VS1X_PROJ)	\
	vs11\$(GDKBROADWAY_VS1X_PROJ)	\
	vs12\$(GDKBROADWAY_VS1X_PROJ)	\
	vs14\$(GDKBROADWAY_VS1X_PROJ)	\
	vs15\$(GDKBROADWAY_VS1X_PROJ)	\
	vs16\$(GDKBROADWAY_VS1X_PROJ)	\
	vs17\$(GDKBROADWAY_VS1X_PROJ)

GTK3_GDK_BROADWAY_VC1X_PROJ_FILTERS =	\
	vs11\$(GDKBROADWAY_VS1X_PROJ_FILTERS)	\
	vs12\$(GDKBROADWAY_VS1X_PROJ_FILTERS)	\
	vs14\$(GDKBROADWAY_VS1X_PROJ_FILTERS)	\
	vs15\$(GDKBROADWAY_VS1X_PROJ_FILTERS)	\
	vs16\$(GDKBROADWAY_VS1X_PROJ_FILTERS)	\
	vs17\$(GDKBROADWAY_VS1X_PROJ_FILTERS)

GTK3_GTK_VC1X_PROJS =	\
	vs10\$(GTK_VS1X_PROJ)	\
	vs11\$(GTK_VS1X_PROJ)	\
	vs12\$(GTK_VS1X_PROJ)	\
	vs14\$(GTK_VS1X_PROJ)	\
	vs15\$(GTK_VS1X_PROJ)	\
	vs16\$(GTK_VS1X_PROJ)	\
	vs17\$(GTK_VS1X_PROJ)

GTK3_GTK_VC1X_PROJ_FILTERS =	\
	vs11\$(GTK_VS1X_PROJ_FILTERS)	\
	vs12\$(GTK_VS1X_PROJ_FILTERS)	\
	vs14\$(GTK_VS1X_PROJ_FILTERS)	\
	vs15\$(GTK_VS1X_PROJ_FILTERS)	\
	vs16\$(GTK_VS1X_PROJ_FILTERS)	\
	vs17\$(GTK_VS1X_PROJ_FILTERS)

GTK3_GAILUTIL_VC1X_PROJS =	\
	vs10\$(GAILUTIL_VS1X_PROJ)	\
	vs11\$(GAILUTIL_VS1X_PROJ)	\
	vs12\$(GAILUTIL_VS1X_PROJ)	\
	vs14\$(GAILUTIL_VS1X_PROJ)	\
	vs15\$(GAILUTIL_VS1X_PROJ)	\
	vs16\$(GAILUTIL_VS1X_PROJ)	\
	vs17\$(GAILUTIL_VS1X_PROJ)

GTK3_GAILUTIL_VC1X_PROJ_FILTERS =	\
	vs11\$(GAILUTIL_VS1X_PROJ_FILTERS)	\
	vs12\$(GAILUTIL_VS1X_PROJ_FILTERS)	\
	vs14\$(GAILUTIL_VS1X_PROJ_FILTERS)	\
	vs15\$(GAILUTIL_VS1X_PROJ_FILTERS)	\
	vs16\$(GAILUTIL_VS1X_PROJ_FILTERS)	\
	vs17\$(GAILUTIL_VS1X_PROJ_FILTERS)

GTK3_BROADWAYD_VC1X_PROJS =	\
	vs10\$(BROADWAYD_VS1X_PROJ)	\
	vs11\$(BROADWAYD_VS1X_PROJ)	\
	vs12\$(BROADWAYD_VS1X_PROJ)	\
	vs14\$(BROADWAYD_VS1X_PROJ)	\
	vs15\$(BROADWAYD_VS1X_PROJ)	\
	vs16\$(BROADWAYD_VS1X_PROJ)	\
	vs17\$(BROADWAYD_VS1X_PROJ)

GTK3_BROADWAYD_VC1X_PROJ_FILTERS =	\
	vs11\$(BROADWAYD_VS1X_PROJ_FILTERS)	\
	vs12\$(BROADWAYD_VS1X_PROJ_FILTERS)	\
	vs14\$(BROADWAYD_VS1X_PROJ_FILTERS)	\
	vs15\$(BROADWAYD_VS1X_PROJ_FILTERS)	\
	vs16\$(BROADWAYD_VS1X_PROJ_FILTERS)	\
	vs17\$(BROADWAYD_VS1X_PROJ_FILTERS)

GTK3_DEMO_VC1X_PROJS =	\
	vs10\$(DEMO_VS1X_PROJ)	\
	vs11\$(DEMO_VS1X_PROJ)	\
	vs12\$(DEMO_VS1X_PROJ)	\
	vs14\$(DEMO_VS1X_PROJ)	\
	vs15\$(DEMO_VS1X_PROJ)	\
	vs16\$(DEMO_VS1X_PROJ)	\
	vs17\$(DEMO_VS1X_PROJ)

GTK3_DEMO_VC1X_PROJ_FILTERS =	\
	vs11\$(DEMO_VS1X_PROJ_FILTERS)	\
	vs12\$(DEMO_VS1X_PROJ_FILTERS)	\
	vs14\$(DEMO_VS1X_PROJ_FILTERS)	\
	vs15\$(DEMO_VS1X_PROJ_FILTERS)	\
	vs16\$(DEMO_VS1X_PROJ_FILTERS)	\
	vs17\$(DEMO_VS1X_PROJ_FILTERS)

GTK3_GDK_WIN32_VCPROJS =	\
	vs9\$(GDKWIN32_VS9_PROJ)	\
	$(GTK3_GDK_WIN32_VC1X_PROJS)	\
	$(GTK3_GDK_WIN32_VC1X_PROJ_FILTERS)

GTK3_GDK_BROADWAY_VCPROJS =	\
	vs9\$(GDKBROADWAY_VS9_PROJ)	\
	$(GTK3_GDK_BROADWAY_VC1X_PROJS)	\
	$(GTK3_GDK_BROADWAY_VC1X_PROJ_FILTERS)

GTK3_BROADWAYD_VCPROJS =	\
	vs9\$(BROADWAYD_VS9_PROJ)	\
	$(GTK3_BROADWAYD_VC1X_PROJS)	\
	$(GTK3_BROADWAYD_VC1X_PROJ_FILTERS)

GTK3_GDK_VCPROJS =	\
	vs9\$(GDK_VS9_PROJ)	\
	$(GTK3_GDK_VC1X_PROJS)	\
	$(GTK3_GDK_VC1X_PROJ_FILTERS)	\
	$(GTK3_GDK_WIN32_VCPROJS)	\
	$(GTK3_GDK_BROADWAY_VCPROJS)	\
	$(GTK3_BROADWAYD_VCPROJS)

GTK3_GTK_VCPROJS =	\
	vs9\$(GTK_VS9_PROJ)	\
	$(GTK3_GTK_VC1X_PROJS)	\
	$(GTK3_GTK_VC1X_PROJ_FILTERS)

GTK3_GAILUTIL_VCPROJS =	\
	vs9\$(GAILUTIL_VS9_PROJ)	\
	$(GTK3_GAILUTIL_VC1X_PROJS)	\
	$(GTK3_GAILUTIL_VC1X_PROJ_FILTERS)

GTK3_DEMO_VCPROJS =	\
	vs9\$(DEMO_VS9_PROJ)	\
	$(GTK3_DEMO_VC1X_PROJS)	\
	$(GTK3_DEMO_VC1X_PROJ_FILTERS)

DEMO_DEP_LIBS_NEW_PANGO=harfbuzz.lib
DEMO_DEP_LIBS_PANGOFT2_VS1X=pangoft2-1.0.lib;harfbuzz.lib;freetype.lib
DEMO_DEP_LIBS_PANGOFT2_VS9=$(DEMO_DEP_LIBS_PANGOFT2_VS1X:;= )

# (Re-) generate Visual Studio projects
# Dependencies for library projects
gdk-3.sourcefiles	\
gdk-3.vs10.sourcefiles	\
gdk-3.vs10.sourcefiles.filters: $(GDK_C_SRCS:/=\)

gdk3-win32.sourcefiles	\
gdk3-win32.vs10.sourcefiles	\
gdk3-win32.vs10.sourcefiles.filters: $(GDK_WIN32_C_SRCS)

gdk3-broadway.sourcefiles	\
gdk3-broadway.vs10.sourcefiles	\
gdk3-broadway.vs10.sourcefiles.filters: $(GDK_BROADWAY_C_SRCS)

# GTK projects-Darn the fatal error U1095...!
gtk-3.misc.sourcefiles	\
gtk-3.misc.vs10.sourcefiles	\
gtk-3.misc.vs10.sourcefiles.filters: $(GTK_MISC_C_SRCS:/=\)

gtk-3.a-h.sourcefiles	\
gtk-3.a-h.vs10.sourcefiles	\
gtk-3.a-h.vs10.sourcefiles.filters: $(GTK_C_SRCS_A_H:/=\)

gtk-3.i-w.sourcefiles	\
gtk-3.i-w.vs10.sourcefiles	\
gtk-3.i-w.vs10.sourcefiles.filters: $(GTK_C_SRCS_I_W:/=\)

gtk-3.win32.sourcefiles	\
gtk-3.win32.vs10.sourcefiles	\
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

gailutil-3.sourcefiles	\
gailutil-3.vs10.sourcefiles	\
gailutil-3.vs10.sourcefiles.filters: $(GAILUTIL_C_SRCS)

# Dependencies for executable projects
broadwayd.sourcefiles	\
broadwayd.vs10.sourcefiles	\
broadwayd.vs10.sourcefiles.filters: $(BROADWAYD_C_SRCS)

gtk3-demo.sourcefiles	\
gtk3-demo.vs10.sourcefiles	\
gtk3-demo.vs10.sourcefiles.filters: $(demo_actual_sources) $(more_demo_sources)

gdk-3.sourcefiles gdk3-win32.sourcefiles	\
gdk3-broadway.sourcefiles gailutil-3.sourcefiles	\
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
vs9\$(GTK_VS9_PROJ).pre: gtk-3.sourcefiles vs9\$(GTK_VS9_PROJ)in
vs9\$(GAILUTIL_VS9_PROJ): gailutil-3.sourcefiles vs9\$(GAILUTIL_VS9_PROJ)in

vs10\$(GDK_VS1X_PROJ): gdk-3.vs10.sourcefiles vs10\$(GDK_VS1X_PROJ)in
vs10\$(GDKWIN32_VS1X_PROJ).pre: gdk3-win32.vs10.sourcefiles vs10\$(GDKWIN32_VS1X_PROJ)in
vs10\$(GDKBROADWAY_VS1X_PROJ): gdk3-broadway.vs10.sourcefiles vs10\$(GDKBROADWAY_VS1X_PROJ)in
vs10\$(GTK_VS1X_PROJ).pre: gtk-3.vs10.sourcefiles vs10\$(GTK_VS1X_PROJ)in
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
vs9\$(GTK_VS9_PROJ).pre	\
vs9\$(GAILUTIL_VS9_PROJ)	\
vs9\$(BROADWAYD_VS9_PROJ)	\
vs9\$(DEMO_VS9_PROJ).pre	\
vs10\$(GDKWIN32_VS1X_PROJ).pre	\
vs10\$(GDKBROADWAY_VS1X_PROJ)	\
vs10\$(GDK_VS1X_PROJ)	\
vs10\$(GTK_VS1X_PROJ).pre	\
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

# Generate the gtk-3 project with or without using the older PangoFT2 +
# HarfBuzz APIs for the font features support (this code is not used if
# Pango 1.44.0 and HarfBuzz 2.2.0 or later are used)
!ifdef FONT_FEATURES_USE_PANGOFT2
vs9\$(GTK_VS9_PROJ): vs9\$(GTK_VS9_PROJ).pre2
	@echo Generating final $@ using older PangoFT2 APIs...
	@$(PYTHON) replace.py -a=replace-str -i=$** -o=$@	\
	--instring="AdditionalDependencies=\"$$(" 	\
	--outstring="AdditionalDependencies=\"$(DEMO_DEP_LIBS_PANGOFT2_VS9) $$("
	@del $**

vs10\$(GTK_VS1X_PROJ): vs10\$(GTK_VS1X_PROJ).pre2
	@echo Generating final $@ using older PangoFT2 APIs...
	@$(PYTHON) replace.py -a=replace-str -i=$** -o=$@	\
	--instring=";%(AdditionalDependencies)<"	\
	--outstring=";$(DEMO_DEP_LIBS_PANGOFT2_VS1X);%(AdditionalDependencies)<"
	@del $**

vs9\$(GTK_VS9_PROJ).pre2: vs9\$(GTK_VS9_PROJ).pre
	@$(PYTHON) replace.py -a=replace-str -i=$** -o=$@	\
	--instring="$$(GtkDefines" 	\
	--outstring="HAVE_HARFBUZZ;HAVE_PANGOFT;$$(GtkDefines"
	@del $**

vs10\$(GTK_VS1X_PROJ).pre2: vs10\$(GTK_VS1X_PROJ).pre
	@$(PYTHON) replace.py -a=replace-str -i=$** -o=$@	\
	--instring="$$(GtkDefines);%"	\
	--outstring="HAVE_HARFBUZZ;HAVE_PANGOFT;$$(GtkDefines);%"
	@del $**
!else
vs9\$(GTK_VS9_PROJ): vs9\$(GTK_VS9_PROJ).pre
vs10\$(GTK_VS1X_PROJ): vs10\$(GTK_VS1X_PROJ).pre

vs9\$(GTK_VS9_PROJ) vs10\$(GTK_VS1X_PROJ):
	@echo Generating final $@...
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

regenerate-demos-h-win32-msg:
	@echo (Re-)Generating demos.h.win32 $(DEMO_MSG)...

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

regenerate-gtk-vsproj-msg:
	@echo Regenerating GTK and gailutil projects...

regenerate-gdk-vsproj: regenerate-gdk-vsproj-msg $(GTK3_GDK_VCPROJS)
regenerate-gtk-vsproj: regenerate-gtk-vsproj-msg $(GTK3_GTK_VCPROJS) $(GTK3_GAILUTIL_VCPROJS)

regenerate-all-msvc-projs:	\
	copy-update-static-projects	\
	regenerate-gdk-vsproj	\
	regenerate-gtk-vsproj	\
	regenerate-demos-h-win32

.SUFFIXES: .vcxproj .filters .sln
