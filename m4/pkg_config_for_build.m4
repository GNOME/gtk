# PKG_PROG_PKG_CONFIG_FOR_BUILD([MIN-VERSION])
# ----------------------------------
AC_DEFUN([PKG_PROG_PKG_CONFIG_FOR_BUILD],
[m4_pattern_allow([^PKG_CONFIG_FOR_BUILD$])
AC_ARG_VAR([PKG_CONFIG_FOR_BUILD], [path to build system's pkg-config utility])

if test "x$ac_cv_env_PKG_CONFIG_FOR_BUILD_set" != "xset"; then
	AC_PATH_PROG([PKG_CONFIG_FOR_BUILD], [pkg-config])
fi
if test -n "$PKG_CONFIG_FOR_BUILD"; then
	_pkg_for_build_min_version=m4_default([$1], [0.9.0])
	AC_MSG_CHECKING([build system's pkg-config is at least version $_pkg_min_version])
	if $PKG_CONFIG_FOR_BUILD --atleast-pkgconfig-version $_pkg_min_version; then
		AC_MSG_RESULT([yes])
	else
		AC_MSG_RESULT([no])
		PKG_CONFIG_FOR_BUILD=""
	fi
fi[]dnl
])# PKG_PROG_PKG_CONFIG_FOR_BUILD
