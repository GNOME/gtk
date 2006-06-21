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

#ifndef __GTK_PRINTER_PRIVATE_H__
#define __GTK_PRINTER_PRIVATE_H__

#include <glib.h>
#include "gtkprinter.h"
#include "gtkprintoperation.h"
#include "gtkprinteroptionset.h"
#include "gtkpagesetup.h"
#include "gtkprintjob.h"

G_BEGIN_DECLS

gboolean             _gtk_printer_has_details               (GtkPrinter          *printer);
void                 _gtk_printer_request_details           (GtkPrinter          *printer);
GtkPrinterOptionSet *_gtk_printer_get_options               (GtkPrinter          *printer,
							     GtkPrintSettings    *settings,
							     GtkPageSetup        *page_setup,
							     GtkPrintCapabilities capabilities);gboolean             _gtk_printer_mark_conflicts            (GtkPrinter          *printer,
							     GtkPrinterOptionSet *options);
void                 _gtk_printer_get_settings_from_options (GtkPrinter          *printer,
							     GtkPrinterOptionSet *options,
							     GtkPrintSettings    *settings);
void                 _gtk_printer_prepare_for_print         (GtkPrinter          *printer,
							     GtkPrintJob         *print_job,
							     GtkPrintSettings    *settings,
							     GtkPageSetup        *page_setup);
cairo_surface_t *    _gtk_printer_create_cairo_surface      (GtkPrinter          *printer,
							     GtkPrintSettings    *settings,
							     gdouble              width,
							     gdouble              height,
							     GIOChannel          *cache_io);
GList  *             _gtk_printer_list_papers               (GtkPrinter          *printer);
void                 _gtk_printer_get_hard_margins          (GtkPrinter          *printer,
							     gdouble             *top,
							     gdouble             *bottom,
							     gdouble             *left,
							     gdouble             *right);
GHashTable *         _gtk_printer_get_custom_widgets        (GtkPrinter          *printer);
GtkPrintCapabilities _gtk_printer_get_capabilities          (GtkPrinter          *printer);


/* GtkPrintJob private methods: */
void gtk_print_job_set_status (GtkPrintJob   *job,
			       GtkPrintStatus status);

G_END_DECLS
#endif /* __GTK_PRINT_OPERATION_PRIVATE_H__ */
