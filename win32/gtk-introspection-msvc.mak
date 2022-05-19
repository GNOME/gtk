# NMake Makefile to build Introspection Files for GTK+

!include detectenv-msvc.mak
!include generate-msvc.mak

APIVERSION = 3.0

CHECK_PACKAGE = gdk-pixbuf-2.0 atk pangocairo gio-2.0

built_install_girs =	\
	vs$(VSVER)\$(CFG)\$(PLAT)\bin\Gdk-$(APIVERSION).gir	\
	vs$(VSVER)\$(CFG)\$(PLAT)\bin\GdkWin32-$(APIVERSION).gir	\
	vs$(VSVER)\$(CFG)\$(PLAT)\bin\Gtk-$(APIVERSION).gir

built_install_typelibs =	\
	vs$(VSVER)\$(CFG)\$(PLAT)\bin\Gdk-$(APIVERSION).typelib	\
	vs$(VSVER)\$(CFG)\$(PLAT)\bin\GdkWin32-$(APIVERSION).typelib	\
	vs$(VSVER)\$(CFG)\$(PLAT)\bin\Gtk-$(APIVERSION).typelib

!include introspection-msvc.mak

!if "$(BUILD_INTROSPECTION)" == "TRUE"

!if "$(PLAT)" == "x64"
AT_PLAT=x86_64
!elseif "$(PLAT)" == "arm64"
AT_PLAT=aarch64
!else
AT_PLAT=i686
!endif

vs$(VSVER)\$(CFG)\$(PLAT)\obj\gdk-3\Gdk-$(APIVERSION).gir.filelist: $(GDK_PUBLIC_HEADERS) $(GDK_C_SRCS) $(GDK_GENERATED_SOURCES)
	@if exist $@ del $@
	@for %f in ($(gdk_h_sources:/=\) $(gdk_c_sources:/=\)) do @if not "%f" == "gdkkeysyms-compat.h" echo ..\gdk\%f>>$@
	@for %f in ($(GDK_GENERATED_SOURCES)) do @if not "%~nxf" == "gdkconfig.h" echo %f>>$@

vs$(VSVER)\$(CFG)\$(PLAT)\obj\gdk3-win32\GdkWin32-$(APIVERSION).gir.filelist: $(GDK_WIN32_INTROSPECTION_SRCS)
	@if exist $@ del $@
	@for %f in ($(w32_introspection_files:/=\)) do @echo ..\gdk\%f>>$@

vs$(VSVER)\$(CFG)\$(PLAT)\obj\gtk-3\Gtk-$(APIVERSION).gir.filelist:	\
$(GTK_PUBLIC_ENUM_HEADERS) $(GTK_SEMI_PRIVATE_HEADERS)	\
$(GTK_GENERATED_PUB_HDRS) $(GTK_C_SRCS)
	@if exist $@ del $@
	@for %f in ($(GTK_PUB_HDRS:.h=)) do @if not "%f" == "gtktextdisplay" if not "%f" == "gtkx" echo ..\gtk\%f.h>>$@
	@for %f in ($(a11y_h_sources:/=\) $(gtk_deprecated_h_sources:/=\) $(gtk_semi_private_h_sources)) do @if not "%f" == "gtktextlayout.h" echo ..\gtk\%f>>$@
	@for %f in ($(a11y_c_sources:/=\) $(gtk_deprecated_c_sources:/=\) $(inspector_c_sources:/=\)) do @echo ..\gtk\%f>>$@
	@for %f in ($(gtk_base_c_sources_base_gtka_gtkh:.c=)) do @echo ..\gtk\%f.c>>$@
	@for %f in ($(gtk_base_c_sources_base_gtki_gtkw:.c=)) do @echo ..\gtk\%f.c>>$@
	@for %f in ($(gtk_clipboard_dnd_c_sources_generic) $(gtk_os_win32_c_sources)) do @echo ..\gtk\%f>>$@
	@for %f in ($(GTK_TYPEBUILTIN_SOURCES) $(GTK_VERSION_H)) do @echo %f>>$@

