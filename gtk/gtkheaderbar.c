/*
 * Copyright (c) 2013 Red Hat, Inc.
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
 *
 */

#include "config.h"

#include "gtkheaderbarprivate.h"

#include "gtkbox.h"
#include "gtkbutton.h"
#include "gtkbuildable.h"
#include "gtkcenterlayout.h"
#include "gtkcontainerprivate.h"
#include "gtkcssnodeprivate.h"
#include "gtkimage.h"
#include "gtkintl.h"
#include "gtklabel.h"
#include "gtkmenubutton.h"
#include "gtkprivate.h"
#include "gtkseparator.h"
#include "gtksizerequest.h"
#include "gtktypebuiltins.h"
#include "gtkwidgetprivate.h"
#include "gtkwindowprivate.h"
#include "gtknative.h"
#include "gtkmenubuttonprivate.h"

#include "a11y/gtkcontaineraccessible.h"

#include <string.h>

/**
 * SECTION:gtkheaderbar
 * @Short_description: A box with a centered child
 * @Title: GtkHeaderBar
 * @See_also: #GtkBox, #GtkActionBar
 *
 * GtkHeaderBar is similar to a horizontal #GtkBox. It allows children to 
 * be placed at the start or the end. In addition, it allows a title and
 * subtitle to be displayed. The title will be centered with respect to
 * the width of the box, even if the children at either side take up
 * different amounts of space. The height of the titlebar will be
 * set to provide sufficient space for the subtitle, even if none is
 * currently set. If a subtitle is not needed, the space reservation
 * can be turned off with gtk_header_bar_set_has_subtitle().
 *
 * GtkHeaderBar can add typical window frame controls, such as minimize,
 * maximize and close buttons, or the window icon.
 *
 * For these reasons, GtkHeaderBar is the natural choice for use as the custom
 * titlebar widget of a #GtkWindow (see gtk_window_set_titlebar()), as it gives
 * features typical of titlebars while allowing the addition of child widgets.
 *
 * # CSS nodes
 *
 * |[<!-- language="plain" -->
 * headerbar
 * ├── box.start
 * │   ╰── box
 * │       ├── [image.titlebutton.icon]
 * │       ├── [menubutton.titlebutton.appmenu]
 * │       ├── [button.titlebutton.minimize]
 * │       ├── [button.titlebutton.maximize]
 * │       ╰── [button.titlebutton.close]
 * ├── [Custom Title]
 * ╰── box.end
 * ]|
 *
 * A #GtkHeaderBar's CSS node is called headerbar. It contains two box subnodes at the start
 * and end of the headerbar, as well as a center node that represents the title.
 *
 * The titlebuttons get their own box subnode, either in the start box or in the end box.
 * Which of the title buttons exist and where they are placed exactly depends on the
 * desktop environment.
 */

#define MIN_TITLE_CHARS 5

typedef struct _GtkHeaderBarPrivate       GtkHeaderBarPrivate;
typedef struct _GtkHeaderBarClass         GtkHeaderBarClass;

struct _GtkHeaderBar
{
  GtkContainer container;
};

struct _GtkHeaderBarClass
{
  GtkContainerClass parent_class;
};

struct _GtkHeaderBarPrivate
{
  GtkWidget *start_box;
  GtkWidget *end_box;

  gchar *title;
  gchar *subtitle;
  GtkWidget *title_label;
  GtkWidget *subtitle_label;
  GtkWidget *label_box;
  GtkWidget *label_sizing_box;
  GtkWidget *subtitle_sizing_label;
  GtkWidget *custom_title;
  gboolean has_subtitle;

  gboolean show_title_buttons;
  gchar *decoration_layout;
  gboolean decoration_layout_set;
  gboolean track_default_decoration;

  GtkWidget *titlebar_start_box;
  GtkWidget *titlebar_end_box;

  GtkWidget *titlebar_start_separator;
  GtkWidget *titlebar_end_separator;

  GtkWidget *titlebar_icon;

  GdkSurfaceState state;

  guint shows_app_menu : 1;
};

enum {
  PROP_0,
  PROP_TITLE,
  PROP_SUBTITLE,
  PROP_HAS_SUBTITLE,
  PROP_CUSTOM_TITLE,
  PROP_SHOW_TITLE_BUTTONS,
  PROP_DECORATION_LAYOUT,
  PROP_DECORATION_LAYOUT_SET,
  LAST_PROP
};

static GParamSpec *header_bar_props[LAST_PROP] = { NULL, };

static void gtk_header_bar_buildable_init (GtkBuildableIface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkHeaderBar, gtk_header_bar, GTK_TYPE_CONTAINER,
                         G_ADD_PRIVATE (GtkHeaderBar)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                                gtk_header_bar_buildable_init));

static void
init_sizing_box (GtkHeaderBar *bar)
{
  GtkHeaderBarPrivate *priv = gtk_header_bar_get_instance_private (bar);
  GtkWidget *w;
  GtkStyleContext *context;

  /* We use this box to always request size for the two labels (title
   * and subtitle) as if they were always visible, but then allocate
   * the real label box with its actual size, to keep it center-aligned
   * in case we have only the title.
   */
  w = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  priv->label_sizing_box = g_object_ref_sink (w);

  w = gtk_label_new (NULL);
  context = gtk_widget_get_style_context (w);
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_TITLE);
  gtk_container_add (GTK_CONTAINER (priv->label_sizing_box), w);
  gtk_label_set_wrap (GTK_LABEL (w), FALSE);
  gtk_label_set_single_line_mode (GTK_LABEL (w), TRUE);
  gtk_label_set_ellipsize (GTK_LABEL (w), PANGO_ELLIPSIZE_END);
  gtk_label_set_width_chars (GTK_LABEL (w), MIN_TITLE_CHARS);

  w = gtk_label_new (NULL);
  context = gtk_widget_get_style_context (w);
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_SUBTITLE);
  gtk_container_add (GTK_CONTAINER (priv->label_sizing_box), w);
  gtk_label_set_wrap (GTK_LABEL (w), FALSE);
  gtk_label_set_single_line_mode (GTK_LABEL (w), TRUE);
  gtk_label_set_ellipsize (GTK_LABEL (w), PANGO_ELLIPSIZE_END);
  gtk_widget_set_visible (w, priv->has_subtitle || (priv->subtitle && priv->subtitle[0]));
  priv->subtitle_sizing_label = w;
}

static GtkWidget *
create_title_box (const char *title,
                  const char *subtitle,
                  GtkWidget **ret_title_label,
                  GtkWidget **ret_subtitle_label)
{
  GtkWidget *label_box;
  GtkWidget *title_label;
  GtkWidget *subtitle_label;
  GtkStyleContext *context;

  label_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_set_valign (label_box, GTK_ALIGN_CENTER);

  title_label = gtk_label_new (title);
  context = gtk_widget_get_style_context (title_label);
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_TITLE);
  gtk_label_set_wrap (GTK_LABEL (title_label), FALSE);
  gtk_label_set_single_line_mode (GTK_LABEL (title_label), TRUE);
  gtk_label_set_ellipsize (GTK_LABEL (title_label), PANGO_ELLIPSIZE_END);
  gtk_container_add (GTK_CONTAINER (label_box), title_label);
  gtk_label_set_width_chars (GTK_LABEL (title_label), MIN_TITLE_CHARS);

  subtitle_label = gtk_label_new (subtitle);
  context = gtk_widget_get_style_context (subtitle_label);
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_SUBTITLE);
  gtk_label_set_wrap (GTK_LABEL (subtitle_label), FALSE);
  gtk_label_set_single_line_mode (GTK_LABEL (subtitle_label), TRUE);
  gtk_label_set_ellipsize (GTK_LABEL (subtitle_label), PANGO_ELLIPSIZE_END);
  gtk_container_add (GTK_CONTAINER (label_box), subtitle_label);
  gtk_widget_set_visible (subtitle_label, subtitle && subtitle[0]);

  if (ret_title_label)
    *ret_title_label = title_label;
  if (ret_subtitle_label)
    *ret_subtitle_label = subtitle_label;

  return label_box;
}

gboolean
_gtk_header_bar_update_window_icon (GtkHeaderBar *bar,
                                    GtkWindow    *window)
{
  GtkHeaderBarPrivate *priv = gtk_header_bar_get_instance_private (bar);
  GdkPaintable *paintable;
  gint scale;

  if (priv->titlebar_icon == NULL)
    return FALSE;

  scale = gtk_widget_get_scale_factor (priv->titlebar_icon);
  if (GTK_IS_BUTTON (gtk_widget_get_parent (priv->titlebar_icon)))
    paintable = gtk_window_get_icon_for_size (window, 16 * scale);
  else
    paintable = gtk_window_get_icon_for_size (window, 20 * scale);

  if (paintable)
    {
      gtk_image_set_from_paintable (GTK_IMAGE (priv->titlebar_icon), paintable);
      g_object_unref (paintable);
      gtk_widget_show (priv->titlebar_icon);

      return TRUE;
    }

  return FALSE;
}

static void
_gtk_header_bar_update_separator_visibility (GtkHeaderBar *bar)
{
  GtkHeaderBarPrivate *priv = gtk_header_bar_get_instance_private (bar);
  gboolean have_visible_at_start = FALSE;
  gboolean have_visible_at_end = FALSE;
  GList *l;
  GList *children;

  children = gtk_container_get_children (GTK_CONTAINER (priv->start_box));
  for (l = children; l; l = l->next)
    {
      if (l->data != priv->titlebar_start_box && gtk_widget_get_visible (l->data))
        have_visible_at_start = TRUE;
    }
  g_list_free (children);

  children = gtk_container_get_children (GTK_CONTAINER (priv->end_box));
  for (l = children; l; l = l->next)
    {
      if (l->data != priv->titlebar_end_box && gtk_widget_get_visible (l->data))
        have_visible_at_end = TRUE;
    }
  g_list_free (children);

  if (priv->titlebar_start_separator != NULL)
    gtk_widget_set_visible (priv->titlebar_start_separator, have_visible_at_start);

  if (priv->titlebar_end_separator != NULL)
    gtk_widget_set_visible (priv->titlebar_end_separator, have_visible_at_end);
}

void
_gtk_header_bar_update_window_buttons (GtkHeaderBar *bar)
{
  GtkHeaderBarPrivate *priv = gtk_header_bar_get_instance_private (bar);
  GtkWidget *widget = GTK_WIDGET (bar);
  GtkWidget *toplevel;
  GtkWindow *window;
  gchar *layout_desc;
  gchar **tokens, **t;
  gint i, j;
  GMenuModel *menu;
  gboolean shown_by_shell;
  gboolean is_sovereign_window;

  if (!gtk_widget_get_realized (widget))
    return;

  toplevel = GTK_WIDGET (gtk_widget_get_root (widget));
  if (!GTK_IS_WINDOW (toplevel))
    return;

  if (priv->titlebar_start_box)
    {
      gtk_widget_unparent (priv->titlebar_start_box);
      priv->titlebar_start_box = NULL;
      priv->titlebar_start_separator = NULL;
    }
  if (priv->titlebar_end_box)
    {
      gtk_widget_unparent (priv->titlebar_end_box);
      priv->titlebar_end_box = NULL;
      priv->titlebar_end_separator = NULL;
    }

  priv->titlebar_icon = NULL;
  priv->shows_app_menu = FALSE;

  if (!priv->show_title_buttons)
    return;

  g_object_get (gtk_widget_get_settings (widget),
                "gtk-shell-shows-app-menu", &shown_by_shell,
                "gtk-decoration-layout", &layout_desc,
                NULL);

  if (priv->decoration_layout_set)
    {
      g_free (layout_desc);
      layout_desc = g_strdup (priv->decoration_layout);
    }

  window = GTK_WINDOW (toplevel);

  if (!shown_by_shell && gtk_window_get_application (window))
    menu = gtk_application_get_app_menu (gtk_window_get_application (window));
  else
    menu = NULL;

  is_sovereign_window = (!gtk_window_get_modal (window) &&
                          gtk_window_get_transient_for (window) == NULL &&
                          gtk_window_get_type_hint (window) == GDK_SURFACE_TYPE_HINT_NORMAL);

  tokens = g_strsplit (layout_desc, ":", 2);
  if (tokens)
    {
      for (i = 0; i < 2; i++)
        {
          GtkWidget *box;
          GtkWidget *separator;
          int n_children = 0;

          if (tokens[i] == NULL)
            break;

          t = g_strsplit (tokens[i], ",", -1);

          separator = gtk_separator_new (GTK_ORIENTATION_VERTICAL);
          gtk_style_context_add_class (gtk_widget_get_style_context (separator), "titlebutton");

          box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

          for (j = 0; t[j]; j++)
            {
              GtkWidget *button = NULL;
              GtkWidget *image = NULL;
              AtkObject *accessible;

              if (strcmp (t[j], "icon") == 0 &&
                  is_sovereign_window)
                {
                  button = gtk_image_new ();
                  gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
                  priv->titlebar_icon = button;
                  gtk_style_context_add_class (gtk_widget_get_style_context (button), "titlebutton");
                  gtk_style_context_add_class (gtk_widget_get_style_context (button), "icon");
                  gtk_widget_set_size_request (button, 20, 20);

                  if (!_gtk_header_bar_update_window_icon (bar, window))
                    {
                      gtk_widget_destroy (button);
                      priv->titlebar_icon = NULL;
                      button = NULL;
                    }
                }
              else if (strcmp (t[j], "menu") == 0 &&
                       menu != NULL &&
                       is_sovereign_window)
                {
                  button = gtk_menu_button_new ();
                  gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
                  gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (button), menu);
                  gtk_menu_button_set_use_popover (GTK_MENU_BUTTON (button), TRUE);
                  gtk_style_context_add_class (gtk_widget_get_style_context (button), "titlebutton");
                  gtk_style_context_add_class (gtk_widget_get_style_context (button), "appmenu");
                  image = gtk_image_new ();
                  gtk_menu_button_add_child (GTK_MENU_BUTTON (button), image);
                  gtk_widget_set_can_focus (button, FALSE);

                  accessible = gtk_widget_get_accessible (button);
                  if (GTK_IS_ACCESSIBLE (accessible))
                    atk_object_set_name (accessible, _("Application menu"));

                  priv->titlebar_icon = image;
                  if (!_gtk_header_bar_update_window_icon (bar, window))
                    gtk_image_set_from_icon_name (GTK_IMAGE (priv->titlebar_icon), "application-x-executable-symbolic");

                  priv->shows_app_menu = TRUE;
                }
              else if (strcmp (t[j], "minimize") == 0 &&
                       is_sovereign_window)
                {
                  button = gtk_button_new ();
                  gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
                  gtk_style_context_add_class (gtk_widget_get_style_context (button), "titlebutton");
                  gtk_style_context_add_class (gtk_widget_get_style_context (button), "minimize");
                  image = gtk_image_new_from_icon_name ("window-minimize-symbolic");
                  g_object_set (image, "use-fallback", TRUE, NULL);
                  gtk_container_add (GTK_CONTAINER (button), image);
                  gtk_widget_set_can_focus (button, FALSE);
                  g_signal_connect_swapped (button, "clicked",
                                            G_CALLBACK (gtk_window_iconify), window);

                  accessible = gtk_widget_get_accessible (button);
                  if (GTK_IS_ACCESSIBLE (accessible))
                    atk_object_set_name (accessible, _("Minimize"));
                }
              else if (strcmp (t[j], "maximize") == 0 &&
                       gtk_window_get_resizable (window) &&
                       is_sovereign_window)
                {
                  const gchar *icon_name;
                  gboolean maximized = gtk_window_is_maximized (window);

                  icon_name = maximized ? "window-restore-symbolic" : "window-maximize-symbolic";
                  button = gtk_button_new ();
                  gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
                  gtk_style_context_add_class (gtk_widget_get_style_context (button), "titlebutton");
                  gtk_style_context_add_class (gtk_widget_get_style_context (button), "maximize");
                  image = gtk_image_new_from_icon_name (icon_name);
                  g_object_set (image, "use-fallback", TRUE, NULL);
                  gtk_container_add (GTK_CONTAINER (button), image);
                  gtk_widget_set_can_focus (button, FALSE);
                  g_signal_connect_swapped (button, "clicked",
                                            G_CALLBACK (_gtk_window_toggle_maximized), window);

                  accessible = gtk_widget_get_accessible (button);
                  if (GTK_IS_ACCESSIBLE (accessible))
                    atk_object_set_name (accessible, maximized ? _("Restore") : _("Maximize"));
                }
              else if (strcmp (t[j], "close") == 0 &&
                       gtk_window_get_deletable (window))
                {
                  button = gtk_button_new ();
                  gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
                  image = gtk_image_new_from_icon_name ("window-close-symbolic");
                  gtk_style_context_add_class (gtk_widget_get_style_context (button), "titlebutton");
                  gtk_style_context_add_class (gtk_widget_get_style_context (button), "close");
                  g_object_set (image, "use-fallback", TRUE, NULL);
                  gtk_container_add (GTK_CONTAINER (button), image);
                  gtk_widget_set_can_focus (button, FALSE);
                  g_signal_connect_swapped (button, "clicked",
                                            G_CALLBACK (gtk_window_close), window);

                  accessible = gtk_widget_get_accessible (button);
                  if (GTK_IS_ACCESSIBLE (accessible))
                    atk_object_set_name (accessible, _("Close"));
                }

              if (button)
                {
                  gtk_container_add (GTK_CONTAINER (box), button);
                  n_children ++;
                }
            }
          g_strfreev (t);

          if (n_children == 0)
            {
              g_object_ref_sink (box);
              g_object_unref (box);
              g_object_ref_sink (separator);
              g_object_unref (separator);
              continue;
            }

          gtk_container_add (GTK_CONTAINER (box), separator);
          if (i == 1)
            gtk_box_reorder_child_after (GTK_BOX (box), separator, NULL);

          if (i == 0)
            gtk_style_context_add_class (gtk_widget_get_style_context (box), GTK_STYLE_CLASS_LEFT);
          else
            gtk_style_context_add_class (gtk_widget_get_style_context (box), GTK_STYLE_CLASS_RIGHT);

          if (i == 0)
            {
              priv->titlebar_start_box = box;
              priv->titlebar_start_separator = separator;
              gtk_container_add (GTK_CONTAINER (priv->start_box), box);
            }
          else
            {
              priv->titlebar_end_box = box;
              priv->titlebar_end_separator = separator;
              gtk_container_add (GTK_CONTAINER (priv->end_box), box);
            }
        }
      g_strfreev (tokens);
    }
  g_free (layout_desc);

  _gtk_header_bar_update_separator_visibility (bar);
}

