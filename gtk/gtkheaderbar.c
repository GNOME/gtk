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

#include "gtkheaderbar.h"
#include "gtkheaderbarprivate.h"
#include "gtkintl.h"
#include "gtkprivate.h"
#include "gtktypebuiltins.h"
#include "gtkwidgetprivate.h"
#include "gtkcssnodeprivate.h"
#include "gtkcsscustomgadgetprivate.h"
#include "gtkwindowprivate.h"
#include "gtkwidgetprivate.h"
#include "gtkcontainerprivate.h"
#include "a11y/gtkheaderbaraccessible.h"

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
 */

#define DEFAULT_SPACING 6
#define MIN_TITLE_CHARS 5

struct _GtkHeaderBarPrivate
{
  gchar *title;
  gchar *subtitle;
  GtkWidget *title_label;
  GtkWidget *subtitle_label;
  GtkWidget *label_box;
  GtkWidget *label_sizing_box;
  GtkWidget *subtitle_sizing_label;
  GtkWidget *custom_title;
  gint spacing;
  gboolean has_subtitle;

  GList *children;

  gboolean shows_wm_decorations;
  gchar *decoration_layout;
  gboolean decoration_layout_set;

  GtkWidget *titlebar_start_box;
  GtkWidget *titlebar_end_box;

  GtkWidget *titlebar_start_separator;
  GtkWidget *titlebar_end_separator;

  GtkWidget *titlebar_icon;

  GtkCssGadget *gadget;
};

typedef struct _Child Child;
struct _Child
{
  GtkWidget *widget;
  GtkPackType pack_type;
};

enum {
  PROP_0,
  PROP_TITLE,
  PROP_SUBTITLE,
  PROP_HAS_SUBTITLE,
  PROP_CUSTOM_TITLE,
  PROP_SPACING,
  PROP_SHOW_CLOSE_BUTTON,
  PROP_DECORATION_LAYOUT,
  PROP_DECORATION_LAYOUT_SET,
  LAST_PROP
};

enum {
  CHILD_PROP_0,
  CHILD_PROP_PACK_TYPE,
  CHILD_PROP_POSITION
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
  gtk_widget_show (w);
  priv->label_sizing_box = g_object_ref_sink (w);

  w = gtk_label_new (NULL);
  gtk_widget_show (w);
  context = gtk_widget_get_style_context (w);
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_TITLE);
  gtk_box_pack_start (GTK_BOX (priv->label_sizing_box), w, FALSE, FALSE, 0);
  gtk_label_set_line_wrap (GTK_LABEL (w), FALSE);
  gtk_label_set_single_line_mode (GTK_LABEL (w), TRUE);
  gtk_label_set_ellipsize (GTK_LABEL (w), PANGO_ELLIPSIZE_END);
  gtk_label_set_width_chars (GTK_LABEL (w), MIN_TITLE_CHARS);

  w = gtk_label_new (NULL);
  context = gtk_widget_get_style_context (w);
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_SUBTITLE);
  gtk_box_pack_start (GTK_BOX (priv->label_sizing_box), w, FALSE, FALSE, 0);
  gtk_label_set_line_wrap (GTK_LABEL (w), FALSE);
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
  gtk_widget_show (label_box);

  title_label = gtk_label_new (title);
  context = gtk_widget_get_style_context (title_label);
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_TITLE);
  gtk_label_set_line_wrap (GTK_LABEL (title_label), FALSE);
  gtk_label_set_single_line_mode (GTK_LABEL (title_label), TRUE);
  gtk_label_set_ellipsize (GTK_LABEL (title_label), PANGO_ELLIPSIZE_END);
  gtk_box_pack_start (GTK_BOX (label_box), title_label, FALSE, FALSE, 0);
  gtk_widget_show (title_label);
  gtk_label_set_width_chars (GTK_LABEL (title_label), MIN_TITLE_CHARS);

  subtitle_label = gtk_label_new (subtitle);
  context = gtk_widget_get_style_context (subtitle_label);
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_SUBTITLE);
  gtk_label_set_line_wrap (GTK_LABEL (subtitle_label), FALSE);
  gtk_label_set_single_line_mode (GTK_LABEL (subtitle_label), TRUE);
  gtk_label_set_ellipsize (GTK_LABEL (subtitle_label), PANGO_ELLIPSIZE_END);
  gtk_box_pack_start (GTK_BOX (label_box), subtitle_label, FALSE, FALSE, 0);
  gtk_widget_set_no_show_all (subtitle_label, TRUE);
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
  GdkPixbuf *pixbuf;
  gint scale;

  if (priv->titlebar_icon == NULL)
    return FALSE;

  scale = gtk_widget_get_scale_factor (priv->titlebar_icon);
  if (GTK_IS_BUTTON (gtk_widget_get_parent (priv->titlebar_icon)))
    pixbuf = gtk_window_get_icon_for_size (window, scale * 16);
  else
    pixbuf = gtk_window_get_icon_for_size (window, scale * 20);

  if (pixbuf)
    {
      cairo_surface_t *surface;

      surface = gdk_cairo_surface_create_from_pixbuf (pixbuf, scale, gtk_widget_get_window (priv->titlebar_icon));

      gtk_image_set_from_surface (GTK_IMAGE (priv->titlebar_icon), surface);
      cairo_surface_destroy (surface);
      g_object_unref (pixbuf);
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
  
  for (l = priv->children; l != NULL; l = l->next)
    {
      Child *child = l->data;

      if (gtk_widget_get_visible (child->widget))
        {
          if (child->pack_type == GTK_PACK_START)
            have_visible_at_start = TRUE;
          else
            have_visible_at_end = TRUE;
        }
    }

  if (priv->titlebar_start_separator != NULL)
    gtk_widget_set_visible (priv->titlebar_start_separator, have_visible_at_start);

  if (priv->titlebar_end_separator != NULL)
    gtk_widget_set_visible (priv->titlebar_end_separator, have_visible_at_end);
}

