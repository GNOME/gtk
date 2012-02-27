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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gtkprintoperationpreview.h"
#include "gtkmarshalers.h"
#include "gtkintl.h"


static void gtk_print_operation_preview_base_init (gpointer g_iface);

GType
gtk_print_operation_preview_get_type (void)
{
  static GType print_operation_preview_type = 0;

  if (!print_operation_preview_type)
    {
      const GTypeInfo print_operation_preview_info =
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
      /**
       * GtkPrintOperationPreview::ready:
       * @preview: the object on which the signal is emitted
       * @context: the current #GtkPrintContext
       *
       * The ::ready signal gets emitted once per preview operation,
       * before the first page is rendered.
       * 
       * A handler for this signal can be used for setup tasks.
       */
      g_signal_new (I_("ready"),
		    GTK_TYPE_PRINT_OPERATION_PREVIEW,
		    G_SIGNAL_RUN_LAST,
		    G_STRUCT_OFFSET (GtkPrintOperationPreviewIface, ready),
		    NULL, NULL,
		    g_cclosure_marshal_VOID__OBJECT,
		    G_TYPE_NONE, 1,
		    GTK_TYPE_PRINT_CONTEXT);

      /**
       * GtkPrintOperationPreview::got-page-size:
       * @preview: the object on which the signal is emitted
       * @context: the current #GtkPrintContext
       * @page_setup: the #GtkPageSetup for the current page
       *
       * The ::got-page-size signal is emitted once for each page
       * that gets rendered to the preview. 
       *
       * A handler for this signal should update the @context
       * according to @page_setup and set up a suitable cairo
       * context, using gtk_print_context_set_cairo_context().
       */
      g_signal_new (I_("got-page-size"),
		    GTK_TYPE_PRINT_OPERATION_PREVIEW,
		    G_SIGNAL_RUN_LAST,
		    G_STRUCT_OFFSET (GtkPrintOperationPreviewIface, got_page_size),
		    NULL, NULL,
		    _gtk_marshal_VOID__OBJECT_OBJECT,
		    G_TYPE_NONE, 2,
		    GTK_TYPE_PRINT_CONTEXT,
		    GTK_TYPE_PAGE_SETUP);

      initialized = TRUE;
    }
}

/**
 * gtk_print_operation_preview_render_page:
 * @preview: a #GtkPrintOperationPreview
 * @page_nr: the page to render
 *
 * Renders a page to the preview, using the print context that
 * was passed to the #GtkPrintOperation::preview handler together
 * with @preview.
 *
 * A custom iprint preview should use this function in its ::expose
 * handler to render the currently selected page.
 * 
 * Note that this function requires a suitable cairo context to 
 * be associated with the print context. 
 *
 * Since: 2.10 
 */
void    
gtk_print_operation_preview_render_page (GtkPrintOperationPreview *preview,
					 gint			   page_nr)
{
  g_return_if_fail (GTK_IS_PRINT_OPERATION_PREVIEW (preview));

  GTK_PRINT_OPERATION_PREVIEW_GET_IFACE (preview)->render_page (preview,
								page_nr);
}

/**
 * gtk_print_operation_preview_end_preview:
 * @preview: a #GtkPrintOperationPreview
 *
 * Ends a preview. 
 *
 * This function must be called to finish a custom print preview.
 *
 * Since: 2.10
 */
void
gtk_print_operation_preview_end_preview (GtkPrintOperationPreview *preview)
{
  g_return_if_fail (GTK_IS_PRINT_OPERATION_PREVIEW (preview));

  GTK_PRINT_OPERATION_PREVIEW_GET_IFACE (preview)->end_preview (preview);
}

/**
 * gtk_print_operation_preview_is_selected:
 * @preview: a #GtkPrintOperationPreview
 * @page_nr: a page number
 *
 * Returns whether the given page is included in the set of pages that
 * have been selected for printing.
 * 
 * Returns: %TRUE if the page has been selected for printing
 *
 * Since: 2.10
 */
gboolean
gtk_print_operation_preview_is_selected (GtkPrintOperationPreview *preview,
					 gint                      page_nr)
{
  g_return_val_if_fail (GTK_IS_PRINT_OPERATION_PREVIEW (preview), FALSE);

  return GTK_PRINT_OPERATION_PREVIEW_GET_IFACE (preview)->is_selected (preview, page_nr);
}
