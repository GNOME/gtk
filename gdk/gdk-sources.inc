#
# GDK header files for public installation (non-generated)
#

#
# setup source file variables
#

gdk_public_h_sources = 				\
	gdk.h					\
	gdk-autocleanup.h			\
	gdkapplaunchcontext.h			\
	gdkcairo.h				\
	gdkcursor.h				\
	gdkdevice.h				\
	gdkdevicepad.h				\
	gdkdevicetool.h				\
	gdkdevicemanager.h			\
	gdkdisplay.h				\
	gdkdisplaymanager.h			\
	gdkdnd.h				\
	gdkdrawingcontext.h			\
	gdkevents.h				\
	gdkframetimings.h			\
	gdkglcontext.h				\
	gdkkeys.h				\
	gdkkeysyms.h				\
	gdkkeysyms-compat.h			\
	gdkmain.h				\
	gdkmonitor.h				\
	gdkpango.h				\
	gdkframeclock.h				\
	gdkpixbuf.h				\
	gdkprivate.h				\
	gdkproperty.h				\
	gdkrectangle.h				\
	gdkrgba.h				\
	gdkscreen.h				\
	gdkseat.h				\
	gdkselection.h				\
	gdktestutils.h				\
	gdkthreads.h				\
	gdktypes.h				\
	gdkvisual.h				\
	gdkwindow.h

gdk_deprecated_h_sources =				\
	deprecated/gdkcolor.h

gdk_h_sources =					\
	$(gdk_public_h_sources)			\
	$(gdk_deprecated_h_sources)

gdk_private_headers = 				\
	gdk-private.h				\
	gdkapplaunchcontextprivate.h		\
	gdkconstructor.h		\
	gdkcursorprivate.h			\
	gdkdevicemanagerprivate.h		\
	gdkdeviceprivate.h			\
	gdkdevicepadprivate.h			\
	gdkdevicetoolprivate.h			\
	gdkdisplaymanagerprivate.h		\
	gdkdisplayprivate.h			\
	gdkdndprivate.h				\
	gdkdrawingcontextprivate.h		\
	gdkframeclockidle.h			\
	gdkframeclockprivate.h			\
	gdkglcontextprivate.h			\
	gdkmonitorprivate.h			\
	gdkprofilerprivate.h			\
	gdkscreenprivate.h			\
	gdkseatprivate.h			\
	gdkseatdefaultprivate.h			\
	gdkinternals.h				\
	gdkintl.h				\
	gdkkeysprivate.h			\
	gdkvisualprivate.h			\
	gdkx.h

gdk_deprecated_c_sources =				\
	deprecated/gdkcolor.c

gdk_c_sources = 				\
	$(gdk_deprecated_c_sources)			\
	gdk-private.c				\
	gdk.c					\
	gdkapplaunchcontext.c			\
	gdkcairo.c				\
	gdkcursor.c				\
	gdkdeprecated.c				\
	gdkdevice.c				\
	gdkdevicepad.c				\
	gdkdevicetool.c				\
	gdkdevicemanager.c			\
	gdkdisplay.c				\
	gdkdisplaymanager.c			\
	gdkdnd.c				\
	gdkdrawingcontext.c			\
	gdkevents.c     			\
	gdkframetimings.c			\
	gdkgl.c					\
	gdkglcontext.c				\
	gdkglobals.c				\
	gdkkeys.c				\
	gdkkeyuni.c				\
	gdkmonitor.c				\
	gdkoffscreenwindow.c			\
	gdkframeclock.c				\
	gdkframeclockidle.c			\
	gdkpango.c				\
	gdkpixbuf-drawable.c			\
	gdkprofiler.c				\
	gdkproperty.c				\
	gdkrectangle.c				\
	gdkrgba.c				\
	gdkscreen.c				\
	gdkseat.c				\
	gdkseatdefault.c			\
	gdkselection.c				\
	gdkvisual.c				\
	gdkwindow.c				\
	gdkwindowimpl.c

