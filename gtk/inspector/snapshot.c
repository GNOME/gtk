/*
 * Copyright (c) 2015 Benjamin Otte <otte@gnome.org>
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

#include "snapshot.h"

#include <gtk/gtkimage.h>
#include <gtk/gtklabel.h>
#include <gtk/gtklistbox.h>

#include "gtkrenderoperationwidget.h"
#include "window.h"

enum {
  PROP_0,
  PROP_OPERATION
};

G_DEFINE_TYPE (GtkInspectorSnapshot, gtk_inspector_snapshot, GTK_TYPE_WINDOW)

static void
gtk_inspector_snapshot_set_property (GObject      *object,
                                     guint         param_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  GtkInspectorSnapshot *snapshot = GTK_INSPECTOR_SNAPSHOT (object);

  switch (param_id)
    {
    case PROP_OPERATION:
      gtk_inspector_snapshot_set_operation (snapshot,
                                            g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
    }
}

static void
gtk_inspector_snapshot_get_property (GObject    *object,
                                     guint       param_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  GtkInspectorSnapshot *snapshot = GTK_INSPECTOR_SNAPSHOT (object);

  switch (param_id)
    {
    case PROP_OPERATION:
      g_value_set_object (value, snapshot->operation);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
    }
}

static cairo_surface_t *
create_surface_for_operation (GtkRenderOperation *oper)
{
  GtkAllocation clip;
  cairo_surface_t *surface;
  cairo_t *cr;

  gtk_render_operation_get_clip (oper, &clip);
  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, clip.width, clip.height);

  cr = cairo_create (surface);
  cairo_translate (cr, -clip.x, -clip.y);
  gtk_render_operation_draw (oper, cr);
  cairo_destroy (cr);

  return surface;
}

static void
on_operation_selected (GtkListBox           *listbox,
                       GtkListBoxRow        *row,
                       GtkInspectorSnapshot *snapshot)
{
  GtkRenderOperation *oper;
  cairo_surface_t *surface;

  if (row == NULL)
    {
      gtk_image_clear (GTK_IMAGE (snapshot->image));
      return;
    }

  oper = g_object_get_data (G_OBJECT (row), "operation");

  surface = create_surface_for_operation (oper);
  gtk_image_set_from_surface (GTK_IMAGE (snapshot->image), surface);
  cairo_surface_destroy (surface);
}

static void
gtk_inspector_snapshot_class_init (GtkInspectorSnapshotClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->set_property = gtk_inspector_snapshot_set_property;
  object_class->get_property = gtk_inspector_snapshot_get_property;

  g_object_class_install_property (object_class,
                                   PROP_OPERATION,
                                   g_param_spec_object ("operation",
                                                        "Operation",
                                                        "Operation to show",
                                                        GTK_TYPE_RENDER_OPERATION,
                                                        G_PARAM_READWRITE));

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/libgtk/inspector/snapshot.ui");

  gtk_widget_class_bind_template_child (widget_class, GtkInspectorSnapshot, operations_listbox);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorSnapshot, image);

  gtk_widget_class_bind_template_callback (widget_class, on_operation_selected);
}

static void
gtk_inspector_snapshot_init (GtkInspectorSnapshot *iw)
{
  gtk_widget_init_template (GTK_WIDGET (iw));
}

GtkWidget *
gtk_inspector_snapshot_new (GtkRenderOperation *oper)
{
  return GTK_WIDGET (g_object_new (GTK_TYPE_INSPECTOR_SNAPSHOT,
                                   "screen", gtk_inspector_get_screen (),
                                   "operation", oper,
                                   NULL));
}

static void
gtk_inspector_snapshot_fill_listbox (GtkInspectorSnapshot *snapshot,
                                     GtkRenderOperation   *oper,
                                     guint                 depth)
{
  GtkWidget *label, *row;
  char *text;

  text = g_strdup_printf ("%*s %s", 2 * depth, "", G_OBJECT_TYPE_NAME (oper));
  label = gtk_label_new (text);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_widget_show (label);
  g_free (text);

  row = gtk_list_box_row_new ();
  gtk_container_add (GTK_CONTAINER (row), label);
  g_object_set_data (G_OBJECT (row), "operation", oper);
  gtk_widget_show (row);

  gtk_container_add (GTK_CONTAINER (snapshot->operations_listbox), row);

  if (GTK_IS_RENDER_OPERATION_WIDGET (oper))
    {
      GList *l;

      for (l = GTK_RENDER_OPERATION_WIDGET (oper)->operations; l; l = l->next)
        {
          gtk_inspector_snapshot_fill_listbox (snapshot, l->data, depth + 1);
        }
    }
}

void
gtk_inspector_snapshot_set_operation (GtkInspectorSnapshot *snapshot,
                                      GtkRenderOperation   *oper)
{
  g_return_if_fail (GTK_IS_INSPECTOR_SNAPSHOT (snapshot));
  g_return_if_fail (oper == NULL || GTK_IS_RENDER_OPERATION (oper));

  if (snapshot->operation)
    {
      /* implicit version of gtk_list_box_clear(): */
      gtk_list_box_bind_model (GTK_LIST_BOX (snapshot->operations_listbox), NULL, NULL, NULL, NULL);
      g_object_unref (snapshot->operation);
    }

  if (oper)
    {
      g_object_ref (oper);
      snapshot->operation = oper;
      gtk_inspector_snapshot_fill_listbox (snapshot, oper, 0);
    }
}

GtkRenderOperation *
gtk_inspector_snapshot_get_operation (GtkInspectorSnapshot *snapshot)
{
  return snapshot->operation;
}

// vim: set et sw=2 ts=2:
