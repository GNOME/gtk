/*
 * Copyright (c) 2020 Alexander Mikhaylenko <alexm@gnome.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"

#include "gtktitlebuttons.h"

#include "gtkaccessible.h"
#include "gtkactionable.h"
#include "gtkboxlayout.h"
#include "gtkbutton.h"
#include "gtkenums.h"
#include "gtkimage.h"
#include "gtkintl.h"
#include "gtknative.h"
#include "gtkprivate.h"
#include "gtkstylecontext.h"
#include "gtktypebuiltins.h"
#include "gtkwindowprivate.h"

/**
 * SECTION:gtktitlebuttons
 * @Short_description: A widget displaying window buttons
 * @Title: GtkTitleButtons
 * @See_also: #GtkHeaderBar
 *
 * GtkTitleButtons shows window frame controls, such as minimize, maximize and
 * close buttons, and the window icon.
 *
 * #GtkTitleButtons only displays start or end part of the controls (see
 * #GtkTitleButtons:pack-type), so it's intended to be always used in pair
 * with another #GtkTitleButtons using the opposite pack type, for example:
 *
 * |[
 * <object class="GtkBox">
 *   <child>
 *     <object class="GtkTitleButtons">
 *       <property name="pack-type">start</property>
 *     </object>
 *   </child>
 *
 *   ...
 *
 *   <child>
 *     <object class="GtkTitleButtons">
 *       <property name="pack-type">end</property>
 *     </object>
 *   </child>
 * </object>
 * ]|
 *
 * # CSS nodes
 *
 * |[<!-- language="plain" -->
 * titlebuttons
 * ├── [image.titlebutton.icon]
 * ├── [button.titlebutton.minimize]
 * ├── [button.titlebutton.maximize]
 * ╰── [button.titlebutton.close]
 * ]|
 *
 * A #GtkTitleButtons' CSS node is called titlebuttons. It contains subnodes
 * corresponding to each title button. Which of the title buttons exist and
 * where they are placed exactly depends on the desktop environment and
 * #GtkTitleButtons:decoration-layout value.
 *
 * When #GtkTitleButton:empty is %TRUE, it gets the .empty style class.
 */

struct _GtkTitleButtons {
  GtkWidget parent_instance;

  GtkPackType pack_type;
  gchar *decoration_layout;
  GdkSurfaceState state;

  GList *controls;
  gboolean empty;
};

enum {
  PROP_0,
  PROP_PACK_TYPE,
  PROP_DECORATION_LAYOUT,
  PROP_EMPTY,
  LAST_PROP
};

static GParamSpec *props[LAST_PROP] = { NULL, };

G_DEFINE_TYPE (GtkTitleButtons, gtk_title_buttons, GTK_TYPE_WIDGET)

static gchar *
get_layout (GtkTitleButtons *self)
{
  GtkWidget *widget = GTK_WIDGET (self);
  GtkWidget *toplevel;
  gchar *layout_desc, *layout_half;
  gchar **tokens;

  toplevel = GTK_WIDGET (gtk_widget_get_root (widget));
  if (!GTK_IS_WINDOW (toplevel))
    return NULL;

  if (self->decoration_layout)
    layout_desc = g_strdup (self->decoration_layout);
  else
    g_object_get (gtk_widget_get_settings (widget),
                  "gtk-decoration-layout", &layout_desc,
                  NULL);

  tokens = g_strsplit (layout_desc, ":", 2);

  switch (self->pack_type)
    {
    case GTK_PACK_START:
      layout_half = g_strdup (tokens[0]);
      break;

    case GTK_PACK_END:
      layout_half = g_strdup (tokens[1]);
      break;

    default:
      g_assert_not_reached ();
    }

  g_free (layout_desc);
  g_strfreev (tokens);

  return layout_half;
}

