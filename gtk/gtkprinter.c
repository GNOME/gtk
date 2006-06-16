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

#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "gtkintl.h"
#include "gtkprivate.h"

#include "gtkprinter.h"
#include "gtkprinter-private.h"
#include "gtkprintbackend.h"
#include "gtkprintjob.h"
#include "gtkalias.h"

#define GTK_PRINTER_GET_PRIVATE(o)  \
   (G_TYPE_INSTANCE_GET_PRIVATE ((o), GTK_TYPE_PRINTER, GtkPrinterPrivate))

static void gtk_printer_finalize     (GObject *object);

struct _GtkPrinterPrivate
{
  gchar *name;
  gchar *location;
  gchar *description;
  gchar *icon_name;

  guint is_active   : 1;
  guint is_new      : 1;
  guint is_virtual  : 1;
  guint is_default  : 1;
  guint has_details : 1;
  guint accepts_pdf : 1;
  guint accepts_ps  : 1;

  gchar *state_message;  
  gint job_count;

  GtkPrintBackend *backend;
};

enum {
  DETAILS_ACQUIRED,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_NAME,
  PROP_BACKEND,
  PROP_IS_VIRTUAL,
  PROP_STATE_MESSAGE,
  PROP_LOCATION,
  PROP_ICON_NAME,
  PROP_JOB_COUNT,
  PROP_ACCEPTS_PDF,
  PROP_ACCEPTS_PS
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

G_DEFINE_TYPE (GtkPrinter, gtk_printer, G_TYPE_OBJECT)

static int
safe_strcmp (const char *a, const char *b)
{
  if (a == b)
    return 0;
  if (a == NULL)
    return -1;
  if (b == NULL)
    return 1;
  return strcmp (a, b);
}

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
						        P_("Name of the printer"),
						        NULL,
							GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (G_OBJECT_CLASS (class),
                                   PROP_BACKEND,
                                   g_param_spec_object ("backend",
						        P_("Backend"),
						        P_("Backend for the printer"),
						        GTK_TYPE_PRINT_BACKEND,
							GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (G_OBJECT_CLASS (class),
                                   PROP_IS_VIRTUAL,
                                   g_param_spec_boolean ("is-virtual",
							 P_("Is Virtual"),
							 P_("FALSE if this represents a real hardware printer"),
							 FALSE,
							 GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (G_OBJECT_CLASS (class),
                                   PROP_ACCEPTS_PDF,
                                   g_param_spec_boolean ("accepts-pdf",
							 P_("Accepts PDF"),
							 P_("TRUE if this printer can accept PDF"),
							 TRUE,
							 GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (G_OBJECT_CLASS (class),
                                   PROP_ACCEPTS_PS,
                                   g_param_spec_boolean ("accepts-ps",
							 P_("Accepts PostScript"),
							 P_("TRUE if this printer can accept PostScript"),
							 TRUE,
							 GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
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

  /**
   * GtkPrinter::details-acquired:
   * @printer: the #GtkPrinter on which the signal is emitted
   * @success: %TRUE if the details were successfully acquired
   *
   * Gets emitted in response to a request for detailed information
   * about a printer from the print backend. The @success parameter
   * indicates if the information was actually obtained.
   *
   * Since: 2.10
   */
  signals[DETAILS_ACQUIRED] =
    g_signal_new (I_("details-acquired"),
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
  GtkPrinterPrivate *priv;

  priv = printer->priv = GTK_PRINTER_GET_PRIVATE (printer); 

  priv->name = NULL;
  priv->location = NULL;
  priv->description = NULL;
  priv->icon_name = NULL;

  priv->is_active = TRUE;
  priv->is_new = TRUE;
  priv->has_details = FALSE;
  priv->accepts_pdf = TRUE;
  priv->accepts_ps = TRUE;

  priv->state_message = NULL;  
  priv->job_count = 0;
}

static void
gtk_printer_finalize (GObject *object)
{
  GtkPrinter *printer = GTK_PRINTER (object);
  GtkPrinterPrivate *priv = printer->priv;

  g_free (priv->name);
  g_free (priv->location);
  g_free (priv->description);
  g_free (priv->state_message);
  g_free (priv->icon_name);

  if (priv->backend)
    g_object_unref (priv->backend);

  G_OBJECT_CLASS (gtk_printer_parent_class)->finalize (object);
}

static void
gtk_printer_set_property (GObject         *object,
			  guint            prop_id,
			  const GValue    *value,
			  GParamSpec      *pspec)
{
  GtkPrinter *printer = GTK_PRINTER (object);
  GtkPrinterPrivate *priv = printer->priv;

  switch (prop_id)
    {
    case PROP_NAME:
      priv->name = g_value_dup_string (value);
      break;
    
    case PROP_BACKEND:
      priv->backend = GTK_PRINT_BACKEND (g_value_dup_object (value));
      break;

    case PROP_IS_VIRTUAL:
      priv->is_virtual = g_value_get_boolean (value);
      break;

    case PROP_ACCEPTS_PDF:
      priv->accepts_pdf = g_value_get_boolean (value);
      break;

    case PROP_ACCEPTS_PS:
      priv->accepts_ps = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_printer_get_property (GObject    *object,
			  guint       prop_id,
			  GValue     *value,
			  GParamSpec *pspec)
{
  GtkPrinter *printer = GTK_PRINTER (object);
  GtkPrinterPrivate *priv = printer->priv;

  switch (prop_id)
    {
    case PROP_NAME:
      if (priv->name)
	g_value_set_string (value, priv->name);
      else
	g_value_set_string (value, "");
      break;
    case PROP_BACKEND:
      g_value_set_object (value, priv->backend);
      break;
    case PROP_STATE_MESSAGE:
      if (priv->state_message)
	g_value_set_string (value, priv->state_message);
      else
	g_value_set_string (value, "");
      break;
    case PROP_LOCATION:
      if (priv->location)
	g_value_set_string (value, priv->location);
      else
	g_value_set_string (value, "");
      break;
    case PROP_ICON_NAME:
      if (priv->icon_name)
	g_value_set_string (value, priv->icon_name);
      else
	g_value_set_string (value, "");
      break;
    case PROP_JOB_COUNT:
      g_value_set_int (value, priv->job_count);
      break;
    case PROP_IS_VIRTUAL:
      g_value_set_boolean (value, priv->is_virtual);
      break;
    case PROP_ACCEPTS_PDF:
      g_value_set_boolean (value, priv->accepts_pdf);
      break;
    case PROP_ACCEPTS_PS:
      g_value_set_boolean (value, priv->accepts_ps);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/**
 * gtk_printer_new:
 * @name: the name of the printer
 * @backend: a #GtkPrintBackend
 * @virtual_: whether the printer is virtual
 *
 * Creates a new #GtkPrinter.
 *
 * Return value: a new #GtkPrinter
 *
 * Since: 2.10
 **/
GtkPrinter *
gtk_printer_new (const gchar     *name,
		 GtkPrintBackend *backend,
		 gboolean         virtual_)
{
  GObject *result;
  
  result = g_object_new (GTK_TYPE_PRINTER,
			 "name", name,
			 "backend", backend,
			 "is-virtual", virtual_,
                         NULL);

  return (GtkPrinter *) result;
}

/**
 * gtk_printer_get_backend:
 * @printer: a #GtkPrinter
 * 
 * Returns the backend of the printer.
 * 
 * Return value: the backend of @printer
 * 
 * Since: 2.10
 */
GtkPrintBackend *
gtk_printer_get_backend (GtkPrinter *printer)
{
  g_return_val_if_fail (GTK_IS_PRINTER (printer), NULL);
  
  return printer->priv->backend;
}

/**
 * gtk_printer_get_name:
 * @printer: a #GtkPrinter
 * 
 * Returns the name of the printer.
 * 
 * Return value: the name of @printer
 *
 * Since: 2.10
 */
G_CONST_RETURN gchar *
gtk_printer_get_name (GtkPrinter *printer)
{
  g_return_val_if_fail (GTK_IS_PRINTER (printer), NULL);

  return printer->priv->name;
}

/**
 * gtk_printer_get_description:
 * @printer: a #GtkPrinter
 * 
 * Gets the description of the printer.
 * 
 * Return value: the description of @printer
 *
 * Since: 2.10
 */
G_CONST_RETURN gchar *
gtk_printer_get_description (GtkPrinter *printer)
{
  g_return_val_if_fail (GTK_IS_PRINTER (printer), NULL);
  
  return printer->priv->description;
}

gboolean
gtk_printer_set_description (GtkPrinter  *printer,
			     const gchar *description)
{
  GtkPrinterPrivate *priv;

  g_return_val_if_fail (GTK_IS_PRINTER (printer), FALSE);

  priv = printer->priv;

  if (safe_strcmp (priv->description, description) == 0)
    return FALSE;

  g_free (priv->description);
  priv->description = g_strdup (description);
  
  return TRUE;
}

/**
 * gtk_printer_get_state_message:
 * @printer: a #GtkPrinter
 * 
 * Returns the state message describing the current state
 * of the printer.
 * 
 * Return value: the state message of @printer
 *
 * Since: 2.10
 */
G_CONST_RETURN gchar *
gtk_printer_get_state_message (GtkPrinter *printer)
{
  g_return_val_if_fail (GTK_IS_PRINTER (printer), NULL);

  return printer->priv->state_message;
}

gboolean
gtk_printer_set_state_message (GtkPrinter  *printer,
			       const gchar *message)
{
  GtkPrinterPrivate *priv;

  g_return_val_if_fail (GTK_IS_PRINTER (printer), FALSE);

  priv = printer->priv;

  if (safe_strcmp (priv->state_message, message) == 0)
    return FALSE;

  g_free (priv->state_message);
  priv->state_message = g_strdup (message);
  g_object_notify (G_OBJECT (printer), "state-message");

  return TRUE;
}

/**
 * gtk_printer_get_location:
 * @printer: a #GtkPrinter
 * 
 * Returns a  description of the location of the printer.
 * 
 * Return value: the location of @printer
 *
 * Since: 2.10
 */
G_CONST_RETURN gchar *
gtk_printer_get_location (GtkPrinter *printer)
{
  g_return_val_if_fail (GTK_IS_PRINTER (printer), NULL);

  return printer->priv->location;
}

gboolean
gtk_printer_set_location (GtkPrinter  *printer,
			  const gchar *location)
{
  GtkPrinterPrivate *priv;

  g_return_val_if_fail (GTK_IS_PRINTER (printer), FALSE);

  priv = printer->priv;

  if (safe_strcmp (priv->location, location) == 0)
    return FALSE;

  g_free (priv->location);
  priv->location = g_strdup (location);
  g_object_notify (G_OBJECT (printer), "location");
  
  return TRUE;
}

/**
 * gtk_printer_get_icon_name:
 * @printer: a #GtkPrinter
 * 
 * Gets the name of the icon to use for the printer.
 * 
 * Return value: the icon name for @printer
 *
 * Since: 2.10
 */
G_CONST_RETURN gchar * 
gtk_printer_get_icon_name (GtkPrinter *printer)
{
  g_return_val_if_fail (GTK_IS_PRINTER (printer), NULL);

  return printer->priv->icon_name;
}

void
gtk_printer_set_icon_name (GtkPrinter  *printer,
			   const gchar *icon)
{
  GtkPrinterPrivate *priv;

  g_return_if_fail (GTK_IS_PRINTER (printer));

  priv = printer->priv;

  g_free (priv->icon_name);
  priv->icon_name = g_strdup (icon);
  g_object_notify (G_OBJECT (printer), "icon-name");
}

/**
 * gtk_printer_get_job_count:
 * @printer: a #GtkPrinter
 * 
 * Gets the number of jobs currently queued on the printer.
 * 
 * Return value: the number of jobs on @printer
 *
 * Since: 2.10
 */
gint 
gtk_printer_get_job_count (GtkPrinter *printer)
{
  g_return_val_if_fail (GTK_IS_PRINTER (printer), 0);

  return printer->priv->job_count;
}

gboolean
gtk_printer_set_job_count (GtkPrinter *printer,
			   gint        count)
{
  GtkPrinterPrivate *priv;

  g_return_val_if_fail (GTK_IS_PRINTER (printer), FALSE);

  priv = printer->priv;

  if (priv->job_count == count)
    return FALSE;

  priv->job_count = count;
  
  g_object_notify (G_OBJECT (printer), "job-count");
  
  return TRUE;
}

gboolean
_gtk_printer_has_details (GtkPrinter *printer)
{
  return printer->priv->has_details;
}

void
gtk_printer_set_has_details (GtkPrinter *printer,
			     gboolean val)
{
  printer->priv->has_details = val;
}

/**
 * gtk_printer_is_active:
 * @printer: a #GtkPrinter
 * 
 * Returns whether the printer is currently active (i.e. 
 * accepts new jobs).
 * 
 * Return value: %TRUE if @printer is active
 *
 * Since: 2.10
 */
gboolean
gtk_printer_is_active (GtkPrinter *printer)
{
  g_return_val_if_fail (GTK_IS_PRINTER (printer), TRUE);
  
  return printer->priv->is_active;
}

void
gtk_printer_set_is_active (GtkPrinter *printer,
			   gboolean val)
{
  g_return_if_fail (GTK_IS_PRINTER (printer));

  printer->priv->is_active = val;
}


/**
 * gtk_printer_is_virtual:
 * @printer: a #GtkPrinter
 * 
 * Returns whether the printer is virtual (i.e. does not
 * represent actual printer hardware, but something like 
 * a CUPS class).
 * 
 * Return value: %TRUE if @printer is virtual
 *
 * Since: 2.10
 */
gboolean
gtk_printer_is_virtual (GtkPrinter *printer)
{
  g_return_val_if_fail (GTK_IS_PRINTER (printer), TRUE);
  
  return printer->priv->is_virtual;
}

gboolean 
gtk_printer_accepts_pdf (GtkPrinter *printer)
{ 
  g_return_val_if_fail (GTK_IS_PRINTER (printer), TRUE);
  
  return printer->priv->accepts_pdf;
}

gboolean 
gtk_printer_accepts_ps (GtkPrinter *printer)
{ 
  g_return_val_if_fail (GTK_IS_PRINTER (printer), TRUE);
  
  return printer->priv->accepts_ps;
}

gboolean
gtk_printer_is_new (GtkPrinter *printer)
{
  g_return_val_if_fail (GTK_IS_PRINTER (printer), FALSE);
  
  return printer->priv->is_new;
}

void
gtk_printer_set_is_new (GtkPrinter *printer,
			gboolean val)
{
  g_return_if_fail (GTK_IS_PRINTER (printer));

  printer->priv->is_new = val;
}


/**
 * gtk_printer_is_default:
 * @printer: a #GtkPrinter
 * 
 * Returns whether the printer is the default printer.
 * 
 * Return value: %TRUE if @printer is the default
 *
 * Since: 2.10
 */
gboolean
gtk_printer_is_default (GtkPrinter *printer)
{
  g_return_val_if_fail (GTK_IS_PRINTER (printer), FALSE);
  
  return printer->priv->is_default;
}

void
gtk_printer_set_is_default (GtkPrinter *printer,
			    gboolean    val)
{
  g_return_if_fail (GTK_IS_PRINTER (printer));

  printer->priv->is_default = TRUE;
}

void
_gtk_printer_request_details (GtkPrinter *printer)
{
  GtkPrintBackendClass *backend_class = GTK_PRINT_BACKEND_GET_CLASS (printer->priv->backend);
  backend_class->printer_request_details (printer);
}

GtkPrinterOptionSet *
_gtk_printer_get_options (GtkPrinter       *printer,
			  GtkPrintSettings *settings,
			  GtkPageSetup     *page_setup)
{
  GtkPrintBackendClass *backend_class = GTK_PRINT_BACKEND_GET_CLASS (printer->priv->backend);
  return backend_class->printer_get_options (printer, settings, page_setup);
}

gboolean
_gtk_printer_mark_conflicts (GtkPrinter          *printer,
			     GtkPrinterOptionSet *options)
{
  GtkPrintBackendClass *backend_class = GTK_PRINT_BACKEND_GET_CLASS (printer->priv->backend);
  return backend_class->printer_mark_conflicts (printer, options);
}
  
void
_gtk_printer_get_settings_from_options (GtkPrinter          *printer,
					GtkPrinterOptionSet *options,
					GtkPrintSettings    *settings)
{
  GtkPrintBackendClass *backend_class = GTK_PRINT_BACKEND_GET_CLASS (printer->priv->backend);
  backend_class->printer_get_settings_from_options (printer, options, settings);
}

void
_gtk_printer_prepare_for_print (GtkPrinter       *printer,
				GtkPrintJob      *print_job,
				GtkPrintSettings *settings,
				GtkPageSetup     *page_setup)
{
  GtkPrintBackendClass *backend_class = GTK_PRINT_BACKEND_GET_CLASS (printer->priv->backend);
  backend_class->printer_prepare_for_print (printer, print_job, settings, page_setup);
}

cairo_surface_t *
_gtk_printer_create_cairo_surface (GtkPrinter       *printer,
				   GtkPrintSettings *settings,
				   gdouble           width, 
				   gdouble           height,
				   gint              cache_fd)
{
  GtkPrintBackendClass *backend_class = GTK_PRINT_BACKEND_GET_CLASS (printer->priv->backend);

  return backend_class->printer_create_cairo_surface (printer, settings,
						      width, height, cache_fd);
}

GList  *
_gtk_printer_list_papers (GtkPrinter *printer)
{
  GtkPrintBackendClass *backend_class = GTK_PRINT_BACKEND_GET_CLASS (printer->priv->backend);

  return backend_class->printer_list_papers (printer);
}

void
_gtk_printer_get_hard_margins (GtkPrinter *printer,
			       gdouble    *top,
			       gdouble    *bottom,
			       gdouble    *left,
			       gdouble    *right)
{
  GtkPrintBackendClass *backend_class = GTK_PRINT_BACKEND_GET_CLASS (printer->priv->backend);

  backend_class->printer_get_hard_margins (printer, top, bottom, left, right);
}

GtkPrintCapabilities
_gtk_printer_get_capabilities (GtkPrinter *printer)
{
  GtkPrintBackendClass *backend_class = GTK_PRINT_BACKEND_GET_CLASS (printer->priv->backend);

  return backend_class->printer_get_capabilities (printer);
}

gint
gtk_printer_compare (GtkPrinter *a, GtkPrinter *b)
{
  const char *name_a, *name_b;
  
  g_assert (GTK_IS_PRINTER (a) && GTK_IS_PRINTER (b));
  
  name_a = gtk_printer_get_name (a);
  name_b = gtk_printer_get_name (b);
  if (name_a == NULL  && name_b == NULL)
    return 0;
  else if (name_a == NULL)
    return G_MAXINT;
  else if (name_b == NULL)
    return G_MININT;
  else
    return g_ascii_strcasecmp (name_a, name_b);
}

#define __GTK_PRINTER_C__
#include "gtkaliasdef.c"
