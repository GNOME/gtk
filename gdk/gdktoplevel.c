/*
 * Copyright © 2020 Red Hat, Inc.
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
 * Authors: Matthias Clasen <mclasen@redhat.com>
 */

#include "config.h"

#include "gdkintl.h"
#include "gdk-private.h"
#include "gdktoplevelprivate.h"

/**
 * SECTION:gdktoplevel
 * @Short_description: Interface for toplevel surfaces
 * @Title: Toplevels
 *
 * A #GdkToplevel is a freestanding toplevel surface.
 */


/* FIXME: this can't have GdkSurface as a prerequisite
 * as long as GdkSurface implements this interface itself
 */
G_DEFINE_INTERFACE (GdkToplevel, gdk_toplevel, G_TYPE_OBJECT)

static gboolean
gdk_toplevel_default_present (GdkToplevel       *toplevel,
                              int                width,
                              int                height,
                              GdkToplevelLayout *layout)
{
  return FALSE;
}

static gboolean
gdk_toplevel_default_minimize (GdkToplevel *toplevel)
{
  return FALSE;
}

static gboolean
gdk_toplevel_default_lower (GdkToplevel *toplevel)
{
  return FALSE;
}

static void
gdk_toplevel_default_focus (GdkToplevel *toplevel,
                            guint32      timestamp)
{
}

static gboolean
gdk_toplevel_default_show_window_menu (GdkToplevel *toplevel,
                                       GdkEvent    *event)
{
  return FALSE;
}

