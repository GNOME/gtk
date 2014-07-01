# NMake Makefile to build Introspection Files for GTK+

!include detectenv_msvc.mak

APIVERSION = 3.0

CHECK_PACKAGE = gdk-pixbuf-2.0 atk pangocairo gio-2.0

built_install_girs = Gdk-$(APIVERSION).gir GdkWin32-$(APIVERSION).gir Gtk-$(APIVERSION).gir
built_install_typelibs = Gdk-$(APIVERSION).typelib GdkWin32-$(APIVERSION).typelib Gtk-$(APIVERSION).typelib

!if "$(PLAT)" == "x64"
TIME_T_DEFINE = -Dtime_t=long long
!else
TIME_T_DEFINE = -Dtime_t=long
!endif

!include introspection-msvc.mak

!if "$(BUILD_INTROSPECTION)" == "TRUE"
all: setgirbuildnev $(built_install_girs) $(built_install_typelibs)

gdk_list gdkwin32_list gtk_list:
	@-echo Generating Filelist to Introspect for GDK/GTK...
	$(PYTHON2) gen-file-list-gtk.py

setgirbuildnev:
	@set CC=$(CC)
	@set PYTHONPATH=$(BASEDIR)\lib\gobject-introspection
	@set PATH=win32\vs$(VSVER)\$(CFG)\$(PLAT)\bin;$(BASEDIR)\bin;$(PATH);$(MINGWDIR)\bin
	@set PKG_CONFIG_PATH=$(PKG_CONFIG_PATH)
	@set LIB=win32\vs$(VSVER)\$(CFG)\$(PLAT)\bin;$(LIB)

Gdk-$(APIVERSION).gir: gdk_list
	@-echo Generating Gdk-$(APIVERSION).gir...
	$(PYTHON2) $(G_IR_SCANNER) --verbose -I.. -I..\gdk	\
	-I$(BASEDIR)\include\glib-2.0 -I$(BASEDIR)\lib\glib-2.0\include	\
	-I$(BASEDIR)\include\pango-1.0 -I$(BASEDIR)\include\atk-1.0	\
	-I$(BASEDIR)\include\gdk-pixbuf-2.0 -I$(BASEDIR)\include	\
	$(TIME_T_DEFINE) --namespace=Gdk --nsversion=3.0	\
	--include=Gio-2.0 --include=GdkPixbuf-2.0	\
	--include=Pango-1.0 --include=cairo-1.0	\
	--no-libtool --library=gdk-3.0	\
	--reparse-validate --add-include-path=$(G_IR_INCLUDEDIR) --add-include-path=.	\
	--pkg-export gdk-3.0 --warn-all --c-include="gdk/gdk.h"	\
	-DG_LOG_DOMAIN=\"Gdk\" -DGDK_COMPILATION	\
	--filelist=gdk_list	-o $@

GdkWin32-$(APIVERSION).gir: gdkwin32_list
	@-echo Generating GdkWin32-$(APIVERSION).gir...
	$(PYTHON2) $(G_IR_SCANNER) --verbose -I.. -I..\gdk	\
	-I$(BASEDIR)\include\glib-2.0 -I$(BASEDIR)\lib\glib-2.0\include	\
	-I$(BASEDIR)\include\pango-1.0 -I$(BASEDIR)\include\atk-1.0	\
	-I$(BASEDIR)\include\gdk-pixbuf-2.0 -I$(BASEDIR)\include	\
	$(TIME_T_DEFINE) --namespace=GdkWin32 --nsversion=3.0	\
	--include=Gio-2.0 --include=GdkPixbuf-2.0	\
	--include=Pango-1.0	--include-uninstalled=./Gdk-$(APIVERSION).gir	\
	--no-libtool --library=gdk-3.0	\
	--reparse-validate --add-include-path=$(G_IR_INCLUDEDIR) --add-include-path=.	\
	--pkg-export gdk-win32-3.0 --warn-all --c-include="gdk/gdkwin32.h"	\
	-DG_LOG_DOMAIN=\"Gdk\" -DGDK_COMPILATION	\
	--filelist=gdkwin32_list	-o $@

Gtk-$(APIVERSION).gir: gtk_list
	$(PYTHON2) $(G_IR_SCANNER) --verbose -I.. -I..\gtk -I..\gdk	\
	-I$(BASEDIR)\include\glib-2.0 -I$(BASEDIR)\lib\glib-2.0\include	\
	-I$(BASEDIR)\include\pango-1.0 -I$(BASEDIR)\include\atk-1.0	\
	-I$(BASEDIR)\include\gdk-pixbuf-2.0 -I$(BASEDIR)\include	\
	--namespace=Gtk --nsversion=3.0	\
	--include=Atk-1.0	\
	--include-uninstalled=./Gdk-$(APIVERSION).gir	\
	--no-libtool --library=gtk-3.0 --library=gdk-3.0	\
	--reparse-validate --add-include-path=$(G_IR_INCLUDEDIR) --add-include-path=.	\
	--pkg-export gtk+-3.0 --warn-all --c-include="gtk/gtkx.h"	\
	-DG_LOG_DOMAIN=\"Gtk\" -DGTK_LIBDIR=\"/dummy/lib\"	\
	$(TIME_T_DEFINE) -DGTK_DATADIR=\"/dummy/share\" -DGTK_DATA_PREFIX=\"/dummy\"	\
	-DGTK_SYSCONFDIR=\"/dummy/etc\" -DGTK_VERSION=\"3.12.0\"	\
	-DGTK_BINARY_VERSION=\"3.0.0\" -DGTK_HOST=\"i686-pc-vs$(VSVER)\"	\
	-DGTK_COMPILATION -DGTK_PRINT_BACKENDS=\"file\"	\
	-DGTK_PRINT_PREVIEW_COMMAND=\"undefined-gtk-print-preview-command\"	\
	-DGTK_FILE_SYSTEM_ENABLE_UNSUPPORTED -DGTK_PRINT_BACKEND_ENABLE_UNSUPPORTED	\
	-DINCLUDE_IM_am_et -DINCLUDE_IM_cedilla -DINCLUDE_IM_cyrillic_translit	\
	-DINCLUDE_IM_ime -DINCLUDE_IM_inuktitut -DINCLUDE_IM_ipa	\
	-DINCLUDE_IM_multipress -DINCLUDE_IM_thai -DINCLUDE_IM_ti_er	\
	-DINCLUDE_IM_ti_et -DINCLUDE_IM_viqr --filelist=gtk_list	\
	-o $@

$(built_install_typelibs): $(built_install_girs)
	@-echo Compiling $*.typelib...
	@-$(G_IR_COMPILER) --includedir=. --debug --verbose $*.gir -o $@

install-introspection: setgirbuildnev $(built_install_girs) $(built_install_typelibs)
	@-copy *.gir $(G_IR_INCLUDEDIR)
	@-copy /b *.typelib $(G_IR_TYPELIBDIR)

!else
all:
	@-echo $(ERROR_MSG)
!endif

clean:
	@-del /f/q *.typelib
	@-del /f/q *.gir
	@-del /f/q gtk_list
	@-del /f/q gdkwin32_list
	@-del /f/q gdk_list
	@-del /f/q *.pyc
