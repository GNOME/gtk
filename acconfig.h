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
#undef HAVE_IPC_H
#undef HAVE_SHM_H
#undef HAVE_XPM
#undef HAVE_XSHM_H
#undef HAVE_SYS_SELECT_H

#undef IPC_RMID_DEFERRED_RELEASE

#undef NO_FD_SET

#undef RESOURCE_BASE

#undef XINPUT_NONE
#undef XINPUT_GXI
#undef XINPUT_XFREE

#undef GTK_MAJOR_VERSION
#undef GTK_MINOR_VERSION
#undef GTK_MICRO_VERSION
#undef GTK_VERSION

/* Define as the return type of signal handlers (int or void).  */
#undef RETSIGTYPE

/* Most machines will be happy with int or void.  IRIX requires '...' */
#undef SIGNAL_ARG_TYPE

/* #undef PACKAGE */
/* #undef VERSION */


/* Leave that blank line there!!  Autoheader needs it.
   If you're adding to this file, keep in mind:
   The entries are in sort -df order: alphabetical, case insensitive,
   ignoring punctuation (such as underscores).  */
