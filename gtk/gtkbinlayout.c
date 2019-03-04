/* gtkbinlayout.c: Layout manager for bin-like widgets
 *
 * Copyright 2019  GNOME Foundation
 *
 * SPDX-License-Identifier: LGPL 2.1+
 */

/**
 * SECTION:gtkbinlayout
 * @Title: GtkBinLayout
 * @Short_description: A layout manager for bin-like widgets
 * @Include: gtk/gtk.h
 *
 * GtkBinLayout is a #GtkLayoutManager subclass useful for create "bins" of
 * widgets. GtkBinLayout will stack each child of a widget on top of each
 * other, using the #GtkWidget:hexpand, #GtkWidget:vexpand, #GtkWidget:halign,
 * and #GtkWidget:valign properties of each child to determine where they
 * should be positioned.
 */

#include "config.h"

#include "gtkbinlayout.h"

#include "gtkwidgetprivate.h"

struct _GtkBinLayout
{
  GtkLayoutManager parent_instance;
};

G_DEFINE_TYPE (GtkBinLayout, gtk_bin_layout, GTK_TYPE_LAYOUT_MANAGER)

static void
gtk_bin_layout_measure (GtkLayoutManager *layout_manager,
                        GtkWidget        *widget,
                        GtkOrientation    orientation,
                        int               for_size,
                        int              *minimum,
                        int              *natural,
                        int              *minimum_baseline,
                        int              *natural_baseline)
{
  GtkWidget *child;

  child = _gtk_widget_get_first_child (widget);
  while (child != NULL)
    {
      GtkWidget *next = _gtk_widget_get_next_sibling (child);

      if (gtk_widget_get_visible (child))
        {
          int child_min = 0;
          int child_nat = 0;

          gtk_widget_measure (child, orientation, for_size, &child_min, &child_nat, NULL, NULL);

          *minimum = MAX (*minimum, child_min);
          *natural = MAX (*natural, child_nat);
        }

      child = next;
    }
}

static void
gtk_bin_layout_allocate (GtkLayoutManager *layout_manager,
                         GtkWidget        *widget,
                         int               width,
                         int               height,
                         int               baseline)
{
  GtkWidget *child;

  child = _gtk_widget_get_first_child (widget);
  while (child != NULL)
    {
      GtkWidget *next = _gtk_widget_get_next_sibling (child);

      if (child && gtk_widget_get_visible (child))
        gtk_widget_size_allocate (child,
                                  &(GtkAllocation) {
                                    0, 0,
                                    width, height
                                  }, baseline);

      child = next;
    }
}
static void
gtk_bin_layout_class_init (GtkBinLayoutClass *klass)
{
  GtkLayoutManagerClass *layout_manager_class = GTK_LAYOUT_MANAGER_CLASS (klass);

  layout_manager_class->measure = gtk_bin_layout_measure;
  layout_manager_class->allocate = gtk_bin_layout_allocate;
}

static void
gtk_bin_layout_init (GtkBinLayout *self)
{
}

/**
 * gtk_bin_layout_new:
 *
 * Creates a new #GtkBinLayout instance.
 *
 * Returns: the newly created #GtkBinLayout
 */
GtkLayoutManager *
gtk_bin_layout_new (void)
{
  return g_object_new (GTK_TYPE_BIN_LAYOUT, NULL);
}