gboolean
_gtk_header_bar_shows_app_menu (GtkHeaderBar *bar)
{
  GtkHeaderBarPrivate *priv = gtk_header_bar_get_instance_private (bar);

  return priv->show_title_buttons && priv->shows_app_menu;
}

static void
update_default_decoration (GtkHeaderBar *bar)
{
  GtkHeaderBarPrivate *priv = gtk_header_bar_get_instance_private (bar);
  GtkLayoutManager *layout = gtk_widget_get_layout_manager (GTK_WIDGET (bar));
  GtkStyleContext *context;
  gboolean have_children = FALSE;

  /* Check whether we have any child widgets that we didn't add ourselves */
  if (gtk_center_layout_get_center_widget (GTK_CENTER_LAYOUT (layout)) != NULL)
    {
      have_children = TRUE;
    }
  else
    {
      GtkWidget *w;

      for (w = _gtk_widget_get_first_child (priv->start_box);
           w != NULL;
           w = _gtk_widget_get_next_sibling (w))
        {
          if (w != priv->titlebar_start_box)
            {
              have_children = TRUE;
              break;
            }
        }

      if (!have_children)
        for (w = _gtk_widget_get_first_child (priv->end_box);
             w != NULL;
             w = _gtk_widget_get_next_sibling (w))
          {
            if (w != priv->titlebar_end_box)
              {
                have_children = TRUE;
                break;
              }
          }
    }

  context = gtk_widget_get_style_context (GTK_WIDGET (bar));

  if (have_children || priv->custom_title != NULL)
    gtk_style_context_remove_class (context, "default-decoration");
  else
    gtk_style_context_add_class (context, "default-decoration");
}

void
_gtk_header_bar_track_default_decoration (GtkHeaderBar *bar)
{
  GtkHeaderBarPrivate *priv = gtk_header_bar_get_instance_private (bar);

  priv->track_default_decoration = TRUE;

  update_default_decoration (bar);
}

static void
construct_label_box (GtkHeaderBar *bar)
{
  GtkHeaderBarPrivate *priv = gtk_header_bar_get_instance_private (bar);
  GtkLayoutManager *layout = gtk_widget_get_layout_manager (GTK_WIDGET (bar));

  g_assert (priv->label_box == NULL);

  priv->label_box = create_title_box (priv->title,
                                      priv->subtitle,
                                      &priv->title_label,
                                      &priv->subtitle_label);
  gtk_widget_insert_after (priv->label_box, GTK_WIDGET (bar), priv->start_box);
  gtk_center_layout_set_center_widget (GTK_CENTER_LAYOUT (layout), priv->label_box);
}

/**
 * gtk_header_bar_set_title:
 * @bar: a #GtkHeaderBar
 * @title: (allow-none): a title, or %NULL
 *
 * Sets the title of the #GtkHeaderBar. The title should help a user
 * identify the current view. A good title should not include the
 * application name.
 */
void
gtk_header_bar_set_title (GtkHeaderBar *bar,
                          const gchar  *title)
{
  GtkHeaderBarPrivate *priv = gtk_header_bar_get_instance_private (bar);
  gchar *new_title;

  g_return_if_fail (GTK_IS_HEADER_BAR (bar));

  new_title = g_strdup (title);
  g_free (priv->title);
  priv->title = new_title;

  if (priv->title_label != NULL)
    {
      gtk_label_set_label (GTK_LABEL (priv->title_label), priv->title);
      gtk_widget_queue_resize (GTK_WIDGET (bar));
    }

  g_object_notify_by_pspec (G_OBJECT (bar), header_bar_props[PROP_TITLE]);
}

/**
 * gtk_header_bar_get_title:
 * @bar: a #GtkHeaderBar
 *
 * Retrieves the title of the header. See gtk_header_bar_set_title().
 *
 * Returns: (nullable): the title of the header, or %NULL if none has
 *    been set explicitly. The returned string is owned by the widget
 *    and must not be modified or freed.
 */
const gchar *
gtk_header_bar_get_title (GtkHeaderBar *bar)
{
  GtkHeaderBarPrivate *priv = gtk_header_bar_get_instance_private (bar);

  g_return_val_if_fail (GTK_IS_HEADER_BAR (bar), NULL);

  return priv->title;
}

/**
 * gtk_header_bar_set_subtitle:
 * @bar: a #GtkHeaderBar
 * @subtitle: (allow-none): a subtitle, or %NULL
 *
 * Sets the subtitle of the #GtkHeaderBar. The title should give a user
 * an additional detail to help him identify the current view.
 *
 * Note that GtkHeaderBar by default reserves room for the subtitle,
 * even if none is currently set. If this is not desired, set the
 * #GtkHeaderBar:has-subtitle property to %FALSE.
 */
void
gtk_header_bar_set_subtitle (GtkHeaderBar *bar,
                             const gchar  *subtitle)
{
  GtkHeaderBarPrivate *priv = gtk_header_bar_get_instance_private (bar);
  gchar *new_subtitle;

  g_return_if_fail (GTK_IS_HEADER_BAR (bar));

  new_subtitle = g_strdup (subtitle);
  g_free (priv->subtitle);
  priv->subtitle = new_subtitle;

  if (priv->subtitle_label != NULL)
    {
      gtk_label_set_label (GTK_LABEL (priv->subtitle_label), priv->subtitle);
      gtk_widget_set_visible (priv->subtitle_label, priv->subtitle && priv->subtitle[0]);
      gtk_widget_queue_resize (GTK_WIDGET (bar));
    }

  gtk_widget_set_visible (priv->subtitle_sizing_label, priv->has_subtitle || (priv->subtitle && priv->subtitle[0]));

  g_object_notify_by_pspec (G_OBJECT (bar), header_bar_props[PROP_SUBTITLE]);
}

/**
 * gtk_header_bar_get_subtitle:
 * @bar: a #GtkHeaderBar
 *
 * Retrieves the subtitle of the header. See gtk_header_bar_set_subtitle().
 *
 * Returns: (nullable): the subtitle of the header, or %NULL if none has
 *    been set explicitly. The returned string is owned by the widget
 *    and must not be modified or freed.
 */
