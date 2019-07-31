# NMake Makefile to build Introspection Files for GTK+

!include detectenv-msvc.mak
!include config-msvc.mak

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

vs$(VSVER)\$(CFG)\$(PLAT)\bin\Gdk-$(APIVERSION).gir: vs$(VSVER)\$(CFG)\$(PLAT)\bin\gdk-$(APIVERSION).lib gdk-introspect-list
	@echo Generating $@...
	$(PYTHON) $(G_IR_SCANNER) --verbose -no-libtool	\
	--namespace=Gdk --nsversion=$(APIVERSION) --library=gdk-$(APIVERSION)	\
	--add-include-path=./$(@D:\=/) --add-include-path=$(G_IR_INCLUDEDIR)	\
	--include=Gio-2.0 --include=GdkPixbuf-2.0 --include=Pango-1.0 --include=cairo-1.0	\
	--pkg-export=gdk-$(APIVERSION)	\
	--cflags-begin	\
	$(GDK_PREPROCESSOR_FLAGS)	\
	--cflags-end	\
	--c-include=gdk/gdk.h --filelist=gdk-introspect-list	\
	-L.\$(@D)	\
	-o $@

vs$(VSVER)\$(CFG)\$(PLAT)\bin\GdkWin32-$(APIVERSION).gir: vs$(VSVER)\$(CFG)\$(PLAT)\bin\Gdk-3.0.gir gdk-win32-introspect-list
	@echo Generating $@...
	$(PYTHON) $(G_IR_SCANNER) --verbose -no-libtool	\
	--namespace=GdkWin32 --nsversion=$(APIVERSION)	\
	--library=gdk-$(APIVERSION)	\
	--add-include-path=./$(@D:\=/) --add-include-path=$(G_IR_INCLUDEDIR)	\
	--cflags-begin	\
	$(GDK_PREPROCESSOR_FLAGS)	\
	--cflags-end	\
	--identifier-prefix=Gdk --c-include=gdk/gdkwin32.h --include-uninstalled=./$(@D:\=/)/Gdk-3.0.gir	\
	--filelist=gdk-win32-introspect-list	\
	-L.\$(@D)	\
	-o $@

vs$(VSVER)\$(CFG)\$(PLAT)\bin\Gtk-3.0.gir: vs$(VSVER)\$(CFG)\$(PLAT)\bin\gtk-3.0.lib vs$(VSVER)\$(CFG)\$(PLAT)\bin\Gdk-3.0.gir gtk-introspect-list
	@echo Generating $@...
	$(PYTHON) $(G_IR_SCANNER) --verbose -no-libtool	\
	--namespace=Gtk	--nsversion=$(APIVERSION)	\
	--library=gtk-$(APIVERSION) --library=gdk-$(APIVERSION)	\
	--add-include-path=./$(@D:\=/) --add-include-path=$(G_IR_INCLUDEDIR)	\
	--include=Atk-1.0	\
	--pkg-export=gtk+-$(APIVERSION)	\
	--cflags-begin	\
	-DG_LOG_USE_STRUCTURED=1 $(GTK_PREPROCESSOR_FLAGS:\"=\\\")	\
	--cflags-end	\
	--warn-all --include-uninstalled=./$(@D:\=/)/Gdk-3.0.gir	\
	--filelist=gtk-introspect-list	\
	-L.\$(@D)	\
	-o $@

vs$(VSVER)\$(CFG)\$(PLAT)\bin\Gdk-3.0.typelib: vs$(VSVER)\$(CFG)\$(PLAT)\bin\Gdk-3.0.gir
vs$(VSVER)\$(CFG)\$(PLAT)\bin\GdkWin32-3.0.typelib: vs$(VSVER)\$(CFG)\$(PLAT)\bin\Gdk-3.0.typelib vs$(VSVER)\$(CFG)\$(PLAT)\bin\GdkWin32-3.0.gir
vs$(VSVER)\$(CFG)\$(PLAT)\bin\Gtk-3.0.typelib: vs$(VSVER)\$(CFG)\$(PLAT)\bin\Gdk-3.0.typelib vs$(VSVER)\$(CFG)\$(PLAT)\bin\Gtk-3.0.gir

vs$(VSVER)\$(CFG)\$(PLAT)\bin\Gdk-3.0.typelib	\
vs$(VSVER)\$(CFG)\$(PLAT)\bin\GdkWin32-3.0.typelib	\
vs$(VSVER)\$(CFG)\$(PLAT)\bin\Gtk-3.0.typelib:
	@-echo Compiling $@...
	$(G_IR_COMPILER) --includedir=$(@D:\=/) --debug --verbose $(@R:\=/).gir -o $@

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
	@-del /f/q *-list
