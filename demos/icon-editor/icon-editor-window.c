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

#include "icon-editor-window.h"

#include "state-editor.h"
#include "paintable-editor.h"
#include "path-paintable-private.h"
#include "border-paintable.h"

#include <glib/gstdio.h>


struct _IconEditorWindow
{
  GtkApplicationWindow parent;

  gboolean importing;
  gboolean exporting;
  GFile *file;
  PathPaintable *paintable;
  PathPaintable *orig_paintable;
  gboolean changed;
  gboolean show_controls;
  gboolean show_thumbnails;
  gboolean show_bounds;
  gboolean invert_colors;
};

struct _IconEditorWindowClass
{
  GtkApplicationWindowClass parent_class;
};

enum
{
  PROP_PAINTABLE = 1,
  PROP_CHANGED,
  PROP_SHOW_CONTROLS,
  PROP_SHOW_THUMBNAILS,
  PROP_SHOW_BOUNDS,
  PROP_INVERT_COLORS,
  NUM_PROPERTIES,
};

static GParamSpec *properties[NUM_PROPERTIES];

G_DEFINE_TYPE(IconEditorWindow, icon_editor_window, GTK_TYPE_APPLICATION_WINDOW);

/* {{{ Setters */

static void
icon_editor_window_set_show_controls (IconEditorWindow *self,
                                      gboolean          show_controls)
{
  if (self->show_controls == show_controls)
    return;

  self->show_controls = show_controls;
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SHOW_CONTROLS]);
}

static void
icon_editor_window_set_show_thumbnails (IconEditorWindow *self,
                                        gboolean          show_thumbnails)
{
  if (self->show_thumbnails == show_thumbnails)
    return;

  self->show_thumbnails = show_thumbnails;
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SHOW_THUMBNAILS]);
}

static void
icon_editor_window_set_show_bounds (IconEditorWindow *self,
                                    gboolean          show_bounds)
{
  if (self->show_bounds == show_bounds)
    return;

  self->show_bounds = show_bounds;
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SHOW_BOUNDS]);
}

static void
icon_editor_window_set_invert_colors (IconEditorWindow *self,
                                      gboolean          invert_colors)
{
  GtkSettings *settings;

  if (self->invert_colors == invert_colors)
    return;

  self->invert_colors = invert_colors;

  settings = gtk_widget_get_settings (GTK_WIDGET (self));

  if (invert_colors)
    {
      GtkInterfaceColorScheme color_scheme;

      g_object_get (settings, "gtk-interface-color-scheme", &color_scheme, NULL);
      if (color_scheme == GTK_INTERFACE_COLOR_SCHEME_DARK)
        color_scheme = GTK_INTERFACE_COLOR_SCHEME_LIGHT;
      else
        color_scheme = GTK_INTERFACE_COLOR_SCHEME_DARK;
      g_object_set (settings, "gtk-interface-color-scheme", color_scheme, NULL);
    }
  else
    gtk_settings_reset_property (settings, "gtk-interface-color-scheme");

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_INVERT_COLORS]);
}

static void
icon_editor_window_set_changed (IconEditorWindow *self,
                                gboolean          changed)
{
  GAction *action;

  if (self->changed == changed)
    return;

  self->changed = changed;
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CHANGED]);

  action = g_action_map_lookup_action (G_ACTION_MAP (self), "save");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), changed);
  action = g_action_map_lookup_action (G_ACTION_MAP (self), "revert");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), changed);
}

/* }}} */
/* {{{ Callbacks, utilities */

static void
toggle_controls (IconEditorWindow *self)
{
  icon_editor_window_set_show_controls (self, !self->show_controls);
}

static void
paintable_changed (IconEditorWindow *self)
{
  gboolean changed;

  changed = !path_paintable_equal (self->paintable, self->orig_paintable);
  icon_editor_window_set_changed (self, changed);
}

static void
show_error (IconEditorWindow *self,
            const char       *title,
            const char       *detail)
{
  g_autoptr (GtkAlertDialog) alert = NULL;

  alert = gtk_alert_dialog_new ("%s", title);
  gtk_alert_dialog_set_detail (alert, detail);
  gtk_alert_dialog_show (alert, GTK_WINDOW (self));
}

