/*
 * Gtk.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2009  Intel Corporation.
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
 *
 * Author:
 *   Emmanuele Bassi <ebassi@linux.intel.com>
 */

#ifndef __GTK_LAYOUT_MANAGER_PRIVATE_H__
#define __GTK_LAYOUT_MANAGER_PRIVATE_H__

#include <gtk/actors/gtkactorprivate.h>

G_BEGIN_DECLS

#define GTK_TYPE_LAYOUT_MANAGER             (_gtk_layout_manager_get_type ())
#define GTK_LAYOUT_MANAGER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_LAYOUT_MANAGER, GtkLayoutManager))
#define GTK_IS_LAYOUT_MANAGER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_LAYOUT_MANAGER))
#define GTK_LAYOUT_MANAGER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_LAYOUT_MANAGER, GtkLayoutManagerClass))
#define GTK_IS_LAYOUT_MANAGER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_LAYOUT_MANAGER))
#define GTK_LAYOUT_MANAGER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_LAYOUT_MANAGER, GtkLayoutManagerClass))

typedef struct _GtkLayoutManagerPrivate     GtkLayoutManagerPrivate;
typedef struct _GtkLayoutManagerClass       GtkLayoutManagerClass;

/**
 * GtkLayoutManager:
 *
 * The #GtkLayoutManager structure contains only private data
 * and should be accessed using the provided API
 *
 * Since: 1.2
 */
struct _GtkLayoutManager
{
  /*< private >*/
  GInitiallyUnowned parent_instance;

  GtkLayoutManagerPrivate *priv;
};

/**
 * GtkLayoutManagerClass:
 * @get_preferred_width: virtual function; override to provide a preferred
 *   width for the layout manager. See also the get_preferred_width()
 *   virtual function in #GtkActor
 * @get_preferred_height: virtual function; override to provide a preferred
 *   height for the layout manager. See also the get_preferred_height()
 *   virtual function in #GtkActor
 * @allocate: virtual function; override to allocate the children of the
 *   layout manager. See also the allocate() virtual function in
 *   #GtkActor
 * @set_container: virtual function; override to set a back pointer
 *   on the #GtkContainer using the layout manager. The implementation
 *   should not take a reference on the container, but just take a weak
 *   reference, to avoid potential leaks due to reference cycles
 * @get_child_meta_type: virtual function; override to return the #GType
 *   of the #GtkLayoutMeta sub-class used by the #GtkLayoutManager
 * @create_child_meta: virtual function; override to create a
 *   #GtkLayoutMeta instance associated to a #GtkContainer and a
 *   child #GtkActor, used to maintain layout manager specific properties
 * @begin_animation: virtual function; override to control the animation
 *   of a #GtkLayoutManager with the given duration and easing mode.
 *   This virtual function is deprecated, and it should not be overridden
 *   in newly written code.
 * @end_animation: virtual function; override to end an animation started
 *   by _gtk_layout_manager_begin_animation(). This virtual function is
 *   deprecated, and it should not be overriden in newly written code.
 * @get_animation_progress: virtual function; override to control the
 *   progress of the animation of a #GtkLayoutManager. This virtual
 *   function is deprecated, and it should not be overridden in newly written
 *   code.
 * @layout_changed: class handler for the #GtkLayoutManager::layout-changed
 *   signal
 *
 * The #GtkLayoutManagerClass structure contains only private
 * data and should be accessed using the provided API
 *
 * Since: 1.2
 */
struct _GtkLayoutManagerClass
{
  /*< private >*/
  GInitiallyUnownedClass parent_class;

  /*< public >*/
  void               (* get_preferred_size)     (GtkLayoutManager       *manager,
                                                 GtkOrientation          orientation,
                                                 gfloat                  for_size,
                                                 gfloat                 *min_size_p,
                                                 gfloat                 *nat_size_p);
  void               (* allocate)               (GtkLayoutManager       *manager,
                                                 const cairo_matrix_t   *transform,
                                                 gfloat                  width,
                                                 gfloat                  height);
};

GType              _gtk_layout_manager_get_type                 (void) G_GNUC_CONST;

void               _gtk_layout_manager_get_preferred_size       (GtkLayoutManager       *manager,
                                                                 GtkOrientation          orientation,
                                                                 gfloat                  for_size,
                                                                 gfloat                 *min_size_p,
                                                                 gfloat                 *nat_size_p);
void               _gtk_layout_manager_allocate                 (GtkLayoutManager       *manager,
                                                                 const cairo_matrix_t   *transform,
                                                                 gfloat                  width,
                                                                 gfloat                  height);

void               _gtk_layout_manager_set_actor                (GtkLayoutManager       *manager,
                                                                 GtkActor               *actor);
GtkActor *         _gtk_layout_manager_get_actor                (GtkLayoutManager       *manager);
void               _gtk_layout_manager_layout_changed           (GtkLayoutManager       *manager);


G_END_DECLS

#endif /* __GTK_LAYOUT_MANAGER_PRIVATE_H__ */
