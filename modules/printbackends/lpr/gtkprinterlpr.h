/* GtkPrinterLpr
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
#ifndef __GTK_PRINTER_LPR_H__
#define __GTK_PRINTER_LPR_H__

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>

#include "gtkprinter.h"
#include "gtkprinteroption.h"

G_BEGIN_DECLS

#define GTK_TYPE_PRINTER_LPR                  (gtk_printer_lpr_get_type ())
#define GTK_PRINTER_LPR(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_PRINTER_LPR, GtkPrinterLpr))
#define GTK_PRINTER_LPR_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_PRINTER_LPR, GtkPrinterLprClass))
#define GTK_IS_PRINTER_LPR(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_PRINTER_LPR))
#define GTK_IS_PRINTER_LPR_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_PRINTER_LPR))
#define GTK_PRINTER_LPR_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_PRINTER_LPR, GtkPrinterLprClass))

typedef struct _GtkPrinterLpr	        GtkPrinterLpr;
typedef struct _GtkPrinterLprClass     GtkPrinterLprClass;
typedef struct _GtkPrinterLprPrivate   GtkPrinterLprPrivate;

struct _GtkPrinterLpr
{
  GtkPrinter parent_instance;

  GtkPrinterOption *options;
};

struct _GtkPrinterLprClass
{
  GtkPrinterClass parent_class;

};

void           gtk_printer_lpr_register_type (GTypeModule *module);         
GType          gtk_printer_lpr_get_type      (void) G_GNUC_CONST;
GtkPrinterLpr *gtk_printer_lpr_new           (void);

G_END_DECLS

#endif /* __GTK_PRINTER_LPR_H__ */