/* }}} */
/* {{{ Opening/Importing */

static void
load_error (IconEditorWindow *self,
            const char       *message)
{
  show_error (self,
              self->importing ? "Importing failed" : "Loading failed",
              message);
}

static void
icon_editor_window_set_paintable (IconEditorWindow *self,
                                  PathPaintable    *paintable)
{
  if (self->paintable == paintable)
    return;

  if (self->paintable)
    g_signal_handlers_disconnect_by_func (self->paintable, paintable_changed, self);

  g_set_object (&self->paintable, paintable);

  if (self->paintable)
    g_signal_connect_swapped (self->paintable, "changed",
                              G_CALLBACK (paintable_changed), self);

  if (!self->importing)
    {
      g_clear_object (&self->orig_paintable);
      if (paintable)
        self->orig_paintable = path_paintable_copy (paintable);

      self->changed = FALSE;
    }
  else
    self->changed = TRUE;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PAINTABLE]);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CHANGED]);
}

static gboolean
load_bytes (IconEditorWindow *self,
            GBytes           *bytes)
{
  g_autoptr (GError) error = NULL;
  g_autoptr (PathPaintable) paintable = NULL;

  if (!g_utf8_validate (g_bytes_get_data (bytes, NULL), g_bytes_get_size (bytes), NULL))
    {
      load_error (self, "Invalid UTF-8");
      return FALSE;
    }

  paintable = path_paintable_new_from_bytes (bytes, &error);
  if (!paintable)
    {
      load_error (self, error->message);
      return FALSE;
    }

  if (self->importing)
    {
      PathPaintable *p = path_paintable_combine (self->paintable, paintable);
      g_set_object (&paintable, p);
    }

  icon_editor_window_set_paintable (self, paintable);

  return TRUE;
}

static gboolean
load_file_contents (IconEditorWindow *self,
                    GFile            *file)
{
  g_autoptr (GBytes) bytes = NULL;
  g_autoptr (GError) error = NULL;

  bytes = g_file_load_bytes (file, NULL, NULL, &error);
  if (!bytes)
    {
      load_error (self, error->message);
      return FALSE;
    }

  return load_bytes (self, bytes);
}

static void
open_response_cb (GObject      *source,
                  GAsyncResult *result,
                  void         *user_data)
{
  GtkFileDialog *dialog = GTK_FILE_DIALOG (source);
  IconEditorWindow *self = user_data;
  g_autoptr (GFile) file = NULL;
  g_autoptr (GError) error = NULL;

  file = gtk_file_dialog_open_finish (dialog, result, &error);
  if (!file)
    {
      if (!g_error_matches (error, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_DISMISSED))
        load_error (self, error->message);
      return;
    }

  icon_editor_window_load (self, file);
}

static void
show_open_filechooser (IconEditorWindow *self)
{
  g_autoptr (GtkFileDialog) dialog = NULL;

  dialog = gtk_file_dialog_new ();
  gtk_file_dialog_set_title (dialog, self->importing ? "Open SVG file" : "Open icon file");
  if (self->file)
    {
      gtk_file_dialog_set_initial_file (dialog, self->file);
    }
  else
    {
      g_autoptr (GFile) cwd = g_file_new_for_path (".");
      gtk_file_dialog_set_initial_folder (dialog, cwd);
    }

  gtk_file_dialog_open (dialog, GTK_WINDOW (self),
                        NULL, open_response_cb, self);
}

/* }}} */
/* {{{ Saving/Exporting */

static void
save_error (IconEditorWindow *self,
            const char       *message)
{
  show_error (self,
              self->exporting ? "Export failed" : "Saving failed",
              message);
}

static void
save_to_file (IconEditorWindow *self,
              GFile            *file)
{
  g_autoptr (GBytes) bytes = NULL;
  g_autoptr (GError) error = NULL;

  bytes = path_paintable_serialize (self->paintable);
  if (!g_file_replace_contents (file,
                                g_bytes_get_data (bytes, NULL),
                                g_bytes_get_size (bytes),
                                NULL, FALSE,
                                G_FILE_CREATE_NONE,
                                NULL,
                                NULL,
                                &error))
    {
      save_error (self, error->message);
      return;
    }

  g_set_object (&self->file, file);
  g_set_object (&self->orig_paintable, path_paintable_copy (self->paintable));
  icon_editor_window_set_changed (self, FALSE);
}

