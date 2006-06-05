/* GTK - The GIMP Toolkit
 * gtkprintoperationpreview.c: Abstract print preview interface
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

#include "config.h"

#include "gtkprintoperationpreview.h"
#include "gtkmarshalers.h"
#include "gtkintl.h"
#include "gtkalias.h"


static void gtk_print_operation_preview_base_init (gpointer g_iface);

GType
gtk_print_operation_preview_get_type (void)
{
  static GType print_operation_preview_type = 0;

  if (!print_operation_preview_type)
    {
      static const GTypeInfo print_operation_preview_info =
      {
        sizeof (GtkPrintOperationPreviewIface), /* class_size */
	gtk_print_operation_preview_base_init,   /* base_init */
	NULL,		/* base_finalize */
	NULL,
	NULL,		/* class_finalize */
	NULL,		/* class_data */
	0,
	0,              /* n_preallocs */
	NULL
      };

      print_operation_preview_type =
	g_type_register_static (G_TYPE_INTERFACE, I_("GtkPrintOperationPreview"),
				&print_operation_preview_info, 0);

      g_type_interface_add_prerequisite (print_operation_preview_type, G_TYPE_OBJECT);
    }

  return print_operation_preview_type;
}

static void
gtk_print_operation_preview_base_init (gpointer g_iface)
{
  static gboolean initialized = FALSE;

  if (!initialized)
    {
      g_signal_new (I_("ready"),
		    GTK_TYPE_PRINT_OPERATION_PREVIEW,
		    G_SIGNAL_RUN_LAST,
		    G_STRUCT_OFFSET (GtkPrintOperationPreviewIface, ready),
		    NULL, NULL,
		    g_cclosure_marshal_VOID__OBJECT,
		    G_TYPE_NONE, 1,
		    G_TYPE_OBJECT);

      g_signal_new (I_("got-page-size"),
		    GTK_TYPE_PRINT_OPERATION_PREVIEW,
		    G_SIGNAL_RUN_LAST,
		    G_STRUCT_OFFSET (GtkPrintOperationPreviewIface, ready),
		    NULL, NULL,
		    _gtk_marshal_VOID__OBJECT_OBJECT,
		    G_TYPE_NONE, 2,
		    GTK_TYPE_PRINT_CONTEXT,
		    GTK_TYPE_PAGE_SETUP);

      initialized = TRUE;
    }
}


void    
gtk_print_operation_preview_render_page (GtkPrintOperationPreview *preview,
					 gint			   page_nr)
{
  g_return_if_fail (GTK_IS_PRINT_OPERATION_PREVIEW (preview));

  GTK_PRINT_OPERATION_PREVIEW_GET_IFACE (preview)->render_page (preview,
								page_nr);
}

void
gtk_print_operation_preview_end_preview (GtkPrintOperationPreview *preview)
{
  g_return_if_fail (GTK_IS_PRINT_OPERATION_PREVIEW (preview));

  GTK_PRINT_OPERATION_PREVIEW_GET_IFACE (preview)->end_preview (preview);
}

gboolean
gtk_print_operation_preview_is_selected (GtkPrintOperationPreview *preview,
					 gint                      page_nr)
{
  g_return_if_fail (GTK_IS_PRINT_OPERATION_PREVIEW (preview));

  return GTK_PRINT_OPERATION_PREVIEW_GET_IFACE (preview)->is_selected (preview, page_nr);
}


#define __GTK_PRINT_OPERATION_PREVIEW_C__
#include "gtkaliasdef.c"
