/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright (C) 2017 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Benjamin Otte <otte@gnome.org>
 */

#include "config.h"

#include "gdktextlistconverter-x11.h"

#include "gdkintl.h"
#include "gdkprivate-x11.h"

#define GDK_X11_TEXT_LIST_CONVERTER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), GDK_TYPE_X11_TEXT_LIST_CONVERTER, GdkX11TextListConverterClass))
#define GDK_IS_X11_TEXT_LIST_CONVERTER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GDK_TYPE_X11_TEXT_LIST_CONVERTER))
#define GDK_X11_TEXT_LIST_CONVERTER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GDK_TYPE_X11_TEXT_LIST_CONVERTER, GdkX11TextListConverterClass))

typedef struct _GdkX11TextListConverterClass GdkX11TextListConverterClass;

struct _GdkX11TextListConverter
{
  GObject parent_instance;

  GdkDisplay *display;
  
  const char *encoding; /* interned */
  gint format;
};

struct _GdkX11TextListConverterClass
{
  GObjectClass parent_class;
};

static GConverterResult
gdk_x11_text_list_converter_convert (GConverter       *converter,
                                     const void       *inbuf,
                                     gsize             inbuf_size,
                                     void             *outbuf,
                                     gsize             outbuf_size,
                                     GConverterFlags   flags,
                                     gsize            *bytes_read,
                                     gsize            *bytes_written,
                                     GError          **error)
{
  GdkX11TextListConverter *conv = GDK_X11_TEXT_LIST_CONVERTER (converter);
  gint count;
  char **list;

  if (!(flags & G_CONVERTER_INPUT_AT_END))
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_PARTIAL_INPUT,
                           _("Need complete input to do conversion"));
      return G_CONVERTER_ERROR;
    }

  count = _gdk_x11_display_text_property_to_utf8_list (conv->display,
                                                       conv->encoding,
                                                       conv->format,
                                                       inbuf,
                                                       inbuf_size,
                                                       &list);
  if (count < 0)
    {
      /* XXX: add error handling from gdkselection-x11.c */
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NO_SPACE,
                           _("Not enough space in destination"));
      return G_CONVERTER_ERROR;
    }
  else if (count == 0)
    {
      if (outbuf_size < 1)
        {
          g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NO_SPACE,
                               _("Not enough space in destination"));
          return G_CONVERTER_ERROR;
        }
      ((gchar *) outbuf)[0] = 0;
      *bytes_read = inbuf_size;
      *bytes_written = 1;
      return G_CONVERTER_FINISHED;
    }
  else
    {
      gsize len = strlen (list[0]) + 1;

      if (outbuf_size < len)
        {
          g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NO_SPACE,
                               _("Not enough space in destination"));
          return G_CONVERTER_ERROR;
        }
      memcpy (outbuf, list[0], len);
      g_strfreev (list);
      *bytes_read = inbuf_size;
      *bytes_written = len;
      return G_CONVERTER_FINISHED;
    }
}

static void
gdk_x11_text_list_converter_reset (GConverter *converter)
{
}

static void
gdk_x11_text_list_converter_iface_init (GConverterIface *iface)
{
  iface->convert = gdk_x11_text_list_converter_convert;
  iface->reset = gdk_x11_text_list_converter_reset;
}

G_DEFINE_TYPE_WITH_CODE (GdkX11TextListConverter, gdk_x11_text_list_converter, G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (G_TYPE_CONVERTER,
						gdk_x11_text_list_converter_iface_init))

static void
gdk_x11_text_list_converter_finalize (GObject *object)
{
  GdkX11TextListConverter *conv = GDK_X11_TEXT_LIST_CONVERTER (object);

  g_object_unref (conv->display);

  G_OBJECT_CLASS (gdk_x11_text_list_converter_parent_class)->finalize (object);
}

static void
gdk_x11_text_list_converter_class_init (GdkX11TextListConverterClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gdk_x11_text_list_converter_finalize;
}

static void
gdk_x11_text_list_converter_init (GdkX11TextListConverter *local)
{
}

GConverter *
gdk_x11_text_list_converter_to_utf8_new (GdkDisplay *display,
                                         const char *encoding,
                                         int         format)
{
  GdkX11TextListConverter *conv;

  conv = g_object_new (GDK_TYPE_X11_TEXT_LIST_CONVERTER, NULL);

  conv->display = g_object_ref (display);
  conv->encoding = g_intern_string (encoding);
  conv->format = format;

  return G_CONVERTER (conv);
}

