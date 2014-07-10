/* gtkprintercloudprint.c: Google Cloud Print -specific Printer class,
 * GtkPrinterCloudprint
 * Copyright (C) 2014, Red Hat, Inc.
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

#include <glib/gi18n-lib.h>
#include <gtk/gtkintl.h>

#include "gtkprintercloudprint.h"
#include "gtkcloudprintaccount.h"

typedef struct _GtkPrinterCloudprintClass GtkPrinterCloudprintClass;

#define GTK_PRINTER_CLOUDPRINT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_PRINTER_CLOUDPRINT, GtkPrinterCloudprintClass))
#define GTK_IS_PRINTER_CLOUDPRINT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_PRINTER_CLOUDPRINT))
#define GTK_PRINTER_CLOUDPRINT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_PRINTER_CLOUDPRINT, GtkPrinterCloudprintClass))

static GtkPrinterClass *gtk_printer_cloudprint_parent_class;
static GType printer_cloudprint_type = 0;

struct _GtkPrinterCloudprintClass
{
  GtkPrinterClass parent_class;
};

struct _GtkPrinterCloudprint
{
  GtkPrinter parent_instance;

  GtkCloudprintAccount *account;
  gchar *id;
};

enum {
  PROP_0,
  PROP_CLOUDPRINT_ACCOUNT,
  PROP_PRINTER_ID
};

static void gtk_printer_cloudprint_class_init	(GtkPrinterCloudprintClass *class);
static void gtk_printer_cloudprint_init		(GtkPrinterCloudprint *impl);
static void gtk_printer_cloudprint_finalize	(GObject *object);
static void gtk_printer_cloudprint_set_property	(GObject *object,
						 guint prop_id,
						 const GValue *value,
						 GParamSpec *pspec);
static void gtk_printer_cloudprint_get_property	(GObject *object,
						 guint prop_id,
						 GValue *value,
						 GParamSpec *pspec);

void
gtk_printer_cloudprint_register_type (GTypeModule *module)
{
  const GTypeInfo printer_cloudprint_info =
  {
    sizeof (GtkPrinterCloudprintClass),
    NULL,		/* base_init */
    NULL,		/* base_finalize */
    (GClassInitFunc) gtk_printer_cloudprint_class_init,
    NULL,		/* class_finalize */
    NULL,		/* class_data */
    sizeof (GtkPrinterCloudprint),
    0,		/* n_preallocs */
    (GInstanceInitFunc) gtk_printer_cloudprint_init,
  };

  printer_cloudprint_type = g_type_module_register_type (module,
							 GTK_TYPE_PRINTER,
							 "GtkPrinterCloudprint",
							 &printer_cloudprint_info, 0);
}

/*
 * GtkPrinterCloudprint
 */
GType
gtk_printer_cloudprint_get_type (void)
{
  return printer_cloudprint_type;
}

/**
 * gtk_printer_cloudprint_new:
 *
 * Creates a new #GtkPrinterCloudprint object. #GtkPrinterCloudprint
 * implements the #GtkPrinter interface and stores a reference to the
 * #GtkCloudprintAccount object and the printer-id to use
 *
 * Returns: the new #GtkPrinterCloudprint object
 **/
GtkPrinterCloudprint *
gtk_printer_cloudprint_new (const char *name,
			    gboolean is_virtual,
			    GtkPrintBackend *backend,
			    GtkCloudprintAccount *account,
			    const gchar *id)
{
  return g_object_new (GTK_TYPE_PRINTER_CLOUDPRINT,
		       "name", name,
		       "backend", backend,
		       "is-virtual", is_virtual,
		       "accepts-pdf", TRUE,
		       "cloudprint-account", account,
		       "printer-id", id,
		       NULL);
}

static void
gtk_printer_cloudprint_class_init (GtkPrinterCloudprintClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gtk_printer_cloudprint_parent_class = g_type_class_peek_parent (klass);
  gobject_class->finalize = gtk_printer_cloudprint_finalize;
  gobject_class->set_property = gtk_printer_cloudprint_set_property;
  gobject_class->get_property = gtk_printer_cloudprint_get_property;

  g_object_class_install_property (G_OBJECT_CLASS (klass),
				   PROP_CLOUDPRINT_ACCOUNT,
				   g_param_spec_object ("cloudprint-account",
							P_("Cloud Print account"),
							P_("GtkCloudprintAccount instance"),
							GTK_TYPE_CLOUDPRINT_ACCOUNT,
							G_PARAM_READWRITE |
							G_PARAM_STATIC_STRINGS |
							G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (G_OBJECT_CLASS (klass),
				   PROP_PRINTER_ID,
				   g_param_spec_string ("printer-id",
							P_("Printer ID"),
							P_("Cloud Print printer ID"),
							"",
							G_PARAM_READWRITE |
							G_PARAM_STATIC_STRINGS |
							G_PARAM_CONSTRUCT_ONLY));
}

static void
gtk_printer_cloudprint_init (GtkPrinterCloudprint *printer)
{
  printer->account = NULL;
  printer->id = NULL;

  GTK_NOTE (PRINTING,
	    g_print ("Cloud Print Backend: +GtkPrinterCloudprint(%p)\n",
		     printer));
}

static void
gtk_printer_cloudprint_finalize (GObject *object)
{
  GtkPrinterCloudprint *printer;

  printer = GTK_PRINTER_CLOUDPRINT (object);

  GTK_NOTE (PRINTING,
	    g_print ("Cloud Print Backend: -GtkPrinterCloudprint(%p)\n",
		     printer));

  if (printer->account != NULL)
    g_object_unref (printer->account);

  g_free (printer->id);

  G_OBJECT_CLASS (gtk_printer_cloudprint_parent_class)->finalize (object);
}

static void
gtk_printer_cloudprint_set_property (GObject *object,
				     guint prop_id,
				     const GValue *value,
				     GParamSpec *pspec)
{
  GtkPrinterCloudprint *printer = GTK_PRINTER_CLOUDPRINT (object);

  switch (prop_id)
    {
    case PROP_CLOUDPRINT_ACCOUNT:
      printer->account = g_value_dup_object (value);
      break;

    case PROP_PRINTER_ID:
      printer->id = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_printer_cloudprint_get_property (GObject *object,
				     guint prop_id,
				     GValue *value,
				     GParamSpec *pspec)
{
  GtkPrinterCloudprint *printer = GTK_PRINTER_CLOUDPRINT (object);

  switch (prop_id)
    {
    case PROP_CLOUDPRINT_ACCOUNT:
      g_value_set_object (value, printer->account);
      break;

    case PROP_PRINTER_ID:
      g_value_set_string (value, printer->id);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}