const gchar *
gtk_header_bar_get_subtitle (GtkHeaderBar *bar)
{
  GtkHeaderBarPrivate *priv = gtk_header_bar_get_instance_private (bar);

  g_return_val_if_fail (GTK_IS_HEADER_BAR (bar), NULL);

  return priv->subtitle;
}

/**
 * gtk_header_bar_set_custom_title:
 * @bar: a #GtkHeaderBar
 * @title_widget: (allow-none): a custom widget to use for a title
 *
 * Sets a custom title for the #GtkHeaderBar.
 *
 * The title should help a user identify the current view. This
 * supersedes any title set by gtk_header_bar_set_title() or
 * gtk_header_bar_set_subtitle(). To achieve the same style as
 * the builtin title and subtitle, use the “title” and “subtitle”
 * style classes.
 *
 * You should set the custom title to %NULL, for the header title
 * label to be visible again.
 */
void
gtk_header_bar_set_custom_title (GtkHeaderBar *bar,
                                 GtkWidget    *title_widget)
{
  GtkHeaderBarPrivate *priv = gtk_header_bar_get_instance_private (bar);

  g_return_if_fail (GTK_IS_HEADER_BAR (bar));
  if (title_widget)
    g_return_if_fail (GTK_IS_WIDGET (title_widget));

  /* No need to do anything if the custom widget stays the same */
  if (priv->custom_title == title_widget)
    return;

  if (priv->custom_title)
    {
      GtkWidget *custom = priv->custom_title;

      priv->custom_title = NULL;
      gtk_widget_unparent (custom);
    }

  if (title_widget != NULL)
    {
      GtkLayoutManager *layout = gtk_widget_get_layout_manager (GTK_WIDGET (bar));
      priv->custom_title = title_widget;

      gtk_widget_insert_after (priv->custom_title, GTK_WIDGET (bar), priv->start_box);
      gtk_center_layout_set_center_widget (GTK_CENTER_LAYOUT (layout), title_widget);

      if (priv->label_box != NULL)
        {
          GtkWidget *label_box = priv->label_box;

          priv->label_box = NULL;
          priv->title_label = NULL;
          priv->subtitle_label = NULL;
          gtk_widget_unparent (label_box);
        }

    }
  else
    {
      if (priv->label_box == NULL)
        construct_label_box (bar);
    }

  g_object_notify_by_pspec (G_OBJECT (bar), header_bar_props[PROP_CUSTOM_TITLE]);
}

/**
 * gtk_header_bar_get_custom_title:
 * @bar: a #GtkHeaderBar
 *
 * Retrieves the custom title widget of the header. See
 * gtk_header_bar_set_custom_title().
 *
 * Returns: (nullable) (transfer none): the custom title widget
 *    of the header, or %NULL if none has been set explicitly.
 */
GtkWidget *
gtk_header_bar_get_custom_title (GtkHeaderBar *bar)
{
  GtkHeaderBarPrivate *priv = gtk_header_bar_get_instance_private (bar);

  g_return_val_if_fail (GTK_IS_HEADER_BAR (bar), NULL);

  return priv->custom_title;
}

static void
gtk_header_bar_dispose (GObject *object)
{
  GtkHeaderBarPrivate *priv = gtk_header_bar_get_instance_private (GTK_HEADER_BAR (object));

  if (priv->label_sizing_box)
    {
      gtk_widget_destroy (priv->label_sizing_box);
      g_clear_object (&priv->label_sizing_box);
    }

  g_clear_pointer (&priv->custom_title, gtk_widget_unparent);
  g_clear_pointer (&priv->label_box, gtk_widget_unparent);
  g_clear_pointer (&priv->start_box, gtk_widget_unparent);
  g_clear_pointer (&priv->end_box, gtk_widget_unparent);

  G_OBJECT_CLASS (gtk_header_bar_parent_class)->dispose (object);
}

static void
gtk_header_bar_finalize (GObject *object)
{
  GtkHeaderBarPrivate *priv = gtk_header_bar_get_instance_private (GTK_HEADER_BAR (object));

  g_free (priv->title);
  g_free (priv->subtitle);
  g_free (priv->decoration_layout);

  G_OBJECT_CLASS (gtk_header_bar_parent_class)->finalize (object);
}

