/* GTK - The GIMP Toolkit
 * Copyright (C) 2012 Red Hat, Inc.
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

#ifndef __GTK_PRESS_AND_HOLD_H__
#define __GTK_PRESS_AND_HOLD_H__

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

G_BEGIN_DECLS

#define GTK_TYPE_PRESS_AND_HOLD                  (gtk_press_and_hold_get_type ())
#define GTK_PRESS_AND_HOLD(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_PRESS_AND_HOLD, GtkPressAndHold))
#define GTK_PRESS_AND_HOLD_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_PRESS_AND_HOLD, GtkPressAndHoldClass))
#define GTK_IS_PRESS_AND_HOLD(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_PRESS_AND_HOLD))
#define GTK_IS_PRESS_AND_HOLD_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_PRESS_AND_HOLD))
#define GTK_PRESS_AND_HOLD_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_PRESS_AND_HOLD, GtkPressAndHoldClass))


typedef struct _GtkPressAndHold        GtkPressAndHold;
typedef struct _GtkPressAndHoldClass   GtkPressAndHoldClass;
typedef struct _GtkPressAndHoldPrivate GtkPressAndHoldPrivate;

struct _GtkPressAndHold
{
  GObject parent;

  /*< private >*/
  GtkPressAndHoldPrivate *priv;
};

struct _GtkPressAndHoldClass
{
  GObjectClass parent_class;

  void ( * hold) (GtkPressAndHold *pah, gint x, gint y);
  void ( * tap)  (GtkPressAndHold *pah, gint x, gint y);
};


G_GNUC_INTERNAL
GType             gtk_press_and_hold_get_type      (void) G_GNUC_CONST;

G_GNUC_INTERNAL
GtkPressAndHold * gtk_press_and_hold_new           (void);

G_GNUC_INTERNAL
void              gtk_press_and_hold_process_event (GtkPressAndHold *pah,
                                                    GdkEvent        *event);

G_END_DECLS

#endif /* __GTK_PRESS_AND_HOLD_H__ */
