/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2020 Red Hat
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
 */

#include "config.h"

#include "gdktoplevellayout.h"

#include "gdkmonitor.h"

struct _GdkToplevelLayout
{
  /* < private >*/
  grefcount ref_count;

  int min_width;
  int min_height;
  int max_width;
  int max_height;
  guint maximized  : 1;
  guint fullscreen : 1;
  guint modal      : 1;
  guint raise      : 1;
  guint lower      : 1;
  GdkSurfaceTypeHint type_hint;
  GdkMonitor *fullscreen_monitor;
};

G_DEFINE_BOXED_TYPE (GdkToplevelLayout, gdk_toplevel_layout,
                     gdk_toplevel_layout_ref,
                     gdk_toplevel_layout_unref)

/**
 * gdk_toplevel_layout_new: (constructor)
 *
 * Create a toplevel layout description. Used together with
 * gdk_surface_present_toplevel() to describe how a toplevel surface
 * should be placed and behave on-screen.
 *
 * Returns: (transfer full): newly created instance of #GdkToplevelLayout
 */
GdkToplevelLayout *
gdk_toplevel_layout_new (int min_width,
                         int min_height,
                         int max_width,
                         int max_height)
{
  GdkToplevelLayout *layout;

  layout = g_new0 (GdkToplevelLayout, 1);
  g_ref_count_init (&layout->ref_count);
  layout->min_width = min_width;
  layout->min_height = min_height;
  layout->max_width = max_width;
  layout->max_height = max_height;
  layout->maximized = FALSE;
  layout->fullscreen = FALSE;
  layout->modal = FALSE;
  layout->raise = TRUE;
  layout->lower = FALSE;
  layout->type_hint = GDK_SURFACE_TYPE_HINT_NORMAL;
  layout->fullscreen_monitor = NULL;

  return layout;
}

/**
 * gdk_toplevel_layout_ref:
 * @layout: a #GdkToplevelLayout
 *
 * Increases the reference count of @layout.
 *
 * Returns: the same @layout
 */
GdkToplevelLayout *
gdk_toplevel_layout_ref (GdkToplevelLayout *layout)
{
  g_ref_count_inc (&layout->ref_count);
  return layout;
}

/**
 * gdk_toplevel_layout_unref:
 * @layout: a #GdkToplevelLayout
 *
 * Decreases the reference count of @layout.
 */
void
gdk_toplevel_layout_unref (GdkToplevelLayout *layout)
{
  if (g_ref_count_dec (&layout->ref_count))
    {
      g_clear_object (&layout->fullscreen_monitor);
      g_free (layout);
    }
}

/**
 * gdk_toplevel_layout_copy:
 * @layout: a #GdkToplevelLayout
 *
 * Create a new #GdkToplevelLayout and copy the contents of @layout into it.
 *
 * Returns: (transfer full): a copy of @layout.
 */
GdkToplevelLayout *
gdk_toplevel_layout_copy (GdkToplevelLayout *layout)
{
  GdkToplevelLayout *new_layout;

  new_layout = g_new0 (GdkToplevelLayout, 1);
  g_ref_count_init (&new_layout->ref_count);

  new_layout->min_width = layout->min_width;
  new_layout->min_height = layout->min_height;
  new_layout->max_width = layout->max_width;
  new_layout->max_height = layout->max_height;
  new_layout->maximized = layout->maximized;
  new_layout->fullscreen = layout->fullscreen;
  new_layout->modal = layout->modal;
  new_layout->raise = layout->raise;
  new_layout->lower = layout->lower;
  new_layout->type_hint = layout->type_hint;
  if (layout->fullscreen_monitor)
    new_layout->fullscreen_monitor = g_object_ref (layout->fullscreen_monitor);

  return new_layout;
}

/**
 * gdk_toplevel_layout_equal:
 * @layout: a #GdkToplevelLayout
 * @other: another #GdkToplevelLayout
 *
 * Check whether @layout and @other has identical layout properties.
 *
 * Returns: %TRUE if @layout and @other have identical layout properties,
 *     otherwise %FALSE.
 */
gboolean
gdk_toplevel_layout_equal (GdkToplevelLayout *layout,
                           GdkToplevelLayout *other)
{
  g_return_val_if_fail (layout, FALSE);
  g_return_val_if_fail (other, FALSE);

  return layout->min_width == other->min_width &&
         layout->min_height == other->min_height &&
         layout->max_width == other->max_width &&
         layout->max_height == other->max_height &&
         layout->maximized == other->maximized &&
         layout->fullscreen == other->fullscreen &&
         layout->modal == other->modal &&
         layout->type_hint == other->type_hint &&
         layout->fullscreen_monitor == other->fullscreen_monitor;
}


void
gdk_toplevel_layout_get_min_size (GdkToplevelLayout *layout,
                                  int               *width,
                                  int               *height)
{
  *width = layout->min_width;
  *height = layout->min_height;
}

void
gdk_toplevel_layout_get_max_size (GdkToplevelLayout *layout,
                                  int               *width,
                                  int               *height)
{
  *width = layout->max_width;
  *height = layout->max_height;
}

void
gdk_toplevel_layout_set_maximized (GdkToplevelLayout *layout,
                                   gboolean           maximized)
{
  layout->maximized = maximized;
}


gboolean
gdk_toplevel_layout_get_maximized (GdkToplevelLayout *layout)
{
  return layout->maximized;
}

void
gdk_toplevel_layout_set_fullscreen (GdkToplevelLayout *layout,
                                    gboolean           fullscreen,
                                    GdkMonitor        *monitor)
{
  layout->fullscreen = fullscreen;
  if (monitor)
    layout->fullscreen_monitor = g_object_ref (monitor);
}

gboolean
gdk_toplevel_layout_get_fullscreen (GdkToplevelLayout *layout)
{
  return layout->fullscreen;
}

GdkMonitor *
gdk_toplevel_layout_get_fullscreen_monitor (GdkToplevelLayout *layout)
{
  return layout->fullscreen_monitor;
}

void
gdk_toplevel_layout_set_modal (GdkToplevelLayout *layout,
                               gboolean           modal)
{
  layout->modal = modal;
}


gboolean
gdk_toplevel_layout_get_modal (GdkToplevelLayout *layout)
{
  return layout->modal;
}

void
gdk_toplevel_layout_set_type_hint (GdkToplevelLayout  *layout,
                                   GdkSurfaceTypeHint  type_hint)
{
  layout->type_hint = type_hint;
}


GdkSurfaceTypeHint
gdk_toplevel_layout_get_type_hint (GdkToplevelLayout *layout)
{
  return layout->type_hint;
}

void
gdk_toplevel_layout_set_raise (GdkToplevelLayout *layout,
                               gboolean           raise)
{
  layout->raise = raise;
}


gboolean
gdk_toplevel_layout_get_raise (GdkToplevelLayout *layout)
{
  return layout->raise;
}

void
gdk_toplevel_layout_set_lower (GdkToplevelLayout *layout,
                               gboolean           lower)
{
  layout->lower = lower;
}


gboolean
gdk_toplevel_layout_get_lower (GdkToplevelLayout *layout)
{
  return layout->lower;
}

