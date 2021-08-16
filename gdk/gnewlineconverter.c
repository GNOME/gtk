/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright (C) 2021 Red Hat, Inc.
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

#include "gnewlineconverter.h"

#include "gdkintl.h"

enum {
  PROP_0,
  PROP_FROM_NEWLINE,
  PROP_TO_NEWLINE
};

/**
 * SECTION:gnewlineconverter
 * @short_description: Convert between newlines
 * @include: gio/gio.h
 *
 * #GNewlineConverter is an implementation of #GConverter that converts
 * between different line endings. This is useful when converting data streams
 * between Windows and UNIX compatibility.
 */

/**
 * GNewlineConverter:
 *
 * Conversions of line endings.
 */
struct _GNewlineConverter
{
  GObject parent_instance;

  GDataStreamNewlineType from;
  GDataStreamNewlineType to;
};

static void g_newline_converter_iface_init          (GConverterIface *iface);

G_DEFINE_TYPE_WITH_CODE (GNewlineConverter, g_newline_converter, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_CONVERTER,
                                                g_newline_converter_iface_init))

static GConverterResult
g_newline_converter_convert (GConverter       *converter,
                             const void       *inbuf,
                             gsize             inbuf_size,
                             void             *outbuf,
                             gsize             outbuf_size,
                             GConverterFlags   flags,
                             gsize            *bytes_read,
                             gsize            *bytes_written,
                             GError          **error)
{
  GNewlineConverter *self = G_NEWLINE_CONVERTER (converter);
  GConverterResult ret;
  const char *inbufp, *inbuf_end;
  char *outbufp, *outbuf_end;
  gsize size;

  inbufp = inbuf;
  inbuf_end = inbufp + inbuf_size;
  outbufp = outbuf;
  outbuf_end = outbufp + outbuf_size;

  /* shortcut for the easy case, avoids special casing later */
  if (self->from == self->to || 
      self->to == G_DATA_STREAM_NEWLINE_TYPE_ANY)
    {
      size = MIN (inbuf_size, outbuf_size);

      if (size > 0)
        memcpy (outbufp, inbufp, size);
      inbufp += size;
      outbufp += size;

      goto done;
    }

  /* ignore \r at end of input when we care about \r\n */
  if ((flags & G_CONVERTER_INPUT_AT_END) == 0 &&
      (self->from == G_DATA_STREAM_NEWLINE_TYPE_CR_LF ||
       self->from == G_DATA_STREAM_NEWLINE_TYPE_ANY) &&
      inbufp < inbuf_end && 
      inbuf_end[-1] == '\r')
    inbuf_end--;

  while (inbufp < inbuf_end && outbufp < outbuf_end)
    {
      const char *linebreak;

      switch (self->from)
        {
          case G_DATA_STREAM_NEWLINE_TYPE_LF:
            linebreak = memchr (inbufp, '\n', inbuf_end - inbufp);
            break;
          case G_DATA_STREAM_NEWLINE_TYPE_CR:
            linebreak = memchr (inbufp, '\r', inbuf_end - inbufp);
            break;
          case G_DATA_STREAM_NEWLINE_TYPE_CR_LF:
            linebreak = inbufp;
            while ((linebreak = memchr (linebreak, '\r', inbuf_end - linebreak)))
              {
                if (inbuf_end - linebreak > 1 &&
                    linebreak[1] == '\n')
                  break;
                linebreak++;
              }
            break;
          case G_DATA_STREAM_NEWLINE_TYPE_ANY:
            {
              const char *lf = memchr (inbufp, '\n', inbuf_end - inbufp);
              linebreak = memchr (inbufp, '\r', (lf ? lf : inbuf_end) - inbufp);
              if (linebreak == NULL)
                linebreak = lf;
              break;
            }
          default:
            g_assert_not_reached();
            break;
        }

      /* copy the part without linebreaks */
      if (linebreak)
        size = linebreak - inbufp;
      else
        size = inbuf_end - inbufp;
      size = MIN (outbuf_end - outbufp, size);
      if (size)
        {
          memcpy (outbufp, inbufp, size);
          outbufp += size;
          inbufp += size;
        }

      if (inbufp >= inbuf_end || outbufp >= outbuf_end)
        break;

      /* We should have broken above */
      g_assert (linebreak != NULL);
      g_assert (inbufp == linebreak);

      switch (self->to)
        {
          case G_DATA_STREAM_NEWLINE_TYPE_LF:
            *outbufp++ = '\n';
            break;
          case G_DATA_STREAM_NEWLINE_TYPE_CR:
            *outbufp++ = '\r';
            break;
          case G_DATA_STREAM_NEWLINE_TYPE_CR_LF:
            if (outbuf_end - outbufp < 2)
              goto done;
            *outbufp++ = '\r';
            *outbufp++ = '\n';
            break;
          case G_DATA_STREAM_NEWLINE_TYPE_ANY:
          default:
            g_assert_not_reached();
            break;
        }
      switch (self->from)
        {
          case G_DATA_STREAM_NEWLINE_TYPE_LF:
          case G_DATA_STREAM_NEWLINE_TYPE_CR:
            inbufp++;
            break;
          case G_DATA_STREAM_NEWLINE_TYPE_CR_LF:
            inbufp += 2;
            break;
          case G_DATA_STREAM_NEWLINE_TYPE_ANY:
            if (inbuf_end - inbufp > 1 && inbufp[0] == '\r' && inbufp[1] == '\n')
              inbufp += 2;
            else
              inbufp++;
            break;
          default:
            g_assert_not_reached ();
            break;
        }
    }

done:
  if (inbufp == inbuf &&
      !(flags & G_CONVERTER_FLUSH))
    {
      g_assert (outbufp == outbuf);
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_PARTIAL_INPUT,
                           _("Not enough input"));
      return G_CONVERTER_ERROR;
    }

  if ((flags & G_CONVERTER_INPUT_AT_END) &&
      inbufp == inbuf_end)
    ret = G_CONVERTER_FINISHED;
  else if (flags & G_CONVERTER_FLUSH)
    ret = G_CONVERTER_FLUSHED;
  else
    ret = G_CONVERTER_CONVERTED;

  *bytes_read = inbufp - (const char *) inbuf;
  *bytes_written = outbufp - (char *) outbuf;

  return ret;
}

