AUTOMAKE_OPTIONS = subdir-objects

include $(top_srcdir)/Makefile.decl

AM_CPPFLAGS = \
	-I$(top_srcdir)			\
	-I$(top_builddir)/gdk		\
	-I$(top_srcdir)/gdk		\
	-I$(top_srcdir)/gdk/x11		\
	-I$(top_srcdir)/tests		\
	$(GTK_DEBUG_FLAGS)		\
	$(GTK_DEP_CFLAGS)

DEPS = \
	$(top_builddir)/gtk/libgtk-3.la

LDADDS = \
	$(top_builddir)/gtk/libgtk-3.la	\
	$(top_builddir)/gdk/libgdk-3.la	\
	$(GTK_DEP_LIBS)			\
	$(GDK_DEP_LIBS)			\
	-lm

if USE_X11
noinst_PROGRAMS = 		\
	doc-shooter
endif

doc_shooter_DEPENDENCIES = $(DEPS)
doc_shooter_LDADD = $(LDADDS)
doc_shooter_SOURCES=	\
	shadow.c	\
	shadow.h	\
	shooter.c	\
	widgets.c	\
	widgets.h	\
	$(top_srcdir)/tests/gtkgears.c \
	$(top_srcdir)/tests/gtkgears.h

clean-local:
	for file in *.png; do \
	  case "$$file" in \
	    folder.png|gnome.png) ;; \
	    *) rm -f $$file ;; \
	  esac ; \
	done

-include $(top_srcdir)/git.mk