vs$(VSVER)\$(CFG)\$(PLAT)\bin\Gdk-$(APIVERSION).gir: vs$(VSVER)\$(CFG)\$(PLAT)\obj\gdk-3\Gdk-$(APIVERSION).gir.filelist
	@-echo Generating $@...
	@set PYTHONPATH=$(PREFIX)\lib\gobject-introspection
	@set PATH=vs$(VSVER)\$(CFG)\$(PLAT)\bin;$(PREFIX)\bin;$(PATH)
	@set PKG_CONFIG_PATH=$(PKG_CONFIG_PATH)
	@set LIB=vs$(VSVER)\$(CFG)\$(PLAT)\bin;$(LIB)
	@$(PYTHON) $(G_IR_SCANNER) --verbose -no-libtool	\
	--namespace=Gdk	--nsversion=$(APIVERSION)	\
	--library=gdk-$(APIVERSION)	\
	--add-include-path=./vs$(VSVER)/$(CFG)/$(PLAT)/bin	\
	--add-include-path=$(G_IR_INCLUDEDIR)	\
	--include=Gio-2.0 --include=GdkPixbuf-2.0 --include=Pango-1.0 --include=cairo-1.0	\
	--cflags-begin	\
	-DG_LOG_USE_STRUCTURED=1 -DGDK_COMPILATION -I./vs$(VSVER)/$(CFG)/$(PLAT)/obj/gdk-3	\
	-I.. -I./vs$(VSVER)/$(CFG)/$(PLAT)/obj/gdk-3/gdk -I../gdk -I../gdk/win32	\
	--cflags-end	\
	--c-include=gdk/gdk.h	\
	--filelist=$**	\
	-L.\vs$(VSVER)\$(CFG)\$(PLAT)\bin	\
	-o $@

vs$(VSVER)\$(CFG)\$(PLAT)\bin\GdkWin32-$(APIVERSION).gir:	\
./vs$(VSVER)/$(CFG)/$(PLAT)/bin/Gdk-$(APIVERSION).gir	\
vs$(VSVER)\$(CFG)\$(PLAT)\obj\gdk3-win32\GdkWin32-$(APIVERSION).gir.filelist
	@-echo Generating $@...
	@set PYTHONPATH=$(PREFIX)\lib\gobject-introspection
	@set PATH=vs$(VSVER)\$(CFG)\$(PLAT)\bin;$(PREFIX)\bin;$(PATH)
	@set PKG_CONFIG_PATH=$(PKG_CONFIG_PATH)
	@set LIB=vs$(VSVER)\$(CFG)\$(PLAT)\bin;$(LIB)
	$(PYTHON) $(G_IR_SCANNER) --verbose -no-libtool	\
	--namespace=GdkWin32 --nsversion=$(APIVERSION)	\
	--library=gdk-$(APIVERSION)	\
	--add-include-path=./vs$(VSVER)/$(CFG)/$(PLAT)/bin	\
	--add-include-path=$(G_IR_INCLUDEDIR)	\
	--cflags-begin	\
	-DG_LOG_USE_STRUCTURED=1 -DGDK_COMPILATION -I./vs$(VSVER)/$(CFG)/$(PLAT)/obj/gdk-3	\
	-I.. -I./vs$(VSVER)/$(CFG)/$(PLAT)/obj/gdk-3/gdk -I../gdk -I../gdk/win32	\
	--cflags-end	\
	--identifier-prefix=Gdk --c-include=gdk/gdkwin32.h	\
	--include-uninstalled=./vs$(VSVER)/$(CFG)/$(PLAT)/bin/Gdk-$(APIVERSION).gir	\
	--filelist=vs$(VSVER)\$(CFG)\$(PLAT)\obj\gdk3-win32\GdkWin32-$(APIVERSION).gir.filelist	\
	-L.\vs$(VSVER)\$(CFG)\$(PLAT)\bin	\
	-o $@