gdk_built_sources = 				\
	gdkenumtypes.h				\
	gdkenumtypes.c				\
	gdkmarshalers.h 			\
	gdkmarshalers.c				\
	gdkresources.h				\
	gdkresources.c				\
	gdkversionmacros.h

x11_introspection_files = 		\
	x11/gdkapplaunchcontext-x11.c	\
	x11/gdkasync.c			\
	x11/gdkcursor-x11.c		\
	x11/gdkdevice-core-x11.c	\
	x11/gdkdevicemanager-core-x11.c	\
	x11/gdkdevicemanager-x11.c	\
	x11/gdkdevicemanager-xi2.c	\
	x11/gdkdevice-xi2.c		\
	x11/gdkdisplay-x11.c		\
	x11/gdkdisplaymanager-x11.c	\
	x11/gdkdnd-x11.c		\
	x11/gdkeventsource.c		\
	x11/gdkeventtranslator.c	\
	x11/gdkgeometry-x11.c		\
	x11/gdkglcontext-x11.c		\
	x11/gdkkeys-x11.c		\
	x11/gdkmain-x11.c		\
	x11/gdkmonitor-x11.c		\
	x11/gdkproperty-x11.c		\
	x11/gdkscreen-x11.c		\
	x11/gdkselection-x11.c		\
	x11/gdksettings.c		\
	x11/gdktestutils-x11.c		\
	x11/gdkvisual-x11.c		\
	x11/gdkwindow-x11.c		\
	x11/gdkxftdefaults.c		\
	x11/gdkxid.c			\
	x11/xsettings-client.c		\
	x11/gdkx.h			\
	x11/gdkx11applaunchcontext.h	\
	x11/gdkx11cursor.h		\
	x11/gdkx11device.h		\
	x11/gdkx11device-core.h		\
	x11/gdkx11device-xi2.h		\
	x11/gdkx11devicemanager.h	\
	x11/gdkx11devicemanager-core.h	\
	x11/gdkx11devicemanager-xi2.h	\
	x11/gdkx11display.h		\
	x11/gdkx11displaymanager.h	\
	x11/gdkx11dnd.h			\
	x11/gdkx11glcontext.h		\
	x11/gdkx11keys.h		\
	x11/gdkx11monitor.h		\
	x11/gdkx11property.h		\
	x11/gdkx11screen.h		\
	x11/gdkx11selection.h		\
	x11/gdkx11utils.h		\
	x11/gdkx11visual.h		\
	x11/gdkx11window.h

w32_introspection_files =		\
	win32/gdkcursor-win32.c		\
	win32/gdkdevicemanager-win32.c	\
	win32/gdkdevice-virtual.c	\
	win32/gdkdevice-win32.c		\
	win32/gdkdevice-wintab.c	\
	win32/gdkdisplaymanager-win32.c	\
	win32/gdkdisplay-win32.c	\
	win32/gdkdnd-win32.c		\
	win32/gdkevents-win32.c		\
	win32/gdkgeometry-win32.c	\
	win32/gdkglobals-win32.c	\
	win32/gdkkeys-win32.c		\
	win32/gdkmain-win32.c		\
	win32/gdkproperty-win32.c	\
	win32/gdkscreen-win32.c		\
	win32/gdkselection-win32.c	\
	win32/gdktestutils-win32.c	\
	win32/gdkwin32.h		\
	win32/gdkwin32cursor.h		\
	win32/gdkwin32display.h		\
	win32/gdkwin32displaymanager.h	\
	win32/gdkwin32dnd.h		\
	win32/gdkwin32id.c		\
	win32/gdkwin32keys.h		\
	win32/gdkwin32misc.h		\
	win32/gdkwin32screen.h		\
	win32/gdkwin32window.h		\
	win32/gdkwindow-win32.c
