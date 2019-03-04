/* gtkbinlayout.h: Layout manager for bin-like widgets
 *
 * Copyright 2019  GNOME Foundation
 *
 * SPDX-License-Identifier: LGPL 2.1+
 */
#pragma once

#include <gtk/gtklayoutmanager.h>

G_BEGIN_DECLS

#define GTK_TYPE_BIN_LAYOUT (gtk_bin_layout_get_type ())

GDK_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (GtkBinLayout, gtk_bin_layout, GTK, BIN_LAYOUT, GtkLayoutManager)

GDK_AVAILABLE_IN_ALL
GtkLayoutManager *      gtk_bin_layout_new      (void);

G_END_DECLS
