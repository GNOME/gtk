# NMake Makefile portion for copying the built files to directories as appropriate
# under $(PREFIX)

!include config-msvc.mak
!include create-lists-msvc.mak

GTK_API_VERSION = 3
BASE_BUILT_BIN_DIR = .\vs$(VSVER)\$(CFG)\$(PLAT)\bin

!ifdef BROADWAY
GDK_OUTPUT_CONFIGDIR = $(CFG)_Broadway
GDK_BROADWAY_BIN = .\vs$(VSVER)\$(GDK_OUTPUT_CONFIGDIR)\$(PLAT)\bin\broadwayd.exe
GDK_BROADWAY_PDB = $(GDK_BROADWAY_BIN:.exe=.pdb)
GEN_PC_BROADWAY_FLAG = --broadway
!else
GDK_OUTPUT_CONFIGDIR = $(CFG)
GDK_BROADWAY_BIN =
GDK_BROADWAY_PDB =
GEN_PC_BROADWAY_FLAG =
!endif

IMPLIB_SUFFIX = $(GTK_API_VERSION).0.lib
GDK_DLL = .\vs$(VSVER)\$(GDK_OUTPUT_CONFIGDIR)\$(PLAT)\bin\gdk-$(GTK_API_VERSION)-vs$(VSVER).dll
GDK_PDB = $(GDK_DLL:.dll=.pdb)
GDK_LIB = .\vs$(VSVER)\$(GDK_OUTPUT_CONFIGDIR)\$(PLAT)\bin\gdk-$(IMPLIB_SUFFIX)
GDK_BIN_ITEMS = $(GDK_DLL) $(GDK_PDB) $(GDK_BROADWAY_BIN) $(GDK_BROADWAY_PDB)
GTK_DLL_FILENAME = gtk-$(GTK_API_VERSION)-vs$(VSVER)
GTK_LIB = $(BASE_BUILT_BIN_DIR)\gtk-$(IMPLIB_SUFFIX)
LIBGAIL_UTIL_DLL_FILENAME = gailutil-$(GTK_API_VERSION)-vs$(VSVER)
LIBGAIL_UTIL_LIB = $(BASE_BUILT_BIN_DIR)\gailutil-$(IMPLIB_SUFFIX)
GTK_PROGRAMS_NAMES = builder-tool encode-symbolic-svg query-settings update-icon-cache
GTK3_PROGRAMS_NAMES = demo demo-application icon-browser widget-factory
GDK_GIR_FILE = $(BASE_BUILT_BIN_DIR)\Gdk-$(IMPLIB_SUFFIX:.lib=.gir)
GDKWIN32_GIR_FILE = $(BASE_BUILT_BIN_DIR)\GdkWin32-$(IMPLIB_SUFFIX:.lib=.gir)
GTK_GIR_FILE = $(BASE_BUILT_BIN_DIR)\Gtk-$(IMPLIB_SUFFIX:.lib=.gir)
GDK_TYPELIB_FILE = $(GDK_GIR_FILE:.gir=.typelib)
GDKWIN32_TYPELIB_FILE = $(GDKWIN32_GIR_FILE:.gir=.typelib)
GTK_TYPELIB_FILE = $(GTK_GIR_FILE:.gir=.typelib)

GTK_HEADERS_BASE_INSTALL_DIR = $(PREFIX)\include\gtk-$(GTK_API_VERSION).0
GAILUTIL_HEADERS_BASE_INSTALL_DIR = $(PREFIX)\include\gail-$(GTK_API_VERSION).0

GDK_BASE_HEADERS = $(gdk_public_h_sources)

GDK_CONFIG_H = .\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gdk-$(GTK_API_VERSION)\gdk\gdkconfig.h
GDK_ENUM_TYPES_H = .\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gdk-$(GTK_API_VERSION)\gdk\gdkenumtypes.h
GDK_VERSION_MACROS_H = .\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gdk-$(GTK_API_VERSION)\gdk\gdkversionmacros.h
GDK_GENERATED_PUBLIC_H = $(GDK_CONFIG_H) $(GDK_ENUM_TYPES_H) $(GDK_VERSION_MACROS_H)

GTK_TYPE_BULITINS_H = .\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gtk-$(GTK_API_VERSION)\gtk\gtktypebuiltins.h
GTK_VERSION_H = .\vs$(VSVER)\$(CFG)\$(PLAT)\obj\gtk-$(GTK_API_VERSION)\gtk\gtkversion.h
GTK_GENERATED_PUBLIC_H = $(GTK_TYPE_BULITINS_H) $(GTK_VERSION_H)

!ifdef INSTALL_TRANSLATIONS
DATA_TARGETS = install-data install-translations
!else
DATA_TARGETS = install-data
!endif

all: install-bin install-headers $(DATA_TARGETS)

# Copy the built files
install-bin:
	@echo Copying built binaries and data files...