static gboolean
update_window_icon (GtkWindow *window,
                    GtkWidget *icon)
{
  GdkPaintable *paintable;
  gint scale;

  scale = gtk_widget_get_scale_factor (icon);
  paintable = gtk_window_get_icon_for_size (window, 20 * scale);

  if (paintable)
    {
      gtk_image_set_from_paintable (GTK_IMAGE (icon), paintable);
      g_object_unref (paintable);
      gtk_widget_show (icon);

      return TRUE;
    }

  return FALSE;
}

static void
set_empty (GtkTitleButtons *self,
           gboolean         empty)
{
  if (empty == self->empty)
    return;

  self->empty = empty;

  if (empty)
    gtk_widget_add_css_class (GTK_WIDGET (self), "empty");
  else
    gtk_widget_remove_css_class (GTK_WIDGET (self), "empty");

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_EMPTY]);
}

static void
update_window_buttons (GtkTitleButtons *self)
{
  GtkWidget *widget = GTK_WIDGET (self);
  GtkWidget *toplevel;
  GtkWindow *window;
  gchar *layout;
  gchar **tokens;
  gint i;
  gboolean is_sovereign_window;
  gboolean empty = TRUE;

  toplevel = GTK_WIDGET (gtk_widget_get_root (widget));
  if (!GTK_IS_WINDOW (toplevel))
    {
      set_empty (self, TRUE);

      return;
    }

  g_clear_list (&self->controls, (GDestroyNotify) gtk_widget_unparent);

  window = GTK_WINDOW (toplevel);

  is_sovereign_window = !gtk_window_get_modal (window) &&
                         gtk_window_get_transient_for (window) == NULL;

  layout = get_layout (self);

  if (!layout)
    {
      set_empty (self, TRUE);

      return;
    }

  tokens = g_strsplit (layout, ",", -1);

  for (i = 0; tokens[i]; i++)
    {
      GtkWidget *button = NULL;
      GtkWidget *image = NULL;
      AtkObject *accessible;

      if (strcmp (tokens[i], "icon") == 0 &&
          is_sovereign_window)
        {
          button = gtk_image_new ();
          gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
          gtk_widget_add_css_class (button, "titlebutton");
          gtk_widget_add_css_class (button, "icon");

          if (!update_window_icon (window, button))
            {
              g_object_ref_sink (button);
              g_object_unref (button);
              button = NULL;
            }
        }
      else if (strcmp (tokens[i], "minimize") == 0 &&
               is_sovereign_window)
        {
          button = gtk_button_new ();
          gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
          gtk_widget_add_css_class (button, "titlebutton");
          gtk_widget_add_css_class (button, "minimize");
          image = gtk_image_new_from_icon_name ("window-minimize-symbolic");
          g_object_set (image, "use-fallback", TRUE, NULL);
          gtk_container_add (GTK_CONTAINER (button), image);
          gtk_widget_set_can_focus (button, FALSE);
          gtk_actionable_set_action_name (GTK_ACTIONABLE (button),
                                          "window.minimize");

          accessible = gtk_widget_get_accessible (button);
          if (GTK_IS_ACCESSIBLE (accessible))
            atk_object_set_name (accessible, _("Minimize"));
        }
      else if (strcmp (tokens[i], "maximize") == 0 &&
               gtk_window_get_resizable (window) &&
               is_sovereign_window)
        {
          const gchar *icon_name;
          gboolean maximized = gtk_window_is_maximized (window);

          icon_name = maximized ? "window-restore-symbolic" : "window-maximize-symbolic";
          button = gtk_button_new ();
          gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
          gtk_widget_add_css_class (button, "titlebutton");
          gtk_widget_add_css_class (button, "maximize");
          image = gtk_image_new_from_icon_name (icon_name);
          g_object_set (image, "use-fallback", TRUE, NULL);
          gtk_container_add (GTK_CONTAINER (button), image);
          gtk_widget_set_can_focus (button, FALSE);
          gtk_actionable_set_action_name (GTK_ACTIONABLE (button),
                                          "window.toggle-maximized");

          accessible = gtk_widget_get_accessible (button);
          if (GTK_IS_ACCESSIBLE (accessible))
            atk_object_set_name (accessible, maximized ? _("Restore") : _("Maximize"));
        }
      else if (strcmp (tokens[i], "close") == 0 &&
               gtk_window_get_deletable (window))
        {
          button = gtk_button_new ();
          gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
          image = gtk_image_new_from_icon_name ("window-close-symbolic");
          gtk_widget_add_css_class (button, "titlebutton");
          gtk_widget_add_css_class (button, "close");
          g_object_set (image, "use-fallback", TRUE, NULL);
          gtk_container_add (GTK_CONTAINER (button), image);
          gtk_widget_set_can_focus (button, FALSE);
          gtk_actionable_set_action_name (GTK_ACTIONABLE (button),
                                          "window.close");

          accessible = gtk_widget_get_accessible (button);
          if (GTK_IS_ACCESSIBLE (accessible))
            atk_object_set_name (accessible, _("Close"));
        }

      if (button)
        {
          gtk_widget_set_parent (button, widget);
          self->controls = g_list_prepend (self->controls, button);
          empty = FALSE;
        }
    }
  g_strfreev (tokens);

  set_empty (self, empty);
}

