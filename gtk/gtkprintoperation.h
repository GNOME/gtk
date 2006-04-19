/* GTK - The GIMP Toolkit
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
#include "gtkmain.h"
#include "gtkenums.h"
#include "gtkwindow.h"
#include "gtkpagesetup.h"
#include "gtkprintsettings.h"
#include "gtkprintcontext.h"

G_BEGIN_DECLS

#define GTK_TYPE_PRINT_OPERATION		(gtk_print_operation_get_type ())
#define GTK_PRINT_OPERATION(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_PRINT_OPERATION, GtkPrintOperation))
#define GTK_PRINT_OPERATION_CLASS(klass)    	(G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_PRINT_OPERATION, GtkPrintOperationClass))
#define GTK_IS_PRINT_OPERATION(obj) 		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_PRINT_OPERATION))
#define GTK_IS_PRINT_OPERATION_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_PRINT_OPERATION))
#define GTK_PRINT_OPERATION_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_PRINT_OPERATION, GtkPrintOperationClass))

typedef struct _GtkPrintOperationClass   GtkPrintOperationClass;
typedef struct _GtkPrintOperationPrivate GtkPrintOperationPrivate;
typedef struct _GtkPrintOperation        GtkPrintOperation;

typedef enum {
  GTK_PRINT_STATUS_INITIAL,
  GTK_PRINT_STATUS_PREPARING,
  GTK_PRINT_STATUS_GENERATING_DATA,
  GTK_PRINT_STATUS_SENDING_DATA,
  GTK_PRINT_STATUS_PENDING,
  GTK_PRINT_STATUS_PENDING_ISSUE,
  GTK_PRINT_STATUS_PRINTING,
  GTK_PRINT_STATUS_FINISHED,
  GTK_PRINT_STATUS_FINISHED_ABORTED
} GtkPrintStatus;

struct _GtkPrintOperation
{
  GObject parent_instance;
  
  GtkPrintOperationPrivate *priv;
};

struct _GtkPrintOperationClass
{
  GObjectClass parent_class;
  
  void (*begin_print)        (GtkPrintOperation *operation, 
                              GtkPrintContext   *context);
  void (*request_page_setup) (GtkPrintOperation *operation,
			      GtkPrintContext   *context,
			      int                page_nr,
			      GtkPageSetup      *setup);
  void (*draw_page)          (GtkPrintOperation *operation,
		              GtkPrintContext   *context,
		              int                page_nr);
  void (*end_print)          (GtkPrintOperation *operation,
		              GtkPrintContext   *context);
  void (*status_changed)     (GtkPrintOperation *operation);
  
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
GtkPrintStatus          gtk_print_operation_get_status             (GtkPrintOperation  *op);
const char *            gtk_print_operation_get_status_string      (GtkPrintOperation  *op);
gboolean                gtk_print_operation_is_finished            (GtkPrintOperation  *op);

GtkPageSetup *gtk_print_run_page_setup_dialog (GtkWindow        *parent,
					       GtkPageSetup     *page_setup,
					       GtkPrintSettings *settings);

G_END_DECLS

#endif /* __GTK_PRINT_OPERATION_H__ */
