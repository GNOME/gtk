/*
 * Copyright (c) 2013 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "button-path.h"

struct _ParasiteButtonPathPrivate
{
  GtkWidget *sw;
  GtkWidget *button_box;
};

G_DEFINE_TYPE_WITH_PRIVATE (ParasiteButtonPath, parasite_button_path, GTK_TYPE_BOX)

static void
parasite_button_path_init (ParasiteButtonPath *bp)
{
  bp->priv = parasite_button_path_get_instance_private (bp);
  gtk_widget_init_template (GTK_WIDGET (bp));
}

static void
parasite_button_path_class_init (ParasiteButtonPathClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/parasite/button-path.ui");
  gtk_widget_class_bind_template_child_private (widget_class, ParasiteButtonPath, sw);
  gtk_widget_class_bind_template_child_private (widget_class, ParasiteButtonPath, button_box);
}

GtkWidget *
parasite_button_path_new (void)
{
  return GTK_WIDGET (g_object_new (PARASITE_TYPE_BUTTON_PATH, NULL));
}

void
parasite_button_path_set_widget (ParasiteButtonPath *bp,
                                 GtkWidget          *widget)
{
  gchar *path, **words;
  gint i;
  GtkWidget *b;
  GtkContainer *box = GTK_CONTAINER (bp->priv->button_box);

  gtk_container_foreach (box, (GtkCallback)gtk_widget_destroy, NULL);

  path = gtk_widget_path_to_string (gtk_widget_get_path (widget));
  words = g_strsplit (path, " ", 0);

  for (i = 0; i < g_strv_length (words); i++)
    {
      b = gtk_button_new_with_label (words[i]);
      gtk_widget_show (b);
      gtk_container_add (box, b);
    }

  g_strfreev (words);
  g_free (path);
}

// vim: set et sw=2 ts=2:
