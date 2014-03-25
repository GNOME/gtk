/* gtkprintercloudprint.h: Google Cloud Print -specific Printer class
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

#ifndef __GTK_PRINTER_CLOUDPRINT_H__
#define __GTK_PRINTER_CLOUDPRINT_H__

#include <glib-object.h>
#include <gtk/gtkprinter-private.h>

#include "gtkcloudprintaccount.h"

G_BEGIN_DECLS

#define GTK_TYPE_PRINTER_CLOUDPRINT	(gtk_printer_cloudprint_get_type ())
#define GTK_PRINTER_CLOUDPRINT(obj)	(G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_PRINTER_CLOUDPRINT, GtkPrinterCloudprint))
#define GTK_IS_PRINTER_CLOUDPRINT(obj)	(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_PRINTER_CLOUDPRINT))

void	gtk_printer_cloudprint_register_type (GTypeModule *module);
GtkPrinterCloudprint *gtk_printer_cloudprint_new	(const char *name,
							 gboolean is_virtual,
							 GtkPrintBackend *backend,
							 GtkCloudprintAccount *account,
							 const gchar *id);
GType	gtk_printer_cloudprint_get_type			(void) G_GNUC_CONST;

G_END_DECLS

#endif /* __GTK_PRINTER_CLOUDPRINT_H__ */