void
_gtk_header_bar_update_window_buttons (GtkHeaderBar *bar)
{
  GtkHeaderBarPrivate *priv = gtk_header_bar_get_instance_private (bar);
  GtkWidget *widget = GTK_WIDGET (bar), *toplevel;
  GtkWindow *window;
  GtkTextDirection direction;
  gchar *layout_desc;
  gchar **tokens, **t;
  gint i, j;
  GMenuModel *menu;
  gboolean shown_by_shell;
  gboolean is_sovereign_window;

  toplevel = gtk_widget_get_toplevel (widget);
  if (!gtk_widget_is_toplevel (toplevel))
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

  if (!priv->shows_wm_decorations)
    return;

  direction = gtk_widget_get_direction (widget);

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
                          gtk_window_get_type_hint (window) == GDK_WINDOW_TYPE_HINT_NORMAL);

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
          gtk_widget_set_no_show_all (separator, TRUE);
          gtk_style_context_add_class (gtk_widget_get_style_context (separator), "titlebutton");

          box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, priv->spacing);

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
                  gtk_widget_show (button);

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
                  gtk_container_add (GTK_CONTAINER (button), image);
                  gtk_widget_set_can_focus (button, FALSE);
                  gtk_widget_show_all (button);

                  accessible = gtk_widget_get_accessible (button);
                  if (GTK_IS_ACCESSIBLE (accessible))
                    atk_object_set_name (accessible, _("Application menu"));

                  priv->titlebar_icon = image;
                  if (!_gtk_header_bar_update_window_icon (bar, window))
                    gtk_image_set_from_icon_name (GTK_IMAGE (priv->titlebar_icon),
                                                  "application-x-executable-symbolic", GTK_ICON_SIZE_MENU);
                }
              else if (strcmp (t[j], "minimize") == 0 &&
                       is_sovereign_window)
                {
                  button = gtk_button_new ();
                  gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
                  gtk_style_context_add_class (gtk_widget_get_style_context (button), "titlebutton");
                  gtk_style_context_add_class (gtk_widget_get_style_context (button), "minimize");
                  image = gtk_image_new_from_icon_name ("window-minimize-symbolic", GTK_ICON_SIZE_MENU);
                  g_object_set (image, "use-fallback", TRUE, NULL);
                  gtk_container_add (GTK_CONTAINER (button), image);
                  gtk_widget_set_can_focus (button, FALSE);
                  gtk_widget_show_all (button);
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
                  image = gtk_image_new_from_icon_name (icon_name, GTK_ICON_SIZE_MENU);
                  g_object_set (image, "use-fallback", TRUE, NULL);
                  gtk_container_add (GTK_CONTAINER (button), image);
                  gtk_widget_set_can_focus (button, FALSE);
                  gtk_widget_show_all (button);
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
                  image = gtk_image_new_from_icon_name ("window-close-symbolic", GTK_ICON_SIZE_MENU);
                  gtk_style_context_add_class (gtk_widget_get_style_context (button), "titlebutton");
                  gtk_style_context_add_class (gtk_widget_get_style_context (button), "close");
                  g_object_set (image, "use-fallback", TRUE, NULL);
                  gtk_container_add (GTK_CONTAINER (button), image);
                  gtk_widget_set_can_focus (button, FALSE);
                  gtk_widget_show_all (button);
                  g_signal_connect_swapped (button, "clicked",
                                            G_CALLBACK (gtk_window_close), window);

                  accessible = gtk_widget_get_accessible (button);
                  if (GTK_IS_ACCESSIBLE (accessible))
                    atk_object_set_name (accessible, _("Close"));
                }

              if (button)
                {
                  gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, 0);
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

          gtk_box_pack_start (GTK_BOX (box), separator, FALSE, FALSE, 0);
          if (i == 1)
            gtk_box_reorder_child (GTK_BOX (box), separator, 0);

          if ((direction == GTK_TEXT_DIR_LTR && i == 0) ||
              (direction == GTK_TEXT_DIR_RTL && i == 1))
            {
              gtk_style_context_add_class (gtk_widget_get_style_context (box), GTK_STYLE_CLASS_LEFT);
              gtk_css_node_insert_after (gtk_widget_get_css_node (GTK_WIDGET (bar)),
                                         gtk_widget_get_css_node (box),
                                         NULL);
            }
          else
            {
              gtk_style_context_add_class (gtk_widget_get_style_context (box), GTK_STYLE_CLASS_RIGHT);
              gtk_css_node_insert_before (gtk_widget_get_css_node (GTK_WIDGET (bar)),
                                          gtk_widget_get_css_node (box),
                                          NULL);
            }

          gtk_widget_show (box);
          gtk_widget_set_parent (box, GTK_WIDGET (bar));

          if (i == 0)
            {
              priv->titlebar_start_box = box;
              priv->titlebar_start_separator = separator;
            }
          else
            {
              priv->titlebar_end_box = box;
              priv->titlebar_end_separator = separator;
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
  GtkWindow *window;
  gchar *layout_desc;
  gboolean ret;

  window = GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (bar)));
  gtk_widget_style_get (GTK_WIDGET (window),
                        "decoration-button-layout", &layout_desc,
                        NULL);

  ret = priv->shows_wm_decorations &&
        (layout_desc && strstr (layout_desc, "menu"));

  g_free (layout_desc);

  return ret;
}

/* As an intended side effect, this function allows @child
 * to be the title/label box */
static void
gtk_header_bar_reorder_css_node (GtkHeaderBar *bar,
                                 GtkPackType   pack_type,
                                 GtkWidget    *widget)
{
  GtkHeaderBarPrivate *priv = gtk_header_bar_get_instance_private (bar);
  GtkWidget *previous_widget;
  GList *l;
  
  if (pack_type == GTK_PACK_START)
    previous_widget = priv->titlebar_start_box;
  else
    previous_widget = priv->titlebar_end_box;

  for (l = priv->children; l; l = l->next)
    {
      Child *iter = l->data;

      if (iter->widget == widget)
        break;

      if (iter->pack_type == pack_type)
        previous_widget = iter->widget;
    }

  if ((pack_type == GTK_PACK_START)
      ^ (gtk_widget_get_direction (GTK_WIDGET (bar)) == GTK_TEXT_DIR_LTR))
    gtk_css_node_insert_after (gtk_widget_get_css_node (GTK_WIDGET (bar)),
                               gtk_widget_get_css_node (widget),
                               previous_widget ? gtk_widget_get_css_node (previous_widget) : NULL);
  else
    gtk_css_node_insert_before (gtk_widget_get_css_node (GTK_WIDGET (bar)),
                                gtk_widget_get_css_node (widget),
                                previous_widget ? gtk_widget_get_css_node (previous_widget) : NULL);
}

static void
construct_label_box (GtkHeaderBar *bar)
{
  GtkHeaderBarPrivate *priv = gtk_header_bar_get_instance_private (bar);

  g_assert (priv->label_box == NULL);

  priv->label_box = create_title_box (priv->title,
                                      priv->subtitle,
                                      &priv->title_label,
                                      &priv->subtitle_label);
  gtk_header_bar_reorder_css_node (bar, GTK_PACK_START, priv->label_box);
  gtk_widget_set_parent (priv->label_box, GTK_WIDGET (bar));
}