# Copy the DLLs and programs and their .pdb's
	@for %d in (bin lib) do @if not exist $(PREFIX)\%d\ mkdir $(PREFIX)\%d
	@for %f in ($(GDK_BIN_ITEMS)) do @copy /b %f "$(PREFIX)\bin"
	@for %f in ($(GTK_DLL_FILENAME) $(LIBGAIL_UTIL_DLL_FILENAME)) do @for %x in (dll pdb) do @copy /b $(BASE_BUILT_BIN_DIR)\%f.%x "$(PREFIX)\bin"
	@for %f in ($(GTK_PROGRAMS_NAMES)) do @for %x in (exe pdb) do @copy /b $(BASE_BUILT_BIN_DIR)\gtk-%f.%x "$(PREFIX)\bin"
	@for %f in ($(GTK3_PROGRAMS_NAMES)) do @for %x in (exe pdb) do @copy /b $(BASE_BUILT_BIN_DIR)\gtk3-%f.%x "$(PREFIX)\bin"

# Copy the .lib's
	@for %f in ($(GDK_LIB) $(GTK_LIB) $(LIBGAIL_UTIL_LIB)) do @copy /b %f "$(PREFIX)\lib"
	@for %f in (gdk gtk gailutil) do @copy /b "$(PREFIX)\lib\%f-$(IMPLIB_SUFFIX)" "$(PREFIX)\lib\%f-$(GTK_API_VERSION).lib"

# Generate the pkg-config files, if Python is callable and if it is not disabled
	@if "$(NO_PKGCONFIG)" == "" if not exist $(PREFIX)\lib\pkgconfig\ mkdir $(PREFIX)\lib\pkgconfig
	@-if "$(NO_PKGCONFIG)" == "" if exist $(PYTHON) $(PYTHON) gtkpc.py --prefix=$(PREFIX) --version=$(GTK_VERSION) --host=$(AT_PLAT)-pc-vs$(VSVER) $(GEN_PC_BROADWAY_FLAG)
	@-if "$(NO_PKGCONFIG)" == "" if exist $(PYTHON).exe $(PYTHON) gtkpc.py --prefix=$(PREFIX) --version=$(GTK_VERSION) --host=$(AT_PLAT)-pc-vs$(VSVER) $(GEN_PC_BROADWAY_FLAG)
	@for %f in (gdk gtk+) do @if exist %f-$(GTK_API_VERSION).0.pc copy "%f-$(GTK_API_VERSION).0.pc" "$(PREFIX)\lib\pkgconfig"
	@for %f in (gail) do @if exist %f-$(GTK_API_VERSION).0.pc move %f-$(GTK_API_VERSION).0.pc $(PREFIX)\lib\pkgconfig
	@for %f in (gdk gtk+) do @if exist %f-$(GTK_API_VERSION).0.pc move %f-$(GTK_API_VERSION).0.pc $(PREFIX)\lib\pkgconfig\%f-win32-$(GTK_API_VERSION).0.pc

# Copy the introspection files, if built
	@if exist $(GTK_TYPELIB_FILE) @for %d in (lib\girepository-1.0 share\gir-1.0) do @if not exist $(PREFIX)\%d\ mkdir $(PREFIX)\%d
	@for %f in ($(GDK_GIR_FILE) $(GDKWIN32_GIR_FILE) $(GTK_GIR_FILE)) do @if exist %f copy /b %f "$(PREFIX)\share\gir-1.0"
	@for %f in ($(GDK_TYPELIB_FILE) $(GDKWIN32_TYPELIB_FILE) $(GTK_TYPELIB_FILE)) do @if exist %f copy /b %f "$(PREFIX)\lib\girepository-1.0"

# Copy the public headers
install-headers:
	@echo Copying the headers...

# Create the GDK header directories
	@for %d in (gdk gdk\deprecated gdk\win32) do @if not exist $(GTK_HEADERS_BASE_INSTALL_DIR)\%d\ mkdir $(GTK_HEADERS_BASE_INSTALL_DIR)\%d
	@if not "$(BROADWAY)" == "" if not exist $(GTK_HEADERS_BASE_INSTALL_DIR)\gdk\broadway\ mkdir $(GTK_HEADERS_BASE_INSTALL_DIR)\gdk\broadway

# Create the GTK header directories
	@for %d in (gtk gtk\a11y gtk\deprecated) do @if not exist $(GTK_HEADERS_BASE_INSTALL_DIR)\%d\ mkdir $(GTK_HEADERS_BASE_INSTALL_DIR)\%d

# Create the libgail-util header directories
	@for %d in (libgail-util) do @if not exist $(GAILUTIL_HEADERS_BASE_INSTALL_DIR)\%d\ mkdir $(GAILUTIL_HEADERS_BASE_INSTALL_DIR)\%d

