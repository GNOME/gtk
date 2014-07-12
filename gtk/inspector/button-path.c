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

#include "config.h"
#include <glib/gi18n-lib.h>

#include "button-path.h"

#include "gtkbutton.h"
#include "gtkwidgetpath.h"

struct _GtkInspectorButtonPathPrivate
{
  GtkWidget *sw;
  GtkWidget *button_box;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkInspectorButtonPath, gtk_inspector_button_path, GTK_TYPE_BOX)

static void
gtk_inspector_button_path_init (GtkInspectorButtonPath *bp)
{
  bp->priv = gtk_inspector_button_path_get_instance_private (bp);
  gtk_widget_init_template (GTK_WIDGET (bp));
}

static void
gtk_inspector_button_path_class_init (GtkInspectorButtonPathClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/inspector/button-path.ui");
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorButtonPath, sw);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorButtonPath, button_box);
}

void
gtk_inspector_button_path_set_object (GtkInspectorButtonPath *bp,
                                      GObject                *object)
{
  GtkContainer *box = GTK_CONTAINER (bp->priv->button_box);
  GtkWidget *b;

  gtk_container_foreach (box, (GtkCallback)gtk_widget_destroy, NULL);

  if (GTK_IS_WIDGET (object))
    {
      GtkWidget *widget = GTK_WIDGET (object);
      gchar *path, **words;
      gint i;

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
}

// vim: set et sw=2 ts=2:
