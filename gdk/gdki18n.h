/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GDK_I18N_H__
#define __GDK_I18N_H__

/* GDK uses "glib". (And so does GTK).
 */
#include <glib.h>

/* international string support */

#include <stdlib.h>

#ifdef X_LOCALE

#include <X11/Xfuncproto.h>
#include <X11/Xosdefs.h>

#ifdef  __cplusplus
extern "C" {
#endif

_XFUNCPROTOBEGIN
extern int _Xmblen (
#if NeedFunctionPrototypes
  const char *s, size_t n
#endif

);
_XFUNCPROTOEND

_XFUNCPROTOBEGIN
extern int _Xmbtowc (
#if NeedFunctionPrototypes
  wchar_t *wstr, const char *str, size_t len
#endif
);
_XFUNCPROTOEND

_XFUNCPROTOBEGIN
extern int _Xwctomb (
#if NeedFunctionPrototypes
  char *str, wchar_t wc
#endif
);
_XFUNCPROTOEND

_XFUNCPROTOBEGIN
extern size_t _Xmbstowcs (
#if NeedFunctionPrototypes
  wchar_t *wstr, const char *str, size_t len
#endif
);
_XFUNCPROTOEND

_XFUNCPROTOBEGIN
extern size_t _Xwcstombs (
#if NeedFunctionPrototypes
  char *str, const wchar_t *wstr, size_t len
#endif
);
_XFUNCPROTOEND

_XFUNCPROTOBEGIN
extern size_t _Xwcslen (
#if NeedFunctionPrototypes
  const wchar_t *wstr
#endif
);
_XFUNCPROTOEND

_XFUNCPROTOBEGIN
extern wchar_t* _Xwcscpy (
#if NeedFunctionPrototypes
  wchar_t *wstr1, const wchar_t *wstr2
#endif
);
_XFUNCPROTOEND

_XFUNCPROTOBEGIN
extern wchar_t* _Xwcsncpy (
#if NeedFunctionPrototypes
  wchar_t *wstr1, const wchar_t *wstr2, size_t len
#endif
);
_XFUNCPROTOEND

_XFUNCPROTOBEGIN
extern int _Xwcscmp (
#if NeedFunctionPrototypes
  const wchar_t *wstr1, const wchar_t *wstr2
#endif
);
_XFUNCPROTOEND

_XFUNCPROTOBEGIN
extern int _Xwcsncmp (
#if NeedFunctionPrototypes
  const wchar_t *wstr1, const wchar_t *wstr2, size_t len
#endif
);
_XFUNCPROTOEND

/* 
 * mblen, mbtowc, and mbstowcs of the locale "ja_JP.eucJP" are buggy.
 */

#ifdef MB_CUR_MAX
# undef MB_CUR_MAX
#endif
#define MB_CUR_MAX 4
extern int _g_mbtowc (wchar_t *wstr, const char *str, size_t len);

/* #define mblen _Xmblen */
/* #define mbtowc _Xmbtowc */
#define mblen(a,b)	_g_mbtowc ((wchar_t *)(NULL), (a), (b))
#define mbtowc(a,b,c)	_g_mbtowc ((a),(b),(c))

#define wctomb(a,b)	_Xwctomb ((a),(b))
#define mbstowcs(a,b,c)	_Xmbstowcs ((a),(b),(c))
#define wcstombs(a,b,c) _Xwcstombs ((a),(b),(c))
#define wcslen(a)	_Xwcslen ((a))
#define wcscpy(a,b)	_Xwcscpy ((a),(b))
#define wcsncpy(a,b,c)	_Xwcsncpy ((a),(b),(c))

#ifdef  __cplusplus
}
#endif

#endif /* X_LOCALE */

#if !defined(HAVE_BROKEN_WCTYPE) && (defined(HAVE_WCTYPE_H) || defined(HAVE_WCHAR_H)) && !defined(X_LOCALE)
#  ifdef HAVE_WCTYPE_H
#    include <wctype.h>
#  else
#    ifdef HAVE_WCHAR_H
#      include <wchar.h>
#    endif
#  endif
#else
#  define iswalnum(c) ((wchar_t)(c) <= 0xFF && isalnum(c))
#endif

#endif /* __GDK_I18N_H__ */
