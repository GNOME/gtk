/*
 * Copyright © 2006, 2007, 2008 OpenedHand Ltd
 * Copyright © 2009, 2010 Intel Corp
 * Copyright © 2012 Red Hat Inc.
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

#ifndef __GTK_ACTOR_PRIVATE_H__
#define __GTK_ACTOR_PRIVATE_H__

#include <cairo.h>
#include <gdk/gdk.h>
#include <gtk/gtkenums.h>
#include <gtk/gtktypes.h>

G_BEGIN_DECLS

#define GTK_TYPE_ACTOR           (_gtk_actor_get_type ())
#define GTK_ACTOR(obj)           (G_TYPE_CHECK_INSTANCE_CAST (obj, GTK_TYPE_ACTOR, GtkActor))
#define GTK_ACTOR_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST (cls, GTK_TYPE_ACTOR, GtkActorClass))
#define GTK_IS_ACTOR(obj)        (G_TYPE_CHECK_INSTANCE_TYPE (obj, GTK_TYPE_ACTOR))
#define GTK_IS_ACTOR_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE (obj, GTK_TYPE_ACTOR))
#define GTK_ACTOR_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_ACTOR, GtkActorClass))

/* forward declarations go here */
typedef struct _GtkLayoutManager   GtkLayoutManager;

typedef struct _GtkActor           GtkActor;
typedef struct _GtkActorPrivate    GtkActorPrivate;
typedef struct _GtkActorClass      GtkActorClass;

struct _GtkActor
{
  GInitiallyUnowned   parent;

  GtkActorPrivate    *priv;
};

struct _GtkActorClass
{
  GInitiallyUnownedClass parent_class;

  void                  (* show)                 (GtkActor                 *self);
  void                  (* hide)                 (GtkActor                 *self);
  void                  (* realize)              (GtkActor                 *self);
  void                  (* unrealize)            (GtkActor                 *self);
  void                  (* map)                  (GtkActor                 *self);
  void                  (* unmap)                (GtkActor                 *self);
  void                  (* draw)                 (GtkActor                 *self,
                                                  cairo_t                  *cr);
  void                  (* parent_set)           (GtkActor                 *self,
                                                  GtkActor                 *old_parent);
  void                  (* queue_relayout)       (GtkActor                 *self);
  void                  (* queue_redraw)         (GtkActor                 *self,
                                                  const cairo_rectangle_t  *box);

  /* size negotiation */
  GtkSizeRequestMode    (* get_request_mode)     (GtkActor                 *self);
  void                  (* get_preferred_size)   (GtkActor                 *self,
                                                  GtkOrientation            orientation,
                                                  gfloat                    for_size,
                                                  gfloat                   *min_size_p,
                                                  gfloat                   *natural_size_p);
  void                  (* position)             (GtkActor                 *self,
                                                  const cairo_matrix_t     *transform);
  void                  (* allocate)             (GtkActor                 *self,
                                                  const cairo_matrix_t     *position,
                                                  gfloat                    width,
                                                  gfloat                    height);

  void                  (* screen_changed)       (GtkActor                 *self,
                                                  GdkScreen                *new_screen,
                                                  GdkScreen                *old_screen);

  /* signals */
  void                  (* actor_added)          (GtkActor                 *self,
                                                  GtkActor                 *child);
  void                  (* actor_removed)        (GtkActor                 *self,
                                                  GtkActor                 *child);
};

GType                           _gtk_actor_get_type                             (void) G_GNUC_CONST;

void                            _gtk_actor_show                                 (GtkActor                   *self);
void                            _gtk_actor_hide                                 (GtkActor                   *self);
void                            _gtk_actor_realize                              (GtkActor                   *self);
void                            _gtk_actor_unrealize                            (GtkActor                   *self);
void                            _gtk_actor_map                                  (GtkActor                   *self);
void                            _gtk_actor_unmap                                (GtkActor                   *self);
void                            _gtk_actor_draw                                 (GtkActor                   *self,
                                                                                 cairo_t                    *cr);
