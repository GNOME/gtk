/*
 * Copyright (C) 2011  Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include <gtk/gtk.h>

static void
clipboard_changed_cb (GdkClipboard *clipboard,
                      GtkWidget    *stack)
{
  GtkWidget *child;

  gtk_stack_set_visible_child_name (GTK_STACK (stack), "info");

  child = gtk_stack_get_child_by_name (GTK_STACK (stack), "image");
  gtk_image_clear (GTK_IMAGE (child));

  child = gtk_stack_get_child_by_name (GTK_STACK (stack), "text");
  gtk_label_set_text (GTK_LABEL (child), "");
}

static void
texture_loaded_cb (GObject      *clipboard,
                  GAsyncResult *res,
                  gpointer      data)
{
  GError *error = NULL;
  GdkTexture *texture;

  texture = gdk_clipboard_read_texture_finish (GDK_CLIPBOARD (clipboard), res, &error);
  if (texture == NULL)
    {
      g_print ("%s\n", error->message);
      g_error_free (error);
      return;
    }

  gtk_image_set_from_paintable (data, GDK_PAINTABLE (texture));
  g_object_unref (texture);
}

static void
text_loaded_cb (GObject      *clipboard,
                GAsyncResult *res,
                gpointer      data)
{
  GError *error = NULL;
  char *text;

  text = gdk_clipboard_read_text_finish (GDK_CLIPBOARD (clipboard), res, &error);
  if (text == NULL)
    {
      g_print ("%s\n", error->message);
      g_error_free (error);
      return;
    }

  gtk_label_set_text (data, text);
  g_free (text);
}

static void
visible_child_changed_cb (GtkWidget    *stack,
                          GParamSpec   *pspec,
                          GdkClipboard *clipboard)
{
  const char *visible_child = gtk_stack_get_visible_child_name (GTK_STACK (stack));

  if (visible_child == NULL)
    { 
      /* nothing to do here but avoiding crashes in g_str_equal() */
    }
  else if (g_str_equal (visible_child, "image"))
    {
      GtkWidget *image = gtk_stack_get_child_by_name (GTK_STACK (stack), "image");

      gdk_clipboard_read_texture_async (clipboard,
                                        NULL,
                                        texture_loaded_cb,
                                        image);
    }
  else if (g_str_equal (visible_child, "text"))
    {
      GtkWidget *label = gtk_stack_get_child_by_name (GTK_STACK (stack), "text");

      gdk_clipboard_read_text_async (clipboard,
                                     NULL,
                                     text_loaded_cb,
                                     label);
    }
}

static GList *
get_file_list (const char *dir)
{
  GFileEnumerator *enumerator;
  GFile *file;
  GList *list = NULL;
  
  file = g_file_new_for_path (dir);
  enumerator = g_file_enumerate_children (file, "standard::name", 0, NULL, NULL);
  g_object_unref (file);
  if (enumerator == NULL)
    return NULL;

  while (g_file_enumerator_iterate (enumerator, NULL, &file, NULL, NULL) && file != NULL)
    list = g_list_prepend (list, g_object_ref (file));

  return g_list_reverse (list);
}

static void
format_list_add_row (GtkWidget         *list,
                     const char        *format_name,
                     GdkContentFormats *formats)
{
  GtkWidget *box;

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
  gtk_container_add (GTK_CONTAINER (box), gtk_label_new (format_name));

  gdk_content_formats_unref (formats);
  gtk_container_add (GTK_CONTAINER (list), box);
}

static void
clipboard_formats_change_cb (GdkClipboard *clipboard,
                             GParamSpec   *pspec,
                             GtkWidget    *list)
{
  GdkContentFormats *formats;
  GtkWidget *row;
  const char * const *mime_types;
  const GType *gtypes;
  gsize i, n;

  while ((row = GTK_WIDGET (gtk_list_box_get_row_at_index (GTK_LIST_BOX (list), 0))))
    gtk_container_remove (GTK_CONTAINER (list), row);

  formats = gdk_clipboard_get_formats (clipboard);
  
  gtypes = gdk_content_formats_get_gtypes (formats, &n);
  for (i = 0; i < n; i++)
    {
      format_list_add_row (list,
                           g_type_name (gtypes[i]),
                           gdk_content_formats_new_for_gtype (gtypes[i]));
    }

  mime_types = gdk_content_formats_get_mime_types (formats, &n);
  for (i = 0; i < n; i++)
    {
      format_list_add_row (list,
                           mime_types[i],
                           gdk_content_formats_new ((const char *[2]) { mime_types[i], NULL }, 1));
    }
}

static GtkWidget *
get_formats_list (GdkClipboard *clipboard)
{
  GtkWidget *sw, *list;

  sw = gtk_scrolled_window_new (NULL, NULL);

  list = gtk_list_box_new ();
  g_signal_connect_object (clipboard, "notify::formats", G_CALLBACK (clipboard_formats_change_cb), list, 0);
  clipboard_formats_change_cb (clipboard, NULL, list);
  gtk_container_add (GTK_CONTAINER (sw), list);

  return sw;
}

static GtkWidget *
get_contents_widget (GdkClipboard *clipboard)
{
  GtkWidget *stack, *child;

  stack = gtk_stack_new ();
  gtk_widget_set_hexpand (stack, TRUE);
  gtk_widget_set_vexpand (stack, TRUE);
  g_signal_connect (stack, "notify::visible-child", G_CALLBACK (visible_child_changed_cb), clipboard);
  g_signal_connect_object (clipboard, "changed", G_CALLBACK (clipboard_changed_cb), stack, 0);

  child = get_formats_list (clipboard);
  gtk_stack_add_titled (GTK_STACK (stack), child, "info", "Info");

  child = gtk_image_new ();
  gtk_stack_add_titled (GTK_STACK (stack), child, "image", "Image");

  child = gtk_label_new (NULL);
  gtk_label_set_wrap (GTK_LABEL (child), TRUE);
  gtk_stack_add_titled (GTK_STACK (stack), child, "text", "Text");

  return stack;
}

static void
provider_button_clicked_cb (GtkWidget    *button,
                            GdkClipboard *clipboard)
{
  gdk_clipboard_set_content (clipboard,
                             g_object_get_data (G_OBJECT (button), "provider"));
}

static void
add_provider_button (GtkWidget          *box,
                     GdkContentProvider *provider,
                     GdkClipboard       *clipboard,
                     const char         *name)
{
  GtkWidget *button;

  button = gtk_button_new_with_label (name);
  g_signal_connect (button, "clicked", G_CALLBACK (provider_button_clicked_cb), clipboard);
  if (provider)
    g_object_set_data_full (G_OBJECT (button), "provider", provider, g_object_unref);

  gtk_container_add (GTK_CONTAINER (box), button);
}

