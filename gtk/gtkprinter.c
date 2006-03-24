/* GtkPrinter
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

#include <config.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "gtkintl.h"
#include <gtk/gtk.h>
#include <gtk/gtkprivate.h>

#include "gtkprinter.h"
#include "gtkprinter-private.h"
#include "gtkprintbackend.h"
#include "gtkprintjob.h"

#define GTK_PRINTER_GET_PRIVATE(o)  \
   (G_TYPE_INSTANCE_GET_PRIVATE ((o), GTK_TYPE_PRINTER, GtkPrinterPrivate))

static void gtk_printer_finalize     (GObject *object);

enum {
  DETAILS_ACQUIRED,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_NAME,
  PROP_STATE_MESSAGE,
  PROP_LOCATION,
  PROP_ICON_NAME,
  PROP_JOB_COUNT
};

static guint signals[LAST_SIGNAL] = { 0 };

static void gtk_printer_set_property (GObject      *object,
				      guint         prop_id,
				      const GValue *value,
				      GParamSpec   *pspec);
static void gtk_printer_get_property (GObject      *object,
				      guint         prop_id,
				      GValue       *value,
				      GParamSpec   *pspec);

G_DEFINE_TYPE (GtkPrinter, gtk_printer, G_TYPE_OBJECT);

static void
gtk_printer_class_init (GtkPrinterClass *class)
{
  GObjectClass *object_class;
  object_class = (GObjectClass *) class;

  object_class->finalize = gtk_printer_finalize;

  object_class->set_property = gtk_printer_set_property;
  object_class->get_property = gtk_printer_get_property;

  g_type_class_add_private (class, sizeof (GtkPrinterPrivate));

  g_object_class_install_property (G_OBJECT_CLASS (class),
                                   PROP_NAME,
                                   g_param_spec_string ("name",
						        P_("Name"),
						        P_("Name of the printer job"),
						        NULL,
							GTK_PARAM_READABLE));
  g_object_class_install_property (G_OBJECT_CLASS (class),
                                   PROP_STATE_MESSAGE,
                                   g_param_spec_string ("state-message",
						        P_("State Message"),
						        P_("String giving the current state of the printer"),
						        NULL,
							GTK_PARAM_READABLE));
  g_object_class_install_property (G_OBJECT_CLASS (class),
                                   PROP_LOCATION,
                                   g_param_spec_string ("location",
						        P_("Location"),
						        P_("The location of the printer"),
						        NULL,
							GTK_PARAM_READABLE));
  g_object_class_install_property (G_OBJECT_CLASS (class),
                                   PROP_ICON_NAME,
                                   g_param_spec_string ("icon-name",
						        P_("Icon Name"),
						        P_("The icon name to use for the printer"),
						        NULL,
							GTK_PARAM_READABLE));
  g_object_class_install_property (G_OBJECT_CLASS (class),
                                   PROP_JOB_COUNT,
				   g_param_spec_int ("job-count",
 						     P_("Job Count"),
 						     P_("Number of jobs queued in the printer"),
 						     0,
 						     G_MAXINT,
 						     0,
 						     GTK_PARAM_READABLE));


  signals[DETAILS_ACQUIRED] =
   g_signal_new ("details-acquired",
                 G_TYPE_FROM_CLASS (class),
                 G_SIGNAL_RUN_LAST,
                 G_STRUCT_OFFSET (GtkPrinterClass, details_acquired),
                 NULL, NULL,
                 g_cclosure_marshal_VOID__BOOLEAN,
                 G_TYPE_NONE, 1, G_TYPE_BOOLEAN);
}

static void
gtk_printer_init (GtkPrinter *printer)
{
  printer->priv = GTK_PRINTER_GET_PRIVATE (printer); 

  printer->priv->name = NULL;
  printer->priv->location = NULL;
  printer->priv->description = NULL;
  printer->priv->icon_name = NULL;

  printer->priv->is_active = TRUE;
  printer->priv->is_new = TRUE;
  printer->priv->has_details = FALSE;

  printer->priv->state_message = NULL;  
  printer->priv->job_count = 0;
}

static void
gtk_printer_finalize (GObject *object)
{
  g_return_if_fail (object != NULL);

  GtkPrinter *printer = GTK_PRINTER (object);

  g_free (printer->priv->name);
  g_free (printer->priv->location);
  g_free (printer->priv->description);
  g_free (printer->priv->state_message);
  g_free (printer->priv->icon_name);

  if (G_OBJECT_CLASS (gtk_printer_parent_class)->finalize)
    G_OBJECT_CLASS (gtk_printer_parent_class)->finalize (object);
}

static void
gtk_printer_set_property (GObject         *object,
			  guint            prop_id,
			  const GValue    *value,
			  GParamSpec      *pspec)
{
  /* No writable properties */
}

