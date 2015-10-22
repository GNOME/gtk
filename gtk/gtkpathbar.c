/* gtkpathbar.c
 *
 * Copyright (C) 2015 Red Hat
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Carlos Soriano <csoriano@gnome.org>
 */

#include "config.h"

#include "gtkmain.h"
#include "gtkpathbar.h"
#include "gtkstack.h"
#include "gtkentry.h"
#include "gtkbutton.h"
#include "gtktogglebutton.h"
#include "gtklabel.h"
#include "gtkbox.h"
#include "gtkmenubutton.h"
#include "gtkpopover.h"
#include "gtkbuilder.h"
#include "gtkstylecontext.h"
#include "gtkpathbarcontainerprivate.h"

#include "gtkintl.h"
#include "gtkmarshalers.h"
#include "gtktypebuiltins.h"
#include "gtkhidingboxprivate.h"

/**
 * SECTION:gtkpathbar
 * @Short_description: Widget that displays a path in UNIX format in a button-like manner
 * @Title: GtkPathBar
 * @See_also: #GtkFileChooser
 *
 * #GtkPathBar is a stock widget that displays a path in UNIX format in a way that
 * the user can interact with it, selecting part of it or providing menus for
 * every part of the path.
 *
 * Given the usual big lenght of paths, it conveniently manages the overflow of it,
 * hiding the parts of the path that doesn't have snouegh space to be displayed
 * in a overflow popover
 *
 * The #GtkPathBar does not perform any file handling.
 * If file handling is needed #GtkFilesPathBar is the correct widget.
 */

struct _GtkPathBarPrivate
{
  GtkWidget *path_bar_containers_stack;
  GtkWidget *path_bar_container_1;
  GtkWidget *path_bar_container_2;
  GtkWidget *path_box_1;
  GtkWidget *path_box_2;
  GtkWidget *overflow_button_1;
  GtkWidget *overflow_button_2;
  GtkWidget *path_chunk_popover_container;

  GIcon *root_icon;
  gchar *root_label;
  gchar *root_path;

  gchar *path;
  gchar *selected_path;
  gint inverted :1;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkPathBar, gtk_path_bar, GTK_TYPE_BIN)

enum {
  POPULATE_POPUP,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_PATH,
  PROP_SELECTED_PATH,
  PROP_INVERTED,
  LAST_PROP
};

static GParamSpec *path_bar_properties[LAST_PROP] = { NULL, };
static guint path_bar_signals[LAST_SIGNAL] = { 0 };

typedef struct {
  GtkWidget *button;
  GIcon *icon;
  gchar *label;
  gchar *path;
  GtkPathBar *path_bar;
} PathChunkData;

static void
emit_populate_popup (GtkPathBar  *self,
                     const gchar *path)
{
  GtkPathBarPrivate *priv = gtk_path_bar_get_instance_private (GTK_PATH_BAR (self));

  g_signal_emit (self, path_bar_signals[POPULATE_POPUP], 0,
                 priv->path_chunk_popover_container, path);
}

static void
get_path_bar_widgets (GtkPathBar *self,
                      GtkWidget  **path_bar,
                      GtkWidget  **overflow_button,
                      GtkWidget  **path_box,
                      gboolean    current)
{
  GtkPathBarPrivate *priv = gtk_path_bar_get_instance_private (GTK_PATH_BAR (self));
  GtkWidget *current_path_bar;

  current_path_bar = gtk_stack_get_visible_child (GTK_STACK (priv->path_bar_containers_stack));
  if (current_path_bar == NULL ||
      (current_path_bar == priv->path_bar_container_1 && current) ||
      (current_path_bar == priv->path_bar_container_2 && !current))
    {
      if (path_bar)
        *path_bar = priv->path_bar_container_1;
      if (overflow_button)
        *overflow_button = priv->overflow_button_1;
      if (path_box)
        *path_box = priv->path_box_1;
    }
  else
    {
      if (path_bar)
        *path_bar = priv->path_bar_container_2;
      if (overflow_button)
        *overflow_button = priv->overflow_button_2;
      if (path_box)
        *path_box = priv->path_box_2;
    }
}

static void
on_path_chunk_popover_destroyed (GtkPathBar *self)
{
  GtkPathBarPrivate *priv;

  if (self == NULL)
    return;

  priv = gtk_path_bar_get_instance_private (GTK_PATH_BAR (self));
  priv->path_chunk_popover_container = NULL;
}

static void
show_path_chunk_popover (GtkPathBar *self,
                         GtkWidget  *path_chunk)
{
  GtkPathBarPrivate *priv = gtk_path_bar_get_instance_private (GTK_PATH_BAR (self));
  GtkWidget *popover;

  if (priv->path_chunk_popover_container != NULL)
    {
      popover = gtk_widget_get_ancestor (priv->path_chunk_popover_container,
                                         GTK_TYPE_POPOVER);
      gtk_widget_destroy (popover);
    }

  popover = gtk_popover_new (path_chunk);
  priv->path_chunk_popover_container = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add (GTK_CONTAINER (popover), priv->path_chunk_popover_container);
  /* Clean private pointer when its destroyed, most of the times due to its
   * relative_to associated widget being destroyed */
  g_signal_connect_swapped (popover, "destroy",
                            G_CALLBACK (on_path_chunk_popover_destroyed), self);

  gtk_widget_show (popover);
}

static void
hide_path_chunk_popover_if_empty (GtkPathBar *self)
{
  GtkPathBarPrivate *priv = gtk_path_bar_get_instance_private (GTK_PATH_BAR (self));
  GList *children;
  GtkWidget *popover;

  children = gtk_container_get_children (GTK_CONTAINER (priv->path_chunk_popover_container));
  popover = gtk_widget_get_ancestor (priv->path_chunk_popover_container,
                                         GTK_TYPE_POPOVER);
  if (g_list_length (children) == 0)
    gtk_widget_destroy (popover);

  g_list_free (children);
}

static gboolean
on_path_chunk_button_release_event (GtkWidget      *path_chunk,
                                    GdkEventButton *event)
{
  PathChunkData *data;
  GtkPathBarPrivate *priv;

  data = g_object_get_data (G_OBJECT (path_chunk), "data");
  g_assert (data != NULL);

  priv = gtk_path_bar_get_instance_private (GTK_PATH_BAR (data->path_bar));

  if (event->type == GDK_BUTTON_RELEASE)
    {
      switch (event->button)
        {
        case GDK_BUTTON_PRIMARY:
          gtk_path_bar_set_selected_path (data->path_bar, data->path);

          return TRUE;

        case GDK_BUTTON_SECONDARY:
          show_path_chunk_popover (data->path_bar, path_chunk);
          emit_populate_popup (data->path_bar, data->path);
          hide_path_chunk_popover_if_empty (data->path_bar);

          return TRUE;

        default:
          break;
        }
    }

  return FALSE;
}

static void
free_path_chunk_data (PathChunkData *data)
{
  if (data->label)
    g_free (data->label);
  g_free (data->path);
  g_clear_object (&data->icon);
  g_slice_free (PathChunkData, data);
}

static GtkWidget *
create_path_chunk (GtkPathBar  *self,
                   const gchar *path,
                   const gchar *label,
                   GIcon       *icon,
                   gboolean     add_separator,
                   gboolean     separator_after_button)
{
  GtkWidget *button;
  GtkWidget *separator;
  GtkWidget *path_chunk;
  GtkWidget *button_label;
  GtkWidget *image;
  GtkStyleContext *style;
  PathChunkData *path_chunk_data;

  path_chunk = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  button = gtk_toggle_button_new ();

  if (icon)
    {
      image = gtk_image_new_from_gicon (icon, GTK_ICON_SIZE_MENU);
      gtk_button_set_image (GTK_BUTTON (button), image);
    }
  else if (label)
    {
      button_label = gtk_label_new (label);
      gtk_label_set_ellipsize (GTK_LABEL (button_label), PANGO_ELLIPSIZE_MIDDLE);
      // FIXME: the GtkLabel requests more than the number of chars set here.
      // For visual testing for now substract 2 chars.
      gtk_label_set_width_chars (GTK_LABEL (button_label),
                                 MIN (strlen (label), 10));
      gtk_container_add (GTK_CONTAINER (button), button_label);
    }
  else
    {
      g_critical ("Path chunk doesn't provide either icon or label");
    }

  style = gtk_widget_get_style_context (button);
  gtk_style_context_add_class (style, "flat");

  g_signal_connect_swapped (button, "button-release-event",
                            G_CALLBACK (on_path_chunk_button_release_event), path_chunk);

  if (add_separator && !separator_after_button)
    {
      separator = gtk_label_new (G_DIR_SEPARATOR_S);
      gtk_widget_set_sensitive (separator, FALSE);
      gtk_container_add (GTK_CONTAINER (path_chunk), separator);
    }
  gtk_container_add (GTK_CONTAINER (path_chunk), button);
  if (add_separator && separator_after_button)
    {
      separator = gtk_label_new (G_DIR_SEPARATOR_S);
      gtk_widget_set_sensitive (separator, FALSE);
      gtk_container_add (GTK_CONTAINER (path_chunk), separator);
    }

  path_chunk_data = g_slice_new (PathChunkData);
  if (label)
    path_chunk_data->label = g_strdup (label);
  else
    path_chunk_data->label = NULL;

  if (icon)
    path_chunk_data->icon = g_object_ref (icon);
  else
    path_chunk_data->icon = NULL;

  path_chunk_data->path = g_strdup (path);
  path_chunk_data->path_bar = self;
  path_chunk_data->button = button;
  g_object_set_data_full (G_OBJECT (path_chunk), "data",
                          path_chunk_data, (GDestroyNotify) free_path_chunk_data);

  gtk_widget_show_all (path_chunk);

  return path_chunk;
}

static gchar**
get_splitted_path (const gchar* path)
{
  gchar *path_no_first_slash;
  gchar **splitted_path;

  path_no_first_slash = g_utf8_substring (path, 1, strlen (path));
  splitted_path = g_strsplit (path_no_first_slash, G_DIR_SEPARATOR_S, -1);

  g_free (path_no_first_slash);

  return splitted_path;
}

static gboolean
is_absolute_root (const gchar *path)
{
  return g_strcmp0 (path, G_DIR_SEPARATOR_S) == 0;
}

static gboolean
validate_path (const gchar *path)
{
  GFile *file = NULL;
  gboolean valid = FALSE;

  if (!path || strlen (path) == 0)
    goto out;

  if (!g_utf8_validate (path, -1, NULL))
    goto out;

  /* Special case absolute root which is always valid */
  if (is_absolute_root (path))
    {
      valid = TRUE;
      goto out;
    }

  file = g_file_new_for_uri (path);
  if (!file)
    goto out;

  valid = TRUE;

out:
  g_clear_object (&file);

  return valid;
}

static gboolean
validate_root_path (const gchar *path,
                    const gchar *root_path)
{
  return validate_path (root_path) &&
         (g_str_has_prefix (path, root_path) || g_strcmp0 (path, root_path) == 0);
}

static void
update_path_bar (GtkPathBar  *self)
{
  GtkPathBarPrivate *priv = gtk_path_bar_get_instance_private (self);
  GtkWidget *path_chunk;
  GtkWidget *path_box;
  GtkWidget *overflow_button;
  GtkWidget *path_bar;
  GtkWidget *root_chunk;
  gchar *unprefixed_path;

  get_path_bar_widgets (GTK_PATH_BAR (self), &path_bar, &overflow_button, &path_box, FALSE);

  /* Make sure we dismiss all popovers */
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->overflow_button_1), FALSE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->overflow_button_2), FALSE);

  gtk_container_foreach (GTK_CONTAINER (path_box), (GtkCallback) gtk_widget_destroy, NULL);

  if (priv->root_path)
    {
      root_chunk = create_path_chunk (self, priv->root_path, priv->root_label,
                                      priv->root_icon, TRUE, TRUE);

      gtk_container_add (GTK_CONTAINER (path_box), root_chunk);
    }

  if (g_strcmp0 (priv->root_path, priv->path) == 0)
    goto done;

  /* We always expect a path in the format /path/path in UNIX or \path\path in Windows.
   * However, the OS separator alone is a valid path, so we need to handle it
   * ourselves if the client didn't set a root label or icon for it.
   */
  if (!is_absolute_root (priv->path) || priv->root_path)
    {
      GString *current_path = NULL;
      gchar **splitted_path;
      gboolean add_separator;
      gboolean separator_after_button;
      gint length;
      gint i;

      current_path = g_string_new ("");
      if (priv->root_path && !is_absolute_root (priv->root_path))
        {
          g_string_append (current_path, priv->root_path);
          unprefixed_path = g_utf8_substring (priv->path, strlen (priv->root_path),
                                              strlen (priv->path));
        }
      else
        {
          unprefixed_path = g_strdup (priv->path);
        }

      splitted_path = get_splitted_path (unprefixed_path);
      length = g_strv_length (splitted_path);
      for (i = 0; i < length; i++)
        {
          g_string_append (current_path, G_DIR_SEPARATOR_S);
          g_string_append (current_path, splitted_path[i]);

          /* We add a separator for all items except the last one, which will result
           * in a pathbar in the form of "Home/Documents/Example".
           * However, if only one item is present, add a separator at the end since
           * is visually more pleasant. The result will be in the form of "Home/" */
          add_separator = length == 1 || (i != length - 1 && priv->inverted) ||
                          (i != 0 && !priv->inverted);
          separator_after_button = length == 1 || priv->inverted;

          path_chunk = create_path_chunk (self, current_path->str, splitted_path[i],
                                          NULL, add_separator, separator_after_button);
          gtk_container_add (GTK_CONTAINER (path_box), path_chunk);
        }

      g_strfreev (splitted_path);
      g_string_free (current_path, TRUE);
      g_free (unprefixed_path);
    }
  else
    {
      path_chunk = create_path_chunk (self, G_DIR_SEPARATOR_S, G_DIR_SEPARATOR_S,
                                      NULL, FALSE, FALSE);
      gtk_container_add (GTK_CONTAINER (path_box), path_chunk);
    }

done:
  gtk_stack_set_visible_child (GTK_STACK (priv->path_bar_containers_stack), path_bar);
}

static void
update_selected_path (GtkPathBar  *self)
{
  GtkPathBarPrivate *priv = gtk_path_bar_get_instance_private (self);
  PathChunkData *data;
  GList *children;
  GList *overflow_children;
  GList *l;
  GtkWidget *path_box;
  GtkWidget *overflow_button;
  GtkPopover *overflow_popover;
  GtkWidget *overflow_container;

  get_path_bar_widgets (GTK_PATH_BAR (self), NULL, &overflow_button, &path_box, TRUE);
  overflow_popover = gtk_menu_button_get_popover (GTK_MENU_BUTTON (overflow_button));
  overflow_container = gtk_bin_get_child (GTK_BIN (overflow_popover));
  children = gtk_container_get_children (GTK_CONTAINER (path_box));
  overflow_children = gtk_container_get_children (GTK_CONTAINER (overflow_container));
  for (l = children; l != NULL; l = l->next)
    {
      data = g_object_get_data (G_OBJECT (l->data), "data");
      g_signal_handlers_block_by_func (data->button,
                                       G_CALLBACK (on_path_chunk_button_release_event),
                                       data);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (data->button),
                                    g_strcmp0 (data->path, priv->selected_path) == 0);
      g_signal_handlers_unblock_by_func (data->button,
                                         G_CALLBACK (on_path_chunk_button_release_event),
                                         data);
    }

  for (l = overflow_children; l != NULL; l = l->next)
    {
      data = g_object_get_data (G_OBJECT (l->data), "data");
      g_signal_handlers_block_by_func (data->button,
                                       G_CALLBACK (on_path_chunk_button_release_event),
                                       data);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (data->button),
                                    g_strcmp0 (data->path, priv->selected_path) == 0);
      g_signal_handlers_unblock_by_func (data->button,
                                         G_CALLBACK (on_path_chunk_button_release_event),
                                         data);
    }

  g_list_free (children);
  g_list_free (overflow_children);
}

