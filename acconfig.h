/* acconfig.h
   This file is in the public domain.

   Descriptive text for the C preprocessor macros that
   the distributed Autoconf macros can define.
   No software package will use all of them; autoheader copies the ones
   your configure.in uses into your configuration header file templates.

   The entries are in sort -df order: alphabetical, case insensitive,
   ignoring punctuation (such as underscores).  Although this order
   can split up related entries, it makes it easier to check whether
   a given entry is in the file.

   Leave the following blank line there!!  Autoheader needs it.  */


/* Other stuff */
#undef ENABLE_NLS
#undef GTK_COMPILED_WITH_DEBUGGING

#undef HAVE_CATGETS
#undef HAVE_DIMM_H
#undef HAVE_GETTEXT
#undef HAVE_IPC_H
#undef HAVE_LC_MESSAGES
#undef HAVE_PROGRESSIVE_JPEG
#undef HAVE_PWD_H
#undef HAVE_SHM_H
#undef HAVE_STPCPY
#undef HAVE_XSHM_H
#undef HAVE_SHAPE_EXT
#undef HAVE_SOLARIS_XINERAMA
#undef HAVE_SYS_SELECT_H
#undef HAVE_SYS_TIME_H
#undef HAVE_XFREE_XINERAMA
#undef HAVE_XINERAMA
#undef HAVE_WINTAB
#undef HAVE_XCONVERTCASE
#undef HAVE_XFT

#undef HAVE_SIGSETJMP

#undef NO_FD_SET

#undef RESOURCE_BASE

#undef USE_GMODULE
#undef USE_MMX

/* Define to use XKB extension */
#undef HAVE_XKB

/* Define to use shadowfb in the linux-fb port */
#undef ENABLE_SHADOW_FB

/* Define to use a fb manager in the linux-fb port */
#undef ENABLE_FB_MANAGER

#undef XINPUT_NONE
#undef XINPUT_GXI
#undef XINPUT_XFREE

/* Define as the return type of signal handlers (int or void).  */
#undef RETSIGTYPE

/* Most machines will be happy with int or void.  IRIX requires '...' */
#undef SIGNAL_ARG_TYPE

#undef GETTEXT_PACKAGE

#undef HAVE_CUPS_API_1_2

/* #undef PACKAGE */
/* #undef VERSION */


/* Leave that blank line there!!  Autoheader needs it.
   If you're adding to this file, keep in mind:
   The entries are in sort -df order: alphabetical, case insensitive,
   ignoring punctuation (such as underscores).  */
