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

# GTK_ADD_LIB(VAR,LIBNAME)
# ---------------------------------
# Helper macro to add a -lBlah to a variable, avoiding repeats
# Note that this needs to be quoted when used in an enclosing macro
AC_DEFUN([GTK_ADD_LIB],
[ case "$$1 " in
    *-l$2[[\ \	]]*)               ;;
    *)               $1="-l$2 $$1" ;;
  esac
])


# Checks the location of the XML Catalog
# Usage:
#   JH_PATH_XML_CATALOG([ACTION-IF-FOUND], [ACTION-IF-NOT-FOUND])
# Defines XMLCATALOG and XML_CATALOG_FILE substitutions
AC_DEFUN([JH_PATH_XML_CATALOG],
[
  # check for the presence of the XML catalog
  AC_ARG_WITH([xml-catalog],
              AC_HELP_STRING([--with-xml-catalog=CATALOG],
                             [path to xml catalog to use]),,
              [with_xml_catalog=/etc/xml/catalog])
  jh_found_xmlcatalog=true
  XML_CATALOG_FILE="$with_xml_catalog"
  AC_SUBST([XML_CATALOG_FILE])
  AC_MSG_CHECKING([for XML catalog ($XML_CATALOG_FILE)])
  if test -f "$XML_CATALOG_FILE"; then
    AC_MSG_RESULT([found])
  else
    jh_found_xmlcatalog=false
    AC_MSG_RESULT([not found])
  fi

  # check for the xmlcatalog program
  AC_PATH_PROG(XMLCATALOG, xmlcatalog, no)
  if test "x$XMLCATALOG" = xno; then
    jh_found_xmlcatalog=false
  fi

  if $jh_found_xmlcatalog; then
    ifelse([$1],,[:],[$1])
  else
    ifelse([$2],,[AC_MSG_ERROR([could not find XML catalog])],[$2])
  fi
])

# Checks if a particular URI appears in the XML catalog
# Usage:
#   JH_CHECK_XML_CATALOG(URI, [FRIENDLY-NAME], [ACTION-IF-FOUND], [ACTION-IF-NOT-FOUND])
AC_DEFUN([JH_CHECK_XML_CATALOG],
[
  AC_REQUIRE([JH_PATH_XML_CATALOG],[JH_PATH_XML_CATALOG(,[:])])dnl
  AC_MSG_CHECKING([for ifelse([$2],,[$1],[$2]) in XML catalog])
  if $jh_found_xmlcatalog && \
     AC_RUN_LOG([$XMLCATALOG --noout "$XML_CATALOG_FILE" "$1" >&2]); then
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
