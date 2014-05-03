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

G_DEFINE_TYPE_WITH_PRIVATE (ParasiteButtonPath, parasite_buttonpath, GTK_TYPE_BOX)

static void
parasite_buttonpath_init (ParasiteButtonPath *bp)
{
  GtkWidget *label;

  bp->priv = parasite_buttonpath_get_instance_private (bp);

  g_object_set (bp,
                "orientation", GTK_ORIENTATION_HORIZONTAL,
                NULL);

  bp->priv->sw = g_object_new (GTK_TYPE_SCROLLED_WINDOW,
                               "hscrollbar-policy", GTK_POLICY_AUTOMATIC,
                               "vscrollbar-policy", GTK_POLICY_NEVER,
                               "hexpand", TRUE,
                               NULL);
  gtk_container_add (GTK_CONTAINER (bp), bp->priv->sw);

  bp->priv->button_box = g_object_new (GTK_TYPE_BOX,
                                       "orientation", GTK_ORIENTATION_HORIZONTAL,
                                       "hexpand", TRUE,
                                       "margin", 6,
                                       NULL);
  gtk_style_context_add_class (gtk_widget_get_style_context (bp->priv->button_box), "linked");
  gtk_container_add (GTK_CONTAINER (bp->priv->sw), bp->priv->button_box);

  label = g_object_new (GTK_TYPE_LABEL,
                        "label", "Choose a widget through the inspector",
                        "xalign", 0.5,
                        "hexpand", TRUE,
                        NULL);
  gtk_container_add (GTK_CONTAINER (bp->priv->button_box), label);
}

static void
parasite_buttonpath_class_init(ParasiteButtonPathClass *klass)
{
}

GtkWidget *
parasite_buttonpath_new ()
{
    return GTK_WIDGET (g_object_new (PARASITE_TYPE_BUTTONPATH, NULL));
}

void
parasite_buttonpath_set_widget(ParasiteButtonPath *bp,
                               GtkWidget *widget)
{
  char *path, **words;
  int i;
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

// vim: set et sw=4 ts=4:
