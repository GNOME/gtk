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
#include "gtkbuildable.h"
#include "gtkcenterlayout.h"
#include "gtkcssnodeprivate.h"
#include "gtkintl.h"
#include "gtklabel.h"
#include "gtknative.h"
#include "gtkprivate.h"
#include "gtksizerequest.h"
#include "gtktypebuiltins.h"
#include "gtkwidgetprivate.h"
#include "gtkwindowcontrols.h"

#include "a11y/gtkcontaineraccessible.h"

#include <string.h>

/**
 * SECTION:gtkheaderbar
 * @Short_description: A box with a centered child
 * @Title: GtkHeaderBar
 * @See_also: #GtkBox, #GtkActionBar
 *
 * GtkHeaderBar is similar to a horizontal #GtkBox. It allows children to 
 * be placed at the start or the end. In addition, it allows the window
 * title to be displayed. The title will be centered with respect to the
 * width of the box, even if the children at either side take up different
 * amounts of space.
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
 * │   ├── windowcontrols.start
 * │   ╰── [other children]
 * ├── [Title Widget]
 * ╰── box.end
 *     ├── [other children]
 *     ╰── windowcontrols.end
 * ]|
 *
 * A #GtkHeaderBar's CSS node is called headerbar. It contains two box subnodes
 * at the start and end of the headerbar, as well as a center node that
 * represents the title.
 *
 * Each of the boxes contains a windowcontrols subnode, see #GtkWindowControls
 * for details, as well as other children.
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

  GtkWidget *title_label;
  GtkWidget *title_widget;

  gboolean show_title_buttons;
  gchar *decoration_layout;
  gboolean track_default_decoration;

  GtkWidget *start_window_controls;
  GtkWidget *end_window_controls;

  GdkSurfaceState state;
};

enum {
  PROP_0,
  PROP_TITLE_WIDGET,
  PROP_SHOW_TITLE_BUTTONS,
  PROP_DECORATION_LAYOUT,
  LAST_PROP
};

static GParamSpec *header_bar_props[LAST_PROP] = { NULL, };

static void gtk_header_bar_buildable_init (GtkBuildableIface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkHeaderBar, gtk_header_bar, GTK_TYPE_CONTAINER,
                         G_ADD_PRIVATE (GtkHeaderBar)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                                gtk_header_bar_buildable_init));

static void
create_window_controls (GtkHeaderBar *bar)
{
  GtkHeaderBarPrivate *priv = gtk_header_bar_get_instance_private (bar);
  GtkWidget *controls;

  controls = gtk_window_controls_new (GTK_PACK_START);
  g_object_bind_property (bar, "decoration-layout",
                          controls, "decoration-layout",
                          G_BINDING_SYNC_CREATE);
  g_object_bind_property (controls, "empty",
                          controls, "visible",
                          G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);
  gtk_container_add (GTK_CONTAINER (priv->start_box), controls);
  priv->start_window_controls = controls;

  controls = gtk_window_controls_new (GTK_PACK_END);
  g_object_bind_property (bar, "decoration-layout",
                          controls, "decoration-layout",
                          G_BINDING_SYNC_CREATE);
  g_object_bind_property (controls, "empty",
                          controls, "visible",
                          G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);
  gtk_container_add (GTK_CONTAINER (priv->end_box), controls);
  priv->end_window_controls = controls;
}

static void
update_default_decoration (GtkHeaderBar *bar)
{
  GtkHeaderBarPrivate *priv = gtk_header_bar_get_instance_private (bar);
  GtkLayoutManager *layout = gtk_widget_get_layout_manager (GTK_WIDGET (bar));
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
          if (w != priv->start_window_controls)
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
            if (w != priv->end_window_controls)
              {
                have_children = TRUE;
                break;
              }
          }
    }

  if (have_children || priv->title_widget != NULL)
    gtk_widget_remove_css_class (GTK_WIDGET (bar), "default-decoration");
  else
    gtk_widget_add_css_class (GTK_WIDGET (bar), "default-decoration");
}

void
_gtk_header_bar_track_default_decoration (GtkHeaderBar *bar)
{
  GtkHeaderBarPrivate *priv = gtk_header_bar_get_instance_private (bar);

  priv->track_default_decoration = TRUE;

  update_default_decoration (bar);
}

static void
update_title (GtkHeaderBar *bar)
{
  GtkHeaderBarPrivate *priv = gtk_header_bar_get_instance_private (bar);
  GtkRoot *root;
  const gchar *title = NULL;

  if (!priv->title_label)
    return;

  root = gtk_widget_get_root (GTK_WIDGET (bar));

  if (GTK_IS_WINDOW (root))
    title = gtk_window_get_title (GTK_WINDOW (root));

  if (!title)
    title = g_get_application_name ();

  if (!title)
    title = g_get_prgname ();

  if (!title)
    title = "";

  gtk_label_set_text (GTK_LABEL (priv->title_label), title);
}

static void
construct_title_label (GtkHeaderBar *bar)
{
  GtkHeaderBarPrivate *priv = gtk_header_bar_get_instance_private (bar);
  GtkLayoutManager *layout = gtk_widget_get_layout_manager (GTK_WIDGET (bar));
  GtkWidget *label;

  g_assert (priv->title_label == NULL);

  label = gtk_label_new (NULL);
  gtk_widget_add_css_class (label, GTK_STYLE_CLASS_TITLE);
  gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
  gtk_label_set_wrap (GTK_LABEL (label), FALSE);
  gtk_label_set_single_line_mode (GTK_LABEL (label), TRUE);
  gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
  gtk_label_set_width_chars (GTK_LABEL (label), MIN_TITLE_CHARS);

  gtk_widget_insert_after (label, GTK_WIDGET (bar), priv->start_box);
  gtk_center_layout_set_center_widget (GTK_CENTER_LAYOUT (layout), label);

  priv->title_label = label;

  update_title (bar);
}

/**
 * gtk_header_bar_set_title_widget:
 * @bar: a #GtkHeaderBar
 * @title_widget: (allow-none): a widget to use for a title
 *
 * Sets a title widget for the #GtkHeaderBar.
 *
 * The title should help a user identify the current view. This
 * supersedes the window title label. To achieve the same style as
 * the builtin title, use the “title” style class.
 *
 * You should set the title widget to %NULL, for the window title
 * label to be visible again.
 */