static void
populate_overflow_popover (GtkPathBar *self)
{
  GList *overflow_children;
  GList *l;
  PathChunkData *data;
  GtkWidget *path_chunk;
  GtkWidget *path_box;
  GtkWidget *overflow_button;
  GtkPopover *overflow_popover;
  GtkWidget *overflow_container;

  get_path_bar_widgets (self, NULL, &overflow_button, &path_box, TRUE);
  overflow_popover = gtk_menu_button_get_popover (GTK_MENU_BUTTON (overflow_button));
  overflow_container = gtk_bin_get_child (GTK_BIN (overflow_popover));

  gtk_container_foreach (GTK_CONTAINER (overflow_container),
                         (GtkCallback) gtk_widget_destroy, NULL);

  overflow_children = gtk_hiding_box_get_overflow_children (GTK_HIDING_BOX (path_box));
  for (l = overflow_children; l != NULL; l = l->next)
    {
      data = g_object_get_data (l->data, "data");
      path_chunk = create_path_chunk (self, data->path, data->label, data->icon, FALSE, FALSE);
      gtk_container_add (GTK_CONTAINER (overflow_container), path_chunk);
    }

  update_selected_path (self);

  g_list_free (overflow_children);
}

static void
gtk_path_bar_finalize (GObject *object)
{
  GtkPathBar *self = (GtkPathBar *)object;
  GtkPathBarPrivate *priv = gtk_path_bar_get_instance_private (self);

  if (priv->path)
    g_free (priv->path);
  if (priv->selected_path)
    g_free (priv->selected_path);

  G_OBJECT_CLASS (gtk_path_bar_parent_class)->finalize (object);
}

