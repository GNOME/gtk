# The extract_strings tool is a build utility that runs on the build system.

noinst_PROGRAMS = extract-strings

extract_strings_SOURCES = extract-strings.c
extract_strings_CFLAGS = $(GLIB_CFLAGS_FOR_BUILD)
extract_strings_LDADD = $(GLIB_LIBS_FOR_BUILD)

-include $(top_srcdir)/git.mk
