#pragma once

#include "gtkcellarea.h"

/* For api stability, this is called from gtkcelllayout.c in order to ensure the correct
 * object is passed to the user function in gtk_cell_layout_set_cell_data_func.
 *
 * This private api takes gpointer & GFunc arguments to circumvent circular header file
 * dependencies.
 */
void                 _gtk_cell_area_set_cell_data_func_with_proxy  (GtkCellArea           *area,
								    GtkCellRenderer       *cell,
								    GFunc                  func,
								    gpointer               func_data,
								    GDestroyNotify         destroy,
								    gpointer               proxy);