static void
gtk_path_bar_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  GtkPathBar *self = GTK_PATH_BAR (object);
  GtkPathBarPrivate *priv = gtk_path_bar_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_PATH:
      g_value_set_string (value, priv->path);
      break;
    case PROP_SELECTED_PATH:
      g_value_set_string (value, priv->selected_path);
      break;
    case PROP_INVERTED:
      g_value_set_boolean (value, priv->inverted);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_path_bar_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  GtkPathBar *self = GTK_PATH_BAR (object);

  switch (prop_id)
    {
    case PROP_PATH:
      gtk_path_bar_set_path (self, g_value_get_string (value));
      break;
    case PROP_SELECTED_PATH:
      gtk_path_bar_set_selected_path (self, g_value_get_string (value));
      break;
    case PROP_INVERTED:
      gtk_path_bar_set_inverted (self, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static GtkSizeRequestMode
get_request_mode (GtkWidget *self)
{
  return GTK_SIZE_REQUEST_WIDTH_FOR_HEIGHT;
}

static void
gtk_path_bar_class_init (GtkPathBarClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gtk_path_bar_finalize;
  object_class->get_property = gtk_path_bar_get_property;
  object_class->set_property = gtk_path_bar_set_property;

  widget_class->get_request_mode = get_request_mode;

  /**
   * GtkPathBar::populate-popup:
   * @path_bar: the object which received the signal.
   * @container: (type Gtk.Widget): a #GtkContainer
   * @path: (type const gchar*): string of the path where the user performed a right click.
   *
   * The path bar emits this signal when the user invokes a contextual
   * popup on one of its items. In the signal handler, the application may
   * add extra items to the menu as appropriate. For example, a file manager
   * may want to add a "Properties" command to the menu.
   *
   * The @container and all its contents are destroyed after the user
   * dismisses the popup. The popup is re-created (and thus, this signal is
   * emitted) every time the user activates the contextual menu.
   *
   * Since: 3.20
   */
  path_bar_signals [POPULATE_POPUP] =
          g_signal_new (I_("populate-popup"),
                        G_OBJECT_CLASS_TYPE (object_class),
                        G_SIGNAL_RUN_FIRST,
                        G_STRUCT_OFFSET (GtkPathBarClass, populate_popup),
                        NULL, NULL,
                        _gtk_marshal_VOID__OBJECT_STRING,
                        G_TYPE_NONE, 2,
                        GTK_TYPE_WIDGET,
                        G_TYPE_STRING);

  path_bar_properties[PROP_PATH] =
          g_param_spec_string ("path",
                               P_("Path"),
                               P_("The path set in the path bar. Should use UNIX path specs"),
                               NULL,
                               G_PARAM_READWRITE);

  path_bar_properties[PROP_SELECTED_PATH] =
          g_param_spec_string ("selected-path",
                               P_("Selected path"),
                               P_("The path selected. Should be a sufix of the path currently set"),
                               NULL,
                               G_PARAM_READWRITE);

  path_bar_properties[PROP_INVERTED] =
          g_param_spec_boolean ("inverted",
                                P_("Direction of hiding children inverted"),
                                P_("If false the container will start hiding widgets from the end when there is not enough space, and the oposite in case inverted is true."),
                                FALSE,
                                G_PARAM_READWRITE);

  g_object_class_install_properties (object_class, LAST_PROP, path_bar_properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/libgtk/ui/gtkpathbar.ui");

  gtk_widget_class_bind_template_child_private (widget_class, GtkPathBar, path_bar_containers_stack);
  gtk_widget_class_bind_template_child_private (widget_class, GtkPathBar, path_bar_container_1);
  gtk_widget_class_bind_template_child_private (widget_class, GtkPathBar, path_bar_container_2);
  gtk_widget_class_bind_template_child_private (widget_class, GtkPathBar, overflow_button_1);
  gtk_widget_class_bind_template_child_private (widget_class, GtkPathBar, overflow_button_2);
  gtk_widget_class_bind_template_child_private (widget_class, GtkPathBar, path_box_1);
  gtk_widget_class_bind_template_child_private (widget_class, GtkPathBar, path_box_2);

  gtk_widget_class_bind_template_callback (widget_class, populate_overflow_popover);

  gtk_widget_class_set_css_name (widget_class, "path-bar");
}

static void
gtk_path_bar_init (GtkPathBar *self)
{
  GtkPathBarPrivate *priv = gtk_path_bar_get_instance_private (self);

  g_type_ensure (GTK_TYPE_PATH_BAR_CONTAINER);
  gtk_widget_init_template (GTK_WIDGET (self));

  priv->inverted = FALSE;
}


void
gtk_path_bar_set_path_extended (GtkPathBar  *self,
                                const gchar *path,
                                const gchar *root_path,
                                const gchar *root_label,
                                GIcon       *root_icon)
{
  GtkPathBarPrivate *priv;
  gchar *old_path;

  g_return_if_fail (GTK_IS_PATH_BAR (self));
  g_return_if_fail (validate_path (path));
  g_return_if_fail (!root_path || validate_root_path (path, root_path));

  priv = gtk_path_bar_get_instance_private (GTK_PATH_BAR (self));

  if (priv->root_icon)
    {
      g_object_unref (priv->root_icon);
      priv->root_icon = NULL;
    }
  if (priv->root_path)
    {
      g_free (priv->root_path);
      priv->root_path = NULL;
    }
  if (priv->root_label)
    {
      g_free (priv->root_label);
      priv->root_label = NULL;
    }

  if (root_icon)
    priv->root_icon = g_object_ref (root_icon);
  if (root_path)
    priv->root_path = g_strdup (root_path);
  if (root_label)
    priv->root_label = g_strdup (root_label);

  old_path = priv->path;
  priv->path = g_strdup (path);

  update_path_bar (self);

  if (old_path)
    {
      if (g_strcmp0 (old_path, path) != 0)
        g_object_notify_by_pspec (G_OBJECT (self), path_bar_properties[PROP_PATH]);
      g_free (old_path);
    }

  gtk_path_bar_set_selected_path (self, priv->path);
}

/**
 * gtk_path_bar_get_selected_path:
 * @path_bar: a #GtkPathBar
 *
 * Get the path represented by the path bar
 *
 * Returns: (transfer none): a string that represents the current path.
 *
 * Since: 3.20
 */
const gchar*
gtk_path_bar_get_path (GtkPathBar *self)
{
  GtkPathBarPrivate *priv;

  g_return_val_if_fail (GTK_IS_PATH_BAR (self), NULL);

  priv = gtk_path_bar_get_instance_private (GTK_PATH_BAR (self));

  return priv->path;
}

/**
 * gtk_path_bar_set_path:
 * @path_bar: a #GtkPathBar
 * @path: a string representing a path in UNIX style.
 *
 * Set the path represented by the path bar.
 *
 * Note that if the path is not in the UNIX format behaviour is undefined,
 * however the path bar will try to represent it, and therefore the path set
 * by this function could be different than the actual one.
 *
 * Since: 3.20
 */
void
gtk_path_bar_set_path (GtkPathBar  *self,
                       const gchar *path)
{
  gtk_path_bar_set_path_extended (self, path, NULL, NULL, NULL);
}

/**
 * gtk_path_bar_get_selected_path:
 * @path_bar: a #GtkPathBar
 *
 * Get the selected path
 *
 * Returns: (transfer none): a string that represents the selected path.
 *
 * Since: 3.20
 */
const gchar*
gtk_path_bar_get_selected_path (GtkPathBar *self)
{
  GtkPathBarPrivate *priv;

  g_return_val_if_fail (GTK_IS_PATH_BAR (self), NULL);

  priv = gtk_path_bar_get_instance_private (GTK_PATH_BAR (self));

  return priv->selected_path;
}

/**
 * gtk_path_bar_set_selected_path:
 * @path_bar: a #GtkPathBar
 * @path: a string representing a path in UNIX style.
 *
 * Set the path selected by the path bar.
 *
 * The path must be a prefix of the current #path.
 *
 * Since: 3.20
 */
void
gtk_path_bar_set_selected_path (GtkPathBar  *self,
                                const gchar *path)
{
  GtkPathBarPrivate *priv ;

  g_return_if_fail (GTK_IS_PATH_BAR (self));

  priv = gtk_path_bar_get_instance_private (GTK_PATH_BAR (self));

  g_return_if_fail (g_str_has_prefix (priv->path, path) ||
                    g_strcmp0 (priv->path, path) == 0);

  if (g_strcmp0 (priv->selected_path, path) != 0)
    {
      if (priv->selected_path)
        g_free (priv->selected_path);

      priv->selected_path = g_strdup (path);
      update_selected_path (self);
      g_object_notify_by_pspec (G_OBJECT (self), path_bar_properties[PROP_SELECTED_PATH]);
    }
  else
    {
      /* Update the style in any case */
      update_selected_path (self);
    }
}

/**
 * gtk_path_bar_get_inverted:
 * @path_bar: a #GtkPathBar
 *
 * Returns a wheter the path bar hides children in the inverted direction.
 *
 * Since: 3.20
 */
gboolean
gtk_path_bar_get_inverted (GtkPathBar *self)
{
  GtkPathBarPrivate *priv ;

  g_return_val_if_fail (GTK_IS_PATH_BAR (self), 0);

  priv = gtk_path_bar_get_instance_private (GTK_PATH_BAR (self));

  return priv->inverted;
}

/**
 * gtk_path_bar_set_inverted:
 * @path_bar: a #GtkPathBar
 * @inverted: Wheter the path bar will start hiding widgets in the oposite direction.
 *
 * If %FALSE, the path bar will start hiding widgets from the end of it, and from
 * the start in the oposite case.
 *
 * Since: 3.20
 */
void
gtk_path_bar_set_inverted (GtkPathBar *self,
                           gboolean    inverted)
{
  GtkPathBarPrivate *priv ;

  g_return_if_fail (GTK_IS_PATH_BAR (self));

  priv = gtk_path_bar_get_instance_private (GTK_PATH_BAR (self));

  if (priv->inverted != inverted)
    {
      priv->inverted = inverted != FALSE;
      gtk_hiding_box_set_inverted (GTK_HIDING_BOX (priv->path_box_1), inverted);
      gtk_hiding_box_set_inverted (GTK_HIDING_BOX (priv->path_box_2), inverted);

      g_object_notify (G_OBJECT (self), "inverted");
    }
}

GtkWidget *
gtk_path_bar_new (void)
{
  return g_object_new (GTK_TYPE_PATH_BAR, NULL);
}