void
gtk_header_bar_set_title_widget (GtkHeaderBar *bar,
                                 GtkWidget    *title_widget)
{
  GtkHeaderBarPrivate *priv = gtk_header_bar_get_instance_private (bar);

  g_return_if_fail (GTK_IS_HEADER_BAR (bar));
  if (title_widget)
    g_return_if_fail (GTK_IS_WIDGET (title_widget));

  /* No need to do anything if the title widget stays the same */
  if (priv->title_widget == title_widget)
    return;

  g_clear_pointer (&priv->title_widget, gtk_widget_unparent);

  if (title_widget != NULL)
    {
      GtkLayoutManager *layout = gtk_widget_get_layout_manager (GTK_WIDGET (bar));
      priv->title_widget = title_widget;

      gtk_widget_insert_after (priv->title_widget, GTK_WIDGET (bar), priv->start_box);
      gtk_center_layout_set_center_widget (GTK_CENTER_LAYOUT (layout), title_widget);

      g_clear_pointer (&priv->title_label, gtk_widget_unparent);
    }
  else
    {
      if (priv->title_label == NULL)
        construct_title_label (bar);
    }

  g_object_notify_by_pspec (G_OBJECT (bar), header_bar_props[PROP_TITLE_WIDGET]);
}

/**
 * gtk_header_bar_get_title_widget:
 * @bar: a #GtkHeaderBar
 *
 * Retrieves the title widget of the header. See
 * gtk_header_bar_set_title_widget().
 *
 * Returns: (nullable) (transfer none): the title widget
 *    of the header, or %NULL if none has been set explicitly.
 */
GtkWidget *
gtk_header_bar_get_title_widget (GtkHeaderBar *bar)
{
  GtkHeaderBarPrivate *priv = gtk_header_bar_get_instance_private (bar);

  g_return_val_if_fail (GTK_IS_HEADER_BAR (bar), NULL);

  return priv->title_widget;
}

static void
gtk_header_bar_root (GtkWidget *widget)
{
  GtkWidget *root;

  GTK_WIDGET_CLASS (gtk_header_bar_parent_class)->root (widget);

  root = GTK_WIDGET (gtk_widget_get_root (widget));

  if (GTK_IS_WINDOW (root))
    g_signal_connect_swapped (root, "notify::title",
                              G_CALLBACK (update_title), widget);

  update_title (GTK_HEADER_BAR (widget));
}

static void
gtk_header_bar_unroot (GtkWidget *widget)
{
  g_signal_handlers_disconnect_by_func (gtk_widget_get_root (widget),
                                        update_title, widget);

  GTK_WIDGET_CLASS (gtk_header_bar_parent_class)->unroot (widget);
}

static void
gtk_header_bar_dispose (GObject *object)
{
  GtkHeaderBarPrivate *priv = gtk_header_bar_get_instance_private (GTK_HEADER_BAR (object));

  g_clear_pointer (&priv->title_widget, gtk_widget_unparent);
  g_clear_pointer (&priv->title_label, gtk_widget_unparent);

  G_OBJECT_CLASS (gtk_header_bar_parent_class)->dispose (object);

  g_clear_pointer (&priv->start_box, gtk_widget_unparent);
  g_clear_pointer (&priv->end_box, gtk_widget_unparent);
}

