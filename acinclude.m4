# autoconf 2.13 / 2.50 compatibility macro

# GLIB_AC_DIVERT_BEFORE_HELP(STUFF)
# ---------------------------------
# Put STUFF early enough so that they are available for $ac_help expansion.
# Handle both classic (<= v2.13) and modern autoconf
AC_DEFUN([GLIB_AC_DIVERT_BEFORE_HELP],
[ifdef([m4_divert_text], [m4_divert_text([NOTICE],[$1])],
       [ifdef([AC_DIVERT], [AC_DIVERT([NOTICE],[$1])],
              [AC_DIVERT_PUSH(AC_DIVERSION_NOTICE)dnl
$1
AC_DIVERT_POP()])])])
