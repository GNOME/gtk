/*  Copyright 2025 Red Hat, Inc.
 *
 * GTK is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * GTK is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with GTK; see the file COPYING.  If not,
 * see <http://www.gnu.org/licenses/>.
 *
 * Author: Matthias Clasen
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <glib/gi18n-lib.h>
#include <glib/gprintf.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include "gtk-image-tool.h"

static void
quit_cb (GtkWidget *widget,
         gpointer   user_data)
{
  gboolean *is_done = user_data;

  *is_done = TRUE;

  g_main_context_wakeup (NULL);
}

static void
update_tooltip (GtkWidget    *widget,
                unsigned int  state)
{
  if (state == GTK_SVG_STATE_EMPTY)
    {
      gtk_widget_set_tooltip_text (widget, "State: empty");
    }
  else
    {
      char *text = g_strdup_printf ("State: %u", state);
      gtk_widget_set_tooltip_text (widget, text);
      g_free (text);
    }
}

static void
clicked (GtkGestureClick *click,
         int              n_press,
         double           x,
         double           y,
         GtkSvg          *svg)
{
  unsigned int state;
  unsigned int n_states;
  GtkWidget *widget;

  widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (click));

  state = gtk_svg_get_state (svg);
  n_states = gtk_svg_get_n_states (svg);

  if (gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (click)) == 1)
    {
      if (state + 1 == n_states)
        state = GTK_SVG_STATE_EMPTY;
      else if (state == GTK_SVG_STATE_EMPTY)
        state = 0;
      else
        state++;
    }
  else
    {
      if (state == 0)
        state = GTK_SVG_STATE_EMPTY;
      else if (state == GTK_SVG_STATE_EMPTY)
        state = n_states - 1;
      else
        state--;
    }

  gtk_svg_set_state (svg, state);
  update_tooltip (widget, state);
}

static gboolean
toggle_playing (GtkWidget *widget,
                GVariant  *args,
                gpointer   user_data)
{
  GtkSvg *svg = user_data;
  gboolean playing;

  g_object_get (svg, "playing", &playing, NULL);
  g_object_set (svg, "playing", !playing, NULL);
  return TRUE;
}

static GtkSvg * load_animation_file (const char *filename);

static gboolean
restart (GtkWidget *widget,
         GVariant  *args,
         gpointer   user_data)
{
  const char *filename;
  GtkSvg *svg;

  filename = (const char *) g_object_get_data (G_OBJECT (widget), "filename");

  svg = load_animation_file (filename);

  gtk_svg_set_frame_clock (svg, gtk_widget_get_frame_clock (widget));

  gtk_svg_play (svg);
  gtk_picture_set_paintable (GTK_PICTURE (widget), GDK_PAINTABLE (svg));
  g_object_unref (svg);

  return TRUE;
}

static void
error_cb (GtkSvg *svg, GError *error)
{
  if (error->domain == GTK_SVG_ERROR)
    {
      const GtkSvgLocation *start = gtk_svg_error_get_start (error);
      const GtkSvgLocation *end = gtk_svg_error_get_end (error);
      const char *element = gtk_svg_error_get_element (error);
      const char *attribute = gtk_svg_error_get_attribute (error);

      if (start)
        {
          if (end->lines != start->lines || end->line_chars != start->line_chars)
            g_print ("%" G_GSIZE_FORMAT ".%" G_GSIZE_FORMAT " - %" G_GSIZE_FORMAT ".%" G_GSIZE_FORMAT ": ",
                     start->lines, start->line_chars,
                     end->lines, end->line_chars);
          else
            g_print ("%" G_GSIZE_FORMAT ".%" G_GSIZE_FORMAT ": ", start->lines, start->line_chars);
        }

      if (element && attribute)
        g_print ("(%s / %s) ", element, attribute);
      else if (element)
        g_print ("(%s) ", element);
    }

  g_print ("%s\n", error->message);
}

static GtkSvg *
load_animation_file (const char *filename)
{
  char *contents;
  size_t length;
  GBytes *bytes;
  GError *error = NULL;
  GtkSvg *svg;

  if (!g_file_get_contents (filename, &contents, &length, &error))
    {
      g_printerr ("%s\n", error->message);
      exit (1);
    }

  bytes = g_bytes_new_take (contents, length);

  svg = gtk_svg_new ();
  g_signal_connect (svg, "error", G_CALLBACK (error_cb), NULL);
  gtk_svg_load_from_bytes (svg, bytes);

  g_bytes_unref (bytes);

  return svg;
}

static void
show_files (char     **filenames,
            gboolean   decorated,
            int        size)
{
  GtkWidget *sw;
  GtkWidget *window;
  gboolean done = FALSE;
  GString *title;
  GtkWidget *box;

  window = gtk_window_new ();
  g_signal_connect (window, "destroy", G_CALLBACK (quit_cb), &done);

  gtk_widget_realize (window);

  title = g_string_new ("");
  for (int i = 0; i < g_strv_length (filenames); i++)
    {
      char *name = g_path_get_basename (filenames[i]);

      if (title->len > 0)
        g_string_append (title, " / ");
      g_string_append (title, name);

      g_free (name);
    }

  gtk_window_set_decorated (GTK_WINDOW (window), decorated);
  gtk_window_set_resizable (GTK_WINDOW (window), decorated);

  gtk_window_set_title (GTK_WINDOW (window), title->str);
  g_string_free (title, TRUE);

  sw = gtk_scrolled_window_new ();
  gtk_scrolled_window_set_propagate_natural_width (GTK_SCROLLED_WINDOW (sw), TRUE);
  gtk_scrolled_window_set_propagate_natural_height (GTK_SCROLLED_WINDOW (sw), TRUE);

  gtk_window_set_child (GTK_WINDOW (window), sw);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), box);

  for (int i = 0; i < g_strv_length (filenames); i++)
    {
      GtkSvg *svg;
      GtkWidget *picture;
      GtkEventController *click;
      GtkEventController *shortcuts;
      GtkShortcutTrigger *trigger;
      GtkShortcutAction *action;

      svg = load_animation_file (filenames[i]);

      gtk_svg_set_frame_clock (svg, gtk_widget_get_frame_clock (window));

      gtk_svg_play (svg);

      if (size == 0)
        {
          picture = gtk_picture_new_for_paintable (GDK_PAINTABLE (svg));
          gtk_picture_set_can_shrink (GTK_PICTURE (picture), FALSE);
          gtk_picture_set_content_fit (GTK_PICTURE (picture), GTK_CONTENT_FIT_CONTAIN);
          gtk_widget_set_hexpand (picture, TRUE);
          gtk_widget_set_vexpand (picture, TRUE);
        }
      else
        {
          picture = gtk_image_new_from_paintable (GDK_PAINTABLE (svg));
          gtk_image_set_pixel_size (GTK_IMAGE (picture), size);
        }

      g_object_set_data_full (G_OBJECT (picture), "filename", g_strdup (filenames[i]), g_free);

      click = GTK_EVENT_CONTROLLER (gtk_gesture_click_new ());
      gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (click), 0);
      g_signal_connect (click, "pressed", G_CALLBACK (clicked), svg);
      gtk_widget_add_controller (picture, click);

      shortcuts = gtk_shortcut_controller_new ();
      gtk_shortcut_controller_set_scope (GTK_SHORTCUT_CONTROLLER (shortcuts), GTK_SHORTCUT_SCOPE_GLOBAL);

      trigger = gtk_alternative_trigger_new (
                  gtk_keyval_trigger_new (GDK_KEY_AudioPlay, 0),
                  gtk_keyval_trigger_new (GDK_KEY_P, GDK_CONTROL_MASK));
      action = gtk_callback_action_new (toggle_playing, svg, NULL);
      gtk_shortcut_controller_add_shortcut (GTK_SHORTCUT_CONTROLLER (shortcuts), gtk_shortcut_new (trigger, action));
      trigger = gtk_alternative_trigger_new (
                  gtk_keyval_trigger_new (GDK_KEY_AudioRewind, 0),
                  gtk_keyval_trigger_new (GDK_KEY_R, GDK_CONTROL_MASK));
      action = gtk_callback_action_new (restart, svg, NULL);
      gtk_shortcut_controller_add_shortcut (GTK_SHORTCUT_CONTROLLER (shortcuts), gtk_shortcut_new (trigger, action));
      gtk_widget_add_controller (picture, shortcuts);

      if (i > 0)
        gtk_box_append (GTK_BOX (box), gtk_separator_new (GTK_ORIENTATION_VERTICAL));
      gtk_box_append (GTK_BOX (box), picture);

      update_tooltip (picture, gtk_svg_get_state (svg));

      g_object_unref (svg);
    }

  gtk_window_present (GTK_WINDOW (window));

  while (!done)
    g_main_context_iteration (NULL, TRUE);
}

void
do_play (int          *argc,
         const char ***argv)
{
  GOptionContext *context;
  char **filenames = NULL;
  gboolean decorated = TRUE;
  int size = 0;
  const GOptionEntry entries[] = {
    { "undecorated", 0, G_OPTION_FLAG_REVERSE, G_OPTION_ARG_NONE, &decorated, N_("Don't add a titlebar"), NULL },
    { "size", 0, 0, G_OPTION_ARG_INT, &size, N_("SIZE") },
    { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &filenames, NULL, N_("FILE…") },
    { NULL, }
  };
  GError *error = NULL;

  g_set_prgname ("gtk4-image-tool play");
  context = g_option_context_new (NULL);
  g_option_context_set_translation_domain (context, GETTEXT_PACKAGE);
  g_option_context_add_main_entries (context, entries, NULL);
  g_option_context_set_summary (context, _("Show one or more animations."));

  if (!g_option_context_parse (context, argc, (char ***)argv, &error))
    {
      g_printerr ("%s\n", error->message);
      g_error_free (error);
      exit (1);
    }

  g_option_context_free (context);

  if (filenames == NULL)
    {
      g_printerr (_("No animation file specified\n"));
      exit (1);
    }

  show_files (filenames, decorated, size);

  g_strfreev (filenames);
}