vs$(VSVER)\$(CFG)\$(PLAT)\bin\Gtk-$(APIVERSION).gir:	\
./vs$(VSVER)/$(CFG)/$(PLAT)/bin/Gdk-$(APIVERSION).gir	\
vs$(VSVER)\$(CFG)\$(PLAT)\obj\gtk-3\Gtk-$(APIVERSION).gir.filelist
	@-echo Generating $@...
	@set PYTHONPATH=$(PREFIX)\lib\gobject-introspection
	@set PATH=vs$(VSVER)\$(CFG)\$(PLAT)\bin;$(PREFIX)\bin;$(PATH)
	@set PKG_CONFIG_PATH=$(PKG_CONFIG_PATH)
	@set LIB=vs$(VSVER)\$(CFG)\$(PLAT)\bin;$(LIB)
	$(PYTHON) $(G_IR_SCANNER) --verbose -no-libtool	\
	--namespace=Gtk --nsversion=$(APIVERSION)	\
	--library=gtk-$(APIVERSION) --library=gdk-$(APIVERSION)	\
	--add-include-path=./vs$(VSVER)/$(CFG)/$(PLAT)/bin	\
	--add-include-path=$(G_IR_INCLUDEDIR)	\
	--include=Atk-1.0	\
	--pkg-export=gtk+-$(APIVERSION)	\
	--cflags-begin	\
	-DG_LOG_USE_STRUCTURED=1 -DGTK_VERSION="$(GTK_VERSION)" -DGTK_BINARY_VERSION="3.0.0" -DGTK_COMPILATION	\
	-DGTK_PRINT_BACKEND_ENABLE_UNSUPPORTED -DGTK_LIBDIR=\"/dummy/lib\" -DGTK_DATADIR=\"/dummy/share\"	\
	-DGTK_DATA_PREFIX=\"/dummy\" -DGTK_SYSCONFDIR=\"/dummy/etc\" -DGTK_HOST=\"$(AT_PLAT)-pc-vs$(VSVER)\"	\
	-DGTK_PRINT_BACKENDS=\"file\" -DINCLUDE_IM_am_et -DINCLUDE_IM_cedilla -DINCLUDE_IM_cyrillic_translit	\
	-DINCLUDE_IM_ime -DINCLUDE_IM_inuktitu -DINCLUDE_IM_ipa -DINCLUDE_IM_multipress -DINCLUDE_IM_thai	\
	-DINCLUDE_IM_ti_er -DINCLUDE_IM_ti_et -DINCLUDE_IM_viqr	\
	-DGTK_TEXT_USE_INTERNAL_UNSUPPORTED_API	\
	-I./vs$(VSVER)/$(CFG)/$(PLAT)/obj/gtk-3	\
	-I./vs$(VSVER)/$(CFG)/$(PLAT)/obj/gdk-3	\
	-I.. -I./vs$(VSVER)/$(CFG)/$(PLAT)/obj/gtk-3/gtk -I../gtk	\
	-I./vs$(VSVER)/$(CFG)/$(PLAT)/obj/gdk-3/gdk -I../gdk	\
	--cflags-end	\
	--warn-all --include-uninstalled=./vs$(VSVER)/$(CFG)/$(PLAT)/bin/Gdk-$(APIVERSION).gir	\
	--filelist=vs$(VSVER)\$(CFG)\$(PLAT)\obj\gtk-3\Gtk-$(APIVERSION).gir.filelist	\
	-L.\vs$(VSVER)\$(CFG)\$(PLAT)\bin	\
	-o $@

vs$(VSVER)\$(CFG)\$(PLAT)\bin\Gdk-$(APIVERSION).typelib: vs$(VSVER)\$(CFG)\$(PLAT)\bin\Gdk-$(APIVERSION).gir
vs$(VSVER)\$(CFG)\$(PLAT)\bin\GdkWin32-$(APIVERSION).typelib: vs$(VSVER)\$(CFG)\$(PLAT)\bin\GdkWin32-$(APIVERSION).gir
vs$(VSVER)\$(CFG)\$(PLAT)\bin\Gtk-$(APIVERSION).typelib: vs$(VSVER)\$(CFG)\$(PLAT)\bin\Gtk-$(APIVERSION).gir

$(built_install_typelibs):
	@-echo Compiling $@...
	$(G_IR_COMPILER) --includedir=$(@D:\=/) --debug --verbose $(@R:\=/).gir	-o $@

introspect: $(built_install_girs) $(built_install_typelibs)

!else
introspect:
	@-echo $(ERROR_MSG)
!endif

introspect-clean:
	@-del /f/q vs$(VSVER)\$(CFG)\$(PLAT)\obj\gtk-3\Gtk-$(APIVERSION).gir.filelist
	@-del /f/q vs$(VSVER)\$(CFG)\$(PLAT)\obj\gdk3-win32\GdkWin32-$(APIVERSION).gir.filelist
	@-del /f/q vs$(VSVER)\$(CFG)\$(PLAT)\obj\gdk-3\Gdk-$(APIVERSION).gir.filelist
	@-del /f/q vs$(VSVER)\$(CFG)\$(PLAT)\bin\*.typelib
	@-del /f/q vs$(VSVER)\$(CFG)\$(PLAT)\bin\*.gir
