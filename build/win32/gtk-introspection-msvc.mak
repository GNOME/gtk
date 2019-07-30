# NMake Makefile to build Introspection Files for GTK+

!include detectenv-msvc.mak
!include ..\..\gdk\Makefile.inc
!include ..\..\gtk\a11y\Makefile.inc
!include ..\..\gtk\deprecated\Makefile.inc
!include ..\..\gtk\inspector\Makefile.inc
!include ..\..\gtk\Makefile.inc

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
!else
AT_PLAT=i686
!endif

all: setgirbuildenv $(built_install_girs) $(built_install_typelibs)

setgirbuildenv:
	@set PYTHONPATH=$(PREFIX)\lib\gobject-introspection
	@set PATH=vs$(VSVER)\$(CFG)\$(PLAT)\bin;$(PREFIX)\bin;$(PATH)
	@set PKG_CONFIG_PATH=$(PKG_CONFIG_PATH)
	@set LIB=vs$(VSVER)\$(CFG)\$(PLAT)\bin;$(LIB)

gdk-introspect-list: ..\..\gdk\Makefile.inc
	@echo Generating $@...
	@$(PYTHON) -c "srcs='$(gdk_public_h_sources:		=) $(gdk_deprecated_h_sources:		=)';filter=['gdkkeysyms-compat.h'];result=[l for l in srcs.replace('/','\\').split() if l not in filter];result[0]='..\\..\\gdk\\' + result[0];f=open('$@', 'a');f.write('\n..\\..\\gdk\\'.join(result));f.close()"
	@$(PYTHON) -c "srcs='$(gdk_c_sources:		=) $(gdk_built_sources:		=)';result=srcs.replace('/','\\').split();result[0]='\n..\\..\\gdk\\' + result[0];f=open('$@', 'a');f.write('\n..\\..\\gdk\\'.join(result));f.close()"

gdk-win32-introspect-list: ..\..\gdk\Makefile.inc
	@echo Generating $@...
	@$(PYTHON) -c "srcs='$(w32_introspection_files:		=)';result=srcs.replace('/','\\').split();result[0]='..\\..\\gdk\\' + result[0];f=open('$@', 'a');f.write('\n..\\..\\gdk\\'.join(result));f.close()"

gtk-introspect-list: ..\..\gtk\Makefile.inc
	@echo Generating $@...
	@$(PYTHON) -c "srcs='$(gtk_public_h_sources:		=) $(gtk_semi_private_h_sources:		=) $(gtk_built_public_sources:		=) gtkversion.h';filter=['gtktextdisplay.h', 'gtktextlayout.h', 'gtkx.h'];result=[l for l in srcs.replace('/','\\').split() if l not in filter];result[0]='..\\..\\gtk\\' + result[0];f=open('$@', 'a');f.write('\n..\\..\\gtk\\'.join(result));f.close()"
	@$(PYTHON) -c "srcs='$(a11y_h_sources:		=) $(gtk_deprecated_h_sources:		=)';result=srcs.replace('/','\\').split();result[0]='\n..\\..\\gtk\\' + result[0];f=open('$@', 'a');f.write('\n..\\..\\gtk\\'.join(result));f.close()"
	@$(PYTHON) -c "import re;srcs='$(a11y_c_sources:		=) $(gtk_deprecated_c_sources:		=) $(inspector_c_sources:		=) $(gtk_base_c_sources_base:		=)';filter=re.compile('(.*)win32.c');result=[l for l in srcs.replace('/','\\').split() if not filter.match(l)];result[0]='\n..\\..\\gtk\\' + result[0];f=open('$@', 'a');f.write('\n..\\..\\gtk\\'.join(result));f.close()"
	@$(PYTHON) -c "srcs='gtkprintoperation-unix.c gtktypebuiltins.h gtktypebuiltins.c';result=srcs.replace('/','\\').split();result[0]='\n..\\..\\gtk\\' + result[0];f=open('$@', 'a');f.write('\n..\\..\\gtk\\'.join(result));f.close()"

!include introspection.body.mak

install-introspection: all
	@-copy vs$(VSVER)\$(CFG)\$(PLAT)\bin\*.gir "$(G_IR_INCLUDEDIR)"
	@-copy /b vs$(VSVER)\$(CFG)\$(PLAT)\bin\*.typelib "$(G_IR_TYPELIBDIR)"

!else
all:
	@-echo $(ERROR_MSG)
!endif

clean:
	@-del /f/q vs$(VSVER)\$(CFG)\$(PLAT)\bin\*.typelib
	@-del /f/q vs$(VSVER)\$(CFG)\$(PLAT)\bin\*.gir
