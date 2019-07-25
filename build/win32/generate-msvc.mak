# NMake Makefile portion for code generation and
# intermediate build directory creation
# Items in here should not need to be edited unless
# one is maintaining the NMake build files.

!include config-msvc.mak

all:	\
	..\..\config.h	\
	..\..\gdk\gdkconfig.h

# Copy the pre-defined config.h.win32
..\..\config.h: ..\..\config.h.win32

# Copy the pre-defined gdkconfig.h.[win32|win32_broadway]
!if "$(CFG)" == "release" || "$(CFG)" == "Release"
GDK_OLD_CFG = debug
!else
GDK_OLD_CFG = release
!endif

!ifdef BROADWAY
GDK_CONFIG = broadway
GDK_DEL_CONFIG = win32
GDK_CONFIG_TEMPLATE = ..\..\gdk\gdkconfig.h.win32_broadway
!else
GDK_CONFIG = win32
GDK_DEL_CONFIG = broadway
GDK_CONFIG_TEMPLATE = ..\..\gdk\gdkconfig.h.win32
!endif

..\..\gdk-$(CFG)-$(GDK_CONFIG)-build: $(GDK_CONFIG_TEMPLATE)
	@if exist ..\..\gdk-$(GDK_OLD_CFG)-$(GDK_DEL_CONFIG)-build del ..\..\gdk-$(GDK_OLD_CFG)-$(GDK_DEL_CONFIG)-build
	@if exist ..\..\gdk-$(GDK_OLD_CFG)-$(GDK_CONFIG)-build del ..\..\gdk-$(GDK_OLD_CFG)-$(GDK_CONFIG)-build
	@if exist ..\..\gdk-$(CFG)-$(GDK_DEL_CONFIG)-build del ..\..\gdk-$(CFG)-$(GDK_DEL_CONFIG)-build
	@copy $** $@
	
..\..\gdk\gdkconfig.h: ..\..\gdk-$(CFG)-$(GDK_CONFIG)-build

..\..\config.h	\
..\..\gdk\gdkconfig.h:
	@copy $** $@

# Remove the generated files
clean:
	@-del ..\..\gdk\gdkconfig.h
	@if exist ..\..\gdk-$(CFG)-$(GDK_CONFIG)-build del ..\..\gdk-$(CFG)-$(GDK_CONFIG)-build
	@if exist ..\..\gdk-$(GDK_OLD_CFG)-$(GDK_DEL_CONFIG)-build del ..\..\gdk-$(GDK_OLD_CFG)-$(GDK_DEL_CONFIG)-build
	@if exist ..\..\gdk-$(GDK_OLD_CFG)-$(GDK_CONFIG)-build del ..\..\gdk-$(GDK_OLD_CFG)-$(GDK_CONFIG)-build
	@if exist ..\..\gdk-$(CFG)-$(GDK_DEL_CONFIG)-build del ..\..\gdk-$(CFG)-$(GDK_DEL_CONFIG)-build
	@-del ..\..\config.h