static void
surface_state_changed (GtkTitleButtons *self)
{
  GtkNative *native;
  GdkSurface *surface;
  GdkSurfaceState changed, new_state;

  native = gtk_widget_get_native (GTK_WIDGET (self));
  surface = gtk_native_get_surface (native);
  new_state = gdk_toplevel_get_state (GDK_TOPLEVEL (surface));
  changed = new_state ^ self->state;
  self->state = new_state;

  if (changed & (GDK_SURFACE_STATE_FULLSCREEN |
                 GDK_SURFACE_STATE_MAXIMIZED |
                 GDK_SURFACE_STATE_TILED |
                 GDK_SURFACE_STATE_TOP_TILED |
                 GDK_SURFACE_STATE_RIGHT_TILED |
                 GDK_SURFACE_STATE_BOTTOM_TILED |
                 GDK_SURFACE_STATE_LEFT_TILED))
    update_window_buttons (self);
}

static void
window_notify_cb (GtkTitleButtons *self,
                  GParamSpec      *pspec,
                  GtkWindow       *window)
{
  if (pspec->name == I_("deletable") ||
      pspec->name == I_("icon-name") ||
      pspec->name == I_("modal") ||
      pspec->name == I_("resizable") ||
      pspec->name == I_("transient-for"))
    update_window_buttons (self);
}

static void
gtk_title_buttons_realize (GtkWidget *widget)
{
  GtkSettings *settings;
  GtkWidget *root;

  GTK_WIDGET_CLASS (gtk_title_buttons_parent_class)->realize (widget);

  settings = gtk_widget_get_settings (widget);
  g_signal_connect_swapped (settings, "notify::gtk-decoration-layout",
                            G_CALLBACK (update_window_buttons), widget);
  g_signal_connect_swapped (gtk_native_get_surface (gtk_widget_get_native (widget)), "notify::state",
                            G_CALLBACK (surface_state_changed), widget);

  root = GTK_WIDGET (gtk_widget_get_root (widget));

  if (GTK_IS_WINDOW (root))
    g_signal_connect_swapped (root, "notify",
                              G_CALLBACK (window_notify_cb), widget);

  update_window_buttons (GTK_TITLE_BUTTONS (widget));
}

static void
gtk_title_buttons_unrealize (GtkWidget *widget)
{
  GtkSettings *settings;

  settings = gtk_widget_get_settings (widget);

  g_signal_handlers_disconnect_by_func (settings, update_window_buttons, widget);
  g_signal_handlers_disconnect_by_func (gtk_native_get_surface (gtk_widget_get_native (widget)), surface_state_changed, widget);
  g_signal_handlers_disconnect_by_func (gtk_widget_get_root (widget), window_notify_cb, widget);

  GTK_WIDGET_CLASS (gtk_title_buttons_parent_class)->unrealize (widget);
}

