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

static GdkTexture *
render_paintable_to_texture (GdkPaintable *paintable)
{
  GtkSnapshot *snapshot;
  GskRenderNode *node;
  int width, height;
  cairo_surface_t *surface;
  cairo_t *cr;
  GdkTexture *texture;
  GBytes *bytes;

  width = gdk_paintable_get_intrinsic_width (paintable);
  height = gdk_paintable_get_intrinsic_height (paintable);

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);

  snapshot = gtk_snapshot_new ();
  gdk_paintable_snapshot (paintable, snapshot, width, height);
  node = gtk_snapshot_free_to_node (snapshot);

  cr = cairo_create (surface);
  gsk_render_node_draw (node, cr);
  cairo_destroy (cr);

  gsk_render_node_unref (node);

  bytes = g_bytes_new_with_free_func (cairo_image_surface_get_data (surface),
                                      cairo_image_surface_get_height (surface)
                                      * cairo_image_surface_get_stride (surface),
                                      (GDestroyNotify) cairo_surface_destroy,
                                      cairo_surface_reference (surface));
  texture = gdk_memory_texture_new (cairo_image_surface_get_width (surface),
                                    cairo_image_surface_get_height (surface),
                                    GDK_MEMORY_DEFAULT,
                                    bytes,
                                    cairo_image_surface_get_stride (surface));
  g_bytes_unref (bytes);
  cairo_surface_destroy (surface);

  return texture;
}

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

#ifdef G_OS_UNIX /* portal usage supported on *nix only */

static GSList *
get_file_list (const char *dir)
{
  GFileEnumerator *enumerator;
  GFile *file;
  GFileInfo *info;
  GSList *list = NULL;
  
  file = g_file_new_for_path (dir);
  enumerator = g_file_enumerate_children (file, "standard::name,standard::type", 0, NULL, NULL);
  g_object_unref (file);
  if (enumerator == NULL)
    return NULL;

  while (g_file_enumerator_iterate (enumerator, &info, &file, NULL, NULL) && file != NULL)
    {
      /* the portal can't handle directories */
      if (g_file_info_get_file_type (info) != G_FILE_TYPE_REGULAR)
        continue;

      list = g_slist_prepend (list, g_object_ref (file));
    }

  return g_slist_reverse (list);
}

#else /* G_OS_UNIX -- original non-portal-enabled code */

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

#endif /* !G_OS_UNIX */

static void
format_list_add_row (GtkWidget         *list,
                     const char        *format_name,
                     GdkContentFormats *formats)
{
  GtkWidget *box;

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
  gtk_box_append (GTK_BOX (box), gtk_label_new (format_name));

  gdk_content_formats_unref (formats);
  gtk_list_box_insert (GTK_LIST_BOX (list), box, -1);
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
    gtk_list_box_remove (GTK_LIST_BOX (list), row);

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

  sw = gtk_scrolled_window_new ();

  list = gtk_list_box_new ();
  g_signal_connect_object (clipboard, "notify::formats", G_CALLBACK (clipboard_formats_change_cb), list, 0);
  clipboard_formats_change_cb (clipboard, NULL, list);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), list);

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

  gtk_box_append (GTK_BOX (box), button);
}

static GtkWidget *
get_button_list (GdkClipboard *clipboard,
                 const char   *info)
{
  static const guchar invalid_utf8[] = {  'L', 'i', 'b', 'e', 'r', 't', 0xe9, ',', ' ',
                                         0xc9, 'g', 'a', 'l', 'i', 't', 0xe9, ',', ' ',
                                          'F', 'r', 'a', 't', 'e', 'r', 'n', 'i', 't', 0xe9, 0 };
  GtkWidget *box;
  GtkIconPaintable *icon;
  GdkTexture *texture;
  GValue value = G_VALUE_INIT;

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

  gtk_box_append (GTK_BOX (box), gtk_label_new (info));

  add_provider_button (box,
                       NULL,
                       clipboard,
                       "Empty");

  g_value_init (&value, GDK_TYPE_PIXBUF);
  icon = gtk_icon_theme_lookup_icon (gtk_icon_theme_get_for_display (gdk_clipboard_get_display (clipboard)),
                                     "utilities-terminal",
                                     NULL,
                                     48, 1,
                                     gtk_widget_get_direction (box),
                                     0);
  texture = render_paintable_to_texture (GDK_PAINTABLE (icon));
  g_value_take_object (&value, gdk_pixbuf_get_from_texture (texture));
  g_object_unref (texture);
  g_object_unref (icon);
  add_provider_button (box,
                       gdk_content_provider_new_for_value (&value),
                       clipboard,
                       "GdkPixbuf");
  g_value_unset (&value);

  add_provider_button (box,
                       gdk_content_provider_new_typed (G_TYPE_STRING, "Hello Clipboard ☺"),
                       clipboard,
                       "gchararry");

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
  gtk_box_append (GTK_BOX (hbox), vbox);
  gtk_box_append (GTK_BOX (vbox), gtk_label_new (name));
  switcher = gtk_stack_switcher_new ();
  gtk_box_append (GTK_BOX (vbox), switcher);
  stack = get_contents_widget (clipboard);
  gtk_box_append (GTK_BOX (vbox), stack);
  gtk_stack_switcher_set_stack (GTK_STACK_SWITCHER (switcher), GTK_STACK (stack));
  gtk_box_append (GTK_BOX (hbox), get_button_list (clipboard, "Set Locally:"));
  if (clipboard != alt_clipboard)
    gtk_box_append (GTK_BOX (hbox), get_button_list (alt_clipboard, "Set Remotely:"));

  return hbox;
}

static GtkWidget *
get_window_contents (GdkDisplay *display,
                     GdkDisplay *alt_display)
{
  GtkWidget *box;

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_box_set_homogeneous (GTK_BOX (box), TRUE);
  gtk_box_append (GTK_BOX (box),
                     get_clipboard_widget (gdk_display_get_clipboard (display),
                                           gdk_display_get_clipboard (alt_display),
                                           "Clipboard"));
  gtk_box_append (GTK_BOX (box),
                     get_clipboard_widget (gdk_display_get_primary_clipboard (display),
                                           gdk_display_get_primary_clipboard (alt_display),
                                           "Primary Clipboard"));

  return box;
}

static void
quit_cb (GtkWidget *widget,
         gpointer   data)
{
  gboolean *done = data;

  *done = TRUE;

  g_main_context_wakeup (NULL);
}

int
main (int argc, char **argv)
{
  GtkWidget *window;
  GdkDisplay *alt_display;
  gboolean done = FALSE;

  gtk_init ();

  alt_display = gdk_display_open (NULL);
  if (alt_display == NULL)
    alt_display = gdk_display_get_default ();

  window = gtk_window_new ();
  g_signal_connect (window, "destroy", G_CALLBACK (quit_cb), &done);
  gtk_window_set_child (GTK_WINDOW (window),
                     get_window_contents (gtk_widget_get_display (window),
                                          alt_display));

  gtk_window_present (GTK_WINDOW (window));

  while (!done)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
