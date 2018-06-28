/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"

#include "gdkproperty.h"
#include "gdkprivate-x11.h"
#include "gdkdisplay-x11.h"

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <string.h>


/**
 * gdk_x11_display_text_property_to_text_list:
 * @display: (type GdkX11Display): The #GdkDisplay where the encoding is defined
 * @encoding: an atom representing the encoding. The most
 *    common values for this are STRING, or COMPOUND_TEXT.
 *    This is value used as the type for the property
 * @format: the format of the property
 * @text: The text data
 * @length: The number of items to transform
 * @list: location to store an  array of strings in
 *    the encoding of the current locale. This array should be
 *    freed using gdk_free_text_list().
 *
 * Convert a text string from the encoding as it is stored
 * in a property into an array of strings in the encoding of
 * the current locale. (The elements of the array represent the
 * nul-separated elements of the original text string.)
 *
 * Returns: the number of strings stored in list, or 0,
 *     if the conversion failed
 */
gint
gdk_x11_display_text_property_to_text_list (GdkDisplay   *display,
                                            GdkAtom       encoding,
                                            gint          format,
                                            const guchar *text,
                                            gint          length,
                                            gchar      ***list)
{
  XTextProperty property;
  gint count = 0;
  gint res;
  gchar **local_list;
  g_return_val_if_fail (GDK_IS_DISPLAY (display), 0);

  if (list)
    *list = NULL;

  if (gdk_display_is_closed (display))
    return 0;

  property.value = (guchar *)text;
  property.encoding = gdk_x11_atom_to_xatom_for_display (display, encoding);
  property.format = format;
  property.nitems = length;
  res = XmbTextPropertyToTextList (GDK_DISPLAY_XDISPLAY (display), &property,
                                   &local_list, &count);
  if (res == XNoMemory || res == XLocaleNotSupported || res == XConverterNotFound)
    return 0;
  else
    {
      if (list)
        *list = local_list;
      else
        XFreeStringList (local_list);

      return count;
    }
}

/**
 * gdk_x11_free_text_list:
 * @list: the value stored in the @list parameter by
 *   a call to gdk_x11_display_text_property_to_text_list().
 *
 * Frees the array of strings created by
 * gdk_x11_display_text_property_to_text_list().
 */
void
gdk_x11_free_text_list (gchar **list)
{
  g_return_if_fail (list != NULL);

  XFreeStringList (list);
}

static gint
make_list (const gchar  *text,
           gint          length,
           gboolean      latin1,
           gchar      ***list)
{
  GSList *strings = NULL;
  gint n_strings = 0;
  gint i;
  const gchar *p = text;
  const gchar *q;
  GSList *tmp_list;
  GError *error = NULL;

  while (p < text + length)
    {
      gchar *str;

      q = p;
      while (*q && q < text + length)
        q++;

      if (latin1)
        {
          str = g_convert (p, q - p,
                           "UTF-8", "ISO-8859-1",
                           NULL, NULL, &error);

          if (!str)
            {
              g_warning ("Error converting selection from STRING: %s",
                         error->message);
              g_error_free (error);
            }
        }
      else
        {
          str = g_strndup (p, q - p);
          if (!g_utf8_validate (str, -1, NULL))
            {
              g_warning ("Error converting selection from UTF8_STRING");
              g_free (str);
              str = NULL;
            }
        }

      if (str)
        {
          strings = g_slist_prepend (strings, str);
          n_strings++;
        }

      p = q + 1;
    }

  if (list)
    {
      *list = g_new (gchar *, n_strings + 1);
      (*list)[n_strings] = NULL;
    }

  i = n_strings;
  tmp_list = strings;
  while (tmp_list)
    {
      if (list)
        (*list)[--i] = tmp_list->data;
      else
        g_free (tmp_list->data);

      tmp_list = tmp_list->next;
    }

  g_slist_free (strings);

  return n_strings;
}