void                            _gtk_actor_queue_redraw                         (GtkActor                   *self);
void                            _gtk_actor_queue_redraw_area                    (GtkActor                   *self,
                                                                                 const cairo_rectangle_t    *box);
void                            _gtk_actor_queue_relayout                       (GtkActor                   *self);
void                            _gtk_actor_set_visible                          (GtkActor                   *self,
                                                                                 gboolean                    visible);
gboolean                        _gtk_actor_get_visible                          (GtkActor                   *self);
gboolean                        _gtk_actor_get_mapped                           (GtkActor                   *self);
gboolean                        _gtk_actor_get_realized                         (GtkActor                   *self);

/* Size negotiation */
GtkSizeRequestMode              _gtk_actor_get_request_mode                     (GtkActor                   *self);
void                            _gtk_actor_get_preferred_size                   (GtkActor                   *self,
                                                                                 GtkOrientation              orientation,
                                                                                 gfloat                      for_size,
                                                                                 gfloat                     *min_size_p,
                                                                                 gfloat                     *natural_size_p);
gfloat                          _gtk_actor_get_width                            (GtkActor                   *self);
gfloat                          _gtk_actor_get_height                           (GtkActor                   *self);
void                            _gtk_actor_allocate                             (GtkActor                   *self,
                                                                                 const cairo_matrix_t       *position,
                                                                                 gfloat                      width,
                                                                                 gfloat                      height);
const cairo_matrix_t *          _gtk_actor_get_position                         (GtkActor                   *actor);
void                            _gtk_actor_position                             (GtkActor                   *actor,
                                                                                 const cairo_matrix_t       *position);
void                            _gtk_actor_set_layout_manager                   (GtkActor                   *actor,
                                                                                 GtkLayoutManager           *layout_manager);
GtkLayoutManager *              _gtk_actor_get_layout_manager                   (GtkActor                   *actor);
void                            _gtk_actor_layout_manager_changed               (GtkActor                   *self);

/* Text */
void                            _gtk_actor_set_text_direction                   (GtkActor                   *self,
                                                                                 GtkTextDirection            text_dir);
GtkTextDirection                _gtk_actor_get_text_direction                   (GtkActor                   *self);

/* GDK integration */
GdkScreen *                     _gtk_actor_get_screen                           (GtkActor                   *self);
GtkWidget *                     _gtk_actor_get_widget                           (GtkActor                   *self);

/* Actor hierarchy */
void                            _gtk_actor_add_child                            (GtkActor                   *self,
                                                                                 GtkActor                   *child);
void                            _gtk_actor_insert_child_at_index                (GtkActor                   *self,
                                                                                 GtkActor                   *child,
                                                                                 gint                        index_);
void                            _gtk_actor_insert_child_above                   (GtkActor                   *self,
                                                                                 GtkActor                   *child,
                                                                                 GtkActor                   *sibling);
void                            _gtk_actor_insert_child_below                   (GtkActor                   *self,
                                                                                 GtkActor                   *child,
                                                                                 GtkActor                   *sibling);
void                            _gtk_actor_remove_child                         (GtkActor                   *self,
                                                                                 GtkActor                   *child);
void                            _gtk_actor_remove_all_children                  (GtkActor                   *self);
gint                            _gtk_actor_get_n_children                       (GtkActor                   *self);
GtkActor *                      _gtk_actor_get_child_at_index                   (GtkActor                   *self,
                                                                                 gint                        index_);
GtkActor *                      _gtk_actor_get_previous_sibling                 (GtkActor                   *self);
GtkActor *                      _gtk_actor_get_next_sibling                     (GtkActor                   *self);
GtkActor *                      _gtk_actor_get_first_child                      (GtkActor                   *self);
GtkActor *                      _gtk_actor_get_last_child                       (GtkActor                   *self);
GtkActor *                      _gtk_actor_get_parent                           (GtkActor                   *self);
gboolean                        _gtk_actor_contains                             (GtkActor                   *self,
                                                                                 GtkActor                   *descendant);

gboolean                        _gtk_actor_is_toplevel                          (GtkActor                   *actor);

G_END_DECLS

#endif /* __GTK_ACTOR_PRIVATE_H__ */
