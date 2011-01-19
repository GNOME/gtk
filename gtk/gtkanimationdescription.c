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

#include "config.h"
#include "gtkanimationdescription.h"
#include "gtkintl.h"

struct GtkAnimationDescription
{
  GtkTimelineProgressType progress_type;
  gdouble duration;
  guint loop : 1;
  guint ref_count;
};

GtkAnimationDescription *
_gtk_animation_description_new (gdouble                 duration,
                                GtkTimelineProgressType progress_type,
                                gboolean                loop)
{
  GtkAnimationDescription *desc;

  desc = g_slice_new (GtkAnimationDescription);
  desc->duration = duration;
  desc->progress_type = progress_type;
  desc->loop = loop;
  desc->ref_count = 1;

  return desc;
}

gdouble
_gtk_animation_description_get_duration (GtkAnimationDescription *desc)
{
  return desc->duration;
}

GtkTimelineProgressType
_gtk_animation_description_get_progress_type (GtkAnimationDescription *desc)
{
  return desc->progress_type;
}

gboolean
_gtk_animation_description_get_loop (GtkAnimationDescription *desc)
{
  return (desc->loop != 0);
}

GtkAnimationDescription *
_gtk_animation_description_ref (GtkAnimationDescription *desc)
{
  desc->ref_count++;
  return desc;
}

void
_gtk_animation_description_unref (GtkAnimationDescription *desc)
{
  desc->ref_count--;

  if (desc->ref_count == 0)
    g_slice_free (GtkAnimationDescription, desc);
}

GtkAnimationDescription *
_gtk_animation_description_from_string (const gchar *str)
{
  gchar timing_function[16] = { 0, };
  gchar duration_unit[3] = { 0, };
  gchar loop_str[5] = { 0, };
  GtkTimelineProgressType progress_type;
  guint duration = 0;
  gboolean loop;

  if (sscanf (str, "%d%2s %15s %5s", &duration, duration_unit, timing_function, loop_str) == 4)
    loop = TRUE;
  else if (sscanf (str, "%d%2s %15s", &duration, duration_unit, timing_function) == 3)
    loop = FALSE;
  else
    return NULL;

  if (strcmp (duration_unit, "s") == 0)
    duration *= 1000;
  else if (strcmp (duration_unit, "ms") != 0)
    {
      g_warning ("Unknown duration unit: %s\n", duration_unit);
      return NULL;
    }

  if (strcmp (timing_function, "linear") == 0)
    progress_type = GTK_TIMELINE_PROGRESS_LINEAR;
  else if (strcmp (timing_function, "ease") == 0)
    progress_type = GTK_TIMELINE_PROGRESS_EASE;
  else if (strcmp (timing_function, "ease-in") == 0)
    progress_type = GTK_TIMELINE_PROGRESS_EASE_IN;
  else if (strcmp (timing_function, "ease-out") == 0)
    progress_type = GTK_TIMELINE_PROGRESS_EASE_OUT;
  else if (strcmp (timing_function, "ease-in-out") == 0)
    progress_type = GTK_TIMELINE_PROGRESS_EASE_IN_OUT;
  else
    {
      g_warning ("Unknown timing function: %s\n", timing_function);
      return NULL;
    }

  return _gtk_animation_description_new ((gdouble) duration, progress_type, loop);
}

GType
_gtk_animation_description_get_type (void)
{
  static GType type = 0;

  if (G_UNLIKELY (!type))
    type = g_boxed_type_register_static (I_("GtkAnimationDescription"),
                                         (GBoxedCopyFunc) _gtk_animation_description_ref,
                                         (GBoxedFreeFunc) _gtk_animation_description_unref);

  return type;
}