gint
_gdk_x11_display_text_property_to_utf8_list (GdkDisplay    *display,
                                             GdkAtom        encoding,
                                             gint           format,
                                             const guchar  *text,
                                             gint           length,
                                             gchar       ***list)
{
  if (encoding == g_intern_static_string ("STRING"))
    {
      return make_list ((gchar *)text, length, TRUE, list);
    }
  else if (encoding == g_intern_static_string ("UTF8_STRING"))
    {
      return make_list ((gchar *)text, length, FALSE, list);
    }
  else
    {
      gchar **local_list;
      gint local_count;
      gint i;
      const gchar *charset = NULL;
      gboolean need_conversion = !g_get_charset (&charset);
      gint count = 0;
      GError *error = NULL;

      /* Probably COMPOUND text, we fall back to Xlib routines
       */
      local_count = gdk_x11_display_text_property_to_text_list (display,
                                                                encoding,
                                                                format,
                                                                text,
                                                                length,
                                                                &local_list);
      if (list)
        *list = g_new (gchar *, local_count + 1);

      for (i=0; i<local_count; i++)
        {
          /* list contains stuff in our default encoding
           */
          if (need_conversion)
            {
              gchar *utf = g_convert (local_list[i], -1,
                                      "UTF-8", charset,
                                      NULL, NULL, &error);
              if (utf)
                {
                  if (list)
                    (*list)[count++] = utf;
                  else
                    g_free (utf);
                }
              else
                {
                  g_warning ("Error converting to UTF-8 from '%s': %s",
                             charset, error->message);
                  g_error_free (error);
                  error = NULL;
                }
            }
          else
            {
              if (list)
                {
                  if (g_utf8_validate (local_list[i], -1, NULL))
                    (*list)[count++] = g_strdup (local_list[i]);
                  else
                    g_warning ("Error converting selection");
                }
            }
        }

      if (local_count)
        gdk_x11_free_text_list (local_list);

      if (list)
        (*list)[count] = NULL;

      return count;
    }
}

/**
 * gdk_x11_display_string_to_compound_text:
 * @display: (type GdkX11Display): the #GdkDisplay where the encoding is defined
 * @str: a nul-terminated string
 * @encoding: (out) (transfer none): location to store the encoding atom
 *     (to be used as the type for the property)
 * @format: (out): location to store the format of the property
 * @ctext: (out) (array length=length): location to store newly
 *     allocated data for the property
 * @length: the length of @ctext, in bytes
 *
 * Convert a string from the encoding of the current
 * locale into a form suitable for storing in a window property.
 *
 * Returns: 0 upon success, non-zero upon failure
 */
gint
gdk_x11_display_string_to_compound_text (GdkDisplay  *display,
                                         const gchar *str,
                                         GdkAtom     *encoding,
                                         gint        *format,
                                         guchar     **ctext,
                                         gint        *length)
{
  gint res;
  XTextProperty property;

  g_return_val_if_fail (GDK_IS_DISPLAY (display), 0);

  if (gdk_display_is_closed (display))
    res = XLocaleNotSupported;
  else
    res = XmbTextListToTextProperty (GDK_DISPLAY_XDISPLAY (display),
                                     (char **)&str, 1, XCompoundTextStyle,
                                     &property);
  if (res != Success)
    {
      property.encoding = None;
      property.format = None;
      property.value = NULL;
      property.nitems = 0;
    }

  if (encoding)
    *encoding = gdk_x11_xatom_to_atom_for_display (display, property.encoding);
  if (format)
    *format = property.format;
  if (ctext)
    *ctext = property.value;
  if (length)
    *length = property.nitems;

  return res;
}

/* The specifications for COMPOUND_TEXT and STRING specify that C0 and
 * C1 are not allowed except for \n and \t, however the X conversions
 * routines for COMPOUND_TEXT only enforce this in one direction,
 * causing cut-and-paste of \r and \r\n separated text to fail.
 * This routine strips out all non-allowed C0 and C1 characters
 * from the input string and also canonicalizes \r, and \r\n to \n
 */