static void
gtk_printer_get_property (GObject    *object,
			  guint       prop_id,
			  GValue     *value,
			  GParamSpec *pspec)
{
  GtkPrinter *printer = GTK_PRINTER (object);

  switch (prop_id)
    {
    case PROP_NAME:
      if (printer->priv->name)
	g_value_set_string (value, printer->priv->name);
      else
	g_value_set_string (value, "");
      break;
    case PROP_STATE_MESSAGE:
      if (printer->priv->state_message)
	g_value_set_string (value, printer->priv->state_message);
      else
	g_value_set_string (value, "");
      break;
    case PROP_LOCATION:
      if (printer->priv->location)
	g_value_set_string (value, printer->priv->location);
      else
	g_value_set_string (value, "");
      break;
    case PROP_ICON_NAME:
      if (printer->priv->icon_name)
	g_value_set_string (value, printer->priv->icon_name);
      else
	g_value_set_string (value, "");
      break;
    case PROP_JOB_COUNT:
      g_value_set_int (value, printer->priv->job_count);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/**
 * gtk_printer_new:
 *
 * Creates a new #GtkPrinter.
 *
 * Return value: a new #GtkPrinter
 *
 * Since: 2.8
 **/
GtkPrinter *
gtk_printer_new (void)
{
  GObject *result;
  
  result = g_object_new (GTK_TYPE_PRINTER,
                         NULL);

  return (GtkPrinter *) result;
}

GtkPrintBackend *
gtk_printer_get_backend (GtkPrinter *printer)
{
  g_return_val_if_fail (GTK_IS_PRINTER (printer), NULL);
  
  return printer->priv->backend;
}

const gchar *
gtk_printer_get_name (GtkPrinter *printer)
{
  g_return_val_if_fail (GTK_IS_PRINTER (printer), NULL);

  return printer->priv->name;
}

const gchar *
gtk_printer_get_state_message (GtkPrinter *printer)
{
  g_return_val_if_fail (GTK_IS_PRINTER (printer), NULL);

  return printer->priv->state_message;
}

const gchar *
gtk_printer_get_location (GtkPrinter *printer)
{
  g_return_val_if_fail (GTK_IS_PRINTER (printer), NULL);

  return printer->priv->location;
}

const gchar * 
gtk_printer_get_icon_name (GtkPrinter *printer)
{
  g_return_val_if_fail (GTK_IS_PRINTER (printer), NULL);

  return printer->priv->icon_name;
}

gint 
gtk_printer_get_job_count (GtkPrinter *printer)
{
  g_return_val_if_fail (GTK_IS_PRINTER (printer), 0);

  return printer->priv->job_count;
}

gboolean
_gtk_printer_has_details (GtkPrinter *printer)
{
  g_return_val_if_fail (GTK_IS_PRINTER (printer), TRUE);
  
  return printer->priv->has_details;
}

gboolean
gtk_printer_is_active (GtkPrinter *printer)
{
  g_return_val_if_fail (GTK_IS_PRINTER (printer), TRUE);
  
  return printer->priv->is_active;
}


gboolean
gtk_printer_is_virtual (GtkPrinter *printer)
{
  g_return_val_if_fail (GTK_IS_PRINTER (printer), TRUE);
  
  return printer->priv->is_virtual;
}

GtkPrintJob *
gtk_printer_prep_job (GtkPrinter *printer,
		      GtkPrintSettings *settings,
		      GtkPageSetup *page_setup,
		      const gchar *title,
	              GError **error)
{
  GtkPrintJob *job;

  job = gtk_print_job_new (title,
			   settings,
			   page_setup,
                           printer);

  if (!gtk_print_job_prep (job, error))
    {
      g_object_unref (G_OBJECT (job));
      job = NULL;
    }
  
  return job;
}

void
_gtk_printer_request_details (GtkPrinter *printer)
{
  GtkPrintBackendIface *backend_iface = GTK_PRINT_BACKEND_GET_IFACE (printer->priv->backend);
  return backend_iface->printer_request_details (printer);
}

GtkPrinterOptionSet *
_gtk_printer_get_options (GtkPrinter *printer,
			  GtkPrintSettings *settings,
			  GtkPageSetup *page_setup)
{
  GtkPrintBackendIface *backend_iface = GTK_PRINT_BACKEND_GET_IFACE (printer->priv->backend);
  return backend_iface->printer_get_options (printer, settings, page_setup);
}

gboolean
_gtk_printer_mark_conflicts (GtkPrinter          *printer,
			     GtkPrinterOptionSet *options)
{
  GtkPrintBackendIface *backend_iface = GTK_PRINT_BACKEND_GET_IFACE (printer->priv->backend);
  return backend_iface->printer_mark_conflicts (printer, options);
}
  
void
_gtk_printer_get_settings_from_options (GtkPrinter          *printer,
					GtkPrinterOptionSet *options,
					GtkPrintSettings    *settings)
{
  GtkPrintBackendIface *backend_iface = GTK_PRINT_BACKEND_GET_IFACE (printer->priv->backend);
  return backend_iface->printer_get_settings_from_options (printer, options, settings);
}

void
_gtk_printer_prepare_for_print (GtkPrinter *printer,
				GtkPrintSettings *settings,
				GtkPageSetup *page_setup)
{
  GtkPrintBackendIface *backend_iface = GTK_PRINT_BACKEND_GET_IFACE (printer->priv->backend);
  return backend_iface->printer_prepare_for_print (printer, settings, page_setup);
}

cairo_surface_t *
_gtk_printer_create_cairo_surface (GtkPrinter *printer,
				   gdouble width, 
				   gdouble height,
				   gint cache_fd)
{
  GtkPrintBackendIface *backend_iface = GTK_PRINT_BACKEND_GET_IFACE (printer->priv->backend);

  return backend_iface->printer_create_cairo_surface (printer, width, height, cache_fd);
}

GList  *
_gtk_printer_list_papers (GtkPrinter *printer)
{
  GtkPrintBackendIface *backend_iface = GTK_PRINT_BACKEND_GET_IFACE (printer->priv->backend);

  return backend_iface->printer_list_papers (printer);
}

void
_gtk_printer_get_hard_margins          (GtkPrinter          *printer,
					double              *top,
					double              *bottom,
					double              *left,
					double              *right)
{
  GtkPrintBackendIface *backend_iface = GTK_PRINT_BACKEND_GET_IFACE (printer->priv->backend);

  backend_iface->printer_get_hard_margins (printer, top, bottom, left, right);
}