static void
export_to_file (IconEditorWindow *self,
                GFile            *file)
{
  g_autofree char *path = g_file_get_path (file);

  if (g_str_has_suffix (path, ".svg"))
    path[strlen (path) - strlen (".svg")] = '\0';
  else if (g_str_has_suffix (path, ".icon"))
    path[strlen (path) - strlen (".icon")] = '\0';

  for (guint idx = 0; idx <= path_paintable_get_max_state (self->paintable); idx++)
    {
      g_autofree char *filename = g_strdup_printf ("%s-%u.icon", path, idx);
      g_autoptr (GBytes) bytes = NULL;
      g_autoptr (GError) error = NULL;

      bytes = path_paintable_serialize_state (self->paintable, idx);

      if (!g_file_set_contents (filename,
                                g_bytes_get_data (bytes, NULL),
                                g_bytes_get_size (bytes),
                                &error))
       {
         save_error (self, error->message);
         return;
       }
    }
}

static void
save_response_cb (GObject      *source,
                  GAsyncResult *result,
                  void         *user_data)
{
  GtkFileDialog *dialog = GTK_FILE_DIALOG (source);
  IconEditorWindow *self = user_data;
  g_autoptr (GFile) file = NULL;
  g_autoptr (GError) error = NULL;

  file = gtk_file_dialog_save_finish (dialog, result, &error);
  if (!file)
    {
      if (!g_error_matches (error, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_DISMISSED))
        save_error (self, error->message);
      return;
    }

  if (self->exporting)
    export_to_file (self, file);
  else
    save_to_file (self, file);
}

static void
show_save_filechooser (IconEditorWindow *self)
{
  g_autoptr (GtkFileDialog) dialog = NULL;

  dialog = gtk_file_dialog_new ();
  gtk_file_dialog_set_title (dialog, self->exporting ? "Export states" : "Save icon");
  if (self->file)
    {
      gtk_file_dialog_set_initial_file (dialog, self->file);
    }
  else
    {
      g_autoptr (GFile) cwd = g_file_new_for_path (".");
      gtk_file_dialog_set_initial_folder (dialog, cwd);
      gtk_file_dialog_set_initial_name (dialog, "demo.icon");
    }

  gtk_file_dialog_save (dialog,
                        GTK_WINDOW (self),
                        NULL,
                        save_response_cb, self);
}

/* }}} */
/* {{{ GtkWindow implementation */

static void
alert_done (GObject      *source,
            GAsyncResult *result,
            gpointer      data)
{
  GtkAlertDialog *alert = GTK_ALERT_DIALOG (source);
  IconEditorWindow *self = ICON_EDITOR_WINDOW (data);
  g_autoptr (GError) error = NULL;
  int res;

  res = gtk_alert_dialog_choose_finish (alert, result, &error);
  if (res == -1)
    return;

  if (res == 0)
    {
      if (self->file)
        save_to_file (self, self->file);
      else
        show_save_filechooser (self);
    }
  else if (res == 1)
    {
      self->changed = FALSE;
      gtk_window_close (GTK_WINDOW (self));
    }
}

static gboolean
icon_editor_window_close_request (GtkWindow *window)
{
  IconEditorWindow *self = ICON_EDITOR_WINDOW (window);

  if (self->changed)
    {
      g_autoptr (GtkAlertDialog) alert = NULL;
      const char *buttons[] = { "Save", "Quit", NULL };

      alert = gtk_alert_dialog_new ("Unsaved changes");
      gtk_alert_dialog_set_detail (alert, "The icon contains unsaved changes.");
      gtk_alert_dialog_set_modal (alert, TRUE);
      gtk_alert_dialog_set_buttons (alert, buttons);
      gtk_alert_dialog_set_default_button (alert, 0);
      gtk_alert_dialog_choose (alert, window, NULL, alert_done, self);

      return TRUE;
    }

  return FALSE;
}