static gint
count_visible_children (GtkHeaderBar *bar)
{
  GtkHeaderBarPrivate *priv = gtk_header_bar_get_instance_private (bar);
  GList *l;
  Child *child;
  gint n;

  n = 0;
  for (l = priv->children; l; l = l->next)
    {
      child = l->data;
      if (gtk_widget_get_visible (child->widget))
        n++;
    }

  return n;
}

static gboolean
add_child_size (GtkWidget      *child,
                GtkOrientation  orientation,
                gint           *minimum,
                gint           *natural)
{
  gint child_minimum, child_natural;

  if (!gtk_widget_get_visible (child))
    return FALSE;

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    gtk_widget_get_preferred_width (child, &child_minimum, &child_natural);
  else
    gtk_widget_get_preferred_height (child, &child_minimum, &child_natural);

  if (GTK_ORIENTATION_HORIZONTAL == orientation)
    {
      *minimum += child_minimum;
      *natural += child_natural;
    }
  else
    {
      *minimum = MAX (*minimum, child_minimum);
      *natural = MAX (*natural, child_natural);
    }

  return TRUE;
}

static void
gtk_header_bar_get_size (GtkWidget      *widget,
                         GtkOrientation  orientation,
                         gint           *minimum_size,
                         gint           *natural_size)
{
  GtkHeaderBar *bar = GTK_HEADER_BAR (widget);
  GtkHeaderBarPrivate *priv = gtk_header_bar_get_instance_private (bar);
  GList *l;
  gint nvis_children;
  gint minimum, natural;
  gint center_min, center_nat;

  minimum = natural = 0;
  nvis_children = 0;

  for (l = priv->children; l; l = l->next)
    {
      Child *child = l->data;

      if (add_child_size (child->widget, orientation, &minimum, &natural))
        nvis_children += 1;
    }

  center_min = center_nat = 0;
  if (priv->label_box != NULL)
    {
      if (orientation == GTK_ORIENTATION_HORIZONTAL)
        add_child_size (priv->label_box, orientation, &center_min, &center_nat);
      else
        add_child_size (priv->label_sizing_box, orientation, &center_min, &center_nat);

      if (_gtk_widget_get_visible (priv->label_sizing_box))
        nvis_children += 1;
    }

  if (priv->custom_title != NULL)
    {
      if (add_child_size (priv->custom_title, orientation, &center_min, &center_nat))
        nvis_children += 1;
    }

  if (priv->titlebar_start_box != NULL)
    {
      if (add_child_size (priv->titlebar_start_box, orientation, &minimum, &natural))
        nvis_children += 1;
    }

  if (priv->titlebar_end_box != NULL)
    {
      if (add_child_size (priv->titlebar_end_box, orientation, &minimum, &natural))
        nvis_children += 1;
    }

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      minimum += center_min;
      natural += center_nat;
    }
  else
    {
      minimum = MAX (minimum, center_min);
      natural = MAX (natural, center_nat);
    }

  if (nvis_children > 0 && orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      minimum += nvis_children * priv->spacing;
      natural += nvis_children * priv->spacing;
    }

  *minimum_size = minimum;
  *natural_size = natural;
}

static void
gtk_header_bar_compute_size_for_orientation (GtkWidget *widget,
                                             gint       avail_size,
                                             gint      *minimum_size,
                                             gint      *natural_size)
{
  GtkHeaderBar *bar = GTK_HEADER_BAR (widget);
  GtkHeaderBarPrivate *priv = gtk_header_bar_get_instance_private (bar);
  GList *children;
  gint required_size = 0;
  gint required_natural = 0;
  gint child_size;
  gint child_natural;
  gint nvis_children;

  nvis_children = 0;

  for (children = priv->children; children != NULL; children = children->next)
    {
      Child *child = children->data;

      if (gtk_widget_get_visible (child->widget))
        {
          gtk_widget_get_preferred_width_for_height (child->widget,
                                                     avail_size, &child_size, &child_natural);

          required_size += child_size;
          required_natural += child_natural;

          nvis_children += 1;
        }
    }

  if (priv->label_box != NULL)
    {
      gtk_widget_get_preferred_width (priv->label_sizing_box,
                                      &child_size, &child_natural);
      required_size += child_size;
      required_natural += child_natural;
    }

  if (priv->custom_title != NULL &&
      gtk_widget_get_visible (priv->custom_title))
    {
      gtk_widget_get_preferred_width (priv->custom_title,
                                      &child_size, &child_natural);
      required_size += child_size;
      required_natural += child_natural;
    }

  if (priv->titlebar_start_box != NULL)
    {
      gtk_widget_get_preferred_width (priv->titlebar_start_box,
                                      &child_size, &child_natural);
      required_size += child_size;
      required_natural += child_natural;
      nvis_children += 1;
    }

  if (priv->titlebar_end_box != NULL)
    {
      gtk_widget_get_preferred_width (priv->titlebar_end_box,
                                      &child_size, &child_natural);
      required_size += child_size;
      required_natural += child_natural;
      nvis_children += 1;
    }

  if (nvis_children > 0)
    {
      required_size += nvis_children * priv->spacing;
      required_natural += nvis_children * priv->spacing;
    }

  *minimum_size = required_size;
  *natural_size = required_natural;
}

