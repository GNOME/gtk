/* gtkscrollablebox.h: A box that handles scrollable children
 *
 * SPDX-FileCopyrightText: 2026 Sergey Bugaev
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtkwidget.h>

G_BEGIN_DECLS

#define GTK_TYPE_SCROLLABLE_BOX (gtk_scrollable_box_get_type())

GDK_AVAILABLE_IN_4_24
G_DECLARE_FINAL_TYPE (GtkScrollableBox, gtk_scrollable_box, GTK, SCROLLABLE_BOX, GtkWidget)

GDK_AVAILABLE_IN_4_24
GtkWidget *gtk_scrollable_box_new           (GtkOrientation orientation);

GDK_AVAILABLE_IN_4_24
void       gtk_scrollable_box_append        (GtkScrollableBox *box,
                                             GtkWidget        *child);

GDK_AVAILABLE_IN_4_24
void       gtk_scrollable_box_prepend       (GtkScrollableBox *box,
                                             GtkWidget        *child);

GDK_AVAILABLE_IN_4_24
void       gtk_scrollable_box_remove        (GtkScrollableBox *box,
                                             GtkWidget        *child);

GDK_AVAILABLE_IN_4_24
void gtk_scrollable_box_insert_child_after  (GtkScrollableBox *box,
                                             GtkWidget        *child,
                                             GtkWidget        *sibling);

GDK_AVAILABLE_IN_4_24
void gtk_scrollable_box_reorder_child_after (GtkScrollableBox *box,
                                             GtkWidget        *child,
                                             GtkWidget        *sibling);

GDK_AVAILABLE_IN_4_24
GtkBaselinePosition gtk_scrollable_box_get_baseline_position (GtkScrollableBox *box);

GDK_AVAILABLE_IN_4_24
void gtk_scrollable_box_set_baseline_position (GtkScrollableBox   *box,
                                               GtkBaselinePosition position);

G_END_DECLS
