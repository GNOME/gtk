/* GtkPrinterCupsCups
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
#include "gtkprintercups.h"

static void gtk_printer_cups_init       (GtkPrinterCups      *printer);
static void gtk_printer_cups_class_init (GtkPrinterCupsClass *class);
static void gtk_printer_cups_finalize   (GObject             *object);

static GtkPrinterClass *gtk_printer_cups_parent_class;
static GType gtk_printer_cups_type = 0;

void 
gtk_printer_cups_register_type (GTypeModule *module)
{
  const GTypeInfo object_info =
  {
    sizeof (GtkPrinterCupsClass),
    (GBaseInitFunc) NULL,
    (GBaseFinalizeFunc) NULL,
    (GClassInitFunc) gtk_printer_cups_class_init,
    NULL,           /* class_finalize */
    NULL,           /* class_data */
    sizeof (GtkPrinterCups),
    0,              /* n_preallocs */
    (GInstanceInitFunc) gtk_printer_cups_init,
  };

 gtk_printer_cups_type = g_type_module_register_type (module,
                                                      GTK_TYPE_PRINTER,
                                                      "GtkPrinterCups",
                                                      &object_info, 0);
}

GType
gtk_printer_cups_get_type (void)
{
  return gtk_printer_cups_type;
}

static void
gtk_printer_cups_class_init (GtkPrinterCupsClass *class)
{
  GObjectClass *object_class = (GObjectClass *) class;
	
  gtk_printer_cups_parent_class = g_type_class_peek_parent (class);

  object_class->finalize = gtk_printer_cups_finalize;
}

static void
gtk_printer_cups_init (GtkPrinterCups *printer)
{
  printer->device_uri = NULL;
  printer->printer_uri = NULL;
  printer->state = 0;
  printer->hostname = NULL;
  printer->port = 0;
  printer->ppd_name = NULL;
  printer->ppd_file = NULL;
  printer->default_cover_before = NULL;
  printer->default_cover_after = NULL;
  printer->remote = FALSE;
  printer->get_remote_ppd_poll = 0;
  printer->get_remote_ppd_attempts = 0;
  printer->remote_cups_connection_test = NULL;
  printer->auth_info_required = NULL;
}

static void
gtk_printer_cups_finalize (GObject *object)
{
  GtkPrinterCups *printer;

  g_return_if_fail (object != NULL);

  printer = GTK_PRINTER_CUPS (object);

  g_free (printer->device_uri);
  g_free (printer->printer_uri);
  g_free (printer->hostname);
  g_free (printer->ppd_name);
  g_free (printer->default_cover_before);
  g_free (printer->default_cover_after);
  g_strfreev (printer->auth_info_required);

  if (printer->ppd_file)
    ppdClose (printer->ppd_file);

  if (printer->get_remote_ppd_poll > 0)
    g_source_remove (printer->get_remote_ppd_poll);
  printer->get_remote_ppd_attempts = 0;

  gtk_cups_connection_test_free (printer->remote_cups_connection_test);

  G_OBJECT_CLASS (gtk_printer_cups_parent_class)->finalize (object);
}

/**
 * gtk_printer_cups_new:
 *
 * Creates a new #GtkPrinterCups.
 *
 * Return value: a new #GtkPrinterCups
 *
 * Since: 2.10
 **/
GtkPrinterCups *
gtk_printer_cups_new (const char      *name,
		      GtkPrintBackend *backend)
{
  GObject *result;
  gboolean accepts_pdf;

#if (CUPS_VERSION_MAJOR == 1 && CUPS_VERSION_MINOR >= 2) || CUPS_VERSION_MAJOR > 1
  accepts_pdf = TRUE;
#else
  accepts_pdf = FALSE;
#endif

  result = g_object_new (GTK_TYPE_PRINTER_CUPS,
			 "name", name,
			 "backend", backend,
			 "is-virtual", FALSE,
			 "accepts-pdf", accepts_pdf,
                         NULL);

  return (GtkPrinterCups *) result;
}

ppd_file_t *
gtk_printer_cups_get_ppd (GtkPrinterCups *printer)
{
  return printer->ppd_file;
}

const gchar *
gtk_printer_cups_get_ppd_name (GtkPrinterCups  *printer)
{
  const gchar *result;

  result = printer->ppd_name;

  if (result == NULL)
    result = gtk_printer_get_name (GTK_PRINTER (printer));

  return result;
}
