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

#ifndef __GTK_CSS_ACTOR_PRIVATE_H__
#define __GTK_CSS_ACTOR_PRIVATE_H__

#include <gtk/actors/gtkactorprivate.h>
#include <gtk/gtkbitmaskprivate.h>
#include <gtk/gtkstylecontext.h>

G_BEGIN_DECLS

#define GTK_TYPE_CSS_ACTOR           (_gtk_css_actor_get_type ())
#define GTK_CSS_ACTOR(obj)           (G_TYPE_CHECK_INSTANCE_CAST (obj, GTK_TYPE_CSS_ACTOR, GtkCssActor))
#define GTK_CSS_ACTOR_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST (cls, GTK_TYPE_CSS_ACTOR, GtkCssActorClass))
#define GTK_IS_CSS_ACTOR(obj)        (G_TYPE_CHECK_INSTANCE_TYPE (obj, GTK_TYPE_CSS_ACTOR))
#define GTK_IS_CSS_ACTOR_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE (obj, GTK_TYPE_CSS_ACTOR))
#define GTK_CSS_ACTOR_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_CSS_ACTOR, GtkCssActorClass))

typedef struct _GtkCssActor           GtkCssActor;
typedef struct _GtkCssActorPrivate    GtkCssActorPrivate;
typedef struct _GtkCssActorClass      GtkCssActorClass;
typedef struct _GtkCssActorIter       GtkCssActorIter;

struct _GtkCssActor
{
  GtkActor            parent;

  GtkCssActorPrivate    *priv;
};

struct _GtkCssActorClass
{
  GtkActorClass       parent_class;

  void                (* style_updated)                 (GtkCssActor            *actor,
                                                         const GtkBitmask       *changed);
};

GType                           _gtk_css_actor_get_type                           (void) G_GNUC_CONST;

GtkStyleContext *               _gtk_css_actor_get_style_context                  (GtkCssActor                  *actor);
void                            _gtk_css_actor_init_box                           (GtkCssActor                  *actor);

G_END_DECLS

#endif /* __GTK_CSS_ACTOR_PRIVATE_H__ */