static void
gtk_header_bar_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  GtkHeaderBar *bar = GTK_HEADER_BAR (object);
  GtkHeaderBarPrivate *priv = gtk_header_bar_get_instance_private (bar);

  switch (prop_id)
    {
    case PROP_TITLE:
      g_value_set_string (value, priv->title);
      break;

    case PROP_SUBTITLE:
      g_value_set_string (value, priv->subtitle);
      break;

    case PROP_CUSTOM_TITLE:
      g_value_set_object (value, priv->custom_title);
      break;

    case PROP_SHOW_TITLE_BUTTONS:
      g_value_set_boolean (value, gtk_header_bar_get_show_title_buttons (bar));
      break;

    case PROP_HAS_SUBTITLE:
      g_value_set_boolean (value, gtk_header_bar_get_has_subtitle (bar));
      break;

    case PROP_DECORATION_LAYOUT:
      g_value_set_string (value, gtk_header_bar_get_decoration_layout (bar));
      break;

    case PROP_DECORATION_LAYOUT_SET:
      g_value_set_boolean (value, priv->decoration_layout_set);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_header_bar_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  GtkHeaderBar *bar = GTK_HEADER_BAR (object);
  GtkHeaderBarPrivate *priv = gtk_header_bar_get_instance_private (bar);

  switch (prop_id)
    {
    case PROP_TITLE:
      gtk_header_bar_set_title (bar, g_value_get_string (value));
      break;

    case PROP_SUBTITLE:
      gtk_header_bar_set_subtitle (bar, g_value_get_string (value));
      break;

    case PROP_CUSTOM_TITLE:
      gtk_header_bar_set_custom_title (bar, g_value_get_object (value));
      break;

    case PROP_SHOW_TITLE_BUTTONS:
      gtk_header_bar_set_show_title_buttons (bar, g_value_get_boolean (value));
      break;

    case PROP_HAS_SUBTITLE:
      gtk_header_bar_set_has_subtitle (bar, g_value_get_boolean (value));
      break;

    case PROP_DECORATION_LAYOUT:
      gtk_header_bar_set_decoration_layout (bar, g_value_get_string (value));
      break;

    case PROP_DECORATION_LAYOUT_SET:
      priv->decoration_layout_set = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
notify_child_cb (GObject      *child,
                 GParamSpec   *pspec,
                 GtkHeaderBar *bar)
{
  _gtk_header_bar_update_separator_visibility (bar);
}

static void
gtk_header_bar_pack (GtkHeaderBar *bar,
                     GtkWidget    *widget,
                     GtkPackType   pack_type)
{
  GtkHeaderBarPrivate *priv = gtk_header_bar_get_instance_private (bar);

  g_return_if_fail (gtk_widget_get_parent (widget) == NULL);

  if (pack_type == GTK_PACK_START)
    gtk_container_add (GTK_CONTAINER (priv->start_box), widget);
  else if (pack_type == GTK_PACK_END)
    gtk_container_add (GTK_CONTAINER (priv->end_box), widget);

  g_signal_connect (widget, "notify::visible", G_CALLBACK (notify_child_cb), bar);
  _gtk_header_bar_update_separator_visibility (bar);

  if (priv->track_default_decoration)
    update_default_decoration (bar);
}

static void
gtk_header_bar_add (GtkContainer *container,
                    GtkWidget    *child)
{
  gtk_header_bar_pack (GTK_HEADER_BAR (container), child, GTK_PACK_START);
}

static void
gtk_header_bar_remove (GtkContainer *container,
                       GtkWidget    *widget)
{
  GtkHeaderBar *bar = GTK_HEADER_BAR (container);
  GtkHeaderBarPrivate *priv = gtk_header_bar_get_instance_private (bar);
  GtkLayoutManager *layout = gtk_widget_get_layout_manager (GTK_WIDGET (bar));
  GtkWidget *parent;
  gboolean removed = FALSE;

  parent = gtk_widget_get_parent (widget);

  if (parent == priv->start_box)
    {
      gtk_container_remove (GTK_CONTAINER (priv->start_box), widget);
      removed = TRUE;
    }
  else if (parent == priv->end_box)
    {
      gtk_container_remove (GTK_CONTAINER (priv->end_box), widget);
      removed = TRUE;
    }
  else if (parent == GTK_WIDGET (container) &&
           gtk_center_layout_get_center_widget (GTK_CENTER_LAYOUT (layout)) == widget)
    {
      gtk_widget_unparent (widget);
      removed = TRUE;
    }

  if (removed)
    {
      _gtk_header_bar_update_separator_visibility (bar);

      if (priv->track_default_decoration)
        update_default_decoration (bar);
    }
}

static void
gtk_header_bar_forall (GtkContainer *container,
                       GtkCallback   callback,
                       gpointer      callback_data)
{
  GtkHeaderBar *bar = GTK_HEADER_BAR (container);
  GtkHeaderBarPrivate *priv = gtk_header_bar_get_instance_private (bar);
  GtkWidget *w;

  if (priv->start_box)
    {
      w = _gtk_widget_get_first_child (priv->start_box);
      while (w != NULL)
        {
          GtkWidget *next = _gtk_widget_get_next_sibling (w);

          if (w != priv->titlebar_start_box)
            (* callback) (w, callback_data);

          w = next;
        }
    }

  if (priv->custom_title != NULL)
    (* callback) (priv->custom_title, callback_data);

  if (priv->end_box)
    {
      w = _gtk_widget_get_first_child (priv->end_box);
      while (w != NULL)
        {
          GtkWidget *next = _gtk_widget_get_next_sibling (w);

          if (w != priv->titlebar_end_box)
            (* callback) (w, callback_data);

          w = next;
        }
    }
}

static GType
gtk_header_bar_child_type (GtkContainer *container)
{
  return GTK_TYPE_WIDGET;
}

static void surface_state_changed (GtkWidget *widget);

static void
gtk_header_bar_realize (GtkWidget *widget)
{
  GtkSettings *settings;

  GTK_WIDGET_CLASS (gtk_header_bar_parent_class)->realize (widget);

  settings = gtk_widget_get_settings (widget);
  g_signal_connect_swapped (settings, "notify::gtk-shell-shows-app-menu",
                            G_CALLBACK (_gtk_header_bar_update_window_buttons), widget);
  g_signal_connect_swapped (settings, "notify::gtk-decoration-layout",
                            G_CALLBACK (_gtk_header_bar_update_window_buttons), widget);
  g_signal_connect_swapped (gtk_native_get_surface (gtk_widget_get_native (widget)), "notify::state",
                            G_CALLBACK (surface_state_changed), widget);
  _gtk_header_bar_update_window_buttons (GTK_HEADER_BAR (widget));
}

static void
gtk_header_bar_unrealize (GtkWidget *widget)
{
  GtkSettings *settings;

  settings = gtk_widget_get_settings (widget);

  g_signal_handlers_disconnect_by_func (settings, _gtk_header_bar_update_window_buttons, widget);
  g_signal_handlers_disconnect_by_func (gtk_native_get_surface (gtk_widget_get_native (widget)), surface_state_changed, widget);

  GTK_WIDGET_CLASS (gtk_header_bar_parent_class)->unrealize (widget);
}

static void
surface_state_changed (GtkWidget *widget)
{
  GtkHeaderBar *bar = GTK_HEADER_BAR (widget);
  GtkHeaderBarPrivate *priv = gtk_header_bar_get_instance_private (bar);
  GdkSurfaceState changed, new_state;

  new_state = gdk_surface_get_state (gtk_native_get_surface (gtk_widget_get_native (widget)));
  changed = new_state ^ priv->state;
  priv->state = new_state;

  if (changed & (GDK_SURFACE_STATE_FULLSCREEN |
                 GDK_SURFACE_STATE_MAXIMIZED |
                 GDK_SURFACE_STATE_TILED |
                 GDK_SURFACE_STATE_TOP_TILED |
                 GDK_SURFACE_STATE_RIGHT_TILED |
                 GDK_SURFACE_STATE_BOTTOM_TILED |
                 GDK_SURFACE_STATE_LEFT_TILED))
    _gtk_header_bar_update_window_buttons (bar);
}

static void
gtk_header_bar_root (GtkWidget *widget)
{
  GtkHeaderBar *bar = GTK_HEADER_BAR (widget);

  GTK_WIDGET_CLASS (gtk_header_bar_parent_class)->root (widget);

  _gtk_header_bar_update_window_buttons (bar);
}

static void
gtk_header_bar_class_init (GtkHeaderBarClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (class);

  object_class->dispose = gtk_header_bar_dispose;
  object_class->finalize = gtk_header_bar_finalize;
  object_class->get_property = gtk_header_bar_get_property;
  object_class->set_property = gtk_header_bar_set_property;

  widget_class->realize = gtk_header_bar_realize;
  widget_class->unrealize = gtk_header_bar_unrealize;
  widget_class->root = gtk_header_bar_root;

  container_class->add = gtk_header_bar_add;
  container_class->remove = gtk_header_bar_remove;
  container_class->forall = gtk_header_bar_forall;
  container_class->child_type = gtk_header_bar_child_type;

  header_bar_props[PROP_TITLE] =
      g_param_spec_string ("title",
                           P_("Title"),
                           P_("The title to display"),
                           NULL,
                           G_PARAM_READWRITE);

  header_bar_props[PROP_SUBTITLE] =
      g_param_spec_string ("subtitle",
                           P_("Subtitle"),
                           P_("The subtitle to display"),
                           NULL,
                           G_PARAM_READWRITE);

  header_bar_props[PROP_CUSTOM_TITLE] =
      g_param_spec_object ("custom-title",
                           P_("Custom Title"),
                           P_("Custom title widget to display"),
                           GTK_TYPE_WIDGET,
                           G_PARAM_READWRITE|G_PARAM_STATIC_STRINGS);

  /**
   * GtkHeaderBar:show-title-buttons:
   *
   * Whether to show title buttons like close, minimize, maximize.
   *
   * Which buttons are actually shown and where is determined
   * by the #GtkHeaderBar:decoration-layout property, and by
   * the state of the window (e.g. a close button will not be
   * shown if the window can't be closed).
   */
  header_bar_props[PROP_SHOW_TITLE_BUTTONS] =
      g_param_spec_boolean ("show-title-buttons",
                            P_("Show title buttons"),
                            P_("Whether to show title buttons"),
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkHeaderBar:decoration-layout:
   *
   * The decoration layout for buttons. If this property is
   * not set, the #GtkSettings:gtk-decoration-layout setting
   * is used.
   *
   * See gtk_header_bar_set_decoration_layout() for information
   * about the format of this string.
   */
  header_bar_props[PROP_DECORATION_LAYOUT] =
      g_param_spec_string ("decoration-layout",
                           P_("Decoration Layout"),
                           P_("The layout for window decorations"),
                           NULL,
                           GTK_PARAM_READWRITE);

  /**
   * GtkHeaderBar:decoration-layout-set:
   *
   * Set to %TRUE if #GtkHeaderBar:decoration-layout is set.
   */
  header_bar_props[PROP_DECORATION_LAYOUT_SET] =
      g_param_spec_boolean ("decoration-layout-set",
                            P_("Decoration Layout Set"),
                            P_("Whether the decoration-layout property has been set"),
                            FALSE,
                            GTK_PARAM_READWRITE);

  /**
   * GtkHeaderBar:has-subtitle:
   *
   * If %TRUE, reserve space for a subtitle, even if none
   * is currently set.
   */
  header_bar_props[PROP_HAS_SUBTITLE] =
      g_param_spec_boolean ("has-subtitle",
                            P_("Has Subtitle"),
                            P_("Whether to reserve space for a subtitle"),
                            TRUE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, header_bar_props);

  gtk_widget_class_set_accessible_role (widget_class, ATK_ROLE_PANEL);
  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_CENTER_LAYOUT);
  gtk_widget_class_set_css_name (widget_class, I_("headerbar"));
}

static void
gtk_header_bar_init (GtkHeaderBar *bar)
{
  GtkHeaderBarPrivate *priv = gtk_header_bar_get_instance_private (bar);
  GtkLayoutManager *layout;

  priv->title = NULL;
  priv->subtitle = NULL;
  priv->custom_title = NULL;
  priv->has_subtitle = TRUE;
  priv->decoration_layout = NULL;
  priv->decoration_layout_set = FALSE;
  priv->shows_app_menu = FALSE;
  priv->state = GDK_SURFACE_STATE_WITHDRAWN;

  layout = gtk_widget_get_layout_manager (GTK_WIDGET (bar));
  priv->start_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_style_context_add_class (gtk_widget_get_style_context (priv->start_box), "start");
  gtk_widget_set_parent (priv->start_box, GTK_WIDGET (bar));
  gtk_center_layout_set_start_widget (GTK_CENTER_LAYOUT (layout), priv->start_box);
  priv->end_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_style_context_add_class (gtk_widget_get_style_context (priv->end_box), "end");
  gtk_widget_set_parent (priv->end_box, GTK_WIDGET (bar));
  gtk_center_layout_set_end_widget (GTK_CENTER_LAYOUT (layout), priv->end_box);

  init_sizing_box (bar);
  construct_label_box (bar);
}

static GtkBuildableIface *parent_buildable_iface;

static void
gtk_header_bar_buildable_add_child (GtkBuildable *buildable,
                                    GtkBuilder   *builder,
                                    GObject      *child,
                                    const gchar  *type)
{
  if (g_strcmp0 (type, "title") == 0)
    gtk_header_bar_set_custom_title (GTK_HEADER_BAR (buildable), GTK_WIDGET (child));
  else if (g_strcmp0 (type, "start") == 0)
    gtk_header_bar_pack_start (GTK_HEADER_BAR (buildable), GTK_WIDGET (child));
  else if (g_strcmp0 (type, "end") == 0)
    gtk_header_bar_pack_end (GTK_HEADER_BAR (buildable), GTK_WIDGET (child));
  else
    parent_buildable_iface->add_child (buildable, builder, child, type);
}

static void
gtk_header_bar_buildable_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);

  iface->add_child = gtk_header_bar_buildable_add_child;
}

