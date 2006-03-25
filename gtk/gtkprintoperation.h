/* EGG - The GIMP Toolkit
 * gtkprintoperation.h: Print Operation
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

#ifndef __GTK_PRINT_OPERATION_H__
#define __GTK_PRINT_OPERATION_H__

#include <glib-object.h>
#include <cairo.h>
#include <gtk/gtk.h>
#include <gtk/gtkenums.h>
#include <gtk/gtkpagesetup.h>
#include <gtk/gtkprintsettings.h>
#include <gtk/gtkprintcontext.h>

G_BEGIN_DECLS

#define GTK_TYPE_PRINT_OPERATION    (gtk_print_operation_get_type ())
#define GTK_PRINT_OPERATION(obj)    (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_PRINT_OPERATION, GtkPrintOperation))
#define GTK_IS_PRINT_OPERATION(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_PRINT_OPERATION))

typedef struct _GtkPrintOperationClass   GtkPrintOperationClass;
typedef struct _GtkPrintOperationPrivate GtkPrintOperationPrivate;
typedef struct _GtkPrintOperation        GtkPrintOperation;

struct _GtkPrintOperation
{
  GObject parent_instance;
  
  GtkPrintOperationPrivate *priv;
};

struct _GtkPrintOperationClass
{
  GObjectClass parent_class;
  
  void (*begin_print) (GtkPrintOperation *operation, GtkPrintContext *context);
  void (*request_page_setup) (GtkPrintOperation *operation,
			      GtkPrintContext *context,
			      int page_nr,
			      GtkPageSetup *setup);
  void (*draw_page) (GtkPrintOperation *operation,
		     GtkPrintContext *context,
		     int page_nr);
  void (*end_print) (GtkPrintOperation *operation,
		     GtkPrintContext *context);

  /* Padding for future expansion */
  void (*_gtk_reserved1) (void);
  void (*_gtk_reserved2) (void);
  void (*_gtk_reserved3) (void);
  void (*_gtk_reserved4) (void);
  void (*_gtk_reserved5) (void);
  void (*_gtk_reserved6) (void);
  void (*_gtk_reserved7) (void);
};

typedef enum {
  GTK_PRINT_OPERATION_RESULT_ERROR,
  GTK_PRINT_OPERATION_RESULT_APPLY,
  GTK_PRINT_OPERATION_RESULT_CANCEL
} GtkPrintOperationResult;

#define GTK_PRINT_ERROR gtk_print_error_quark ()

typedef enum
{
  GTK_PRINT_ERROR_GENERAL,
  GTK_PRINT_ERROR_INTERNAL_ERROR,
  GTK_PRINT_ERROR_NOMEM
} GtkPrintError;

GQuark gtk_print_error_quark (void);

GType                   gtk_print_operation_get_type               (void) G_GNUC_CONST;
GtkPrintOperation *     gtk_print_operation_new                    (void);
void                    gtk_print_operation_set_default_page_setup (GtkPrintOperation  *op,
								    GtkPageSetup       *default_page_setup);
GtkPageSetup *          gtk_print_operation_get_default_page_setup (GtkPrintOperation  *op);
void                    gtk_print_operation_set_print_settings     (GtkPrintOperation  *op,
								    GtkPrintSettings   *print_settings);
GtkPrintSettings *      gtk_print_operation_get_print_settings     (GtkPrintOperation  *op);
void                    gtk_print_operation_set_job_name           (GtkPrintOperation  *op,
								    const char         *job_name);
void                    gtk_print_operation_set_nr_of_pages        (GtkPrintOperation  *op,
								    int                 n_pages);
void                    gtk_print_operation_set_current_page       (GtkPrintOperation  *op,
								    int                 current_page);
void                    gtk_print_operation_set_use_full_page      (GtkPrintOperation  *op,
								    gboolean            full_page);
void                    gtk_print_operation_set_unit               (GtkPrintOperation  *op,
								    GtkUnit             unit);
void                    gtk_print_operation_set_show_dialog        (GtkPrintOperation  *op,
								    gboolean            show_dialog);
void                    gtk_print_operation_set_pdf_target         (GtkPrintOperation  *op,
								    const char         *filename);
GtkPrintOperationResult gtk_print_operation_run                    (GtkPrintOperation  *op,
								    GtkWindow          *parent,
								    GError            **error);

GtkPageSetup *gtk_print_run_page_setup_dialog (GtkWindow        *parent,
					       GtkPageSetup     *page_setup,
					       GtkPrintSettings *settings);

G_END_DECLS

#endif /* __GTK_PRINT_OPERATION_H__ */
