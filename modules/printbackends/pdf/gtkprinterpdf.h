/* GtkPrinterPdf
 * Copyright (C) 2006 John (J5) Palmieri <johnp@redhat.com>
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
#ifndef __GTK_PRINTER_PDF_H__
#define __GTK_PRINTER_PDF_H__

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>

#include "gtkprinter.h"
#include "gtkprinteroption.h"

G_BEGIN_DECLS

#define GTK_TYPE_PRINTER_PDF                  (gtk_printer_pdf_get_type ())
#define GTK_PRINTER_PDF(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_PRINTER_PDF, GtkPrinterPdf))
#define GTK_PRINTER_PDF_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_PRINTER_PDF, GtkPrinterPdfClass))
#define GTK_IS_PRINTER_PDF(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_PRINTER_PDF))
#define GTK_IS_PRINTER_PDF_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_PRINTER_PDF))
#define GTK_PRINTER_PDF_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_PRINTER_PDF, GtkPrinterPdfClass))

typedef struct _GtkPrinterPdf	        GtkPrinterPdf;
typedef struct _GtkPrinterPdfClass     GtkPrinterPdfClass;
typedef struct _GtkPrinterPdfPrivate   GtkPrinterPdfPrivate;

struct _GtkPrinterPdf
{
  GtkPrinter parent_instance;

  GtkPrinterOption *file_option;
};

struct _GtkPrinterPdfClass
{
  GtkPrinterClass parent_class;

};

void           gtk_printer_pdf_register_type (GTypeModule *module);
GType          gtk_printer_pdf_get_type      (void) G_GNUC_CONST;
GtkPrinterPdf *gtk_printer_pdf_new           (void);

G_END_DECLS

#endif /* __GTK_PRINTER_PDF_H__ */
