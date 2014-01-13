# NMake Makefile to build Introspection Files for ATK

# Change or pass in as a variable/env var if needed
GDK_DLLNAME = gdk-3-vs$(VSVER)
GTK_DLLNAME = gtk-3-vs$(VSVER)

# Please do not change anything after this line

!include detectenv_msvc.mak

APIVERSION = 3.0

CHECK_PACKAGE = gdk-pixbuf-2.0 atk pangocairo gio-2.0

!if "$(PLAT)" == "x64"
TIME_T_DEFINE = -Dtime_t=long long
!else
TIME_T_DEFINE = -Dtime_t=long
!endif

!include introspection-msvc.mak

!if "$(BUILD_INTROSPECTION)" == "TRUE"
all: setgirbuildnev Gdk-$(APIVERSION).gir Gdk-$(APIVERSION).typelib Gtk-$(APIVERSION).gir Gtk-$(APIVERSION).typelib

gdk_list gtk_list:
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
	--no-libtool --library=$(GDK_DLLNAME)	\
	--reparse-validate --add-include-path=$(G_IR_INCLUDEDIR) --add-include-path=.	\
	--pkg-export gdk-3.0 --warn-all --c-include="gdk/gdk.h"	\
	-DG_LOG_DOMAIN=\"Gdk\" -DGDK_COMPILATION	\
	--filelist=gdk_list	-o Gdk-3.0.gir

Gtk-$(APIVERSION).gir: gtk_list
	$(PYTHON2) $(G_IR_SCANNER) --verbose -I.. -I..\gtk -I..\gdk	\
	-I$(BASEDIR)\include\glib-2.0 -I$(BASEDIR)\lib\glib-2.0\include	\
	-I$(BASEDIR)\include\pango-1.0 -I$(BASEDIR)\include\atk-1.0	\
	-I$(BASEDIR)\include\gdk-pixbuf-2.0 -I$(BASEDIR)\include	\
	--namespace=Gtk --nsversion=3.0	\
	--include=Atk-1.0	\
	--include-uninstalled=./Gdk-$(APIVERSION).gir	\
	--no-libtool --library=$(GTK_DLLNAME)	\
	--reparse-validate --add-include-path=$(G_IR_INCLUDEDIR) --add-include-path=.	\
	--pkg-export gtk+-3.0 --warn-all --c-include="gtk/gtkx.h"	\
	-DG_LOG_DOMAIN=\"Gtk\" -DGTK_LIBDIR=\"/dummy/lib\"	\
	$(TIME_T_DEFINE) -DGTK_DATADIR=\"/dummy/share\" -DGTK_DATA_PREFIX=\"/dummy\"	\
	-DGTK_SYSCONFDIR=\"/dummy/etc\" -DGTK_VERSION=\"3.11.4\"	\
	-DGTK_BINARY_VERSION=\"3.0.0\" -DGTK_HOST=\"i686-pc-vs$(VSVER)\"	\
	-DGTK_COMPILATION -DGTK_PRINT_BACKENDS=\"file\"	\
	-DGTK_PRINT_PREVIEW_COMMAND=\"undefined-gtk-print-preview-command\"	\
	-DGTK_FILE_SYSTEM_ENABLE_UNSUPPORTED -DGTK_PRINT_BACKEND_ENABLE_UNSUPPORTED	\
	-DINCLUDE_IM_am_et -DINCLUDE_IM_cedilla -DINCLUDE_IM_cyrillic_translit	\
	-DINCLUDE_IM_ime -DINCLUDE_IM_inuktitut -DINCLUDE_IM_ipa	\
	-DINCLUDE_IM_multipress -DINCLUDE_IM_thai -DINCLUDE_IM_ti_er	\
	-DINCLUDE_IM_ti_et -DINCLUDE_IM_viqr --filelist=gtk_list	\
	-o Gtk-3.0.gir

Gdk-$(APIVERSION).typelib: Gdk-$(APIVERSION).gir
	@-echo Compiling Gdk-$(APIVERSION).typelib...
	$(G_IR_COMPILER) --includedir=. --debug --verbose Gdk-$(APIVERSION).gir -o Gdk-$(APIVERSION).typelib

Gtk-$(APIVERSION).typelib: Gtk-$(APIVERSION).gir Gdk-$(APIVERSION).typelib
	@-echo Compiling Gtk-$(APIVERSION).typelib...
	$(G_IR_COMPILER) --includedir=. --debug --verbose Gtk-$(APIVERSION).gir -o Gtk-$(APIVERSION).typelib

install-introspection: setgirbuildnev Gdk-$(APIVERSION).gir Gdk-$(APIVERSION).typelib Gtk-$(APIVERSION).gir Gtk-$(APIVERSION).typelib
	@-copy Gdk-$(APIVERSION).gir $(G_IR_INCLUDEDIR)
	@-copy /b Gdk-$(APIVERSION).typelib $(G_IR_TYPELIBDIR)
	@-copy Gtk-$(APIVERSION).gir $(G_IR_INCLUDEDIR)
	@-copy /b Gtk-$(APIVERSION).typelib $(G_IR_TYPELIBDIR)

!else
all:
	@-echo $(ERROR_MSG)
!endif

clean:
	@-del /f/q Gtk-$(APIVERSION).typelib
	@-del /f/q Gtk-$(APIVERSION).gir
	@-del /f/q Gdk-$(APIVERSION).typelib
	@-del /f/q Gdk-$(APIVERSION).gir
	@-del /f/q gtk_list
	@-del /f/q gdk_list
	@-del /f/q *.pyc
