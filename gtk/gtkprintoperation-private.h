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

#ifndef __GTK_PRINT_OPERATION_PRIVATE_H__
#define __GTK_PRINT_OPERATION_PRIVATE_H__

#include "gtkprintoperation.h"

G_BEGIN_DECLS

struct _GtkPrintOperationPrivate
{
  GtkPageSetup *default_page_setup;
  GtkPrintSettings *print_settings;
  char *job_name;
  int nr_of_pages;
  int current_page;
  gboolean use_full_page;
  GtkUnit unit;
  gboolean show_dialog;
  char *pdf_target;

  /* Data for the print job: */
  cairo_surface_t *surface;
  double dpi_x, dpi_y;

  GtkPrintPages print_pages;
  GtkPageRange *page_ranges;
  int num_page_ranges;
  
  int manual_num_copies;
  gboolean manual_collation;
  gboolean manual_reverse;
  gboolean manual_orientation;
  double manual_scale;
  GtkPageSet manual_page_set;
 
  void *platform_data;

  void (*start_page) (GtkPrintOperation *operation,
		      GtkPrintContext *print_context,
		      GtkPageSetup *page_setup);
  void (*end_page) (GtkPrintOperation *operation,
		    GtkPrintContext *print_context);
  void (*end_run) (GtkPrintOperation *operation);
};

GtkPrintOperationResult _gtk_print_operation_platform_backend_run_dialog (GtkPrintOperation *operation,
									  GtkWindow *parent,
									  gboolean *do_print,
									  GError **error);



/* GtkPrintContext private functions: */

GtkPrintContext *_gtk_print_context_new                             (GtkPrintOperation *op);
void             _gtk_print_context_set_page_setup                  (GtkPrintContext   *context,
								     GtkPageSetup      *page_setup);
void             _gtk_print_context_translate_into_margin           (GtkPrintContext   *context);
void             _gtk_print_context_rotate_according_to_orientation (GtkPrintContext   *context);

G_END_DECLS

#endif /* __GTK_PRINT_OPERATION_PRIVATE_H__ */
