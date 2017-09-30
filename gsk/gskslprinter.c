/* GTK - The GIMP Toolkit
 *   
 * Copyright © 2017 Benjamin Otte <otte@gnome.org>
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

#include "config.h"

#include "gskslprinterprivate.h"

struct _GskSlPrinter
{
  guint ref_count;

  GString *string;

  guint indentation;
};

GskSlPrinter *
gsk_sl_printer_new (void)
{
  GskSlPrinter *printer;

  printer = g_slice_new0 (GskSlPrinter);

  printer->ref_count = 1;

  printer->string = g_string_new (NULL);

  return printer;
}

GskSlPrinter *
gsk_sl_printer_ref (GskSlPrinter *printer)
{
  g_return_val_if_fail (printer != NULL, NULL);

  printer->ref_count += 1;

  return printer;
}

void
gsk_sl_printer_unref (GskSlPrinter *printer)
{
  if (printer == NULL)
    return;

  printer->ref_count -= 1;
  if (printer->ref_count > 0)
    return;

  if (printer->indentation > 0)
    {
      g_warning ("Missing call to gsk_sl_printer_pop_indentation().");
    }

  g_string_free (printer->string, TRUE);

  g_slice_free (GskSlPrinter, printer);
}

char *
gsk_sl_printer_write_to_string (GskSlPrinter *printer)
{
  return g_strdup (printer->string->str);
}

void
gsk_sl_printer_push_indentation (GskSlPrinter *printer)
{
  printer->indentation++;
}

void
gsk_sl_printer_pop_indentation (GskSlPrinter *printer)
{
  if (printer->indentation == 0)
    {
      g_warning ("Calling gsk_sl_printer_pop_indentation() without preceding call to gsk_sl_printer_push_indentation()");
      return;
    }

  printer->indentation--;
}


void
gsk_sl_printer_append (GskSlPrinter *printer,
                       const char   *str)
{
  g_string_append (printer->string, str);
}

void
gsk_sl_printer_append_c (GskSlPrinter *printer,
                         char          c)
{
  g_string_append_c (printer->string, c);
}

void
gsk_sl_printer_append_int (GskSlPrinter *printer,
                           int           i)
{
  g_string_append_printf (printer->string, "%d", i);
}

void
gsk_sl_printer_append_uint (GskSlPrinter *printer,
                            guint         u)
{
  g_string_append_printf (printer->string, "%u", u);
}

void
gsk_sl_printer_append_double (GskSlPrinter *printer,
                              double        d,
                              gboolean      with_dot)
{
  char buf[G_ASCII_DTOSTR_BUF_SIZE];

  g_ascii_dtostr (buf, G_ASCII_DTOSTR_BUF_SIZE, d);
  g_string_append (printer->string, buf);
  if (with_dot && strchr (buf, '.') == NULL)
    g_string_append (printer->string, ".0");
}

void
gsk_sl_printer_newline (GskSlPrinter *printer)
{
  guint i;

  g_string_append_c (printer->string, '\n');

  for (i = 0; i < printer->indentation; i++)
    {
      g_string_append (printer->string, "  ");
    }
}

