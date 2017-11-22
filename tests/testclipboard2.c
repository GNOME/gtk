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
pixbuf_loaded_cb (GObject      *clipboard,
                  GAsyncResult *res,
                  gpointer      data)
{
  GError *error = NULL;
  GdkPixbuf *pixbuf;

  pixbuf = gdk_clipboard_read_pixbuf_finish (GDK_CLIPBOARD (clipboard), res, &error);
  if (pixbuf == NULL)
    {
      g_print ("%s\n", error->message);
      g_error_free (error);
      return;
    }

  gtk_image_set_from_pixbuf (data, pixbuf);
  g_object_unref (pixbuf);
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

      gdk_clipboard_read_pixbuf_async (clipboard,
                                       NULL,
                                       pixbuf_loaded_cb,
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
                
static void
clipboard_formats_change_cb (GdkClipboard *clipboard,
                             GParamSpec   *pspec,
                             GtkWidget    *label)
{
  char *s;

  s = gdk_content_formats_to_string (gdk_clipboard_get_formats (clipboard));
  gtk_label_set_text (GTK_LABEL (label), s);
  g_free (s);
}

static GtkWidget *
get_contents_widget (GdkClipboard *clipboard)
{
  GtkWidget *stack, *child;

  stack = gtk_stack_new ();
  gtk_widget_set_hexpand (stack, TRUE);
  gtk_widget_set_vexpand (stack, TRUE);
  g_signal_connect (stack, "notify::visible-child", G_CALLBACK (visible_child_changed_cb), clipboard);
  g_signal_connect (clipboard, "changed", G_CALLBACK (clipboard_changed_cb), stack);

  child = gtk_label_new (NULL);
  gtk_label_set_line_wrap (GTK_LABEL (child), TRUE);
  g_signal_connect (clipboard, "notify::formats", G_CALLBACK (clipboard_formats_change_cb), child);
  clipboard_formats_change_cb (clipboard, NULL, child);
  gtk_stack_add_titled (GTK_STACK (stack), child, "info", "Info");

  child = gtk_image_new ();
  gtk_stack_add_titled (GTK_STACK (stack), child, "image", "Image");

  child = gtk_label_new (NULL);
  gtk_label_set_line_wrap (GTK_LABEL (child), TRUE);
  gtk_stack_add_titled (GTK_STACK (stack), child, "text", "Text");

  return stack;
}

static GtkWidget *
get_clipboard_widget (GdkClipboard *clipboard,
                      const char   *name)
{
  GtkWidget *box, *stack, *switcher;

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add (GTK_CONTAINER (box), gtk_label_new (name));
  switcher = gtk_stack_switcher_new ();
  gtk_container_add (GTK_CONTAINER (box), switcher);
  stack = get_contents_widget (clipboard);
  gtk_container_add (GTK_CONTAINER (box), stack);
  gtk_stack_switcher_set_stack (GTK_STACK_SWITCHER (switcher), GTK_STACK (stack));

  return box;
}

static GtkWidget *
get_window_contents (GdkDisplay *display)
{
  GtkWidget *box;

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_box_set_homogeneous (GTK_BOX (box), TRUE);
  gtk_container_add (GTK_CONTAINER (box),
                     get_clipboard_widget (gdk_display_get_clipboard (display),
                                           "Clipboard"));
  gtk_container_add (GTK_CONTAINER (box),
                     get_clipboard_widget (gdk_display_get_primary_clipboard (display),
                                           "Primary Clipboard"));

  return box;
}

int
main (int argc, char **argv)
{
  GtkWidget *window;

  gtk_init ();

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);
  gtk_container_add (GTK_CONTAINER (window), get_window_contents (gtk_widget_get_display (window)));

  gtk_widget_show (window);

  gtk_main ();

  return 0;
}
