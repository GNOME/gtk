include $(top_srcdir)/Makefile.decl

TEST_PROGS += gtk-reftest

check_PROGRAMS = $(TEST_PROGS)

gtk_reftest_CFLAGS = \
       	-I$(top_srcdir)                 \
       	-I$(top_builddir)/gdk           \
       	-I$(top_srcdir)/gdk             \
       	-DGDK_DISABLE_DEPRECATED        \
       	-DGTK_DISABLE_DEPRECATED        \
       	$(GTK_DEBUG_FLAGS)              \
       	$(GTK_DEP_CFLAGS)

gtk_reftest_LDADD = \
       	$(top_builddir)/gdk/libgdk-3.la \
      	$(top_builddir)/gtk/libgtk-3.la \
	$(GTK_DEP_LIBS)

gtk_reftest_SOURCES = \
	gtk-reftest.c

clean-local:
	rm -rf output/ || true

EXTRA_DIST += \
	label-sizing.css \
	label-sizing.ref.ui \
	label-sizing.ui \
	simpe.ref.png \
	simple.ui