static void
gtk_header_bar_compute_size_for_opposing_orientation (GtkWidget *widget,
                                                      gint       avail_size,
                                                      gint      *minimum_size,
                                                      gint      *natural_size)
{
  GtkHeaderBar *bar = GTK_HEADER_BAR (widget);
  GtkHeaderBarPrivate *priv = gtk_header_bar_get_instance_private (bar);
  Child *child;
  GList *children;
  gint nvis_children;
  gint computed_minimum = 0;
  gint computed_natural = 0;
  GtkRequestedSize *sizes;
  GtkPackType packing;
  gint size = 0;
  gint i;
  gint child_size;
  gint child_minimum;
  gint child_natural;
  gint center_min, center_nat;

  nvis_children = count_visible_children (bar);

  if (nvis_children <= 0)
    return;

  sizes = g_newa (GtkRequestedSize, nvis_children);

  /* Retrieve desired size for visible children */
  for (i = 0, children = priv->children; children; children = children->next)
    {
      child = children->data;

      if (gtk_widget_get_visible (child->widget))
        {
          gtk_widget_get_preferred_width (child->widget,
                                          &sizes[i].minimum_size,
                                          &sizes[i].natural_size);

          size -= sizes[i].minimum_size;
          sizes[i].data = child;
          i += 1;
        }
    }

  /* Bring children up to size first */
  size = gtk_distribute_natural_allocation (MAX (0, avail_size), nvis_children, sizes);

  /* Allocate child positions. */
  for (packing = GTK_PACK_START; packing <= GTK_PACK_END; ++packing)
    {
      for (i = 0, children = priv->children; children; children = children->next)
        {
          child = children->data;

          /* If widget is not visible, skip it. */
          if (!gtk_widget_get_visible (child->widget))
            continue;

          /* If widget is packed differently skip it, but still increment i,
           * since widget is visible and will be handled in next loop
           * iteration.
           */
          if (child->pack_type != packing)
            {
              i++;
              continue;
            }

          child_size = sizes[i].minimum_size;

          gtk_widget_get_preferred_height_for_width (child->widget,
                                                     child_size, &child_minimum, &child_natural);

          computed_minimum = MAX (computed_minimum, child_minimum);
          computed_natural = MAX (computed_natural, child_natural);
        }
      i += 1;
    }

  center_min = center_nat = 0;
  if (priv->label_box != NULL)
    {
      gtk_widget_get_preferred_height (priv->label_sizing_box,
                                       &center_min, &center_nat);
    }

  if (priv->custom_title != NULL &&
      gtk_widget_get_visible (priv->custom_title))
    {
      gtk_widget_get_preferred_height (priv->custom_title,
                                       &center_min, &center_nat);
    }

  if (priv->titlebar_start_box != NULL)
    {
      gtk_widget_get_preferred_height (priv->titlebar_start_box,
                                       &child_minimum, &child_natural);
      computed_minimum = MAX (computed_minimum, child_minimum);
      computed_natural = MAX (computed_natural, child_natural);
    }

  if (priv->titlebar_end_box != NULL)
    {
      gtk_widget_get_preferred_height (priv->titlebar_end_box,
                                       &child_minimum, &child_natural);
      computed_minimum = MAX (computed_minimum, child_minimum);
      computed_natural = MAX (computed_natural, child_natural);
    }

  *minimum_size = computed_minimum;
  *natural_size = computed_natural;
}

static void
gtk_header_bar_get_content_size (GtkCssGadget   *gadget,
                                 GtkOrientation  orientation,
                                 gint            for_size,
                                 gint           *minimum,
                                 gint           *natural,
                                 gint           *minimum_baseline,
                                 gint           *natural_baseline,
                                 gpointer        unused)
{
  GtkWidget *widget = gtk_css_gadget_get_owner (gadget);

  if (for_size < 0)
    gtk_header_bar_get_size (widget, orientation, minimum, natural);
  else if (orientation == GTK_ORIENTATION_HORIZONTAL)
    gtk_header_bar_compute_size_for_orientation (widget, for_size, minimum, natural);
  else
    gtk_header_bar_compute_size_for_opposing_orientation (widget, for_size, minimum, natural);
}

static void
gtk_header_bar_get_preferred_width (GtkWidget *widget,
                                    gint      *minimum,
                                    gint      *natural)
{
  GtkHeaderBarPrivate *priv = gtk_header_bar_get_instance_private (GTK_HEADER_BAR (widget));

  gtk_css_gadget_get_preferred_size (priv->gadget,
                                     GTK_ORIENTATION_HORIZONTAL,
                                     -1,
                                     minimum, natural,
                                     NULL, NULL);
}

static void
gtk_header_bar_get_preferred_height (GtkWidget *widget,
                                     gint      *minimum,
                                     gint      *natural)
{
  GtkHeaderBarPrivate *priv = gtk_header_bar_get_instance_private (GTK_HEADER_BAR (widget));

  gtk_css_gadget_get_preferred_size (priv->gadget,
                                     GTK_ORIENTATION_VERTICAL,
                                     -1,
                                     minimum, natural,
                                     NULL, NULL);
}

static void
gtk_header_bar_get_preferred_width_for_height (GtkWidget *widget,
                                               gint       height,
                                               gint      *minimum,
                                               gint      *natural)
{
  GtkHeaderBarPrivate *priv = gtk_header_bar_get_instance_private (GTK_HEADER_BAR (widget));

  gtk_css_gadget_get_preferred_size (priv->gadget,
                                     GTK_ORIENTATION_HORIZONTAL,
                                     height,
                                     minimum, natural,
                                     NULL, NULL);
}

static void
gtk_header_bar_get_preferred_height_for_width (GtkWidget *widget,
                                               gint       width,
                                               gint      *minimum,
                                               gint      *natural)
{
  GtkHeaderBarPrivate *priv = gtk_header_bar_get_instance_private (GTK_HEADER_BAR (widget));

  gtk_css_gadget_get_preferred_size (priv->gadget,
                                     GTK_ORIENTATION_VERTICAL,
                                     width,
                                     minimum, natural,
                                     NULL, NULL);
}

static void
gtk_header_bar_size_allocate (GtkWidget     *widget,
                              GtkAllocation *allocation)
{
  GtkHeaderBarPrivate *priv = gtk_header_bar_get_instance_private (GTK_HEADER_BAR (widget));
  GtkAllocation clip;

  gtk_widget_set_allocation (widget, allocation);

  gtk_css_gadget_allocate (priv->gadget, allocation, gtk_widget_get_allocated_baseline (widget), &clip);

  gtk_widget_set_clip (widget, &clip);
}

