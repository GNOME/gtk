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

# Checks the location of the XML Catalog
# Usage:
#   JH_PATH_XML_CATALOG
# Defines XMLCATALOG and XML_CATALOG_FILE substitutions
AC_DEFUN([JH_PATH_XML_CATALOG],
[
  # check for the presence of the XML catalog
  AC_ARG_WITH([xml-catalog],
              AC_HELP_STRING([--with-xml-catalog=CATALOG],
                             [path to xml catalog to use]),,
              [with_xml_catalog=/etc/xml/catalog])
  XML_CATALOG_FILE="$with_xml_catalog"
  AC_MSG_CHECKING([for XML catalog ($XML_CATALOG_FILE)])
  if test -f "$XML_CATALOG_FILE"; then
    AC_MSG_RESULT([found])
  else
    AC_MSG_RESULT([not found])
    AC_MSG_ERROR([XML catalog not found])
  fi
  AC_SUBST([XML_CATALOG_FILE])

  # check for the xmlcatalog program
  AC_PATH_PROG(XMLCATALOG, xmlcatalog, no)
  if test "x$XMLCATALOG" = xno; then
    AC_MSG_ERROR([could not find xmlcatalog program])
  fi
])

# Checks if a particular URI appears in the XML catalog
# Usage:
#   JH_CHECK_XML_CATALOG(URI, [FRIENDLY-NAME], [ACTION-IF-FOUND], [ACTION-IF-NOT-FOUND])
AC_DEFUN([JH_CHECK_XML_CATALOG],
[
  AC_REQUIRE([JH_PATH_XML_CATALOG])dnl
  AC_MSG_CHECKING([for ifelse([$2],,[$1],[$2]) in XML catalog])
  if AC_RUN_LOG([$XMLCATALOG --noout "$XML_CATALOG_FILE" "$1" >&2]); then
    AC_MSG_RESULT([found])
    ifelse([$3],,,[$3
])dnl
  else
    AC_MSG_RESULT([not found])
    ifelse([$4],,
       [AC_MSG_ERROR([could not find ifelse([$2],,[$1],[$2]) in XML catalog])],
       [$4])
  fi
])