static gchar *
sanitize_utf8 (const gchar *src,
               gboolean return_latin1)
{
  gint len = strlen (src);
  GString *result = g_string_sized_new (len);
  const gchar *p = src;

  while (*p)
    {
      if (*p == '\r')
        {
          p++;
          if (*p == '\n')
            p++;

          g_string_append_c (result, '\n');
        }
      else
        {
          gunichar ch = g_utf8_get_char (p);

          if (!((ch < 0x20 && ch != '\t' && ch != '\n') || (ch >= 0x7f && ch < 0xa0)))
            {
              if (return_latin1)
                {
                  if (ch <= 0xff)
                    g_string_append_c (result, ch);
                  else
                    g_string_append_printf (result,
                                            ch < 0x10000 ? "\\u%04x" : "\\U%08x",
                                            ch);
                }
              else
                {
                  char buf[7];
                  gint buflen;

                  buflen = g_unichar_to_utf8 (ch, buf);
                  g_string_append_len (result, buf, buflen);
                }
            }

          p = g_utf8_next_char (p);
        }
    }

  return g_string_free (result, FALSE);
}

gchar *
_gdk_x11_display_utf8_to_string_target (GdkDisplay  *display,
                                        const gchar *str)
{
  return sanitize_utf8 (str, TRUE);
}

/**
 * gdk_x11_display_utf8_to_compound_text:
 * @display: (type GdkX11Display): a #GdkDisplay
 * @str: a UTF-8 string
 * @encoding: (out): location to store resulting encoding
 * @format: (out): location to store format of the result
 * @ctext: (out) (array length=length): location to store the data of the result
 * @length: location to store the length of the data
 *     stored in @ctext
 *
 * Converts from UTF-8 to compound text.
 *
 * Returns: %TRUE if the conversion succeeded,
 *     otherwise %FALSE
 */
gboolean
gdk_x11_display_utf8_to_compound_text (GdkDisplay  *display,
                                       const gchar *str,
                                       GdkAtom     *encoding,
                                       gint        *format,
                                       guchar     **ctext,
                                       gint        *length)
{
  gboolean need_conversion;
  const gchar *charset;
  gchar *locale_str, *tmp_str;
  GError *error = NULL;
  gboolean result;

  g_return_val_if_fail (str != NULL, FALSE);
  g_return_val_if_fail (GDK_IS_DISPLAY (display), FALSE);

  need_conversion = !g_get_charset (&charset);

  tmp_str = sanitize_utf8 (str, FALSE);

  if (need_conversion)
    {
      locale_str = g_convert (tmp_str, -1,
                              charset, "UTF-8",
                              NULL, NULL, &error);
      g_free (tmp_str);

      if (!locale_str)
        {
          if (!g_error_matches (error, G_CONVERT_ERROR, G_CONVERT_ERROR_ILLEGAL_SEQUENCE))
            {
              g_warning ("Error converting from UTF-8 to '%s': %s",
                         charset, error->message);
            }
          g_error_free (error);

          if (encoding)
            *encoding = None;
          if (format)
            *format = None;
          if (ctext)
            *ctext = NULL;
          if (length)
            *length = 0;

          return FALSE;
        }
    }
  else
    locale_str = tmp_str;

  result = gdk_x11_display_string_to_compound_text (display, locale_str,
                                                    encoding, format,
                                                    ctext, length);
  result = (result == Success? TRUE : FALSE);

  g_free (locale_str);

  return result;
}

/**
 * gdk_x11_free_compound_text:
 * @ctext: The pointer stored in @ctext from a call to
 *   gdk_x11_display_string_to_compound_text().
 *
 * Frees the data returned from gdk_x11_display_string_to_compound_text().
 */
void
gdk_x11_free_compound_text (guchar *ctext)
{
  if (ctext)
    XFree (ctext);
}