static void
gtk_header_bar_allocate_contents (GtkCssGadget        *gadget,
                                  const GtkAllocation *allocation,
                                  int                  baseline,
                                  GtkAllocation       *out_clip,
                                  gpointer             unused)
{
  GtkWidget *widget = gtk_css_gadget_get_owner (gadget);
  GtkWidget *title_widget;
  GtkHeaderBar *bar = GTK_HEADER_BAR (widget);
  GtkHeaderBarPrivate *priv = gtk_header_bar_get_instance_private (bar);
  GtkRequestedSize *sizes;
  gint width, height;
  gint nvis_children;
  gint title_minimum_size;
  gint title_natural_size;
  gboolean title_expands = FALSE;
  gint start_width, end_width;
  gint uniform_expand_bonus[2] = { 0 };
  gint leftover_expand_bonus[2] = { 0 };
  gint nexpand_children[2] = { 0 };
  gint side[2];
  GList *l;
  gint i;
  Child *child;
  GtkPackType packing;
  GtkAllocation child_allocation;
  gint x;
  gint child_size;
  GtkTextDirection direction;

  direction = gtk_widget_get_direction (widget);
  nvis_children = count_visible_children (bar);
  sizes = g_newa (GtkRequestedSize, nvis_children);

  width = allocation->width - nvis_children * priv->spacing;
  height = allocation->height;

  i = 0;
  for (l = priv->children; l; l = l->next)
    {
      child = l->data;
      if (!gtk_widget_get_visible (child->widget))
        continue;

      if (gtk_widget_compute_expand (child->widget, GTK_ORIENTATION_HORIZONTAL))
        nexpand_children[child->pack_type]++;

      gtk_widget_get_preferred_width_for_height (child->widget,
                                                 height,
                                                 &sizes[i].minimum_size,
                                                 &sizes[i].natural_size);
      width -= sizes[i].minimum_size;
      i++;
    }

  title_minimum_size = 0;
  title_natural_size = 0;

  if (priv->custom_title != NULL &&
      gtk_widget_get_visible (priv->custom_title))
    title_widget = priv->custom_title;
  else if (priv->label_box != NULL)
    title_widget = priv->label_box;
  else
    title_widget = NULL;

  if (title_widget)
    {
      gtk_widget_get_preferred_width_for_height (title_widget,
                                                 height,
                                                 &title_minimum_size,
                                                 &title_natural_size);
      width -= title_natural_size;

      title_expands = gtk_widget_compute_expand (title_widget, GTK_ORIENTATION_HORIZONTAL);
    }

  start_width = 0;
  if (priv->titlebar_start_box != NULL)
    {
      gint min, nat;
      gtk_widget_get_preferred_width_for_height (priv->titlebar_start_box,
                                                 height,
                                                 &min, &nat);
      start_width = nat + priv->spacing;
    }
  width -= start_width;

  end_width = 0;
  if (priv->titlebar_end_box != NULL)
    {
      gint min, nat;
      gtk_widget_get_preferred_width_for_height (priv->titlebar_end_box,
                                                 height,
                                                 &min, &nat);
      end_width = nat + priv->spacing;
    }
  width -= end_width;

  width = gtk_distribute_natural_allocation (MAX (0, width), nvis_children, sizes);

  /* compute the nominal size of the children filling up each side of
   * the title in titlebar
   */
  side[0] = start_width;
  side[1] = end_width;
  for (packing = GTK_PACK_START; packing <= GTK_PACK_END; packing++)
    {
      i = 0;
      for (l = priv->children; l != NULL; l = l->next)
        {
          child = l->data;
          if (!gtk_widget_get_visible (child->widget))
            continue;

          if (child->pack_type == packing)
            side[packing] += sizes[i].minimum_size + priv->spacing;

          i++;
        }
    }

  /* figure out how much space is left on each side of the title,
   * and earkmark that space for the expanded children.
   *
   * If the title itself is expanded, then it gets half the spoils
   * from each side.
   */
  for (packing = GTK_PACK_START; packing <= GTK_PACK_END; packing++)
    {
      gint side_free_space;

      side_free_space = allocation->width / 2 - title_natural_size / 2 - side[packing];

      if (side_free_space > 0 && nexpand_children[packing] > 0)
        {
          width -= side_free_space;

          if (title_expands)
            side_free_space -= side_free_space / 2;

          side[packing] += side_free_space;
          uniform_expand_bonus[packing] = side_free_space / nexpand_children[packing];
          leftover_expand_bonus[packing] = side_free_space % nexpand_children[packing];
        }
    }

  /* allocate the children on both sides of the title */
  for (packing = GTK_PACK_START; packing <= GTK_PACK_END; packing++)
    {
      child_allocation.y = allocation->y;
      child_allocation.height = height;
      if (packing == GTK_PACK_START)
        x = allocation->x + start_width;
      else
        x = allocation->x + allocation->width - end_width;

      i = 0;
      for (l = priv->children; l != NULL; l = l->next)
        {
          child = l->data;
          if (!gtk_widget_get_visible (child->widget))
            continue;

          if (child->pack_type != packing)
            goto next;

          child_size = sizes[i].minimum_size;

          /* if this child is expanded, give it extra space from the reserves */
          if (gtk_widget_compute_expand (child->widget, GTK_ORIENTATION_HORIZONTAL))
            {
              gint expand_bonus;

              expand_bonus = uniform_expand_bonus[packing];

              if (leftover_expand_bonus[packing] > 0)
                {
                  expand_bonus++;
                  leftover_expand_bonus[packing]--;
                }

              child_size += expand_bonus;
            }

          child_allocation.width = child_size;

          if (packing == GTK_PACK_START)
            {
              child_allocation.x = x;
              x += child_size;
              x += priv->spacing;
            }
          else
            {
              x -= child_size;
              child_allocation.x = x;
              x -= priv->spacing;
            }

          if (direction == GTK_TEXT_DIR_RTL)
            child_allocation.x = allocation->x + allocation->width - (child_allocation.x - allocation->x) - child_allocation.width;

          gtk_widget_size_allocate (child->widget, &child_allocation);

        next:
          i++;
        }
    }

  /* We don't enforce css borders on the center widget, to make
   * title/subtitle combinations fit without growing the header
   */
  child_allocation.y = allocation->y;
  child_allocation.height = allocation->height;

  child_size = MIN (allocation->width - side[0] - side[1], title_natural_size);

  child_allocation.x = allocation->x + (allocation->width - child_size) / 2;
  child_allocation.width = child_size;

  /* if the title widget is expanded, then grow it by all the available
   * free space, and recenter it
   */
  if (title_expands && width > 0)
    {
      child_allocation.width += width;
      child_allocation.x -= width / 2;
    }

  if (allocation->x + side[0] > child_allocation.x)
    child_allocation.x = allocation->x + side[0];
  else if (allocation->x + allocation->width - side[1] < child_allocation.x + child_allocation.width)
    child_allocation.x = allocation->x + allocation->width - side[1] - child_allocation.width;

  if (direction == GTK_TEXT_DIR_RTL)
    child_allocation.x = allocation->x + allocation->width - (child_allocation.x - allocation->x) - child_allocation.width;

  if (title_widget != NULL)
    gtk_widget_size_allocate (title_widget, &child_allocation);

  child_allocation.y = allocation->y;
  child_allocation.height = height;

  if (priv->titlebar_start_box)
    {
      gboolean left = (direction == GTK_TEXT_DIR_LTR);
      if (left)
        child_allocation.x = allocation->x;
      else
        child_allocation.x = allocation->x + allocation->width - start_width + priv->spacing;
      child_allocation.width = start_width - priv->spacing;
      gtk_widget_size_allocate (priv->titlebar_start_box, &child_allocation);
    }

  if (priv->titlebar_end_box)
    {
      gboolean left = (direction != GTK_TEXT_DIR_LTR);
      if (left)
        child_allocation.x = allocation->x;
      else
        child_allocation.x = allocation->x + allocation->width - end_width + priv->spacing;
      child_allocation.width = end_width - priv->spacing;
      gtk_widget_size_allocate (priv->titlebar_end_box, &child_allocation);
    }

  gtk_container_get_children_clip (GTK_CONTAINER (widget), out_clip);
}

