# Common NMake Makefile module for checking the build environment is sane
# for building introspection files under MSVC/NMake.
# This can be copied from $(gi_srcroot)\build\win32 for GNOME items
# that support MSVC builds and introspection under MSVC.

# Can override with env vars as needed
# You will need to have built gobject-introspection for this to work.
# Change or pass in or set the following to suit your environment

!if "$(PREFIX)" == ""
PREFIX = ..\..\..\vs$(VSVER)\$(PLAT)
!endif

!if ![setlocal]		&& \
    ![set PFX=$(PREFIX)]	&& \
    ![for %P in (%PFX%) do @echo PREFIX_FULL=%~dpnfP > pfx.x]
!endif
!include pfx.x

!if "$(PKG_CONFIG_PATH)" == ""
PKG_CONFIG_PATH=$(PREFIX_FULL)\lib\pkgconfig
!else
PKG_CONFIG_PATH=$(PREFIX_FULL)\lib\pkgconfig;$(PKG_CONFIG_PATH)
!endif

!if ![del $(ERRNUL) /q/f pfx.x]
!endif

# Note: The PYTHON must be the Python release series that was used to build
# the GObject-introspection scanner Python module!
# Either having python.exe your PATH will work or passing in
# PYTHON=<full path to your Python interpretor> will do

# This is required, and gobject-introspection needs to be built
# before this can be successfully run.
!if "$(PYTHON)" == ""
PYTHON=python
!endif

# Path to the pkg-config tool, if not already in the PATH
!if "$(PKG_CONFIG)" == ""
PKG_CONFIG=pkg-config
!endif

# Don't change anything following this line!

GIR_SUBDIR = share\gir-1.0
GIR_TYPELIBDIR = lib\girepository-1.0
G_IR_SCANNER = $(PREFIX)\bin\g-ir-scanner
G_IR_COMPILER = $(PREFIX)\bin\g-ir-compiler.exe
G_IR_INCLUDEDIR = $(PREFIX)\$(GIR_SUBDIR)
G_IR_TYPELIBDIR = $(PREFIX)\$(GIR_TYPELIBDIR)

VALID_PKG_CONFIG_PATH = FALSE

MSG_INVALID_PKGCONFIG = You must set or specifiy a valid PKG_CONFIG_PATH
MSG_INVALID_CFG = You need to specify or set CFG to be release or debug to use this Makefile to build the Introspection Files

ERROR_MSG =

BUILD_INTROSPECTION = TRUE

!if ![set PKG_CONFIG_PATH=$(PKG_CONFIG_PATH)]	\
	&& ![$(PKG_CONFIG) --print-errors --errors-to-stdout $(CHECK_PACKAGE) > pkgconfig.x]	\
	&& ![setlocal]	\
	&& ![set file="pkgconfig.x"]	\
	&& ![FOR %A IN (%file%) DO @echo PKG_CHECK_SIZE=%~zA > pkgconfig.chksize]	\
	&& ![del $(ERRNUL) /q/f pkgconfig.x]
!endif

!include pkgconfig.chksize
!if "$(PKG_CHECK_SIZE)" == "0"
VALID_PKG_CONFIG_PATH = TRUE
!else
VALID_PKG_CONFIG_PATH = FALSE
!endif

!if ![del $(ERRNUL) /q/f pkgconfig.chksize]
!endif

VALID_CFGSET = FALSE
!if "$(CFG)" == "release" || "$(CFG)" == "debug" || "$(CFG)" == "Release" || "$(CFG)" == "Debug"
VALID_CFGSET = TRUE
!endif

!if "$(VALID_PKG_CONFIG_PATH)" != "TRUE"
BUILD_INTROSPECTION = FALSE
ERROR_MSG = $(MSG_INVALID_PKGCONFIG)
!endif

!if "$(VALID_CFGSET)" != "TRUE"
BUILD_INTROSPECTION = FALSE
ERROR_MSG = $(MSG_INVALID_CFG)
!endif
