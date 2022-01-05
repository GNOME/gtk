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

introspect: setgirbuildenv $(built_install_girs) $(built_install_typelibs)

setgirbuildenv:
	@set PYTHONPATH=$(PREFIX)\lib\gobject-introspection
	@set PATH=vs$(VSVER)\$(CFG)\$(PLAT)\bin;$(PREFIX)\bin;$(PATH)
	@set PKG_CONFIG_PATH=$(PKG_CONFIG_PATH)
	@set LIB=vs$(VSVER)\$(CFG)\$(PLAT)\bin;$(LIB)

!include introspection.body.mak

install-introspection: introspect
	@-copy vs$(VSVER)\$(CFG)\$(PLAT)\bin\*.gir "$(G_IR_INCLUDEDIR)"
	@-copy /b vs$(VSVER)\$(CFG)\$(PLAT)\bin\*.typelib "$(G_IR_TYPELIBDIR)"

!else
introspect:
	@-echo $(ERROR_MSG)
!endif

introspect-clean:
	@-del /f/q Gtk_3_0_gir_list_final
	@-del /f/q GdkWin32_3_0_gir_list_final
	@-del /f/q Gdk_3_0_gir_list_final
	@-del /f/q vs$(VSVER)\$(CFG)\$(PLAT)\bin\*.typelib
	@-del /f/q vs$(VSVER)\$(CFG)\$(PLAT)\bin\*.gir