static void
g_newline_converter_reset (GConverter *converter)
{
  /* nothing to do here */
}

static void
g_newline_converter_iface_init (GConverterIface *iface)
{
  iface->convert = g_newline_converter_convert;
  iface->reset = g_newline_converter_reset;
}

static void
g_newline_converter_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  GNewlineConverter *conv;

  conv = G_NEWLINE_CONVERTER (object);

  switch (prop_id)
    {
    case PROP_TO_NEWLINE:
      conv->to = g_value_get_enum (value);
      break;

    case PROP_FROM_NEWLINE:
      conv->from = g_value_get_enum (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }

}

static void
g_newline_converter_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  GNewlineConverter *conv;

  conv = G_NEWLINE_CONVERTER (object);

  switch (prop_id)
    {
    case PROP_TO_NEWLINE:
      g_value_set_enum (value, conv->to);
      break;

    case PROP_FROM_NEWLINE:
      g_value_set_enum (value, conv->from);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
g_newline_converter_class_init (GNewlineConverterClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->get_property = g_newline_converter_get_property;
  gobject_class->set_property = g_newline_converter_set_property;

  g_object_class_install_property (gobject_class,
                                   PROP_TO_NEWLINE,
                                   g_param_spec_enum ("to-newline",
                                                      P_("To Newline"),
                                                      P_("The newline type to convert to"),
                                                      G_TYPE_DATA_STREAM_NEWLINE_TYPE,
                                                      G_DATA_STREAM_NEWLINE_TYPE_LF,
                                                      G_PARAM_READWRITE|G_PARAM_STATIC_NAME|G_PARAM_STATIC_BLURB));
  g_object_class_install_property (gobject_class,
                                   PROP_FROM_NEWLINE,
                                   g_param_spec_enum ("from-newline",
                                                      P_("From Newline"),
                                                      P_("The newline type to convert from"),
                                                      G_TYPE_DATA_STREAM_NEWLINE_TYPE,
                                                      G_DATA_STREAM_NEWLINE_TYPE_LF,
                                                      G_PARAM_READWRITE|G_PARAM_STATIC_NAME|G_PARAM_STATIC_BLURB));
}

static void
g_newline_converter_init (GNewlineConverter *local)
{
}


/**
 * g_newline_converter_new:
 * @to_newline: destination newline
 * @from_newline: source newline
 *
 * Creates a new #GNewlineConverter.
 *
 * Returns: a new #GNewlineConverter
 **/
GNewlineConverter *
g_newline_converter_new (GDataStreamNewlineType to_newline,
                         GDataStreamNewlineType from_newline)
{
  GNewlineConverter *conv;

  conv = g_object_new (G_TYPE_NEWLINE_CONVERTER,
                       "to-newline", to_newline,
                       "from-newline", from_newline,
                       NULL);

  return conv;
}