/**
 * gtk_header_bar_pack_start:
 * @bar: A #GtkHeaderBar
 * @child: the #GtkWidget to be added to @bar
 *
 * Adds @child to @bar, packed with reference to the
 * start of the @bar.
 */
void
gtk_header_bar_pack_start (GtkHeaderBar *bar,
                           GtkWidget    *child)
{
  gtk_header_bar_pack (bar, child, GTK_PACK_START);
}

/**
 * gtk_header_bar_pack_end:
 * @bar: A #GtkHeaderBar
 * @child: the #GtkWidget to be added to @bar
 *
 * Adds @child to @bar, packed with reference to the
 * end of the @bar.
 */
void
gtk_header_bar_pack_end (GtkHeaderBar *bar,
                         GtkWidget    *child)
{
  gtk_header_bar_pack (bar, child, GTK_PACK_END);
}

/**
 * gtk_header_bar_new:
 *
 * Creates a new #GtkHeaderBar widget.
 *
 * Returns: a new #GtkHeaderBar
 */
GtkWidget *
gtk_header_bar_new (void)
{
  return GTK_WIDGET (g_object_new (GTK_TYPE_HEADER_BAR, NULL));
}

/**
 * gtk_header_bar_get_show_title_buttons:
 * @bar: a #GtkHeaderBar
 *
 * Returns whether this header bar shows the standard window
 * title buttons.
 *
 * Returns: %TRUE if title buttons are shown
 */