static void
gtk_header_bar_finalize (GObject *object)
{
  GtkHeaderBarPrivate *priv = gtk_header_bar_get_instance_private (GTK_HEADER_BAR (object));

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
    case PROP_TITLE_WIDGET:
      g_value_set_object (value, priv->title_widget);
      break;

    case PROP_SHOW_TITLE_BUTTONS:
      g_value_set_boolean (value, gtk_header_bar_get_show_title_buttons (bar));
      break;

    case PROP_DECORATION_LAYOUT:
      g_value_set_string (value, gtk_header_bar_get_decoration_layout (bar));
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

  switch (prop_id)
    {
    case PROP_TITLE_WIDGET:
      gtk_header_bar_set_title_widget (bar, g_value_get_object (value));
      break;

    case PROP_SHOW_TITLE_BUTTONS:
      gtk_header_bar_set_show_title_buttons (bar, g_value_get_boolean (value));
      break;

    case PROP_DECORATION_LAYOUT:
      gtk_header_bar_set_decoration_layout (bar, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_header_bar_pack (GtkHeaderBar *bar,
                     GtkWidget    *widget,
                     GtkPackType   pack_type)
{
  GtkHeaderBarPrivate *priv = gtk_header_bar_get_instance_private (bar);

  g_return_if_fail (gtk_widget_get_parent (widget) == NULL);

  if (pack_type == GTK_PACK_START)
    {
      gtk_container_add (GTK_CONTAINER (priv->start_box), widget);
    }
  else if (pack_type == GTK_PACK_END)
    {
      gtk_container_add (GTK_CONTAINER (priv->end_box), widget);
      gtk_box_reorder_child_after (GTK_BOX (priv->end_box), widget, NULL);
    }

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

  if (removed && priv->track_default_decoration)
    update_default_decoration (bar);
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

          if (w != priv->start_window_controls)
            (* callback) (w, callback_data);

          w = next;
        }
    }

  if (priv->title_widget != NULL)
    (* callback) (priv->title_widget, callback_data);

  if (priv->end_box)
    {
      w = _gtk_widget_get_first_child (priv->end_box);
      while (w != NULL)
        {
          GtkWidget *next = _gtk_widget_get_next_sibling (w);

          if (w != priv->end_window_controls)
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

  widget_class->root = gtk_header_bar_root;
  widget_class->unroot = gtk_header_bar_unroot;

  container_class->add = gtk_header_bar_add;
  container_class->remove = gtk_header_bar_remove;
  container_class->forall = gtk_header_bar_forall;
  container_class->child_type = gtk_header_bar_child_type;

  header_bar_props[PROP_TITLE_WIDGET] =
      g_param_spec_object ("title-widget",
                           P_("Title Widget"),
                           P_("Title widget to display"),
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

  priv->title_widget = NULL;
  priv->decoration_layout = NULL;
  priv->state = GDK_SURFACE_STATE_WITHDRAWN;

  layout = gtk_widget_get_layout_manager (GTK_WIDGET (bar));
  priv->start_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_add_css_class (priv->start_box, "start");
  gtk_widget_set_parent (priv->start_box, GTK_WIDGET (bar));
  gtk_center_layout_set_start_widget (GTK_CENTER_LAYOUT (layout), priv->start_box);
  priv->end_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_add_css_class (priv->end_box, "end");
  gtk_widget_set_parent (priv->end_box, GTK_WIDGET (bar));
  gtk_center_layout_set_end_widget (GTK_CENTER_LAYOUT (layout), priv->end_box);

  construct_title_label (bar);
}

static GtkBuildableIface *parent_buildable_iface;

static void
gtk_header_bar_buildable_add_child (GtkBuildable *buildable,
                                    GtkBuilder   *builder,
                                    GObject      *child,
                                    const gchar  *type)
{
  if (g_strcmp0 (type, "title") == 0)
    gtk_header_bar_set_title_widget (GTK_HEADER_BAR (buildable), GTK_WIDGET (child));
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

  if (setting)
    create_window_controls (bar);
  else
    {
      g_clear_pointer (&priv->start_window_controls, gtk_widget_unparent);
      g_clear_pointer (&priv->end_window_controls, gtk_widget_unparent);
    }

  g_object_notify_by_pspec (G_OBJECT (bar), header_bar_props[PROP_SHOW_TITLE_BUTTONS]);
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
 * maximize, close and icon (the window icon).
 *
 * For example, “icon:minimize,maximize,close” specifies a icon
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

  g_object_notify_by_pspec (G_OBJECT (bar), header_bar_props[PROP_DECORATION_LAYOUT]);
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
