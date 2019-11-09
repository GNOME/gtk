/*
 * Copyright © 2019 Benjamin Otte
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#ifndef __GTK_FILTER_H__
#define __GTK_FILTER_H__

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gdk/gdk.h>

G_BEGIN_DECLS

/**
 * GtkFilterChange:
 * @GTK_FILTER_CHANGE_DIFFERENT: The filter change cannot be
 *     described with any of the other enumeration values.
 * @GTK_FILTER_CHANGE_MATCH_ALL: The filter now matches every
 *     item: The filter func always returns %TRUE
 * @GTK_FILTER_CHANGE_LESS_STRICT: The filter is less strict than
 *     it was before: All items that it used to return %TRUE for
 *     still return %TRUE, others now may, too.
 * @GTK_FILTER_CHANGE_MORE_STRICT: The filter is more strict than
 *     it was before: All items that it used to return %FALSE for
 *     still return %FALSE, others now may, too.
 * @GTK_FILTER_CHANGE_MATCH_NONE: The filter now matches no item:
 *     The filter func always returns %FALSE.
 *
 * Describes changes in a filter in more detail and allows objects
 * using the filter to optimize refiltering items.
 *
 * If you are writing an implementation and are not sure which
 * value to pass, @GTK_FILTER_CHANGE_DIFFERENT is always a correct
 * choice.
 */
typedef enum {
  GTK_FILTER_CHANGE_DIFFERENT = 0,
  GTK_FILTER_CHANGE_MATCH_ALL,
  GTK_FILTER_CHANGE_LESS_STRICT,
  GTK_FILTER_CHANGE_MORE_STRICT,
  GTK_FILTER_CHANGE_MATCH_NONE
} GtkFilterChange;

#define GTK_TYPE_FILTER             (gtk_filter_get_type ())

/**
 * GtkFilter:
 *
 * The object describing a filter.
 */
GDK_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (GtkFilter, gtk_filter, GTK, FILTER, GObject)

struct _GtkFilterClass
{
  GObjectClass parent_class;

  gboolean              (* filter)                              (GtkFilter      *self,
                                                                 gpointer        item);

  /* Padding for future expansion */
  void (*_gtk_reserved1) (void);
  void (*_gtk_reserved2) (void);
  void (*_gtk_reserved3) (void);
  void (*_gtk_reserved4) (void);
  void (*_gtk_reserved5) (void);
  void (*_gtk_reserved6) (void);
  void (*_gtk_reserved7) (void);
  void (*_gtk_reserved8) (void);
};

GDK_AVAILABLE_IN_ALL
gboolean                gtk_filter_filter                       (GtkFilter              *filter,
                                                                 gpointer                item);

/* for filter implementations */
GDK_AVAILABLE_IN_ALL
void                    gtk_filter_changed                      (GtkFilter              *filter,
                                                                 GtkFilterChange         change);


G_END_DECLS

#endif /* __GTK_FILTER_H__ */