static void
gtk_title_buttons_root (GtkWidget *widget)
{
  GTK_WIDGET_CLASS (gtk_title_buttons_parent_class)->root (widget);

  update_window_buttons (GTK_TITLE_BUTTONS (widget));
}

static void
gtk_title_buttons_finalize (GObject *object)
{
  GtkTitleButtons *self = GTK_TITLE_BUTTONS (object);

  g_list_free_full (self->controls, (GDestroyNotify) gtk_widget_unparent);
  g_free (self->decoration_layout);

  G_OBJECT_CLASS (gtk_title_buttons_parent_class)->finalize (object);
}

static void
gtk_title_buttons_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  GtkTitleButtons *self = GTK_TITLE_BUTTONS (object);

  switch (prop_id)
    {
    case PROP_PACK_TYPE:
      g_value_set_enum (value, gtk_title_buttons_get_pack_type (self));
      break;

    case PROP_DECORATION_LAYOUT:
      g_value_set_string (value, gtk_title_buttons_get_decoration_layout (self));
      break;

    case PROP_EMPTY:
      g_value_set_boolean (value, gtk_title_buttons_get_empty (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_title_buttons_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  GtkTitleButtons *self = GTK_TITLE_BUTTONS (object);

  switch (prop_id)
    {
    case PROP_PACK_TYPE:
      gtk_title_buttons_set_pack_type (self, g_value_get_enum (value));
      break;

    case PROP_DECORATION_LAYOUT:
      gtk_title_buttons_set_decoration_layout (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_title_buttons_class_init (GtkTitleButtonsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gtk_title_buttons_finalize;
  object_class->get_property = gtk_title_buttons_get_property;
  object_class->set_property = gtk_title_buttons_set_property;

  widget_class->realize = gtk_title_buttons_realize;
  widget_class->unrealize = gtk_title_buttons_unrealize;
  widget_class->root = gtk_title_buttons_root;

  /**
   * GtkTitleButtons:pack-type:
   *
   * Whether the widget shows start or end portion of the decoration layout.
   *
   * See gtk_title_buttons_set_decoration_layout().
   */
  props[PROP_PACK_TYPE] =
      g_param_spec_enum ("pack-type",
                         P_("Pack type"),
                         P_("Whether the widget shows start or end portion of the decoration layout"),
                         GTK_TYPE_PACK_TYPE,
                         GTK_PACK_START,
                         GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkTitleButtons:decoration-layout:
   *
   * The decoration layout for buttons. If this property is
   * not set, the #GtkSettings:gtk-decoration-layout setting
   * is used.
   *
   * See gtk_title_buttons_set_decoration_layout() for information
   * about the format of this string.
   */
  props[PROP_DECORATION_LAYOUT] =
      g_param_spec_string ("decoration-layout",
                           P_("Decoration Layout"),
                           P_("The layout for window decorations"),
                           NULL,
                           GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkTitleButtons:empty:
   *
   * Whether the widget has any window buttons.
   *
   * This can be used to show a separator near the window buttons etc.
   */
  props[PROP_EMPTY] =
    g_param_spec_boolean ("empty",
                          P_("Empty"),
                          P_("Whether the widget has any widget controls"),
                          TRUE,
                          GTK_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BOX_LAYOUT);
  gtk_widget_class_set_css_name (widget_class, I_("titlebuttons"));
}

static void
gtk_title_buttons_init (GtkTitleButtons *self)
{
  self->decoration_layout = NULL;
  self->state = GDK_SURFACE_STATE_WITHDRAWN;
  self->empty = TRUE;

  gtk_widget_add_css_class (GTK_WIDGET (self), "empty");
}

/**
 * gtk_title_buttons_new:
 * @pack_type: the pack type
 *
 * Creates a new #GtkTitleButtons.
 *
 * Returns: a new #GtkTitleButtons.
 **/
GtkWidget *
gtk_title_buttons_new (GtkPackType pack_type)
{
  return g_object_new (GTK_TYPE_TITLE_BUTTONS,
                       "pack-type", pack_type,
                       NULL);
}

/**
 * gtk_title_buttons_get_pack_type:
 * @self: a #GtkTitleButtons
 *
 * Gets the pack type set with gtk_title_buttons_set_pack_type().
 *
 * Returns: the pack type
 */
GtkPackType
gtk_title_buttons_get_pack_type (GtkTitleButtons *self)
{
  g_return_val_if_fail (GTK_IS_TITLE_BUTTONS (self), GTK_PACK_START);

  return self->pack_type;
}

/**
 * gtk_title_buttons_set_pack_type:
 * @self: a #GtkTitleButtons
 * @pack_type: a pack type
 *
 * Sets the pack type for @self. @GTK_PACK_START means using
 *
 * See gtk_title_buttons_set_decoration_layout()
 */
void
gtk_title_buttons_set_pack_type (GtkTitleButtons *self,
                                 GtkPackType      pack_type)
{
  g_return_if_fail (GTK_IS_TITLE_BUTTONS (self));

  switch (pack_type)
    {
    case GTK_PACK_START:
      gtk_widget_add_css_class (GTK_WIDGET (self), "start");
      gtk_widget_remove_css_class (GTK_WIDGET (self), "end");
      break;

    case GTK_PACK_END:
      gtk_widget_add_css_class (GTK_WIDGET (self), "end");
      gtk_widget_remove_css_class (GTK_WIDGET (self), "start");
      break;

    default:
      g_warning ("Unexpected pack type: %d", pack_type);
      return;
    }

  self->pack_type = pack_type;

  update_window_buttons (self);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_PACK_TYPE]);
}

/**
 * gtk_title_buttons_get_decoration_layout:
 * @self: a #GtkTitleButtons
 *
 * Gets the decoration layout set with
 * gtk_title_buttons_set_decoration_layout().
 *
 * Returns: the decoration layout
 */
const gchar *
gtk_title_buttons_get_decoration_layout (GtkTitleButtons *self)
{
  g_return_val_if_fail (GTK_IS_TITLE_BUTTONS (self), NULL);

  return self->decoration_layout;
}

/**
 * gtk_title_buttons_set_decoration_layout:
 * @self: a #GtkTitlebuttons
 * @layout: (allow-none): a decoration layout, or %NULL to
 *     unset the layout
 *
 * Sets the decoration layout for the title buttons, overriding
 * the #GtkSettings:gtk-decoration-layout setting.
 *
 * The format of the string is button names, separated by commas.
 * A colon separates the buttons that should appear on the left
 * from those on the right. Recognized button names are minimize,
 * maximize, close and icon (the window icon).
 *
 * For example, “icon:minimize,maximize,close” specifies a icon
 * on the left, and minimize, maximize and close buttons on the right.
 *
 * If #GtkTitleButtons:pack-type value is @GTK_PACK_START, @self will
 * display the part before the colon, otherwise after that.
 */
void
gtk_title_buttons_set_decoration_layout (GtkTitleButtons *self,
                                         const gchar     *layout)
{
  g_return_if_fail (GTK_IS_TITLE_BUTTONS (self));

  g_free (self->decoration_layout);
  self->decoration_layout = g_strdup (layout);

  update_window_buttons (self);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_DECORATION_LAYOUT]);
}

/**
 * gtk_title_buttons_get_decoration_layout:
 * @self: a #GtkTitleButtons
 *
 * Gets whether the widget has any window buttons.
 *
 * Returns: %TRUE if the widget has window buttons, otherwise %FALSE
 */
gboolean
gtk_title_buttons_get_empty (GtkTitleButtons *self)
{
  g_return_val_if_fail (GTK_IS_TITLE_BUTTONS (self), FALSE);

  return self->empty;
}