/**
 * gtk_header_bar_set_title:
 * @bar: a #GtkHeaderBar
 * @title: (allow-none): a title, or %NULL
 *
 * Sets the title of the #GtkHeaderBar. The title should help a user
 * identify the current view. A good title should not include the
 * application name.
 *
 * Since: 3.10
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
 *
 * Since: 3.10
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
 *
 * Since: 3.10
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
 *
 * Since: 3.10
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
 *
 * Since: 3.10
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
      priv->custom_title = title_widget;

      gtk_header_bar_reorder_css_node (bar, GTK_PACK_START, priv->custom_title);
      gtk_widget_set_parent (priv->custom_title, GTK_WIDGET (bar));
      gtk_widget_set_valign (priv->custom_title, GTK_ALIGN_CENTER);

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

  gtk_widget_queue_resize (GTK_WIDGET (bar));

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
 *
 * Since: 3.10
 */
GtkWidget *
gtk_header_bar_get_custom_title (GtkHeaderBar *bar)
{
  GtkHeaderBarPrivate *priv = gtk_header_bar_get_instance_private (bar);

  g_return_val_if_fail (GTK_IS_HEADER_BAR (bar), NULL);

  return priv->custom_title;
}

static void
gtk_header_bar_destroy (GtkWidget *widget)
{
  GtkHeaderBarPrivate *priv = gtk_header_bar_get_instance_private (GTK_HEADER_BAR (widget));

  if (priv->label_sizing_box)
    {
      gtk_widget_destroy (priv->label_sizing_box);
      g_clear_object (&priv->label_sizing_box);
    }

  if (priv->custom_title)
    {
      gtk_widget_unparent (priv->custom_title);
      priv->custom_title = NULL;
    }

  if (priv->label_box)
    {
      gtk_widget_unparent (priv->label_box);
      priv->label_box = NULL;
    }

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

  GTK_WIDGET_CLASS (gtk_header_bar_parent_class)->destroy (widget);
}

static void
gtk_header_bar_finalize (GObject *object)
{
  GtkHeaderBarPrivate *priv = gtk_header_bar_get_instance_private (GTK_HEADER_BAR (object));

  g_free (priv->title);
  g_free (priv->subtitle);
  g_free (priv->decoration_layout);

  g_clear_object (&priv->gadget);

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

    case PROP_SPACING:
      g_value_set_int (value, priv->spacing);
      break;

    case PROP_SHOW_CLOSE_BUTTON:
      g_value_set_boolean (value, gtk_header_bar_get_show_close_button (bar));
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

    case PROP_SPACING:
      if (priv->spacing != g_value_get_int (value))
        {
          priv->spacing = g_value_get_int (value);
          gtk_widget_queue_resize (GTK_WIDGET (bar));
          g_object_notify_by_pspec (object, pspec);
        }
      break;

    case PROP_SHOW_CLOSE_BUTTON:
      gtk_header_bar_set_show_close_button (bar, g_value_get_boolean (value));
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
  Child *child;

  g_return_if_fail (gtk_widget_get_parent (widget) == NULL);

  child = g_new (Child, 1);
  child->widget = widget;
  child->pack_type = pack_type;

  priv->children = g_list_append (priv->children, child);

  gtk_widget_freeze_child_notify (widget);
  gtk_header_bar_reorder_css_node (bar, GTK_PACK_START, widget);
  gtk_widget_set_parent (widget, GTK_WIDGET (bar));
  g_signal_connect (widget, "notify::visible", G_CALLBACK (notify_child_cb), bar);
  gtk_widget_child_notify (widget, "pack-type");
  gtk_widget_child_notify (widget, "position");
  gtk_widget_thaw_child_notify (widget);

  _gtk_header_bar_update_separator_visibility (bar);
}

static void
gtk_header_bar_add (GtkContainer *container,
                    GtkWidget    *child)
{
  gtk_header_bar_pack (GTK_HEADER_BAR (container), child, GTK_PACK_START);
}

static GList *
find_child_link (GtkHeaderBar *bar,
                 GtkWidget    *widget,
                 gint         *position)
{
  GtkHeaderBarPrivate *priv = gtk_header_bar_get_instance_private (bar);
  GList *l;
  Child *child;
  gint i;

  for (l = priv->children, i = 0; l; l = l->next, i++)
    {
      child = l->data;
      if (child->widget == widget)
        {
          if (position)
            *position = i;
          return l;
        }
    }

  return NULL;
}

static void
gtk_header_bar_remove (GtkContainer *container,
                       GtkWidget    *widget)
{
  GtkHeaderBar *bar = GTK_HEADER_BAR (container);
  GtkHeaderBarPrivate *priv = gtk_header_bar_get_instance_private (bar);
  GList *l;
  Child *child;

  l = find_child_link (bar, widget, NULL);
  if (l)
    {
      child = l->data;
      g_signal_handlers_disconnect_by_func (widget, notify_child_cb, bar);
      gtk_widget_unparent (child->widget);
      priv->children = g_list_delete_link (priv->children, l);
      g_free (child);
      gtk_widget_queue_resize (GTK_WIDGET (container));
      _gtk_header_bar_update_separator_visibility (bar);
    }
}

static void
gtk_header_bar_forall (GtkContainer *container,
                       gboolean      include_internals,
                       GtkCallback   callback,
                       gpointer      callback_data)
{
  GtkHeaderBar *bar = GTK_HEADER_BAR (container);
  GtkHeaderBarPrivate *priv = gtk_header_bar_get_instance_private (bar);
  Child *child;
  GList *children;

  if (include_internals && priv->titlebar_start_box != NULL)
    (* callback) (priv->titlebar_start_box, callback_data);

  children = priv->children;
  while (children)
    {
      child = children->data;
      children = children->next;
      if (child->pack_type == GTK_PACK_START)
        (* callback) (child->widget, callback_data);
    }

  if (priv->custom_title != NULL)
    (* callback) (priv->custom_title, callback_data);

  if (include_internals && priv->label_box != NULL)
    (* callback) (priv->label_box, callback_data);

  children = priv->children;
  while (children)
    {
      child = children->data;
      children = children->next;
      if (child->pack_type == GTK_PACK_END)
        (* callback) (child->widget, callback_data);
    }

  if (include_internals && priv->titlebar_end_box != NULL)
    (* callback) (priv->titlebar_end_box, callback_data);
}

static void
gtk_header_bar_reorder_child (GtkHeaderBar *bar,
                              GtkWidget    *widget,
                              gint          position)
{
  GtkHeaderBarPrivate *priv = gtk_header_bar_get_instance_private (bar);
  GList *l;
  gint old_position;
  Child *child;

  l = find_child_link (bar, widget, &old_position);

  if (l == NULL)
    return;

  if (old_position == position)
    return;

  child = l->data; 
  priv->children = g_list_delete_link (priv->children, l);

  if (position < 0)
    l = NULL;
  else
    l = g_list_nth (priv->children, position);

  priv->children = g_list_insert_before (priv->children, l, child);
  gtk_header_bar_reorder_css_node (bar, child->pack_type, widget);
  gtk_widget_child_notify (widget, "position");
  gtk_widget_queue_resize (widget);
}

