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

#include "paintable-editor.h"
#include "path-paintable.h"
#include "border-paintable.h"
#include "state-editor.h"

#include <glib/gstdio.h>


struct _IconEditorWindow
{
  GtkApplicationWindow parent;

  GFile *file;
  PathPaintable *paintable;
  PathPaintable *orig_paintable;
  gboolean changed;
  gboolean show_controls;
  gboolean show_thumbnails;
  gboolean show_bounds;
  gboolean show_spines;
  gboolean invert_colors;
  float weight;
  unsigned int state;
  unsigned int initial_state;
  GtkStack *main_stack;
  GtkImage *empty_logo;
  union {
    GtkImage *images[24];
    struct {
      GtkImage *image48_0, *image48_1, *image48_2, *image48_3;
      GtkImage *image48_4, *image48_5, *image48_6, *image48_7;
      GtkImage *image24_0, *image24_1, *image24_2, *image24_3;
      GtkImage *image24_4, *image24_5, *image24_6, *image24_7;
      GtkImage *image24_8, *image24_9, *image24_10, *image24_11;
      GtkImage *image24_12, *image24_13, *image24_14, *image24_15;
    };
  };
  PaintableEditor *paintable_editor;
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
  PROP_SHOW_BOUNDS,
  PROP_SHOW_SPINES,
  PROP_INVERT_COLORS,
  PROP_WEIGHT,
  PROP_STATE,
  PROP_INITIAL_STATE,
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

  if (show_controls)
    {
      GAction *action;

      action = g_action_map_lookup_action (G_ACTION_MAP (self), "close");
      g_simple_action_set_enabled (G_SIMPLE_ACTION (action), TRUE);
      gtk_stack_set_visible_child_name (self->main_stack, "content");
    }

  self->show_controls = show_controls;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SHOW_CONTROLS]);
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
icon_editor_window_set_show_spines (IconEditorWindow *self,
                                    gboolean          show_spines)
{
  if (self->show_spines == show_spines)
    return;

  self->show_spines = show_spines;
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SHOW_SPINES]);
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
icon_editor_window_set_weight (IconEditorWindow *self,
                               float             weight)
{
  if (self->weight == weight)
    return;

  self->weight = weight;

  path_paintable_set_weight (self->paintable, weight);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_WEIGHT]);
}

static void
icon_editor_window_set_state (IconEditorWindow *self,
                              unsigned int      state)
{
  if (self->state == state)
    return;

  self->state = state;

  path_paintable_set_state (self->paintable, state);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_STATE]);
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

static void
icon_editor_window_set_initial_state (IconEditorWindow *self,
                                      unsigned int      initial_state)
{
  if (self->initial_state == initial_state)
    return;

  self->initial_state = initial_state;

  icon_editor_window_set_changed (self, TRUE);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_INITIAL_STATE]);
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
  show_error (self, "Loading failed", message);
}

