/* GtkPrintJob 
 * Copyright (C) 2006 Red Hat,Inc. 
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#ifndef __GTK_PRINT_JOB_H__
#define __GTK_PRINT_JOB_H__

#include <glib-object.h>
#include <cairo.h>

#include "gtkprinter.h"
#include "gtkprintsettings.h"

G_BEGIN_DECLS

#define GTK_TYPE_PRINT_JOB                  (gtk_print_job_get_type ())
#define GTK_PRINT_JOB(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_PRINT_JOB, GtkPrintJob))
#define GTK_PRINT_JOB_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_PRINT_JOB, GtkPrintJobClass))
#define GTK_IS_PRINT_JOB(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_PRINT_JOB))
#define GTK_IS_PRINT_JOB_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_PRINT_JOB))
#define GTK_PRINT_JOB_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_PRINT_JOB, GtkPrintJobClass))


typedef struct _GtkPrintJobClass     GtkPrintJobClass;
typedef struct _GtkPrintJobPrivate   GtkPrintJobPrivate;

typedef void (*GtkPrintJobCompleteFunc) (GtkPrintJob *print_job,
                                         void *user_data, 
                                         GError **error);

struct _GtkPrinter;

struct _GtkPrintJob
{
  GObject parent_instance;

  GtkPrintJobPrivate *priv;
};

struct _GtkPrintJobClass
{
  GObjectClass parent_class;


  /* Padding for future expansion */
  void (*_gtk_reserved1) (void);
  void (*_gtk_reserved2) (void);
  void (*_gtk_reserved3) (void);
  void (*_gtk_reserved4) (void);
  void (*_gtk_reserved5) (void);
  void (*_gtk_reserved6) (void);
  void (*_gtk_reserved7) (void);
};

GType                    gtk_print_job_get_type     (void) G_GNUC_CONST;
GtkPrintJob             *gtk_print_job_new          (const gchar              *title,
						     GtkPrintSettings         *settings,
						     GtkPageSetup             *page_setup,
						     GtkPrinter               *printer);
GtkPrintSettings        *gtk_print_job_get_settings (GtkPrintJob              *print_job);
GtkPrinter              *gtk_print_job_get_printer  (GtkPrintJob              *print_job);
cairo_surface_t         *gtk_print_job_get_surface  (GtkPrintJob              *print_job);
gboolean                 gtk_print_job_send         (GtkPrintJob              *print_job,
						     GtkPrintJobCompleteFunc   callback,
						     gpointer                  user_data,
						     GError                  **error);
gboolean                 gtk_print_job_prep         (GtkPrintJob              *job,
						     GError                  **error);



G_END_DECLS

#endif /* __GTK_PRINT_JOB_H__ */
