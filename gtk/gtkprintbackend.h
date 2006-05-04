/* GTK - The GIMP Toolkit
 * gtkprintbackend.h: Abstract printer backend interfaces
 * Copyright (C) 2006, Red Hat, Inc.
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

#ifndef __GTK_PRINT_BACKEND_H__
#define __GTK_PRINT_BACKEND_H__

/* This is a "semi-private" header; it is meant only for
 * alternate GtkPrintDialog backend modules; no stability guarantees 
 * are made at this point
 */
#ifndef GTK_PRINT_BACKEND_ENABLE_UNSUPPORTED
#error "GtkPrintBackend is not supported API for general use"
#endif

#include <glib-object.h>
#include <cairo.h>

#include "gtkprinter-private.h"
#include "gtkprintsettings.h"
#include "gtkprinteroption.h"
#include "gtkprintjob.h"

G_BEGIN_DECLS

typedef struct _GtkPrintBackendIface  GtkPrintBackendIface;

#define GTK_PRINT_BACKEND_ERROR (gtk_print_backend_error_quark ())

typedef enum
{
  /* TODO: add specific errors */
  GTK_PRINT_BACKEND_ERROR_GENERIC
} GtkPrintBackendError;

GQuark     gtk_print_backend_error_quark      (void);

#define GTK_TYPE_PRINT_BACKEND             (gtk_print_backend_get_type ())
#define GTK_PRINT_BACKEND(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_PRINT_BACKEND, GtkPrintBackend))
#define GTK_IS_PRINT_BACKEND(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_PRINT_BACKEND))
#define GTK_PRINT_BACKEND_GET_IFACE(inst)  (G_TYPE_INSTANCE_GET_INTERFACE ((inst), GTK_TYPE_PRINT_BACKEND, GtkPrintBackendIface))

struct _GtkPrintBackendIface
{
  GTypeInterface base_iface;

  /* Global backend methods: */
  GList      * (*get_printer_list) (GtkPrintBackend *printer_backend);

  GtkPrinter * (*find_printer) (GtkPrintBackend *print_backend,
                                const gchar *printer_name);
  void         (*print_stream) (GtkPrintBackend *print_backend,
                                GtkPrintJob *job,
				gint data_fd,
				GtkPrintJobCompleteFunc callback,
				gpointer user_data,
				GDestroyNotify dnotify);

  /* Printer methods: */
  void                  (*printer_request_details)           (GtkPrinter *printer);
  cairo_surface_t *     (*printer_create_cairo_surface)      (GtkPrinter *printer,
							      gdouble height,
							      gdouble width,
							      gint cache_fd);
  GtkPrinterOptionSet * (*printer_get_options)               (GtkPrinter *printer,
							      GtkPrintSettings *settings,
							      GtkPageSetup *page_setup);
  gboolean              (*printer_mark_conflicts)            (GtkPrinter *printer,
							      GtkPrinterOptionSet *options);
  void                  (*printer_get_settings_from_options) (GtkPrinter *printer,
							      GtkPrinterOptionSet *options,
							      GtkPrintSettings *settings);
  void                  (*printer_prepare_for_print)         (GtkPrinter *printer,
							      GtkPrintJob *print_job,
							      GtkPrintSettings *settings,
							      GtkPageSetup *page_setup);
  GList  *              (*printer_list_papers)               (GtkPrinter *printer);
  void                  (*printer_get_hard_margins)          (GtkPrinter *printer,
							      double     *top,
							      double     *bottom,
							      double     *left,
							      double     *right);

  /* Signals 
   */
  void (*printer_list_changed)   (void);
  void (*printer_added)          (GtkPrinter *printer);
  void (*printer_removed)        (GtkPrinter *printer);
  void (*printer_status_changed) (GtkPrinter *printer);
};

GType   gtk_print_backend_get_type       (void) G_GNUC_CONST;

GList      *gtk_print_backend_get_printer_list (GtkPrintBackend         *print_backend);
GtkPrinter *gtk_print_backend_find_printer     (GtkPrintBackend         *print_backend,
						const gchar             *printer_name);
void        gtk_print_backend_print_stream     (GtkPrintBackend         *print_backend,
						GtkPrintJob             *job,
						gint                     data_fd,
						GtkPrintJobCompleteFunc  callback,
						gpointer                 user_data,
						GDestroyNotify           dnotify);
GList *     gtk_print_backend_load_modules     (void);


/* Backend-only functions for GtkPrinter */

GtkPrinter *gtk_printer_new               (const char      *name,
					   GtkPrintBackend *backend,
					   gboolean         is_virtual);
void        gtk_printer_set_backend       (GtkPrinter      *printer,
			                   GtkPrintBackend *backend);
gboolean    gtk_printer_is_new            (GtkPrinter      *printer);
void        gtk_printer_set_is_new        (GtkPrinter      *printer,
					   gboolean         val);
void        gtk_printer_set_is_active     (GtkPrinter      *printer,
					   gboolean         val);
void        gtk_printer_set_has_details   (GtkPrinter      *printer,
					   gboolean         val);
void        gtk_printer_set_is_default    (GtkPrinter      *printer,
					   gboolean         val);
void        gtk_printer_set_icon_name     (GtkPrinter      *printer,
					   const gchar     *icon);
gboolean    gtk_printer_set_job_count     (GtkPrinter      *printer,
					   gint             count);
gboolean    gtk_printer_set_location      (GtkPrinter      *printer,
					   const gchar     *location);
gboolean    gtk_printer_set_description   (GtkPrinter      *printer,
					   const gchar     *description);
gboolean    gtk_printer_set_state_message (GtkPrinter      *printer,
					   const gchar     *message);
void        gtk_printer_set_is_active     (GtkPrinter      *printer,
					   gboolean         active);


G_END_DECLS

#endif /* __GTK_PRINT_BACKEND_H__ */
