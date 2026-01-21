/*
 * Copyright © 2025 Red Hat, Inc
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
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Authors: Matthias Clasen <mclasen@redhat.com>
 */

#include "path-editor.h"


struct _PathEditor
{
  GtkWidget parent_instance;

  double width;
  double height;
  GskPath *path;

  GtkStack *path_cmds_stack;
  GtkLabel *path_cmds_label;
  GtkEntry *path_cmds_entry;
};

enum
{
  PROP_PATH = 1,
  PROP_WIDTH,
  PROP_HEIGHT,
  NUM_PROPERTIES,
};

static GParamSpec *properties[NUM_PROPERTIES];

/* {{{ Path-to-SVG */

static gboolean
collect_path (GskPathOperation        op,
              const graphene_point_t *pts,
              size_t                  n_pts,
              float                   weight,
              gpointer                user_data)
{
  GskPathBuilder *builder = user_data;

  switch (op)
    {
    case GSK_PATH_MOVE:
      gsk_path_builder_move_to (builder, pts[0].x, pts[0].y);
      break;

    case GSK_PATH_CLOSE:
      gsk_path_builder_close (builder);
      break;

    case GSK_PATH_LINE:
      gsk_path_builder_line_to (builder, pts[1].x, pts[1].y);
      break;

    case GSK_PATH_QUAD:
      gsk_path_builder_quad_to (builder, pts[1].x, pts[1].y,
                                         pts[2].x, pts[2].y);
      break;

    case GSK_PATH_CUBIC:
      gsk_path_builder_cubic_to (builder, pts[1].x, pts[1].y,
                                          pts[2].x, pts[2].y,
                                          pts[3].x, pts[3].y);
      break;

    case GSK_PATH_CONIC:
      gsk_path_builder_conic_to (builder, pts[1].x, pts[1].y,
                                          pts[2].x, pts[2].y,
                                          weight);
      break;

    default:
      g_assert_not_reached ();
    }

  return TRUE;
}

static GskPath *
path_to_svg_path (GskPath *path)
{
  GskPathBuilder *builder = gsk_path_builder_new ();

  gsk_path_foreach (path,
                    GSK_PATH_FOREACH_ALLOW_QUAD | GSK_PATH_FOREACH_ALLOW_CUBIC,
                    collect_path,
                    builder);

  return gsk_path_builder_free_to_path (builder);
}

/* }}} */
/* {{{ Callbacks */

static void
path_cmds_clicked (PathEditor *self)
{
  gtk_stack_set_visible_child_name (self->path_cmds_stack, "entry");

  gtk_entry_grab_focus_without_selecting (self->path_cmds_entry);
}

static gboolean
path_cmds_key (PathEditor      *self,
               unsigned int     keyval,
               unsigned int     keycode,
               GdkModifierType  state)
{
  if (keyval == GDK_KEY_Escape)
    {
      g_autofree char *text = NULL;

      text = gsk_path_to_string (self->path);
      gtk_editable_set_text (GTK_EDITABLE (self->path_cmds_entry), text);

      gtk_widget_remove_css_class (GTK_WIDGET (self->path_cmds_entry), "error");
      gtk_accessible_reset_state (GTK_ACCESSIBLE (self->path_cmds_entry), GTK_ACCESSIBLE_STATE_INVALID);
      gtk_stack_set_visible_child_name (self->path_cmds_stack, "label");
      return TRUE;
    }

  return FALSE;
}

static void
path_cmds_activated (PathEditor *self)
{
  const char *text;
  g_autoptr (GskPath) path = NULL;

  text = gtk_editable_get_text (GTK_EDITABLE (self->path_cmds_entry));
  path = gsk_path_parse (text);

  if (!path)
    {
      gtk_widget_error_bell (GTK_WIDGET (self->path_cmds_entry));
      gtk_widget_add_css_class (GTK_WIDGET (self->path_cmds_entry), "error");
      gtk_accessible_update_state (GTK_ACCESSIBLE (self->path_cmds_entry),
                                   GTK_ACCESSIBLE_STATE_INVALID, GTK_ACCESSIBLE_INVALID_TRUE,
                                   -1);
      return;
    }
  else
    {
      gtk_widget_remove_css_class (GTK_WIDGET (self->path_cmds_entry), "error");
      gtk_accessible_reset_state (GTK_ACCESSIBLE (self->path_cmds_entry), GTK_ACCESSIBLE_STATE_INVALID);
      gtk_label_set_label (self->path_cmds_label, text);
      gtk_stack_set_visible_child_name (self->path_cmds_stack, "label");
    }

  gsk_path_unref (self->path);
  self->path = gsk_path_ref (path);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PATH]);
}

static void
show_error (PathEditor *self,
            const char *title,
            const char *detail)
{
  GtkWindow *window = GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (self)));
  g_autoptr (GtkAlertDialog) alert = NULL;

  alert = gtk_alert_dialog_new ("title");
  gtk_alert_dialog_set_detail (alert, detail);
  gtk_alert_dialog_show (alert, window);
}

static void
temp_file_changed (GFileMonitor      *monitor,
                   GFile             *file,
                   GFile             *other,
                   GFileMonitorEvent  event,
                   PathEditor        *self)
{
  if (event == G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT)
    {
      g_autoptr (GBytes) bytes = NULL;
      g_autoptr (GError) error = NULL;
      g_autoptr (PathPaintable) paintable = NULL;

      bytes = g_file_load_bytes (file, NULL, NULL, &error);
      if (!bytes)
        {
          show_error (self, "Editing Failed", error->message);
          return;
        }

      paintable = path_paintable_new_from_bytes (bytes, &error);
      if (!paintable)
        {
          show_error (self, "Editing Failed", error->message);
          return;
        }

      gsk_path_unref (self->path);
      self->path = gsk_path_ref (path_paintable_get_path_by_id (paintable, "path0"));

      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PATH]);
    }
}

static void
file_launched (GObject      *source,
               GAsyncResult *result,
               gpointer      data)
{
  GtkFileLauncher *launcher = GTK_FILE_LAUNCHER (source);
  g_autoptr (GError) error = NULL;

  if (!gtk_file_launcher_launch_finish (launcher, result, &error))
    {
      g_print ("Failed to launch path editor: %s", error->message);
    }
}

static void
edit_path_externally (PathEditor *self)
{
  GString *str;
  g_autoptr (GBytes) bytes = NULL;
  g_autofree char *filename = NULL;
  g_autoptr (GFile) file = NULL;
  g_autoptr (GOutputStream) ostream = NULL;
  gssize written;
  g_autoptr (GError) error = NULL;
  GFileMonitor *monitor;
  g_autoptr (GskPath) path = NULL;
  g_autoptr(GtkFileLauncher) launcher = NULL;
  GtkRoot *root = gtk_widget_get_root (GTK_WIDGET (self));

  path = path_to_svg_path (self->path);

  str = g_string_new ("");
  g_string_append_printf (str, "<svg width='%g' height='%g'>\n",
                          self->width, self->height);

  g_string_append (str, "<path id='path0'\n"
                        "      d='");
  if (path)
    gsk_path_print (path, str);

  g_string_append (str, "'\n");

  g_string_append (str,
                   "      fill='none'\n"
                   "      stroke='black'\n"
                   "      stroke-width='2'\n"
                   "      stroke-linejoin='round'\n"
                   "      stroke-linecap='round'/>\n");

  g_string_append (str, "</svg>");
  bytes = g_string_free_to_bytes (str);

  filename = g_build_filename (g_get_user_cache_dir (), "org.gtk.Shaper-path.svg", NULL);
  file = g_file_new_for_path (filename);
  ostream = G_OUTPUT_STREAM (g_file_replace (file, NULL, FALSE, G_FILE_CREATE_NONE, NULL, &error));
  if (!ostream)
    {
      show_error (self, "Editing Failed", error->message);
      return;
    }

  written = g_output_stream_write_bytes (ostream, bytes, NULL, &error);
  if (written == -1)
    {
      show_error (self, "Editing Failed", error->message);
      return;
    }

  g_output_stream_close (ostream, NULL, NULL);

  monitor = g_file_monitor_file (file, G_FILE_MONITOR_NONE, NULL, &error);
  if (error)
    {
      show_error (self, "Editing Failed", error->message);
      return;
    }

  g_signal_connect_object (monitor, "changed",
                           G_CALLBACK (temp_file_changed), self, G_CONNECT_DEFAULT);

  launcher = gtk_file_launcher_new (file);

  gtk_file_launcher_set_writable (launcher, TRUE);

  gtk_file_launcher_launch (launcher, GTK_WINDOW (root), NULL, file_launched, self);
}

static void
edit_path (PathEditor *self)
{
  edit_path_externally (self);
}

/* }}} */
/* {{{ GObject boilerplate */

struct _PathEditorClass
{
  GtkWidgetClass parent_class;
};

G_DEFINE_TYPE (PathEditor, path_editor, GTK_TYPE_WIDGET)

static void
path_editor_init (PathEditor *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

static void
path_editor_set_property (GObject      *object,
                          unsigned int  prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  PathEditor *self = PATH_EDITOR (object);

  switch (prop_id)
    {
    case PROP_PATH:
      path_editor_set_path (self, g_value_get_boxed (value));
      break;

    case PROP_WIDTH:
      self->width = g_value_get_double (value);
      break;

    case PROP_HEIGHT:
      self->height = g_value_get_double (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
path_editor_get_property (GObject      *object,
                          unsigned int  prop_id,
                          GValue       *value,
                          GParamSpec   *pspec)
{
  PathEditor *self = PATH_EDITOR (object);

  switch (prop_id)
    {
    case PROP_PATH:
      g_value_set_boxed (value, self->path);
      break;

    case PROP_WIDTH:
      g_value_set_double (value, self->width);
      break;

    case PROP_HEIGHT:
      g_value_set_double (value, self->height);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
path_editor_dispose (GObject *object)
{
  gtk_widget_dispose_template (GTK_WIDGET (object), PATH_EDITOR_TYPE);

  G_OBJECT_CLASS (path_editor_parent_class)->dispose (object);
}

static void
path_editor_finalize (GObject *object)
{
  PathEditor *self = PATH_EDITOR (object);

  g_clear_pointer (&self->path, gsk_path_unref);

  G_OBJECT_CLASS (path_editor_parent_class)->finalize (object);
}

static void
path_editor_class_init (PathEditorClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->set_property = path_editor_set_property;
  object_class->get_property = path_editor_get_property;
  object_class->dispose = path_editor_dispose;
  object_class->finalize = path_editor_finalize;

  properties[PROP_PATH] =
    g_param_spec_boxed ("path", NULL, NULL,
                        GSK_TYPE_PATH,
                        G_PARAM_READWRITE | G_PARAM_STATIC_NAME);

  properties[PROP_WIDTH] =
    g_param_spec_double ("width", NULL, NULL,
                         0, DBL_MAX, 0,
                         G_PARAM_READWRITE | G_PARAM_STATIC_NAME);

  properties[PROP_HEIGHT] =
    g_param_spec_double ("height", NULL, NULL,
                         0, DBL_MAX, 0,
                         G_PARAM_READWRITE | G_PARAM_STATIC_NAME);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/Shaper/path-editor.ui");

  gtk_widget_class_bind_template_child (widget_class, PathEditor, path_cmds_stack);
  gtk_widget_class_bind_template_child (widget_class, PathEditor, path_cmds_label);
  gtk_widget_class_bind_template_child (widget_class, PathEditor, path_cmds_entry);

  gtk_widget_class_bind_template_callback (widget_class, path_cmds_clicked);
  gtk_widget_class_bind_template_callback (widget_class, path_cmds_activated);
  gtk_widget_class_bind_template_callback (widget_class, path_cmds_key);
  gtk_widget_class_bind_template_callback (widget_class, edit_path);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
}

/* }}} */
/* {{{ Public API */

PathEditor *
path_editor_new (GskPath *path,
                 double   width,
                 double   height)
{
  return g_object_new (PATH_EDITOR_TYPE,
                       "path", path,
                       "width", width,
                       "height", height,
                       NULL);
}

GskPath *
path_editor_get_path (PathEditor *self)
{
  g_return_val_if_fail (PATH_IS_EDITOR (self), NULL);

  return self->path;
}

void
path_editor_set_path (PathEditor *self,
                      GskPath    *path)
{
  if (self->path == path ||
      (self->path && path && gsk_path_equal (self->path, path)))
    return;

  g_clear_pointer (&self->path, gsk_path_unref);
  self->path = gsk_path_ref (path);

  if (gsk_path_is_empty (path))
    {
      gtk_label_set_label (self->path_cmds_label, "—");
      gtk_editable_set_text (GTK_EDITABLE (self->path_cmds_entry), "");
    }
  else
    {
      g_autofree char *text = gsk_path_to_string (path);
      gtk_label_set_label (self->path_cmds_label, text);
      gtk_editable_set_text (GTK_EDITABLE (self->path_cmds_entry), text);
    }

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PATH]);
}

/* }}} */

/* vim:set foldmethod=marker: */