static void
gdk_toplevel_default_init (GdkToplevelInterface *iface)
{
  iface->present = gdk_toplevel_default_present;
  iface->minimize = gdk_toplevel_default_minimize;
  iface->lower = gdk_toplevel_default_lower;
  iface->focus = gdk_toplevel_default_focus;
  iface->show_window_menu = gdk_toplevel_default_show_window_menu;

  g_object_interface_install_property (iface,
      g_param_spec_flags ("state",
                          P_("State"),
                          P_("State"),
                          GDK_TYPE_SURFACE_STATE, GDK_SURFACE_STATE_WITHDRAWN,
                          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_interface_install_property (iface,
      g_param_spec_string ("title",
                           "Title",
                           "The title of the surface",
                           NULL,
                           G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY));
  g_object_interface_install_property (iface,
      g_param_spec_string ("startup-id",
                           "Startup ID",
                           "The startup ID of the surface",
                           NULL,
                           G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY));
  g_object_interface_install_property (iface,
      g_param_spec_object ("transient-for",
                           "Transient For",
                           "The transient parent of the surface",
                           GDK_TYPE_SURFACE,
                           G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY));
  g_object_interface_install_property (iface,
      g_param_spec_pointer ("icon-list",
                            "Icon List",
                            "The list of icon textures",
                            G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY));
  g_object_interface_install_property (iface,
      g_param_spec_boolean ("sticky",
                            "Sticky",
                            "Whether the surface is on all workspaces",
                            FALSE,
                            G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY));
  g_object_interface_install_property (iface,
      g_param_spec_boolean ("keep-above",
                            "Keep above",
                            "Whether the surface is on above all other surfaces",
                            FALSE,
                            G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY));
  g_object_interface_install_property (iface,
      g_param_spec_boolean ("keep-below",
                            "Keep below",
                            "Whether the surface is below all other surfaces",
                            FALSE,
                            G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY));
  g_object_interface_install_property (iface,
      g_param_spec_boolean ("accept-focus",
                            "Accept focus",
                            "Whether the surface should accept keyboard focus",
                            TRUE,
                            G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY));
  g_object_interface_install_property (iface,
      g_param_spec_boolean ("focus-on-map",
                            "Focus on map",
                            "Whether the surface should receive keyboard focus on map",
                            TRUE,
                            G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY));
  g_object_interface_install_property (iface,
      g_param_spec_flags ("decorations",
                          "Decorations",
                          "Decorations",
                          GDK_TYPE_WM_DECORATION, 0,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY));
  g_object_interface_install_property (iface,
      g_param_spec_flags ("functions",
                          "Functions",
                          "Functions",
                          GDK_TYPE_WM_FUNCTION, 0,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY));
}

guint
gdk_toplevel_install_properties (GObjectClass *object_class,
                                 guint         first_prop)
{
  g_object_class_override_property (object_class, first_prop + GDK_TOPLEVEL_PROP_STATE, "state");
  g_object_class_override_property (object_class, first_prop + GDK_TOPLEVEL_PROP_TITLE, "title");
  g_object_class_override_property (object_class, first_prop + GDK_TOPLEVEL_PROP_STARTUP_ID, "startup-id");
  g_object_class_override_property (object_class, first_prop + GDK_TOPLEVEL_PROP_TRANSIENT_FOR, "transient-for");
  g_object_class_override_property (object_class, first_prop + GDK_TOPLEVEL_PROP_ICON_LIST, "icon-list");
  g_object_class_override_property (object_class, first_prop + GDK_TOPLEVEL_PROP_STICKY, "sticky");
  g_object_class_override_property (object_class, first_prop + GDK_TOPLEVEL_PROP_KEEP_ABOVE, "keep-above");
  g_object_class_override_property (object_class, first_prop + GDK_TOPLEVEL_PROP_KEEP_BELOW, "keep-below");
  g_object_class_override_property (object_class, first_prop + GDK_TOPLEVEL_PROP_ACCEPT_FOCUS, "accept-focus");
  g_object_class_override_property (object_class, first_prop + GDK_TOPLEVEL_PROP_FOCUS_ON_MAP, "focus-on-map");
  g_object_class_override_property (object_class, first_prop + GDK_TOPLEVEL_PROP_DECORATIONS, "decorations");
  g_object_class_override_property (object_class, first_prop + GDK_TOPLEVEL_PROP_FUNCTIONS, "functions");

  return GDK_TOPLEVEL_NUM_PROPERTIES;
}

/**
 * gdk_toplevel_present:
 * @toplevel: the #GdkToplevel to show
 * @width: the unconstrained toplevel width to layout
 * @height: the unconstrained toplevel height to layout
 * @layout: the #GdkToplevelLayout object used to layout
 *
 * Present @toplevel after having processed the #GdkToplevelLayout rules.
 * If the toplevel was previously now showing, it will be showed,
 * otherwise it will change layout according to @layout.
 *
 * Presenting may fail.
 *
 * Returns: %FALSE if @toplevel failed to be presented, otherwise %TRUE.
 */
gboolean
gdk_toplevel_present (GdkToplevel       *toplevel,
                      int                width,
                      int                height,
                      GdkToplevelLayout *layout)
{
  g_return_val_if_fail (GDK_IS_TOPLEVEL (toplevel), FALSE);
  g_return_val_if_fail (width > 0, FALSE);
  g_return_val_if_fail (height > 0, FALSE);
  g_return_val_if_fail (layout != NULL, FALSE);

  return GDK_TOPLEVEL_GET_IFACE (toplevel)->present (toplevel, width, height, layout);
}

/**
 * gdk_toplevel_minimize:
 * @toplevel: a #GdkToplevel
 *
 * Asks to minimize the @toplevel.
 *
 * The windowing system may choose to ignore the request.
 *
 * Returns: %TRUE if the surface was minimized
 */
gboolean
gdk_toplevel_minimize (GdkToplevel *toplevel)
{
  g_return_val_if_fail (GDK_IS_TOPLEVEL (toplevel), FALSE);

  return GDK_TOPLEVEL_GET_IFACE (toplevel)->minimize (toplevel);
}

/**
 * gdk_toplevel_lower:
 * @toplevel: a #GdkToplevel
 *
 * Asks to lower the @toplevel below other windows.
 *
 * The windowing system may choose to ignore the request.
 *
 * Returns: %TRUE if the surface was lowered
 */
gboolean
gdk_toplevel_lower (GdkToplevel *toplevel)
{
  g_return_val_if_fail (GDK_IS_TOPLEVEL (toplevel), FALSE);

  return GDK_TOPLEVEL_GET_IFACE (toplevel)->lower (toplevel);
}

/**
 * gdk_toplevel_focus:
 * @toplevel: a #GdkToplevel
 * @timestamp: timestamp of the event triggering the surface focus
 *
 * Sets keyboard focus to @surface.
 *
 * In most cases, gtk_window_present_with_time() should be used
 * on a #GtkWindow, rather than calling this function.
 */
void
gdk_toplevel_focus (GdkToplevel *toplevel,
                   guint32       timestamp)
{
  g_return_if_fail (GDK_IS_TOPLEVEL (toplevel));

  return GDK_TOPLEVEL_GET_IFACE (toplevel)->focus (toplevel, timestamp);
}

/**
 * gdk_toplevel_get_state:
 * @toplevel: a #GdkToplevel
 *
 * Gets the bitwise OR of the currently active surface state flags,
 * from the #GdkSurfaceState enumeration.
 *
 * Returns: surface state bitfield
 */
GdkSurfaceState
gdk_toplevel_get_state (GdkToplevel *toplevel)
{
  GdkSurfaceState state;

  g_return_val_if_fail (GDK_IS_TOPLEVEL (toplevel), 0);

  g_object_get (toplevel, "state", &state, NULL);

  return state;
}

/**
 * gdk_toplevel_set_title:
 * @toplevel: a #GdkToplevel
 * @title: title of @surface
 *
 * Sets the title of a toplevel surface, to be displayed in the titlebar,
 * in lists of windows, etc.
 */
void
gdk_toplevel_set_title (GdkToplevel *toplevel,
                        const char  *title)
{
  g_return_if_fail (GDK_IS_TOPLEVEL (toplevel));

  g_object_set (toplevel, "title", title, NULL);
}

/**
 * gdk_toplevel_set_startup_id:
 * @toplevel: a #GdkToplevel
 * @startup_id: a string with startup-notification identifier
 *
 * When using GTK, typically you should use gtk_window_set_startup_id()
 * instead of this low-level function.
 */
void
gdk_toplevel_set_startup_id (GdkToplevel *toplevel,
                             const char  *startup_id)
{
  g_return_if_fail (GDK_IS_TOPLEVEL (toplevel));

  g_object_set (toplevel, "startup-id", startup_id, NULL);
}

/**
 * gdk_toplevel_set_transient_for:
 * @toplevel: a #GdkToplevel
 * @parent: another toplevel #GdkSurface
 *
 * Indicates to the window manager that @surface is a transient dialog
 * associated with the application surface @parent. This allows the
 * window manager to do things like center @surface on @parent and
 * keep @surface above @parent.
 *
 * See gtk_window_set_transient_for() if you’re using #GtkWindow or
 * #GtkDialog.
 */
void
gdk_toplevel_set_transient_for (GdkToplevel *toplevel,
                                GdkSurface  *parent)
{
  g_return_if_fail (GDK_IS_TOPLEVEL (toplevel));

  g_object_set (toplevel, "transient-for", parent, NULL);
}


/**
 * gdk_toplevel_set_icon_list:
 * @toplevel: a #GdkToplevel
 * @surfaces: (transfer none) (element-type GdkTexture):
 *     A list of textures to use as icon, of different sizes
 *
 * Sets a list of icons for the surface.
 *
 * One of these will be used to represent the surface in iconic form.
 * The icon may be shown in window lists or task bars. Which icon
 * size is shown depends on the window manager. The window manager
 * can scale the icon but setting several size icons can give better
 * image quality.
 *
 * Note that some platforms don't support surface icons.
 */
void
gdk_toplevel_set_icon_list (GdkToplevel *toplevel,
                            GList       *surfaces)
{
  g_return_if_fail (GDK_IS_TOPLEVEL (toplevel));

  g_object_set (toplevel, "icon-list", surfaces, NULL);
}

/**
 * gdk_toplevel_show_window_menu:
 * @toplevel: a #GdkToplevel
 * @event: a #GdkEvent to show the menu for
 *
 * Asks the windowing system to show the window menu.
 *
 * The window menu is the menu shown when right-clicking the titlebar
 * on traditional windows managed by the window manager. This is useful
 * for windows using client-side decorations, activating it with a
 * right-click on the window decorations.
 *
 * Returns: %TRUE if the window menu was shown and %FALSE otherwise.
 */
gboolean
gdk_toplevel_show_window_menu (GdkToplevel *toplevel,
                               GdkEvent    *event)
{
  g_return_val_if_fail (GDK_IS_TOPLEVEL (toplevel), FALSE);

  return GDK_TOPLEVEL_GET_IFACE (toplevel)->show_window_menu (toplevel, event); 
}

/**
 * gdk_toplevel_set_sticky:
 * @toplevel: a #GdkToplevel
 * @sticky: whether to make @toplevel show on all workspaces
 *
 * Set if @surface is sticky.
 **/
void
gdk_toplevel_set_sticky (GdkToplevel *toplevel,
                         gboolean     sticky)
{
  g_return_if_fail (GDK_IS_TOPLEVEL (toplevel));

  g_object_set (toplevel, "sticky", sticky, NULL);
}

/**
 * gdk_toplevel_set_keep_above:
 * @toplevel: a #GdkToplevel
 * @above: whether to keep @toplevel above other surfaces
 *
 * Set if @surface must be kept above other surfaces.
 **/
void
gdk_toplevel_set_keep_above (GdkToplevel *toplevel,
                             gboolean     above)
{
  g_return_if_fail (GDK_IS_TOPLEVEL (toplevel));

  g_object_set (toplevel, "keep-above", above, NULL);
}

/**
 * gdk_toplevel_set_keep_below:
 * @toplevel: a #GdkToplevel
 * @above: whether to keep @toplevel below other surfaces
 *
 * Set if @surface must be kept below other surfaces.
 **/
void
gdk_toplevel_set_keep_below (GdkToplevel *toplevel,
                             gboolean     below)
{
  g_return_if_fail (GDK_IS_TOPLEVEL (toplevel));

  g_object_set (toplevel, "keep-below", below, NULL);
}

/**
 * gdk_toplevel_set_accept_focus:
 * @toplevel: a #GdkToplevel
 * @accept_focus: whether @toplevel should accept keyboard focus
 *
 * Setting @accept_focus to %FALSE hints the desktop environment
 * that the surface doesn’t want to receive input focus.
 */
void
gdk_toplevel_set_accept_focus (GdkToplevel *toplevel,
                               gboolean     accept_focus)
{
  g_return_if_fail (GDK_IS_TOPLEVEL (toplevel));

  g_object_set (toplevel, "accept-focus", accept_focus, NULL);
}

/**
 * gdk_toplevel_set_focus_on_map:
 * @toplevel: a #GdkToplevel
 * @focus_on_map: whether @toplevel should receive input focus when mapped
 *
 * Setting @focus_on_map to %FALSE hints the desktop environment that the
 * surface doesn’t want to receive input focus when it is mapped.
 * focus_on_map should be turned off for surfaces that aren’t triggered
 * interactively (such as popups from network activity).
 */
void
gdk_toplevel_set_focus_on_map (GdkToplevel *toplevel,
                               gboolean     focus_on_map)
{
  g_return_if_fail (GDK_IS_TOPLEVEL (toplevel));

  g_object_set (toplevel, "focus-on-map", focus_on_map, NULL);
}

void
gdk_toplevel_set_decorations (GdkToplevel     *toplevel,
                              GdkWMDecoration  decorations)
{
  g_return_if_fail (GDK_IS_TOPLEVEL (toplevel));

  g_object_set (toplevel, "decorations", decorations, NULL);
}

void
gdk_toplevel_set_functions (GdkToplevel   *toplevel,
                            GdkWMFunction  functions)
{
  g_return_if_fail (GDK_IS_TOPLEVEL (toplevel));

  g_object_set (toplevel, "functions", functions, NULL);
}