static GType
gtk_header_bar_child_type (GtkContainer *container)
{
  return GTK_TYPE_WIDGET;
}

static void
gtk_header_bar_get_child_property (GtkContainer *container,
                                   GtkWidget    *widget,
                                   guint         property_id,
                                   GValue       *value,
                                   GParamSpec   *pspec)
{
  GtkHeaderBar *bar = GTK_HEADER_BAR (container);
  GtkHeaderBarPrivate *priv = gtk_header_bar_get_instance_private (bar);
  GList *l;
  Child *child;

  l = find_child_link (bar, widget, NULL);
  if (l == NULL)
    {
      g_param_value_set_default (pspec, value);
      return;
    }

  child = l->data;

  switch (property_id)
    {
    case CHILD_PROP_PACK_TYPE:
      g_value_set_enum (value, child->pack_type);
      break;

    case CHILD_PROP_POSITION:
      g_value_set_int (value, g_list_position (priv->children, l));
      break;

    default:
      GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, property_id, pspec);
      break;
    }
}

static void
gtk_header_bar_set_child_property (GtkContainer *container,
                                   GtkWidget    *widget,
                                   guint         property_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  GtkHeaderBar *bar = GTK_HEADER_BAR (container);
  GList *l;
  Child *child;

  l = find_child_link (bar, widget, NULL);
  if (l == NULL)
    return;

  child = l->data;

  switch (property_id)
    {
    case CHILD_PROP_PACK_TYPE:
      child->pack_type = g_value_get_enum (value);
      _gtk_header_bar_update_separator_visibility (bar);
      gtk_widget_queue_resize (widget);
      break;

    case CHILD_PROP_POSITION:
      gtk_header_bar_reorder_child (bar, widget, g_value_get_int (value));
      break;

    default:
      GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, property_id, pspec);
      break;
    }
}

static gint
gtk_header_bar_draw (GtkWidget *widget,
                     cairo_t   *cr)
{
  GtkHeaderBarPrivate *priv = gtk_header_bar_get_instance_private (GTK_HEADER_BAR (widget));

  gtk_css_gadget_draw (priv->gadget, cr);

  return FALSE;
}

static gboolean
gtk_header_bar_render_contents (GtkCssGadget *gadget,
                                cairo_t      *cr,
                                int           x,
                                int           y,
                                int           width,
                                int           height,
                                gpointer      unused)
{
  GtkWidget *widget = gtk_css_gadget_get_owner (gadget);

  GTK_WIDGET_CLASS (gtk_header_bar_parent_class)->draw (widget, cr);

  return FALSE;
}

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
  _gtk_header_bar_update_window_buttons (GTK_HEADER_BAR (widget));
}

static void
gtk_header_bar_unrealize (GtkWidget *widget)
{
  GtkSettings *settings;

  settings = gtk_widget_get_settings (widget);

  g_signal_handlers_disconnect_by_func (settings, _gtk_header_bar_update_window_buttons, widget);

  GTK_WIDGET_CLASS (gtk_header_bar_parent_class)->unrealize (widget);
}

static gboolean
window_state_changed (GtkWidget           *window,
                      GdkEventWindowState *event,
                      gpointer             data)
{
  GtkHeaderBar *bar = GTK_HEADER_BAR (data);

  if (event->changed_mask & (GDK_WINDOW_STATE_FULLSCREEN |
                             GDK_WINDOW_STATE_MAXIMIZED |
                             GDK_WINDOW_STATE_TILED |
                             GDK_WINDOW_STATE_TOP_TILED |
                             GDK_WINDOW_STATE_RIGHT_TILED |
                             GDK_WINDOW_STATE_BOTTOM_TILED |
                             GDK_WINDOW_STATE_LEFT_TILED))
    _gtk_header_bar_update_window_buttons (bar);

  return FALSE;
}

static void
gtk_header_bar_hierarchy_changed (GtkWidget *widget,
                                  GtkWidget *previous_toplevel)
{
  GtkWidget *toplevel;
  GtkHeaderBar *bar = GTK_HEADER_BAR (widget);

  toplevel = gtk_widget_get_toplevel (widget);

  if (previous_toplevel)
    g_signal_handlers_disconnect_by_func (previous_toplevel,
                                          window_state_changed, widget);

  if (toplevel)
    g_signal_connect_after (toplevel, "window-state-event",
                            G_CALLBACK (window_state_changed), widget);

  _gtk_header_bar_update_window_buttons (bar);
}

static void
gtk_header_bar_direction_changed (GtkWidget        *widget,
                                  GtkTextDirection  previous_direction)
{
  GTK_WIDGET_CLASS (gtk_header_bar_parent_class)->direction_changed (widget, previous_direction);

  gtk_css_node_reverse_children (gtk_widget_get_css_node (widget));
}

