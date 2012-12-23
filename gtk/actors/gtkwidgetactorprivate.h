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

#ifndef __GTK_WIDGET_ACTOR_PRIVATE_H__
#define __GTK_WIDGET_ACTOR_PRIVATE_H__

#include <gtk/actors/gtkcssboxprivate.h>

G_BEGIN_DECLS

#define GTK_TYPE_WIDGET_ACTOR           (_gtk_widget_actor_get_type ())
#define GTK_WIDGET_ACTOR(obj)           (G_TYPE_CHECK_INSTANCE_CAST (obj, GTK_TYPE_WIDGET_ACTOR, GtkWidgetActor))
#define GTK_WIDGET_ACTOR_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST (cls, GTK_TYPE_WIDGET_ACTOR, GtkWidgetActorClass))
#define GTK_IS_WIDGET_ACTOR(obj)        (G_TYPE_CHECK_INSTANCE_TYPE (obj, GTK_TYPE_WIDGET_ACTOR))
#define GTK_IS_WIDGET_ACTOR_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE (obj, GTK_TYPE_WIDGET_ACTOR))
#define GTK_WIDGET_ACTOR_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_WIDGET_ACTOR, GtkWidgetActorClass))

typedef struct _GtkWidgetActor           GtkWidgetActor;
typedef struct _GtkWidgetActorPrivate    GtkWidgetActorPrivate;
typedef struct _GtkWidgetActorClass      GtkWidgetActorClass;

struct _GtkWidgetActor
{
  GtkCssBox                 parent;

  GtkWidgetActorPrivate    *priv;
};

struct _GtkWidgetActorClass
{
  GtkCssBoxClass            parent_class;
};

GType                       _gtk_widget_actor_get_type                  (void) G_GNUC_CONST;

void                        _gtk_widget_actor_show                      (GtkActor       *actor);
void                        _gtk_widget_actor_hide                      (GtkActor       *actor);
void                        _gtk_widget_actor_map                       (GtkActor       *actor);
void                        _gtk_widget_actor_unmap                     (GtkActor       *actor);
void                        _gtk_widget_actor_realize                   (GtkActor       *actor);
void                        _gtk_widget_actor_unrealize                 (GtkActor       *actor);
GtkSizeRequestMode          _gtk_widget_actor_get_request_mode          (GtkActor       *self);
void                        _gtk_widget_actor_get_preferred_size        (GtkActor       *self,
                                                                         GtkOrientation  orientation,
                                                                         gfloat          for_size,
                                                                         gfloat         *min_size_p,
                                                                         gfloat         *natural_size_p);

void                        _gtk_widget_actor_screen_changed            (GtkActor       *actor,
                                                                         GdkScreen      *new_screen,
                                                                         GdkScreen      *old_screen);

G_END_DECLS

#endif /* __GTK_WIDGET_ACTOR_PRIVATE_H__ */
