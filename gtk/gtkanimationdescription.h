/* GTK - The GIMP Toolkit
 * Copyright (C) 2010 Carlos Garnacho <carlosg@gnome.org>
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GTK_ANIMATION_DESCRIPTION_H__
#define __GTK_ANIMATION_DESCRIPTION_H__

#include "gtktimeline.h"

G_BEGIN_DECLS

/* Dummy typedefs */
typedef struct GtkAnimationDescription GtkAnimationDescription;

#define GTK_TYPE_ANIMATION_DESCRIPTION (_gtk_animation_description_get_type ())

GType                     _gtk_animation_description_get_type          (void) G_GNUC_CONST;

GtkAnimationDescription * _gtk_animation_description_new               (gdouble                  duration,
                                                                        GtkTimelineProgressType  progress_type,
                                                                        gboolean                 loop);

gdouble                   _gtk_animation_description_get_duration      (GtkAnimationDescription *desc);
GtkTimelineProgressType   _gtk_animation_description_get_progress_type (GtkAnimationDescription *desc);
gboolean                  _gtk_animation_description_get_loop          (GtkAnimationDescription *desc);

GtkAnimationDescription * _gtk_animation_description_ref               (GtkAnimationDescription *desc);
void                      _gtk_animation_description_unref             (GtkAnimationDescription *desc);

GtkAnimationDescription * _gtk_animation_description_from_string       (const gchar *str);

G_END_DECLS

#endif /* __GTK_ANIMATION_DESCRIPTION_H__ */