/* }}} */
/* {{{ Actions */

static void
file_open (GSimpleAction *action,
           GVariant      *parameter,
           gpointer       user_data)
{
  IconEditorWindow *self = user_data;

  self->importing = FALSE;
  show_open_filechooser (self);
}

static void
file_save (GSimpleAction *action,
           GVariant      *parameter,
           gpointer       user_data)
{
  IconEditorWindow *self = user_data;

  self->exporting = FALSE;
  if (self->file)
    save_to_file (self, self->file);
  else
    show_save_filechooser (self);
}

static void
file_save_as (GSimpleAction *action,
              GVariant      *parameter,
              gpointer       user_data)
{
  IconEditorWindow *self = user_data;

  self->exporting = FALSE;
  show_save_filechooser (self);
}

static void
revert_changes (GSimpleAction *action,
                GVariant      *parameter,
                gpointer       user_data)
{
  IconEditorWindow *self = user_data;

  if (!self->paintable)
    return;

  path_paintable_set_state (self->orig_paintable, path_paintable_get_state (self->paintable));
  icon_editor_window_set_paintable (self, self->orig_paintable);
}

static void
file_import (GSimpleAction *action,
             GVariant      *parameter,
             gpointer       user_data)
{
  IconEditorWindow *self = user_data;

  self->importing = self->paintable != NULL;
  show_open_filechooser (self);
}

static void
file_export (GSimpleAction *action,
             GVariant      *parameter,
             gpointer       user_data)
{
  IconEditorWindow *self = user_data;

  self->exporting = TRUE;
  show_save_filechooser (self);
}

static void
add_path (GSimpleAction *action,
          GVariant      *parameter,
          gpointer       user_data)
{
  IconEditorWindow *self = user_data;
  g_autoptr (GskPath) path = NULL;

  path = gsk_path_parse ("M 0 0 L 100 100");
  path_paintable_add_path (self->paintable, path);
}

static GActionEntry win_entries[] = {
  { "open", file_open, NULL, NULL, NULL },
  { "save", file_save, NULL, NULL, NULL },
  { "save-as", file_save_as, NULL, NULL, NULL },
  { "revert", revert_changes, NULL, NULL, NULL },
  { "import", file_import, NULL, NULL, NULL },
  { "export", file_export, NULL, NULL, NULL },
  { "add-path", add_path, NULL, NULL, NULL },
};

/* }}} */
/* {{{ GObject boilerplate */

