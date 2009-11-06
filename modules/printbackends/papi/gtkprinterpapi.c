/* GtkPrinterPapi
 * Copyright (C) 2006 John (J5) Palmieri  <johnp@redhat.com>
 * Copyright (C) 2009 Ghee Teo <ghee.teo@sun.com>
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
#include "gtkprinterpapi.h"

static void gtk_printer_papi_init       (GtkPrinterPapi      *printer);
static void gtk_printer_papi_class_init (GtkPrinterPapiClass *class);
static void gtk_printer_papi_finalize   (GObject             *object);

static GtkPrinterClass *gtk_printer_papi_parent_class;
static GType gtk_printer_papi_type = 0;

void 
gtk_printer_papi_register_type (GTypeModule *module)
{
  const GTypeInfo object_info =
  {
    sizeof (GtkPrinterPapiClass),
    (GBaseInitFunc) NULL,
    (GBaseFinalizeFunc) NULL,
    (GClassInitFunc) gtk_printer_papi_class_init,
    NULL,           /* class_finalize */
    NULL,           /* class_data */
    sizeof (GtkPrinterPapi),
    0,              /* n_preallocs */
    (GInstanceInitFunc) gtk_printer_papi_init,
  };

 gtk_printer_papi_type = g_type_module_register_type (module,
                                                      GTK_TYPE_PRINTER,
                                                      "GtkPrinterPapi",
                                                      &object_info, 0);
}

GType
gtk_printer_papi_get_type (void)
{
  return gtk_printer_papi_type;
}

static void
gtk_printer_papi_class_init (GtkPrinterPapiClass *class)
{
  GObjectClass *object_class = (GObjectClass *) class;
	
  gtk_printer_papi_parent_class = g_type_class_peek_parent (class);

  object_class->finalize = gtk_printer_papi_finalize;
}

static void
gtk_printer_papi_init (GtkPrinterPapi *printer)
{
  printer->printer_name = NULL;
}

static void
gtk_printer_papi_finalize (GObject *object)
{
  GtkPrinterPapi *printer;

  g_return_if_fail (object != NULL);

  printer = GTK_PRINTER_PAPI (object);

  g_free(printer->printer_name);

  G_OBJECT_CLASS (gtk_printer_papi_parent_class)->finalize (object);
}

/**
 * gtk_printer_papi_new:
 *
 * Creates a new #GtkPrinterPapi.
 *
 * Return value: a new #GtkPrinterPapi
 *
 * Since: 2.10
 **/
GtkPrinterPapi *
gtk_printer_papi_new (const char      *name,
		      GtkPrintBackend *backend)
{
  GObject *result;
  GtkPrinterPapi *pp;
  
  result = g_object_new (GTK_TYPE_PRINTER_PAPI,
			 "name", name,
			 "backend", backend,
			 "is-virtual", TRUE,
                         NULL);
  pp = GTK_PRINTER_PAPI(result);

  pp->printer_name = g_strdup (name);

  return (GtkPrinterPapi *) pp;
}