gboolean
gtk_header_bar_get_show_title_buttons (GtkHeaderBar *bar)
{
  GtkHeaderBarPrivate *priv;

  g_return_val_if_fail (GTK_IS_HEADER_BAR (bar), FALSE);

  priv = gtk_header_bar_get_instance_private (bar);

  return priv->show_title_buttons;
}

/**
 * gtk_header_bar_set_show_title_buttons:
 * @bar: a #GtkHeaderBar
 * @setting: %TRUE to show standard title buttons
 *
 * Sets whether this header bar shows the standard window
 * title buttons including close, maximize, and minimize.
 */
void
gtk_header_bar_set_show_title_buttons (GtkHeaderBar *bar,
                                       gboolean      setting)
{
  GtkHeaderBarPrivate *priv;

  g_return_if_fail (GTK_IS_HEADER_BAR (bar));

  priv = gtk_header_bar_get_instance_private (bar);

  setting = setting != FALSE;

  if (priv->show_title_buttons == setting)
    return;

  priv->show_title_buttons = setting;
  _gtk_header_bar_update_window_buttons (bar);
  g_object_notify_by_pspec (G_OBJECT (bar), header_bar_props[PROP_SHOW_TITLE_BUTTONS]);
}

/**
 * gtk_header_bar_set_has_subtitle:
 * @bar: a #GtkHeaderBar
 * @setting: %TRUE to reserve space for a subtitle
 *
 * Sets whether the header bar should reserve space
 * for a subtitle, even if none is currently set.
 */
void
gtk_header_bar_set_has_subtitle (GtkHeaderBar *bar,
                                 gboolean      setting)
{
  GtkHeaderBarPrivate *priv;

  g_return_if_fail (GTK_IS_HEADER_BAR (bar));

  priv = gtk_header_bar_get_instance_private (bar);

  setting = setting != FALSE;

  if (priv->has_subtitle == setting)
    return;

  priv->has_subtitle = setting;
  gtk_widget_set_visible (priv->subtitle_sizing_label, setting || (priv->subtitle && priv->subtitle[0]));

  gtk_widget_queue_resize (GTK_WIDGET (bar));

  g_object_notify_by_pspec (G_OBJECT (bar), header_bar_props[PROP_HAS_SUBTITLE]);
}

/**
 * gtk_header_bar_get_has_subtitle:
 * @bar: a #GtkHeaderBar
 *
 * Retrieves whether the header bar reserves space for
 * a subtitle, regardless if one is currently set or not.
 *
 * Returns: %TRUE if the header bar reserves space
 *     for a subtitle
 */
gboolean
gtk_header_bar_get_has_subtitle (GtkHeaderBar *bar)
{
  GtkHeaderBarPrivate *priv;

  g_return_val_if_fail (GTK_IS_HEADER_BAR (bar), FALSE);

  priv = gtk_header_bar_get_instance_private (bar);

  return priv->has_subtitle;
}

/**
 * gtk_header_bar_set_decoration_layout:
 * @bar: a #GtkHeaderBar
 * @layout: (allow-none): a decoration layout, or %NULL to
 *     unset the layout
 *
 * Sets the decoration layout for this header bar, overriding
 * the #GtkSettings:gtk-decoration-layout setting. 
 *
 * There can be valid reasons for overriding the setting, such
 * as a header bar design that does not allow for buttons to take
 * room on the right, or only offers room for a single close button.
 * Split header bars are another example for overriding the
 * setting.
 *
 * The format of the string is button names, separated by commas.
 * A colon separates the buttons that should appear on the left
 * from those on the right. Recognized button names are minimize,
 * maximize, close, icon (the window icon) and menu (a menu button
 * for the fallback app menu).
 *
 * For example, “menu:minimize,maximize,close” specifies a menu
 * on the left, and minimize, maximize and close buttons on the right.
 */
void
gtk_header_bar_set_decoration_layout (GtkHeaderBar *bar,
                                      const gchar  *layout)
{
  GtkHeaderBarPrivate *priv;

  g_return_if_fail (GTK_IS_HEADER_BAR (bar));

  priv = gtk_header_bar_get_instance_private (bar);

  g_free (priv->decoration_layout);
  priv->decoration_layout = g_strdup (layout);
  priv->decoration_layout_set = (layout != NULL);

  _gtk_header_bar_update_window_buttons (bar);

  g_object_notify_by_pspec (G_OBJECT (bar), header_bar_props[PROP_DECORATION_LAYOUT]);
  g_object_notify_by_pspec (G_OBJECT (bar), header_bar_props[PROP_DECORATION_LAYOUT_SET]);
}

/**
 * gtk_header_bar_get_decoration_layout:
 * @bar: a #GtkHeaderBar
 *
 * Gets the decoration layout set with
 * gtk_header_bar_set_decoration_layout().
 *
 * Returns: the decoration layout
 */
const gchar *
gtk_header_bar_get_decoration_layout (GtkHeaderBar *bar)
{
  GtkHeaderBarPrivate *priv;

  g_return_val_if_fail (GTK_IS_HEADER_BAR (bar), NULL);

  priv = gtk_header_bar_get_instance_private (bar);

  return priv->decoration_layout;
}
