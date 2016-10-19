# NMake Makefile to build Introspection Files for GTK+

!include detectenv-msvc.mak

APIVERSION = 4.0

CHECK_PACKAGE = gdk-pixbuf-2.0 atk pangocairo gio-2.0

built_install_girs = Gdk-$(APIVERSION).gir GdkWin32-$(APIVERSION).gir Gtk-$(APIVERSION).gir
built_install_typelibs = Gdk-$(APIVERSION).typelib GdkWin32-$(APIVERSION).typelib Gtk-$(APIVERSION).typelib

!include introspection-msvc.mak

!if "$(BUILD_INTROSPECTION)" == "TRUE"

!if "$(PLAT)" == "x64"
AT_PLAT=x86_64
!else
AT_PLAT=i686
!endif

all: setgirbuildenv $(built_install_girs) $(built_install_typelibs)

setgirbuildenv:
	@set PYTHONPATH=$(PREFIX)\lib\gobject-introspection
	@set PATH=vs$(VSVER)\$(CFG)\$(PLAT)\bin;$(PREFIX)\bin;$(PATH)
	@set PKG_CONFIG_PATH=$(PKG_CONFIG_PATH)
	@set LIB=vs$(VSVER)\$(CFG)\$(PLAT)\bin;$(LIB)

!include introspection.body.mak

install-introspection: all 
	@-copy *.gir $(G_IR_INCLUDEDIR)
	@-copy /b *.typelib $(G_IR_TYPELIBDIR)

!else
all:
	@-echo $(ERROR_MSG)
!endif

clean:
	@-del /f/q *.typelib
	@-del /f/q *.gir
