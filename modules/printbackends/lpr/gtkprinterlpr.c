/* GtkPrinterLpr
 * Copyright (C) 2006 John (J5) Palmieri  <johnp@redhat.com>
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"
#include "gtkprinterlpr.h"
#include "gtkprinter-private.h"

static void gtk_printer_lpr_finalize     (GObject *object);

G_DEFINE_TYPE (GtkPrinterLpr, gtk_printer_lpr, GTK_TYPE_PRINTER);

static void
gtk_printer_lpr_class_init (GtkPrinterLprClass *class)
{
  GObjectClass *object_class;
  object_class = (GObjectClass *) class;

  object_class->finalize = gtk_printer_lpr_finalize;

}

static void
gtk_printer_lpr_init (GtkPrinterLpr *printer)
{
  GtkPrinter *parent;

  parent = GTK_PRINTER (printer);

  printer->options = NULL;

  parent->priv->has_details = TRUE;
  parent->priv->is_virtual = TRUE;
  
}

static void
gtk_printer_lpr_finalize (GObject *object)
{
  g_return_if_fail (object != NULL);

  GtkPrinterLpr *printer = GTK_PRINTER_LPR (object);

  if (printer->options)
    g_object_unref (printer->options);

  if (G_OBJECT_CLASS (gtk_printer_lpr_parent_class)->finalize)
    G_OBJECT_CLASS (gtk_printer_lpr_parent_class)->finalize (object);
}

/**
 * gtk_printer_lpr_new:
 *
 * Creates a new #GtkPrinterLpr.
 *
 * Return value: a new #GtkPrinterLpr
 *
 * Since: 2.10
 **/
GtkPrinterLpr *
gtk_printer_lpr_new (void)
{
  GObject *result;
  
  result = g_object_new (GTK_TYPE_PRINTER_LPR,
                         NULL);

  return (GtkPrinterLpr *) result;
}

