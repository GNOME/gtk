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

#ifndef __GTK_PRINT_OPERATION_PRIVATE_H__
#define __GTK_PRINT_OPERATION_PRIVATE_H__

#include "gtkprintoperation.h"

G_BEGIN_DECLS

struct _GtkPrintOperationPrivate
{
  GtkPrintStatus status;
  gchar *status_string;
  GtkPageSetup *default_page_setup;
  GtkPrintSettings *print_settings;
  gchar *job_name;
  gint nr_of_pages;
  gint current_page;
  GtkUnit unit;
  gchar *pdf_target;
  guint use_full_page      : 1;
  guint show_dialog        : 1;
  guint track_print_status : 1;
  guint show_progress      : 1;
  guint cancelled          : 1;

  guint print_pages_idle_id;
  guint show_progress_timeout_id;

  /* Data for the print job: */
  cairo_surface_t *surface;
  gdouble dpi_x, dpi_y;

  GtkPrintPages print_pages;
  GtkPageRange *page_ranges;
  gint num_page_ranges;
  
  gint manual_num_copies;
  guint manual_collation   : 1;
  guint manual_reverse     : 1;
  guint manual_orientation : 1;
  double manual_scale;
  GtkPageSet manual_page_set;
  GtkWidget *custom_widget;
  gchar *custom_tab_label;
  
  gpointer platform_data;
  GDestroyNotify free_platform_data;

  void (*start_page) (GtkPrintOperation *operation,
		      GtkPrintContext   *print_context,
		      GtkPageSetup      *page_setup);
  void (*end_page)   (GtkPrintOperation *operation,
		      GtkPrintContext   *print_context);
  void (*end_run)    (GtkPrintOperation *operation,
		      gboolean           wait,
		      gboolean           cancelled);
};

GtkPrintOperationResult _gtk_print_operation_platform_backend_run_dialog (GtkPrintOperation *operation,
									  GtkWindow         *parent,
									  gboolean          *do_print,
									  GError           **error);

typedef void (* GtkPrintOperationPrintFunc) (GtkPrintOperation *op,
					     GtkWindow         *parent,
					     gboolean           wait);

void _gtk_print_operation_platform_backend_run_dialog_async (GtkPrintOperation          *op,
							     GtkWindow                  *parent,
							     GtkPrintOperationPrintFunc  print_cb);

void _gtk_print_operation_set_status (GtkPrintOperation *op,
				      GtkPrintStatus     status,
				      const gchar       *string);

/* GtkPrintContext private functions: */

GtkPrintContext *_gtk_print_context_new                             (GtkPrintOperation *op);
void             _gtk_print_context_set_page_setup                  (GtkPrintContext   *context,
								     GtkPageSetup      *page_setup);
void             _gtk_print_context_translate_into_margin           (GtkPrintContext   *context);
void             _gtk_print_context_rotate_according_to_orientation (GtkPrintContext   *context);

G_END_DECLS

#endif /* __GTK_PRINT_OPERATION_PRIVATE_H__ */