static GtkWidget *
get_button_list (GdkClipboard *clipboard,
                 const char   *info)
{
  static const guchar invalid_utf8[] = {  'L', 'i', 'b', 'e', 'r', 't', 0xe9, ',', ' ',
                                         0xc9, 'g', 'a', 'l', 'i', 't', 0xe9, ',', ' ',
                                          'F', 'r', 'a', 't', 'e', 'r', 'n', 'i', 't', 0xe9, 0 };
  GtkWidget *box;
  GValue value = G_VALUE_INIT;

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

  gtk_container_add (GTK_CONTAINER (box), gtk_label_new (info));

  add_provider_button (box,
                       NULL,
                       clipboard,
                       "Empty");

  g_value_init (&value, GDK_TYPE_PIXBUF);
  g_value_take_object (&value, gtk_icon_theme_load_icon (gtk_icon_theme_get_default (),
                                                         "utilities-terminal",
                                                         48, 0, NULL));
  add_provider_button (box,
                       gdk_content_provider_new_for_value (&value),
                       clipboard,
                       "GdkPixbuf");
  g_value_unset (&value);

  g_value_init (&value, G_TYPE_STRING);
  g_value_set_string (&value, "Hello Clipboard ☺");
  add_provider_button (box,
                       gdk_content_provider_new_for_value (&value),
                       clipboard,
                       "gchararry");
  g_value_unset (&value);

  add_provider_button (box,
                       gdk_content_provider_new_for_bytes ("text/plain;charset=utf-8",
                                                           g_bytes_new_static ("𝕳𝖊𝖑𝖑𝖔 𝖀𝖓𝖎𝖈𝖔𝖉𝖊",
                                                                               strlen ("𝕳𝖊𝖑𝖑𝖔 𝖀𝖓𝖎𝖈𝖔𝖉𝖊") + 1)),
                       clipboard,
                       "text/plain");

  add_provider_button (box,
                       gdk_content_provider_new_for_bytes ("text/plain;charset=utf-8",
                                                           g_bytes_new_static (invalid_utf8, sizeof(invalid_utf8))),
                       clipboard,
                       "Invalid UTF-8");

  g_value_init (&value, G_TYPE_FILE);
  g_value_take_object (&value, g_file_new_for_path (g_get_home_dir ()));
  add_provider_button (box,
                       gdk_content_provider_new_for_value (&value),
                       clipboard,
                       "home directory");
  g_value_unset (&value);

  g_value_init (&value, GDK_TYPE_FILE_LIST);
  g_value_take_boxed (&value, get_file_list (g_get_home_dir ()));
  add_provider_button (box,
                       gdk_content_provider_new_for_value (&value),
                       clipboard,
                       "files in home");
  return box;
}

static GtkWidget *
get_clipboard_widget (GdkClipboard *clipboard,
                      GdkClipboard *alt_clipboard,
                      const char   *name)
{
  GtkWidget *vbox, *hbox, *stack, *switcher;

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add (GTK_CONTAINER (hbox), vbox);
  gtk_container_add (GTK_CONTAINER (vbox), gtk_label_new (name));
  switcher = gtk_stack_switcher_new ();
  gtk_container_add (GTK_CONTAINER (vbox), switcher);
  stack = get_contents_widget (clipboard);
  gtk_container_add (GTK_CONTAINER (vbox), stack);
  gtk_stack_switcher_set_stack (GTK_STACK_SWITCHER (switcher), GTK_STACK (stack));
  gtk_container_add (GTK_CONTAINER (hbox), get_button_list (clipboard, "Set Locally:"));
  if (clipboard != alt_clipboard)
    gtk_container_add (GTK_CONTAINER (hbox), get_button_list (alt_clipboard, "Set Remotely:"));

  return hbox;
}

static GtkWidget *
get_window_contents (GdkDisplay *display,
                     GdkDisplay *alt_display)
{
  GtkWidget *box;

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_box_set_homogeneous (GTK_BOX (box), TRUE);
  gtk_container_add (GTK_CONTAINER (box),
                     get_clipboard_widget (gdk_display_get_clipboard (display),
                                           gdk_display_get_clipboard (alt_display),
                                           "Clipboard"));
  gtk_container_add (GTK_CONTAINER (box),
                     get_clipboard_widget (gdk_display_get_primary_clipboard (display),
                                           gdk_display_get_primary_clipboard (alt_display),
                                           "Primary Clipboard"));

  return box;
}

int
main (int argc, char **argv)
{
  GtkWidget *window;
  GdkDisplay *alt_display;

  gtk_init ();

  alt_display = gdk_display_open (NULL);
  if (alt_display == NULL)
    alt_display = gdk_display_get_default ();

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);
  gtk_container_add (GTK_CONTAINER (window),
                     get_window_contents (gtk_widget_get_display (window),
                                          alt_display));

  gtk_widget_show (window);

  gtk_main ();

  return 0;
}