static void
icon_editor_window_init (IconEditorWindow *self)
{
  g_autoptr (GPropertyAction) action = NULL;
  g_autoptr (PathPaintable) paintable = NULL;

  gtk_widget_init_template (GTK_WIDGET (self));

  g_action_map_add_action_entries (G_ACTION_MAP (self),
                                   win_entries, G_N_ELEMENTS (win_entries),
                                   self);

  g_set_object (&action, g_property_action_new ("invert-colors", self, "invert-colors"));
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (action));

  g_set_object (&action, g_property_action_new ("show-thumbnails", self, "show-thumbnails"));
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (action));

  g_set_object (&action, g_property_action_new ("show-bounds", self, "show-bounds"));
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (action));

  g_set_object (&paintable, path_paintable_new ());
  icon_editor_window_set_paintable (self, paintable);
}
static void
icon_editor_window_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  IconEditorWindow *self = ICON_EDITOR_WINDOW (object);

  switch (prop_id)
    {
    case PROP_PAINTABLE:
      icon_editor_window_set_paintable (self, g_value_get_object (value));
      break;

    case PROP_SHOW_CONTROLS:
      icon_editor_window_set_show_controls (self, g_value_get_boolean (value));
      break;

    case PROP_SHOW_THUMBNAILS:
      icon_editor_window_set_show_thumbnails (self, g_value_get_boolean (value));
      break;

    case PROP_SHOW_BOUNDS:
      icon_editor_window_set_show_bounds (self, g_value_get_boolean (value));
      break;

    case PROP_INVERT_COLORS:
      icon_editor_window_set_invert_colors (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
icon_editor_window_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  IconEditorWindow *self = ICON_EDITOR_WINDOW (object);

  switch (prop_id)
    {
    case PROP_PAINTABLE:
      g_value_set_object (value, self->paintable);
      break;

    case PROP_CHANGED:
      g_value_set_boolean (value, self->changed);
      break;

    case PROP_SHOW_CONTROLS:
      g_value_set_boolean (value, self->show_controls);
      break;

    case PROP_SHOW_THUMBNAILS:
      g_value_set_boolean (value, self->show_thumbnails);
      break;

    case PROP_SHOW_BOUNDS:
      g_value_set_boolean (value, self->show_bounds);
      break;

    case PROP_INVERT_COLORS:
      g_value_set_boolean (value, self->invert_colors);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
icon_editor_window_dispose (GObject *object)
{
  gtk_widget_dispose_template (GTK_WIDGET (object), ICON_EDITOR_WINDOW_TYPE);

  G_OBJECT_CLASS (icon_editor_window_parent_class)->dispose (object);
}

static void
icon_editor_window_finalize (GObject *object)
{
  IconEditorWindow *self = ICON_EDITOR_WINDOW (object);

  if (self->paintable)
    g_signal_handlers_disconnect_by_func (self->paintable, paintable_changed, self);

  g_clear_object (&self->paintable);
  g_clear_object (&self->orig_paintable);
  g_clear_object (&self->file);

  G_OBJECT_CLASS (icon_editor_window_parent_class)->finalize (object);
}

static void
icon_editor_window_class_init (IconEditorWindowClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
  GtkWindowClass *window_class = GTK_WINDOW_CLASS (class);

  g_type_ensure (STATE_EDITOR_TYPE);
  g_type_ensure (PAINTABLE_EDITOR_TYPE);
  g_type_ensure (BORDER_PAINTABLE_TYPE);

  object_class->set_property = icon_editor_window_set_property;
  object_class->get_property = icon_editor_window_get_property;
  object_class->dispose = icon_editor_window_dispose;
  object_class->finalize = icon_editor_window_finalize;

  window_class->close_request = icon_editor_window_close_request;

  properties[PROP_PAINTABLE] =
    g_param_spec_object ("paintable", NULL, NULL,
                         PATH_PAINTABLE_TYPE,
                         G_PARAM_READWRITE | G_PARAM_STATIC_NAME);

  properties[PROP_CHANGED] =
    g_param_spec_boolean ("changed", NULL, NULL,
                          FALSE,
                          G_PARAM_READABLE | G_PARAM_STATIC_NAME);

  properties[PROP_SHOW_CONTROLS] =
    g_param_spec_boolean ("show-controls", NULL, NULL,
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_STATIC_NAME);

  properties[PROP_SHOW_THUMBNAILS] =
    g_param_spec_boolean ("show-thumbnails", NULL, NULL,
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_STATIC_NAME);

  properties[PROP_SHOW_BOUNDS] =
    g_param_spec_boolean ("show-bounds", NULL, NULL,
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_STATIC_NAME);

  properties[PROP_INVERT_COLORS] =
    g_param_spec_boolean ("invert-colors", NULL, NULL,
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_STATIC_NAME);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, properties);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gtk/icon-editor/icon-editor-window.ui");

  gtk_widget_class_bind_template_callback (widget_class, show_open_filechooser);
  gtk_widget_class_bind_template_callback (widget_class, toggle_controls);
}

/* }}} */
/* {{{ Public API */

IconEditorWindow *
icon_editor_window_new (IconEditorApplication *application)
{
  return g_object_new (ICON_EDITOR_WINDOW_TYPE,
                       "application", application,
                       NULL);
}

gboolean
icon_editor_window_load (IconEditorWindow *self,
                         GFile            *file)
{
  g_autofree char *basename = NULL;

  if (!load_file_contents (self, file))
    return FALSE;

  if (!self->importing)
    {
      g_set_object (&self->file, file);

      basename = g_file_get_basename (file);
      gtk_window_set_title (GTK_WINDOW (self), basename);
    }

  return TRUE;
}

/* }}} */

/* vim:set foldmethod=marker: */

