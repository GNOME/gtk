/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"

#include "gtkdebugprivate.h"
#include "gtkprivate.h"


typedef struct {
  GdkDisplay *display;
  guint flags;
} DisplayDebugFlags;

#define N_DEBUG_DISPLAYS 4

DisplayDebugFlags debug_flags[N_DEBUG_DISPLAYS];

/* This is a flag to speed up development builds. We set it to TRUE when
 * any of the debug displays has debug flags >0, but we never set it back
 * to FALSE. This way we don't need to call gtk_widget_get_display() in
 * hot paths.
 */
gboolean any_display_debug_flags_set = FALSE;

GtkDebugFlags
gtk_get_display_debug_flags (GdkDisplay *display)
{
  int i;

  if (display == NULL)
    display = gdk_display_get_default ();

  for (i = 0; i < N_DEBUG_DISPLAYS; i++)
    {
      if (debug_flags[i].display == display)
        return (GtkDebugFlags)debug_flags[i].flags;
    }

  return 0;
}

gboolean
gtk_get_any_display_debug_flag_set (void)
{
  return any_display_debug_flags_set;
}

void
gtk_set_display_debug_flags (GdkDisplay    *display,
                             GtkDebugFlags  flags)
{
  int i;

  for (i = 0; i < N_DEBUG_DISPLAYS; i++)
    {
      if (debug_flags[i].display == NULL)
        debug_flags[i].display = display;

      if (debug_flags[i].display == display)
        {
          debug_flags[i].flags = flags;
          if (flags > 0)
            any_display_debug_flags_set = TRUE;

          return;
        }
    }
}

/**
 * gtk_get_debug_flags:
 *
 * Returns the GTK debug flags that are currently active.
 *
 * This function is intended for GTK modules that want
 * to adjust their debug output based on GTK debug flags.
 *
 * Returns: the GTK debug flags.
 */
GtkDebugFlags
gtk_get_debug_flags (void)
{
  if (gtk_get_any_display_debug_flag_set ())
    return gtk_get_display_debug_flags (gdk_display_get_default ());

  return 0;
}

/**
 * gtk_set_debug_flags:
 * @flags: the debug flags to set
 *
 * Sets the GTK debug flags.
 */
void
gtk_set_debug_flags (GtkDebugFlags flags)
{
  gtk_set_display_debug_flags (gdk_display_get_default (), flags);
}

static const GdkDebugKey gtk_debug_keys[] = {
  { "keybindings", GTK_DEBUG_KEYBINDINGS, "Information about keyboard shortcuts" },
  { "modules", GTK_DEBUG_MODULES, "Information about modules and extensions" },
  { "icontheme", GTK_DEBUG_ICONTHEME, "Information about icon themes" },
  { "printing", GTK_DEBUG_PRINTING, "Information about printing" },
  { "geometry", GTK_DEBUG_GEOMETRY, "Information about size allocation" },
  { "size-request", GTK_DEBUG_SIZE_REQUEST, "Information about size requests" },
  { "actions", GTK_DEBUG_ACTIONS, "Information about actions and menu models" },
  { "constraints", GTK_DEBUG_CONSTRAINTS, "Information from the constraints solver" },
  { "text", GTK_DEBUG_TEXT, "Information about GtkTextView" },
  { "tree", GTK_DEBUG_TREE, "Information about GtkTreeView" },
  { "layout", GTK_DEBUG_LAYOUT, "Information from layout managers" },
  { "builder-trace", GTK_DEBUG_BUILDER_TRACE, "Trace GtkBuilder operation" },
  { "builder-objects", GTK_DEBUG_BUILDER_OBJECTS, "Log unused GtkBuilder objects" },
  { "no-css-cache", GTK_DEBUG_NO_CSS_CACHE, "Disable style property cache" },
  { "interactive", GTK_DEBUG_INTERACTIVE, "Enable the GTK inspector" },
  { "touch-ui", GTK_DEBUG_TOUCHSCREEN, "Show touch ui elements for pointer events" },
  { "snapshot", GTK_DEBUG_SNAPSHOT, "Generate debug render nodes" },
  { "accessibility", GTK_DEBUG_A11Y, "Information about accessibility state changes" },
  { "iconfallback", GTK_DEBUG_ICONFALLBACK, "Information about icon fallback" },
  { "invert-text-dir", GTK_DEBUG_INVERT_TEXT_DIR, "Invert the default text direction" },
  { "css", GTK_DEBUG_CSS, "Information about deprecated CSS features" },
  { "builder", GTK_DEBUG_BUILDER, "Information about deprecated GtkBuilder features" },
  { "session-mgmt", GTK_DEBUG_SESSION, "Information about session saving" },
  { "general-info", GTK_DEBUG_GENERAL_INFO, "General information (in markdown)" },
};

void
gtk_debug_init (void)
{
  debug_flags[0].flags = gdk_parse_debug_var ("GTK_DEBUG",
      "GTK_DEBUG can be set to values that make GTK print out different\n"
      "types of debugging information or change the behavior of GTK for\n"
      "debugging purposes.\n",
      gtk_debug_keys,
      G_N_ELEMENTS (gtk_debug_keys));
  any_display_debug_flags_set = debug_flags[0].flags > 0;
}

void
gtk_debug_init_display (void)
{
  debug_flags[0].display = gdk_display_get_default ();
}
