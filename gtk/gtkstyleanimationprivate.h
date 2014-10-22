/*
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

#ifndef __GTK_STYLE_ANIMATION_PRIVATE_H__
#define __GTK_STYLE_ANIMATION_PRIVATE_H__

#include <glib-object.h>
#include "gtkcssstyleprivate.h"

G_BEGIN_DECLS

#define GTK_TYPE_STYLE_ANIMATION           (_gtk_style_animation_get_type ())
#define GTK_STYLE_ANIMATION(obj)           (G_TYPE_CHECK_INSTANCE_CAST (obj, GTK_TYPE_STYLE_ANIMATION, GtkStyleAnimation))
#define GTK_STYLE_ANIMATION_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST (cls, GTK_TYPE_STYLE_ANIMATION, GtkStyleAnimationClass))
#define GTK_IS_STYLE_ANIMATION(obj)        (G_TYPE_CHECK_INSTANCE_TYPE (obj, GTK_TYPE_STYLE_ANIMATION))
#define GTK_IS_STYLE_ANIMATION_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE (obj, GTK_TYPE_STYLE_ANIMATION))
#define GTK_STYLE_ANIMATION_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_STYLE_ANIMATION, GtkStyleAnimationClass))

typedef struct _GtkStyleAnimation           GtkStyleAnimation;
typedef struct _GtkStyleAnimationClass      GtkStyleAnimationClass;

struct _GtkStyleAnimation
{
  GObject parent;
};

struct _GtkStyleAnimationClass
{
  GObjectClass parent_class;

  gboolean      (* is_finished)                         (GtkStyleAnimation      *animation,
                                                         gint64                  at_time_us);
  gboolean      (* is_static)                           (GtkStyleAnimation      *animation,
                                                         gint64                  at_time_us);
  void          (* set_values)                          (GtkStyleAnimation      *animation,
                                                         gint64                  for_time_us,
                                                         GtkCssStyle   *values);
};

GType           _gtk_style_animation_get_type           (void) G_GNUC_CONST;

void            _gtk_style_animation_set_values         (GtkStyleAnimation      *animation,
                                                         gint64                  for_time_us,
                                                         GtkCssStyle   *values);
gboolean        _gtk_style_animation_is_finished        (GtkStyleAnimation      *animation,
                                                         gint64                  at_time_us);
gboolean        _gtk_style_animation_is_static          (GtkStyleAnimation      *animation,
                                                         gint64                  at_time_us);


G_END_DECLS

#endif /* __GTK_STYLE_ANIMATION_PRIVATE_H__ */