static void
gtk_header_bar_class_init (GtkHeaderBarClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (class);

  object_class->finalize = gtk_header_bar_finalize;
  object_class->get_property = gtk_header_bar_get_property;
  object_class->set_property = gtk_header_bar_set_property;

  widget_class->destroy = gtk_header_bar_destroy;
  widget_class->size_allocate = gtk_header_bar_size_allocate;
  widget_class->get_preferred_width = gtk_header_bar_get_preferred_width;
  widget_class->get_preferred_height = gtk_header_bar_get_preferred_height;
  widget_class->get_preferred_height_for_width = gtk_header_bar_get_preferred_height_for_width;
  widget_class->get_preferred_width_for_height = gtk_header_bar_get_preferred_width_for_height;
  widget_class->draw = gtk_header_bar_draw;
  widget_class->realize = gtk_header_bar_realize;
  widget_class->unrealize = gtk_header_bar_unrealize;
  widget_class->hierarchy_changed = gtk_header_bar_hierarchy_changed;
  widget_class->direction_changed = gtk_header_bar_direction_changed;

  container_class->add = gtk_header_bar_add;
  container_class->remove = gtk_header_bar_remove;
  container_class->forall = gtk_header_bar_forall;
  container_class->child_type = gtk_header_bar_child_type;
  container_class->set_child_property = gtk_header_bar_set_child_property;
  container_class->get_child_property = gtk_header_bar_get_child_property;
  gtk_container_class_handle_border_width (container_class);

  gtk_container_class_install_child_property (container_class,
                                              CHILD_PROP_PACK_TYPE,
                                              g_param_spec_enum ("pack-type",
                                                                 P_("Pack type"),
                                                                 P_("A GtkPackType indicating whether the child is packed with reference to the start or end of the parent"),
                                                                 GTK_TYPE_PACK_TYPE, GTK_PACK_START,
                                                                 GTK_PARAM_READWRITE));
  gtk_container_class_install_child_property (container_class,
                                              CHILD_PROP_POSITION,
                                              g_param_spec_int ("position",
                                                                P_("Position"),
                                                                P_("The index of the child in the parent"),
                                                                -1, G_MAXINT, 0,
                                                                GTK_PARAM_READWRITE));

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

  header_bar_props[PROP_SPACING] =
      g_param_spec_int ("spacing",
                        P_("Spacing"),
                        P_("The amount of space between children"),
                        0, G_MAXINT,
                        DEFAULT_SPACING,
                        GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkHeaderBar:show-close-button:
   *
   * Whether to show window decorations.
   *
   * Which buttons are actually shown and where is determined
   * by the #GtkHeaderBar:decoration-layout property, and by
   * the state of the window (e.g. a close button will not be
   * shown if the window can't be closed).
   */
  header_bar_props[PROP_SHOW_CLOSE_BUTTON] =
      g_param_spec_boolean ("show-close-button",
                            P_("Show decorations"),
                            P_("Whether to show window decorations"),
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
   *
   * Since: 3.12
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
   *
   * Since: 3.12
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
   *
   * Since: 3.12
   */
  header_bar_props[PROP_HAS_SUBTITLE] =
      g_param_spec_boolean ("has-subtitle",
                            P_("Has Subtitle"),
                            P_("Whether to reserve space for a subtitle"),
                            TRUE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, header_bar_props);

  gtk_widget_class_set_accessible_type (widget_class, GTK_TYPE_HEADER_BAR_ACCESSIBLE);
  gtk_widget_class_set_accessible_role (widget_class, ATK_ROLE_PANEL);
  gtk_widget_class_set_css_name (widget_class, "headerbar");
}

static void
gtk_header_bar_init (GtkHeaderBar *bar)
{
  GtkHeaderBarPrivate *priv;
  GtkCssNode *widget_node;

  priv = gtk_header_bar_get_instance_private (bar);

  gtk_widget_set_has_window (GTK_WIDGET (bar), FALSE);

  priv->title = NULL;
  priv->subtitle = NULL;
  priv->custom_title = NULL;
  priv->children = NULL;
  priv->spacing = DEFAULT_SPACING;
  priv->has_subtitle = TRUE;
  priv->decoration_layout = NULL;
  priv->decoration_layout_set = FALSE;

  init_sizing_box (bar);
  construct_label_box (bar);

  widget_node = gtk_widget_get_css_node (GTK_WIDGET (bar));
  priv->gadget = gtk_css_custom_gadget_new_for_node (widget_node,
                                                     GTK_WIDGET (bar),
                                                     gtk_header_bar_get_content_size,
                                                     gtk_header_bar_allocate_contents,
                                                     gtk_header_bar_render_contents,
                                                     NULL,
                                                     NULL);

}

static void
gtk_header_bar_buildable_add_child (GtkBuildable *buildable,
                                    GtkBuilder   *builder,
                                    GObject      *child,
                                    const gchar  *type)
{
  if (type && strcmp (type, "title") == 0)
    gtk_header_bar_set_custom_title (GTK_HEADER_BAR (buildable), GTK_WIDGET (child));
  else if (!type)
    gtk_container_add (GTK_CONTAINER (buildable), GTK_WIDGET (child));
  else
    GTK_BUILDER_WARN_INVALID_CHILD_TYPE (GTK_HEADER_BAR (buildable), type);
}

static void
gtk_header_bar_buildable_init (GtkBuildableIface *iface)
{
  iface->add_child = gtk_header_bar_buildable_add_child;
}

/**
 * gtk_header_bar_pack_start:
 * @bar: A #GtkHeaderBar
 * @child: the #GtkWidget to be added to @bar
 *
 * Adds @child to @bar, packed with reference to the
 * start of the @bar.
 *
 * Since: 3.10
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
 *
 * Since: 3.10
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
 *
 * Since: 3.10
 */
GtkWidget *
gtk_header_bar_new (void)
{
  return GTK_WIDGET (g_object_new (GTK_TYPE_HEADER_BAR, NULL));
}

/**
 * gtk_header_bar_get_show_close_button:
 * @bar: a #GtkHeaderBar
 *
 * Returns whether this header bar shows the standard window
 * decorations.
 *
 * Returns: %TRUE if the decorations are shown
 *
 * Since: 3.10
 */
gboolean
gtk_header_bar_get_show_close_button (GtkHeaderBar *bar)
{
  GtkHeaderBarPrivate *priv;

  g_return_val_if_fail (GTK_IS_HEADER_BAR (bar), FALSE);

  priv = gtk_header_bar_get_instance_private (bar);

  return priv->shows_wm_decorations;
}

/**
 * gtk_header_bar_set_show_close_button:
 * @bar: a #GtkHeaderBar
 * @setting: %TRUE to show standard window decorations
 *
 * Sets whether this header bar shows the standard window decorations,
 * including close, maximize, and minimize.
 *
 * Since: 3.10
 */
void
gtk_header_bar_set_show_close_button (GtkHeaderBar *bar,
                                      gboolean      setting)
{
  GtkHeaderBarPrivate *priv;

  g_return_if_fail (GTK_IS_HEADER_BAR (bar));

  priv = gtk_header_bar_get_instance_private (bar);

  setting = setting != FALSE;

  if (priv->shows_wm_decorations == setting)
    return;

  priv->shows_wm_decorations = setting;
  _gtk_header_bar_update_window_buttons (bar);
  g_object_notify_by_pspec (G_OBJECT (bar), header_bar_props[PROP_SHOW_CLOSE_BUTTON]);
}

/**
 * gtk_header_bar_set_has_subtitle:
 * @bar: a #GtkHeaderBar
 * @setting: %TRUE to reserve space for a subtitle
 *
 * Sets whether the header bar should reserve space
 * for a subtitle, even if none is currently set.
 *
 * Since: 3.12
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
 *
 * Since: 3.12
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
 *
 * Since: 3.12
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
 *
 * Since: 3.12 
 */
const gchar *
gtk_header_bar_get_decoration_layout (GtkHeaderBar *bar)
{
  GtkHeaderBarPrivate *priv;

  g_return_val_if_fail (GTK_IS_HEADER_BAR (bar), NULL);

  priv = gtk_header_bar_get_instance_private (bar);

  return priv->decoration_layout;
}