static void
set_random_icons (IconEditorWindow *self)
{
  const char *names[] = {
    "bookmark-new-symbolic",
    "color-select-symbolic",
    "document-open-recent-symbolic",
    "document-open-symbolic",
    "document-save-as-symbolic",
    "document-save-symbolic",
    "edit-clear-all-symbolic",
    "edit-clear-symbolic-rtl",
    "edit-clear-symbolic",
    "edit-copy-symbolic",
    "edit-cut-symbolic",
    "edit-delete-symbolic",
    "edit-find-symbolic",
    "edit-paste-symbolic",
    "edit-select-all-symbolic",
    "find-location-symbolic",
    "folder-new-symbolic",
    "function-linear-symbolic",
    "go-down-symbolic",
    "go-next-symbolic-rtl",
    "go-next-symbolic",
    "go-previous-symbolic-rtl",
    "go-previous-symbolic",
    "go-up-symbolic",
    "info-outline-symbolic",
    "insert-image-symbolic",
    "insert-object-symbolic",
    "list-add-symbolic",
    "list-remove-all-symbolic",
    "list-remove-symbolic",
    "media-eject-symbolic",
    "media-playback-pause-symbolic",
    "media-playback-start-symbolic",
    "media-playback-stop-symbolic",
    "media-record-symbolic",
    "object-select-symbolic",
    "open-menu-symbolic",
    "pan-down-symbolic",
    "pan-end-symbolic-rtl",
    "pan-end-symbolic",
    "pan-start-symbolic-rtl",
    "pan-start-symbolic",
    "pan-up-symbolic",
    "system-run-symbolic",
    "system-search-symbolic",
    "value-decrease-symbolic",
    "value-increase-symbolic",
    "view-conceal-symbolic",
    "view-grid-symbolic",
    "view-list-symbolic",
    "view-more-symbolic",
    "view-refresh-symbolic",
    "view-reveal-symbolic",
    "window-close-symbolic",
    "window-maximize-symbolic",
    "window-minimize-symbolic",
    "window-restore-symbolic",
    "zoom-in-symbolic",
    "zoom-original-symbolic",
    "zoom-out-symbolic",
    "emoji-activities-symbolic",
    "emoji-body-symbolic",
    "emoji-flags-symbolic",
    "emoji-food-symbolic",
    "emoji-nature-symbolic",
    "emoji-objects-symbolic",
    "emoji-people-symbolic",
    "emoji-recent-symbolic",
    "emoji-symbols-symbolic",
    "emoji-travel-symbolic",
    "audio-volume-high-symbolic",
    "audio-volume-low-symbolic",
    "audio-volume-medium-symbolic",
    "audio-volume-muted-symbolic",
    "caps-lock-symbolic",
    "changes-allow-symbolic",
    "changes-prevent-symbolic",
    "dialog-error-symbolic",
    "dialog-information-symbolic",
    "dialog-password-symbolic",
    "dialog-question-symbolic",
    "dialog-warning-symbolic",
    "display-brightness-symbolic",
    "media-playlist-repeat-symbolic",
    "orientation-landscape-inverse-symbolic",
    "orientation-landscape-symbolic",
    "orientation-portrait-inverse-symbolic",
    "orientation-portrait-symbolic",
    "process-working-symbolic",
    "switch-off-symbolic",
    "switch-on-symbolic",
  };
  g_autoptr (GtkBitset) used = gtk_bitset_new_empty ();

  for (unsigned int i = 0; i < 24; i++)
    {
      uint32_t r;
      g_autofree char *path;
      g_autoptr (PathPaintable) paintable = NULL;

      do {
        r = g_random_int_range (0, (uint32_t) G_N_ELEMENTS (names));
      } while (gtk_bitset_contains (used, r));

      path = g_strconcat ("/org/gtk/libgtk/icons/", names[r], ".svg", NULL);
      paintable = path_paintable_new_from_resource (path);
      path_paintable_set_state (paintable, 0);
      g_object_bind_property (self, "weight",
                              paintable, "weight",
                              G_BINDING_SYNC_CREATE);
      gtk_image_set_from_paintable (self->images[i], GDK_PAINTABLE (paintable));
      gtk_widget_set_tooltip_text (GTK_WIDGET (self->images[i]), names[r]);
      gtk_bitset_add (used, r);
    }

  gtk_image_set_from_paintable (self->images[g_random_int_range (0, 3)], GDK_PAINTABLE (self->paintable));
  gtk_image_set_from_paintable (self->images[g_random_int_range (4, 7)], GDK_PAINTABLE (self->paintable));
  gtk_image_set_from_paintable (self->images[g_random_int_range (8, 15)], GDK_PAINTABLE (self->paintable));
  gtk_image_set_from_paintable (self->images[g_random_int_range (16, 23)], GDK_PAINTABLE (self->paintable));
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
    {
      icon_editor_window_set_state (self, path_paintable_get_state (paintable));
      icon_editor_window_set_initial_state (self, path_paintable_get_state (paintable));

      g_signal_connect_swapped (self->paintable, "changed",
                                G_CALLBACK (paintable_changed), self);

      set_random_icons (self);

      if (path_paintable_get_n_paths (self->paintable) > 0)
        icon_editor_window_set_show_controls (self, TRUE);
    }

  g_clear_object (&self->orig_paintable);
  if (paintable)
    self->orig_paintable = path_paintable_copy (paintable);

  icon_editor_window_set_changed (self, FALSE);

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
  g_autoptr (GListStore) filters = NULL;
  GtkFileFilter *filter;

  filters = g_list_store_new (GTK_TYPE_FILE_FILTER);
  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, "All files");
  gtk_file_filter_add_pattern (filter, "*");
  g_list_store_append (filters, filter);
  g_object_unref (filter);
  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, "SVG files");
  gtk_file_filter_add_mime_type (filter, "image/svg+xml");
  g_list_store_append (filters, filter);
  g_object_unref (filter);
  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, "GTK icons");
  gtk_file_filter_add_mime_type (filter, "image/x-gtk-path-animation");
  gtk_file_filter_add_pattern (filter, "*.gpa");
  g_list_store_append (filters, filter);
  g_object_unref (filter);

  dialog = gtk_file_dialog_new ();
  gtk_file_dialog_set_title (dialog, "Open icon file");

  gtk_file_dialog_set_filters (dialog, G_LIST_MODEL (filters));

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
  show_error (self, "Saving failed", message);
}

static void
save_to_file (IconEditorWindow *self,
              GFile            *file)
{
  g_autoptr (GBytes) bytes = NULL;
  g_autoptr (GError) error = NULL;

  bytes = path_paintable_serialize (self->paintable, self->initial_state);
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

  save_to_file (self, file);
}

static void
show_save_filechooser (IconEditorWindow *self)
{
  g_autoptr (GtkFileDialog) dialog = NULL;

  dialog = gtk_file_dialog_new ();
  gtk_file_dialog_set_title (dialog, "Save icon");
  if (self->file)
    {
      gtk_file_dialog_set_initial_file (dialog, self->file);
    }
  else
    {
      g_autoptr (GFile) cwd = g_file_new_for_path (".");
      gtk_file_dialog_set_initial_folder (dialog, cwd);
      gtk_file_dialog_set_initial_name (dialog, "demo.gpa");
    }

  gtk_file_dialog_save (dialog,
                        GTK_WINDOW (self),
                        NULL,
                        save_response_cb, self);
}

/* }}} */
/* {{{ GtkWindow implementation */

static void
quit_alert_done (GObject      *source,
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
      gtk_alert_dialog_choose (alert, window, NULL, quit_alert_done, self);

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

  show_open_filechooser (self);
}

static void
file_save (GSimpleAction *action,
           GVariant      *parameter,
           gpointer       user_data)
{
  IconEditorWindow *self = user_data;

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

  show_save_filechooser (self);
}

static void
back_to_empty (IconEditorWindow *self)
{
  g_autoptr (PathPaintable) paintable = path_paintable_new ();
  GAction *action;

  icon_editor_window_set_paintable (self, paintable);
  icon_editor_window_set_show_controls (self, FALSE);

  gtk_stack_set_visible_child_name (self->main_stack, "empty");
  action = g_action_map_lookup_action (G_ACTION_MAP (self), "close");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);
}

static void
close_alert_done (GObject      *source,
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
      back_to_empty (self);
    }
}

static void
file_close (GSimpleAction *action,
            GVariant      *parameter,
            gpointer       user_data)
{
  IconEditorWindow *self = user_data;

  if (self->changed)
    {
      g_autoptr (GtkAlertDialog) alert = NULL;
      const char *buttons[] = { "Save", "Close", NULL };

      alert = gtk_alert_dialog_new ("Unsaved changes");
      gtk_alert_dialog_set_detail (alert, "The icon contains unsaved changes.");
      gtk_alert_dialog_set_modal (alert, TRUE);
      gtk_alert_dialog_set_buttons (alert, buttons);
      gtk_alert_dialog_set_default_button (alert, 0);
      gtk_alert_dialog_choose (alert, GTK_WINDOW (self), NULL, close_alert_done, self);
    }
  else
    {
      back_to_empty (self);
    }
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
add_path (GSimpleAction *action,
          GVariant      *parameter,
          gpointer       user_data)
{
  IconEditorWindow *self = user_data;

  paintable_editor_add_path (self->paintable_editor);
}

static void
edit_states (GSimpleAction *action,
             GVariant      *parameter,
             gpointer       user_data)
{
  IconEditorWindow *self = user_data;
  StateEditor *editor;

  editor = state_editor_new ();
  gtk_window_set_transient_for (GTK_WINDOW (editor), GTK_WINDOW (self));

  state_editor_set_paintable (editor, self->paintable);

  gtk_window_present (GTK_WINDOW (editor));
}

static void
show_controls (GSimpleAction *action,
               GVariant      *parameter,
               gpointer       user_data)
{
  IconEditorWindow *self = user_data;

  icon_editor_window_set_show_controls (self, TRUE);
}

static void
open_example (GSimpleAction *action,
              GVariant      *parameter,
              gpointer       user_data)
{
  IconEditorWindow *self = user_data;
  g_autofree char *path = NULL;
  g_autoptr (PathPaintable) paintable = NULL;

  path = g_strconcat ("/org/gtk/Shaper/",
                      g_variant_get_string (parameter, NULL),
                      NULL);

  paintable = path_paintable_new_from_resource (path);

  icon_editor_window_set_paintable (self, paintable);
}

static void
reshuffle (GSimpleAction *action,
           GVariant      *parameter,
           gpointer       user_data)
{
  IconEditorWindow *self = user_data;

  set_random_icons (self);
}

static GActionEntry win_entries[] = {
  { "open", file_open, NULL, NULL, NULL },
  { "save", file_save, NULL, NULL, NULL },
  { "save-as", file_save_as, NULL, NULL, NULL },
  { "revert", revert_changes, NULL, NULL, NULL },
  { "close", file_close, NULL, NULL, NULL },
  { "add-path", add_path, NULL, NULL, NULL },
  { "edit-states", edit_states, NULL, NULL, NULL },
  { "show-controls", show_controls, NULL, NULL, NULL },
  { "open-example", open_example, "s", NULL, NULL },
  { "reshuffle", reshuffle, NULL, NULL, NULL },
};

/* }}} */
/* {{{ GObject boilerplate */

static void
icon_editor_window_init (IconEditorWindow *self)
{
  g_autoptr (GPropertyAction) action = NULL;
  g_autoptr (PathPaintable) paintable = path_paintable_new ();

  self->weight = 400;
  self->state = 0;

  gtk_widget_init_template (GTK_WIDGET (self));

  g_action_map_add_action_entries (G_ACTION_MAP (self),
                                   win_entries, G_N_ELEMENTS (win_entries),
                                   self);

  g_set_object (&action, g_property_action_new ("invert-colors", self, "invert-colors"));
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (action));

  g_set_object (&action, g_property_action_new ("show-bounds", self, "show-bounds"));
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (action));

  g_set_object (&action, g_property_action_new ("show-spines", self, "show-spines"));
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (action));

  g_simple_action_set_enabled (G_SIMPLE_ACTION (g_action_map_lookup_action (G_ACTION_MAP (self), "save")), FALSE);

  g_simple_action_set_enabled (G_SIMPLE_ACTION (g_action_map_lookup_action (G_ACTION_MAP (self), "revert")), FALSE);

  g_simple_action_set_enabled (G_SIMPLE_ACTION (g_action_map_lookup_action (G_ACTION_MAP (self), "close")), FALSE);

  icon_editor_window_set_paintable (self, paintable);
}

static void
icon_editor_window_set_property (GObject      *object,
                                 unsigned int  prop_id,
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

    case PROP_SHOW_BOUNDS:
      icon_editor_window_set_show_bounds (self, g_value_get_boolean (value));
      break;

    case PROP_SHOW_SPINES:
      icon_editor_window_set_show_spines (self, g_value_get_boolean (value));
      break;

    case PROP_INVERT_COLORS:
      icon_editor_window_set_invert_colors (self, g_value_get_boolean (value));
      break;

    case PROP_WEIGHT:
      icon_editor_window_set_weight (self, g_value_get_float (value));
      break;

    case PROP_STATE:
      icon_editor_window_set_state (self, g_value_get_uint (value));
      break;

    case PROP_INITIAL_STATE:
      icon_editor_window_set_initial_state (self, g_value_get_uint (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
icon_editor_window_get_property (GObject      *object,
                                 unsigned int  prop_id,
                                 GValue       *value,
                                 GParamSpec   *pspec)
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

    case PROP_SHOW_BOUNDS:
      g_value_set_boolean (value, self->show_bounds);
      break;

    case PROP_SHOW_SPINES:
      g_value_set_boolean (value, self->show_spines);
      break;

    case PROP_INVERT_COLORS:
      g_value_set_boolean (value, self->invert_colors);
      break;

    case PROP_WEIGHT:
      g_value_set_float (value, self->weight);
      break;

    case PROP_STATE:
      g_value_set_uint (value, self->state);
      break;

    case PROP_INITIAL_STATE:
      g_value_set_uint (value, self->initial_state);
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
icon_editor_window_realize (GtkWidget *widget)
{
  IconEditorWindow *self = ICON_EDITOR_WINDOW (widget);
  GtkApplication *app = gtk_window_get_application (GTK_WINDOW (self));
  g_autofree char *path = NULL;
  g_autoptr (GFile) file = NULL;
  g_autoptr (GtkIconPaintable) logo = NULL;

  GTK_WIDGET_CLASS (icon_editor_window_parent_class)->realize (widget);

  path = g_strconcat ("resource:///org/gtk/Shaper/",
                      g_application_get_application_id (G_APPLICATION (app)),
                      ".svg",
                      NULL);
  file = g_file_new_for_uri (path);
  logo = gtk_icon_paintable_new_for_file (file, 128, 1);
  gtk_image_set_from_paintable (self->empty_logo, GDK_PAINTABLE (logo));
}

static void
icon_editor_window_class_init (IconEditorWindowClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
  GtkWindowClass *window_class = GTK_WINDOW_CLASS (class);

  g_type_ensure (PAINTABLE_EDITOR_TYPE);
  g_type_ensure (BORDER_PAINTABLE_TYPE);

  object_class->set_property = icon_editor_window_set_property;
  object_class->get_property = icon_editor_window_get_property;
  object_class->dispose = icon_editor_window_dispose;
  object_class->finalize = icon_editor_window_finalize;

  widget_class->realize = icon_editor_window_realize;
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

  properties[PROP_SHOW_BOUNDS] =
    g_param_spec_boolean ("show-bounds", NULL, NULL,
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_STATIC_NAME);

  properties[PROP_SHOW_SPINES] =
    g_param_spec_boolean ("show-spines", NULL, NULL,
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_STATIC_NAME);

  properties[PROP_INVERT_COLORS] =
    g_param_spec_boolean ("invert-colors", NULL, NULL,
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_STATIC_NAME);

  properties[PROP_WEIGHT] =
    g_param_spec_float ("weight", NULL, NULL,
                        1.f, 1000.f, 400.f,
                        G_PARAM_READWRITE | G_PARAM_STATIC_NAME);

  properties[PROP_STATE] =
    g_param_spec_uint ("state", NULL, NULL,
                       0, G_MAXUINT, 0,
                       G_PARAM_READWRITE | G_PARAM_STATIC_NAME);

  properties[PROP_INITIAL_STATE] =
    g_param_spec_uint ("initial-state", NULL, NULL,
                       0, G_MAXUINT, 0,
                       G_PARAM_READWRITE | G_PARAM_STATIC_NAME);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/Shaper/icon-editor-window.ui");

  gtk_widget_class_bind_template_child (widget_class, IconEditorWindow, main_stack);
  gtk_widget_class_bind_template_child (widget_class, IconEditorWindow, empty_logo);
  gtk_widget_class_bind_template_child (widget_class, IconEditorWindow, image48_0);
  gtk_widget_class_bind_template_child (widget_class, IconEditorWindow, image48_1);
  gtk_widget_class_bind_template_child (widget_class, IconEditorWindow, image48_2);
  gtk_widget_class_bind_template_child (widget_class, IconEditorWindow, image48_3);
  gtk_widget_class_bind_template_child (widget_class, IconEditorWindow, image48_4);
  gtk_widget_class_bind_template_child (widget_class, IconEditorWindow, image48_5);
  gtk_widget_class_bind_template_child (widget_class, IconEditorWindow, image48_6);
  gtk_widget_class_bind_template_child (widget_class, IconEditorWindow, image48_7);
  gtk_widget_class_bind_template_child (widget_class, IconEditorWindow, image24_0);
  gtk_widget_class_bind_template_child (widget_class, IconEditorWindow, image24_1);
  gtk_widget_class_bind_template_child (widget_class, IconEditorWindow, image24_2);
  gtk_widget_class_bind_template_child (widget_class, IconEditorWindow, image24_3);
  gtk_widget_class_bind_template_child (widget_class, IconEditorWindow, image24_4);
  gtk_widget_class_bind_template_child (widget_class, IconEditorWindow, image24_5);
  gtk_widget_class_bind_template_child (widget_class, IconEditorWindow, image24_6);
  gtk_widget_class_bind_template_child (widget_class, IconEditorWindow, image24_7);
  gtk_widget_class_bind_template_child (widget_class, IconEditorWindow, image24_8);
  gtk_widget_class_bind_template_child (widget_class, IconEditorWindow, image24_9);
  gtk_widget_class_bind_template_child (widget_class, IconEditorWindow, image24_10);
  gtk_widget_class_bind_template_child (widget_class, IconEditorWindow, image24_11);
  gtk_widget_class_bind_template_child (widget_class, IconEditorWindow, image24_12);
  gtk_widget_class_bind_template_child (widget_class, IconEditorWindow, image24_13);
  gtk_widget_class_bind_template_child (widget_class, IconEditorWindow, image24_14);
  gtk_widget_class_bind_template_child (widget_class, IconEditorWindow, image24_15);
  gtk_widget_class_bind_template_child (widget_class, IconEditorWindow, paintable_editor);

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

  g_set_object (&self->file, file);

  basename = g_file_get_basename (file);
  gtk_window_set_title (GTK_WINDOW (self), basename);

  return TRUE;
}

/* }}} */

/* vim:set foldmethod=marker: */

