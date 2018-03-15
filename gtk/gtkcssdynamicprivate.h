/*
 * Copyright © 2018 Red Hat Inc.
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

#ifndef __GTK_CSS_DYNAMIC_PRIVATE_H__
#define __GTK_CSS_DYNAMIC_PRIVATE_H__

#include "gtkstyleanimationprivate.h"

#include "gtkprogresstrackerprivate.h"

G_BEGIN_DECLS

#define GTK_TYPE_CSS_DYNAMIC           (gtk_css_dynamic_get_type ())
#define GTK_CSS_DYNAMIC(obj)           (G_TYPE_CHECK_INSTANCE_CAST (obj, GTK_TYPE_CSS_DYNAMIC, GtkCssDynamic))
#define GTK_CSS_DYNAMIC_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST (cls, GTK_TYPE_CSS_DYNAMIC, GtkCssDynamicClass))
#define GTK_IS_CSS_DYNAMIC(obj)        (G_TYPE_CHECK_INSTANCE_TYPE (obj, GTK_TYPE_CSS_DYNAMIC))
#define GTK_IS_CSS_DYNAMIC_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE (obj, GTK_TYPE_CSS_DYNAMIC))
#define GTK_CSS_DYNAMIC_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_CSS_DYNAMIC, GtkCssDynamicClass))

typedef struct _GtkCssDynamic           GtkCssDynamic;
typedef struct _GtkCssDynamicClass      GtkCssDynamicClass;

struct _GtkCssDynamic
{
  GtkStyleAnimation   parent;

  gint64              timestamp;
};

struct _GtkCssDynamicClass
{
  GtkStyleAnimationClass parent_class;
};

GType                   gtk_css_dynamic_get_type        (void) G_GNUC_CONST;

GtkStyleAnimation *     gtk_css_dynamic_new             (gint64 timestamp);

G_END_DECLS

#endif /* __GTK_CSS_DYNAMIC_PRIVATE_H__ */