# Copy the GDK headers (with GdkWin32 and with Gdk-Broadway, if built)
	@for %f in ($(GDK_GENERATED_PUBLIC_H)) do @copy %f "$(GTK_HEADERS_BASE_INSTALL_DIR)\gdk"
	@for %f in ($(gdk_public_h_sources)) do @copy ..\gdk\%f "$(GTK_HEADERS_BASE_INSTALL_DIR)\gdk"
	@for %f in ($(gdk_deprecated_h_sources:/=\)) do @copy ..\gdk\%f "$(GTK_HEADERS_BASE_INSTALL_DIR)\gdk\deprecated"
	@for %f in ($(GDK_PUBLIC_H_SRCS_WIN32)) do @copy ..\gdk\win32\%f "$(GTK_HEADERS_BASE_INSTALL_DIR)\gdk"
	@for %f in ($(libgdkwin32include_HEADERS)) do @copy ..\gdk\win32\%f "$(GTK_HEADERS_BASE_INSTALL_DIR)\gdk\win32"
	@if not "$(BROADWAY)" == "" for %f in ($(GDK_PUBLIC_H_SRCS_BROADWAY)) do @copy ..\gdk\broadway\%f "$(GTK_HEADERS_BASE_INSTALL_DIR)\gdk"
	@if not "$(BROADWAY)" == "" for %f in ($(libgdkbroadwayinclude_HEADERS)) do @copy ..\gdk\broadway\%f "$(GTK_HEADERS_BASE_INSTALL_DIR)\gdk\broadway"

# Copy the GTK headers
	@for %f in ($(GTK_GENERATED_PUBLIC_H)) do @copy %f "$(GTK_HEADERS_BASE_INSTALL_DIR)\gtk"
# I hate the U1095 command line too long fatal error :|, work around it...
	@for %f in ($(GTK_PUB_HDRS:.h=) $(gtk_semi_private_h_sources:.h=)) do @copy ..\gtk\%f.h "$(GTK_HEADERS_BASE_INSTALL_DIR)\gtk"
	@for %f in ($(a11y_h_sources:a11y/=)) do @copy ..\gtk\a11y\%f "$(GTK_HEADERS_BASE_INSTALL_DIR)\gtk\a11y"
	@for %f in ($(gtk_deprecated_h_sources:deprecated/=)) do @copy ..\gtk\deprecated\%f "$(GTK_HEADERS_BASE_INSTALL_DIR)\gtk\deprecated"

# Copy the libgail-util headers
	@for %f in ($(util_public_h_sources)) do @copy ..\libgail-util\%f "$(GAILUTIL_HEADERS_BASE_INSTALL_DIR)\libgail-util"

# install the data files
install-data:
	@echo Copying the non-generated data files...
# GSettings schemas
	@if not exist $(PREFIX)\share\glib-2.0\schemas\ mkdir $(PREFIX)\share\glib-2.0\schemas
	@for %f in (..\gtk\org.gtk.Settings.*.gschema.xml ..\demos\gtk-demo\org.gtk.Demo.gschema.xml) do @copy %f "$(PREFIX)\share\glib-2.0\schemas"
	@-$(GLIB_COMPILE_SCHEMAS) $(PREFIX)\share\glib-2.0\schemas
# Demo icons
	@for %t in (16 22 24 32 48 256) do @for %d in ($(PREFIX)\share\icons\hicolor\%tx%t\apps) do @((if not exist %d\ mkdir %d) & copy /b ..\demos\gtk-demo\data\%tx%t\gtk3-demo*.png "%d")
	@for %t in (16 22 24 32 48 256) do @for %d in ($(PREFIX)\share\icons\hicolor\%tx%t\apps) do @((if not exist %d\ mkdir %d) & copy /b ..\demos\widget-factory\data\%tx%t\gtk3-widget-factory*.png "%d")
	@-$(BASE_BUILT_BIN_DIR)\gtk-update-icon-cache.exe --ignore-theme-index --force "$(PREFIX)\share\icons\hicolor"
# Auxiliary build-related data files (m4, ITS files, RelaxNG files)
	@for %d in (aclocal gettext\its gtk-3.0\emoji) do @if not exist $(PREFIX)\share\%d\ mkdir $(PREFIX)\share\%d
	@copy ..\m4macros\gtk-3.0.m4 $(PREFIX)\share\aclocal
	@for %x in (its loc) do @copy ..\gtk\gtkbuilder.%x $(PREFIX)\share\gettext\its
	@for %x in (rng) do @copy ..\gtk\gtkbuilder.%x $(PREFIX)\share\gtk-3.0
	@for %l in (de es fr zh) do @for %f in ($(BASE_BUILT_BIN_DIR)\%l.gresource) do @copy %f $(PREFIX)\share\gtk-3.0\emoji

# Generate and install the translations
install-translations:
	@for %d in (po po-properties) do @for %l in (..\%d\*.po) do @if not exist $(PREFIX)\share\locale\%~nl\LC_MESSAGES\ md $(PREFIX)\share\locale\%~nl\LC_MESSAGES
	@for %l in (..\po\*.po) do @$(MSGFMT) -c -v -o $(PREFIX)\share\locale\%~nl\LC_MESSAGES\gtk30.mo %l
	@for %l in (..\po-properties\*.po) do @$(MSGFMT) -c -v -o $(PREFIX)\share\locale\%~nl\LC_MESSAGES\gtk30-properties.mo %l
