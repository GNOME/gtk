# Public header list for GdkWin32

GDK_PUBLIC_H_SRCS_WIN32 =		\
	gdkwin32.h

libgdkwin32include_HEADERS =	\
	gdkwin32cursor.h	\
	gdkwin32display.h	\
	gdkwin32displaymanager.h\
	gdkwin32dnd.h		\
	gdkwin32glcontext.h	\
	gdkwin32keys.h		\
	gdkwin32misc.h		\
	gdkwin32monitor.h	\
	gdkwin32screen.h	\
	gdkwin32window.h

# Sources list for GdkWin32
libgdk_win32_la_SOURCES = \
	gdkcursor-win32.c \
	gdkdevicemanager-win32.c \
	gdkdevicemanager-win32.h \
	gdkdevice-virtual.c \
	gdkdevice-virtual.h \
	gdkdevice-win32.c \
	gdkdevice-win32.h \
	gdkdevice-winpointer.c \
	gdkdevice-winpointer.h \
	gdkdevice-wintab.c \
	gdkdevice-wintab.h \
	gdkdisplay-win32.c \
	gdkdisplay-win32.h \
	gdkdisplaymanager-win32.c \
	gdkdnd-win32.c \
	gdkevents-win32.c \
	gdkgeometry-win32.c \
	gdkglcontext-win32.c \
	gdkglcontext-win32.h \
	gdkglobals-win32.c \
	gdkkeys-win32.c \
	gdkkeys-win32.h \
	gdkkeys-win32-impl.c \
	gdkkeys-win32-impl-wow64.c \
	gdkmain-win32.c \
	gdkmonitor-win32.c \
	gdkmonitor-win32.h \
	gdkprivate-win32.h \
	gdkproperty-win32.c \
	gdkscreen-win32.c \
	gdkselection-win32.c \
	gdkselection-win32.h \
	gdktestutils-win32.c \
	gdkwin32cursor.h \
	gdkwin32display.h \
	gdkwin32displaymanager.h \
	gdkwin32dnd.h \
	gdkwin32dnd-private.h \
	gdkwin32glcontext.h \
	gdkwin32langnotification.c \
	gdkwin32langnotification.h \
	gdkwin32.h \
	gdkwin32id.c \
	gdkwin32keys.h \
	gdkwin32monitor.h \
	gdkwin32screen.h \
	gdkwin32window.h \
	gdkwindow-win32.c \
	gdkwindow-win32.h \
	pktdef.h \
	winpointer.h \
	wintab.h \
	xcursors.h
