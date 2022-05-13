# Public header list for GdkBroadway

GDK_PUBLIC_H_SRCS_BROADWAY =		\
	gdkbroadway.h

libgdkbroadwayinclude_HEADERS = 	\
	gdkbroadwaydisplay.h		\
	gdkbroadwaywindow.h		\
	gdkbroadwaycursor.h		\
	gdkbroadwaymonitor.h		\
	gdkbroadwayvisual.h

# built headers that don't get installed

broadway_built_private_headers =	\
	clienthtml.h			\
	broadwayjs.h

# Other non-generated sources
GDK_BROADWAY_NON_GENERATED_SOURCES =	\
	gdkcursor-broadway.c	\
	gdkdevice-broadway.h	\
	gdkdevice-broadway.c	\
	gdkdevicemanager-broadway.h \
	gdkdevicemanager-broadway.c \
	gdkdisplay-broadway.c	\
	gdkdisplay-broadway.h	\
	gdkdnd-broadway.c	\
	gdkeventsource.c	\
	gdkeventsource.h	\
	gdkglobals-broadway.c	\
	gdkkeys-broadway.c	\
	gdkmonitor-broadway.c	\
	gdkmonitor-broadway.h	\
	gdkproperty-broadway.c	\
	gdkscreen-broadway.c	\
	gdkscreen-broadway.h	\
	gdkselection-broadway.c	\
	gdktestutils-broadway.c	\
	gdkvisual-broadway.c	\
	gdkwindow-broadway.c	\
	gdkwindow-broadway.h	\
	gdkprivate-broadway.h   \
	gdkbroadway-server.h	\
	gdkbroadway-server.c

broadwayd_SOURCES = \
	broadway-protocol.h		\
	broadwayd.c 			\
	broadway-server.h		\
	broadway-server.c		\
	broadway-buffer.c		\
	broadway-buffer.h		\
	broadway-output.h		\
	broadway-output.c
