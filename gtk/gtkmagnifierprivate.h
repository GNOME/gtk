/* GTK - The GIMP Toolkit
 * Copyright © 2013 Carlos Garnacho <carlosg@gnome.org>
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
 */

#ifndef __GTK_MAGNIFIER_H__
#define __GTK_MAGNIFIER_H__

G_BEGIN_DECLS

#define GTK_TYPE_MAGNIFIER           (gtk_magnifier_get_type ())
#define GTK_MAGNIFIER(o)             (G_TYPE_CHECK_INSTANCE_CAST ((o), GTK_TYPE_MAGNIFIER, GtkMagnifier))
#define GTK_MAGNIFIER_CLASS(c)       (G_TYPE_CHECK_CLASS_CAST ((c), GTK_TYPE_MAGNIFIER, GtkMagnifierClass))
#define GTK_IS_MAGNIFIER(o)          (G_TYPE_CHECK_INSTANCE_TYPE ((o), GTK_TYPE_MAGNIFIER))
#define GTK_IS_MAGNIFIER_CLASS(o)    (G_TYPE_CHECK_CLASS_TYPE ((o), GTK_TYPE_MAGNIFIER))
#define GTK_MAGNIFIER_GET_CLASS(o)   (G_TYPE_INSTANCE_GET_CLASS ((o), GTK_TYPE_MAGNIFIER, GtkMagnifierClass))

typedef struct _GtkMagnifier GtkMagnifier;
typedef struct _GtkMagnifierClass GtkMagnifierClass;

struct _GtkMagnifier
{
  GtkWidget parent_instance;
};

struct _GtkMagnifierClass
{
  GtkWidgetClass parent_class;
};

GType       gtk_magnifier_get_type           (void) G_GNUC_CONST;

GtkWidget * _gtk_magnifier_new               (GtkWidget       *inspected);

GtkWidget * _gtk_magnifier_get_inspected     (GtkMagnifier *magnifier);
void        _gtk_magnifier_set_inspected     (GtkMagnifier *magnifier,
                                              GtkWidget    *inspected);

void        _gtk_magnifier_set_coords        (GtkMagnifier *magnifier,
                                              gdouble       x,
                                              gdouble       y);
void        _gtk_magnifier_get_coords        (GtkMagnifier *magnifier,
                                              gdouble      *x,
                                              gdouble      *y);

void        _gtk_magnifier_set_magnification (GtkMagnifier *magnifier,
                                              gdouble       magnification);
gdouble     _gtk_magnifier_get_magnification (GtkMagnifier *magnifier);

void        _gtk_magnifier_set_resize        (GtkMagnifier *magnifier,
                                              gboolean      resize);
gboolean    _gtk_magnifier_get_resize        (GtkMagnifier *magnifier);

G_END_DECLS

#endif /* __GTK_MAGNIFIER_H__ */
