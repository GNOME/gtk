/*
 * Copyright (c) 2016 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include <glib/gi18n-lib.h>

#include "recorder.h"

#include <gtk/gtkbutton.h>
#include <gtk/gtklabel.h>
#include <gtk/gtklistbox.h>
#include <gtk/gtktreeselection.h>
#include <gtk/gtktreeview.h>
#include <gsk/gskrendererprivate.h>
#include <gsk/gskrendernodeprivate.h>

#include "gtktreemodelrendernode.h"
#include "recording.h"
#include "rendernodeview.h"
#include "renderrecording.h"
#include "startrecording.h"

struct _GtkInspectorRecorderPrivate
{
  GListModel *recordings;
  GtkTreeModel *render_node_model;

  GtkWidget *recordings_list;
  GtkWidget *render_node_view;
  GtkWidget *render_node_tree;
  GtkWidget *node_property_tree;
  GtkTreeModel *render_node_properties;

  GtkInspectorRecording *recording; /* start recording if recording or NULL if not */
};

enum {
  COLUMN_NODE_NAME,
  COLUMN_NODE_VISIBLE,
  /* add more */
  N_NODE_COLUMNS
};

enum
{
  PROP_0,
  PROP_RECORDING,
  LAST_PROP
};

static GParamSpec *props[LAST_PROP] = { NULL, };

G_DEFINE_TYPE_WITH_PRIVATE (GtkInspectorRecorder, gtk_inspector_recorder, GTK_TYPE_BIN)

static void
recordings_clear_all (GtkButton            *button,
                      GtkInspectorRecorder *recorder)
{
  GtkInspectorRecorderPrivate *priv = gtk_inspector_recorder_get_instance_private (recorder);

  g_list_store_remove_all (G_LIST_STORE (priv->recordings));
}

static void
recordings_list_row_selected (GtkListBox           *box,
                              GtkListBoxRow        *row,
                              GtkInspectorRecorder *recorder)
{
  GtkInspectorRecorderPrivate *priv = gtk_inspector_recorder_get_instance_private (recorder);
  GtkInspectorRecording *recording;

  if (row)
    recording = g_list_model_get_item (priv->recordings, gtk_list_box_row_get_index (row));
  else
    recording = NULL;

  if (GTK_INSPECTOR_IS_RENDER_RECORDING (recording))
    {
      gtk_render_node_view_set_render_node (GTK_RENDER_NODE_VIEW (priv->render_node_view),
                                            gtk_inspector_render_recording_get_node (GTK_INSPECTOR_RENDER_RECORDING (recording)));
      gtk_render_node_view_set_clip_region (GTK_RENDER_NODE_VIEW (priv->render_node_view),
                                            gtk_inspector_render_recording_get_clip_region (GTK_INSPECTOR_RENDER_RECORDING (recording)));
      gtk_render_node_view_set_viewport (GTK_RENDER_NODE_VIEW (priv->render_node_view),
                                         gtk_inspector_render_recording_get_area (GTK_INSPECTOR_RENDER_RECORDING (recording)));
      gtk_tree_model_render_node_set_root_node (GTK_TREE_MODEL_RENDER_NODE (priv->render_node_model),
                                                gtk_inspector_render_recording_get_node (GTK_INSPECTOR_RENDER_RECORDING (recording)));
    }
  else
    {
      gtk_render_node_view_set_render_node (GTK_RENDER_NODE_VIEW (priv->render_node_view), NULL);
      gtk_tree_model_render_node_set_root_node (GTK_TREE_MODEL_RENDER_NODE (priv->render_node_model), NULL);
    }

  gtk_tree_view_expand_all (GTK_TREE_VIEW (priv->render_node_tree));
}

static void
render_node_list_get_value (GtkTreeModelRenderNode *model,
                            GskRenderNode          *node,
                            int                     column,
                            GValue                 *value)
{
  switch (column)
    {
    case COLUMN_NODE_NAME:
      g_value_set_string (value, gsk_render_node_get_name (node));
      break;

    case COLUMN_NODE_VISIBLE:
      g_value_set_boolean (value, !gsk_render_node_is_hidden (node));
      break;

    default:
      g_assert_not_reached ();
      break;
    }
}

static void
populate_render_node_properties (GtkListStore  *store,
                                 GskRenderNode *node)
{
  graphene_matrix_t m;
  graphene_rect_t bounds;
  GString *s;
  int i;
  char *tmp;
  GEnumClass *class;

  gtk_list_store_clear (store);

  gsk_render_node_get_transform (node, &m);

  s = g_string_new ("");
  for (i = 0; i < 4; i++)
    {
      if (i > 0)
        g_string_append (s, "\n");
      g_string_append_printf (s, "| %+.6f %+.6f %+.6f %+.6f |",
                              graphene_matrix_get_value (&m, i, 0),
                              graphene_matrix_get_value (&m, i, 1),
                              graphene_matrix_get_value (&m, i, 2),
                              graphene_matrix_get_value (&m, i, 3));
    }

  gtk_list_store_insert_with_values (store, NULL, -1,
                                     0, "Transform",
                                     1, s->str,
                                     -1);

  g_string_free (s, TRUE);

  gsk_render_node_get_bounds (node, &bounds);

  tmp = g_strdup_printf ("%.6f x %.6f + %.6f + %.6f",
                         bounds.size.width,
                         bounds.size.height,
                         bounds.origin.x,
                         bounds.origin.y);
  gtk_list_store_insert_with_values (store, NULL, -1,
                                     0, "Bounds",
                                     1, tmp,
                                     -1);
  g_free (tmp);

  tmp = g_strdup_printf ("%.2f", gsk_render_node_get_opacity (node));
  gtk_list_store_insert_with_values (store, NULL, -1,
                                     0, "Opacity",
                                     1, tmp,
                                     -1);
  g_free (tmp);

  gtk_list_store_insert_with_values (store, NULL, -1,
                                     0, "Has Surface",
                                     1, gsk_render_node_has_surface (node) ? "TRUE" : "FALSE",
                                     -1);

  gtk_list_store_insert_with_values (store, NULL, -1,
                                     0, "Has Texture",
                                     1, gsk_render_node_has_texture (node) ? "TRUE" : "FALSE",
                                     -1);

  class = g_type_class_ref (gsk_blend_mode_get_type ());
  for (i = 0; i < class->n_values; i++)
    {
      if (class->values[i].value == gsk_render_node_get_blend_mode (node))
        {
          tmp = g_strdup (class->values[i].value_nick);
          break;
        }
    }
  g_type_class_unref (class);

  gtk_list_store_insert_with_values (store, NULL, -1,
                                     0, "Blendmode",
                                     1, tmp,
                                     -1);
  g_free (tmp);
}

static void
render_node_list_selection_changed (GtkTreeSelection     *selection,
                                    GtkInspectorRecorder *recorder)
{
  GtkInspectorRecorderPrivate *priv = gtk_inspector_recorder_get_instance_private (recorder);
  GskRenderNode *node;
  GtkTreeIter iter;

  if (!gtk_tree_selection_get_selected (selection, NULL, &iter))
    return;

  node = gtk_tree_model_render_node_get_node_from_iter (GTK_TREE_MODEL_RENDER_NODE (priv->render_node_model), &iter);
  gtk_render_node_view_set_render_node (GTK_RENDER_NODE_VIEW (priv->render_node_view), node);

  populate_render_node_properties (GTK_LIST_STORE (priv->render_node_properties), node);
}

static GtkWidget *
gtk_inspector_recorder_recordings_list_create_widget (gpointer item,
                                                      gpointer user_data)
{
  GtkInspectorRecording *recording = GTK_INSPECTOR_RECORDING (item);
  GtkWidget *widget;
  char *str;

  if (GTK_INSPECTOR_IS_RENDER_RECORDING (recording))
    {
      str = g_strdup_printf ("Frame at %lld", (long long) gtk_inspector_recording_get_timestamp (recording));
      widget = gtk_label_new (str);
      g_free (str);
      gtk_label_set_xalign (GTK_LABEL (widget), 0);
      gtk_widget_show_all (widget);
    }
  else
    {
      widget = gtk_label_new ("Start Recording");
      gtk_label_set_xalign (GTK_LABEL (widget), 0);
      gtk_widget_show_all (widget);

    }

  return widget;
}

static void
gtk_inspector_recorder_get_property (GObject    *object,
                                     guint       param_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  GtkInspectorRecorder *recorder = GTK_INSPECTOR_RECORDER (object);
  GtkInspectorRecorderPrivate *priv = gtk_inspector_recorder_get_instance_private (recorder);

  switch (param_id)
    {
    case PROP_RECORDING:
      g_value_set_boolean (value, priv->recording != NULL);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }
}

static void
gtk_inspector_recorder_set_property (GObject      *object,
                                     guint         param_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  GtkInspectorRecorder *recorder = GTK_INSPECTOR_RECORDER (object);

  switch (param_id)
    {
    case PROP_RECORDING:
      gtk_inspector_recorder_set_recording (recorder, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }
}

static void
gtk_inspector_recorder_class_init (GtkInspectorRecorderClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = gtk_inspector_recorder_get_property;
  object_class->set_property = gtk_inspector_recorder_set_property;

  props[PROP_RECORDING] =
    g_param_spec_boolean ("recording",
                          "Recording",
                          "Whether the recorder is currently recording",
                          FALSE,
                          G_PARAM_READWRITE);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/libgtk/inspector/recorder.ui");

  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorRecorder, recordings);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorRecorder, recordings_list);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorRecorder, render_node_view);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorRecorder, render_node_tree);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorRecorder, node_property_tree);

  gtk_widget_class_bind_template_callback (widget_class, recordings_clear_all);
  gtk_widget_class_bind_template_callback (widget_class, recordings_list_row_selected);
  gtk_widget_class_bind_template_callback (widget_class, render_node_list_selection_changed);
}

static void
gtk_inspector_recorder_init (GtkInspectorRecorder *recorder)
{
  GtkInspectorRecorderPrivate *priv = gtk_inspector_recorder_get_instance_private (recorder);

  gtk_widget_init_template (GTK_WIDGET (recorder));

  gtk_list_box_bind_model (GTK_LIST_BOX (priv->recordings_list),
                           priv->recordings,
                           gtk_inspector_recorder_recordings_list_create_widget,
                           NULL,
                           NULL);

  priv->render_node_model = gtk_tree_model_render_node_new (render_node_list_get_value,
                                                            N_NODE_COLUMNS,
                                                            G_TYPE_STRING,
                                                            G_TYPE_BOOLEAN);
  gtk_tree_view_set_model (GTK_TREE_VIEW (priv->render_node_tree), priv->render_node_model);
  g_object_unref (priv->render_node_model);

  priv->render_node_properties = GTK_TREE_MODEL (gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING));
  gtk_tree_view_set_model (GTK_TREE_VIEW (priv->node_property_tree), priv->render_node_properties);
  g_object_unref (priv->render_node_properties);
}

static void
gtk_inspector_recorder_add_recording (GtkInspectorRecorder  *recorder,
                                      GtkInspectorRecording *recording)
{
  GtkInspectorRecorderPrivate *priv = gtk_inspector_recorder_get_instance_private (recorder);
  guint count;
  GtkListBoxRow *selected_row;
  gboolean should_select_new_row;

  count = g_list_model_get_n_items (priv->recordings);
  selected_row = gtk_list_box_get_selected_row (GTK_LIST_BOX (priv->recordings_list));
  if (count == 0 || selected_row == NULL)
    should_select_new_row = TRUE;
  else
    should_select_new_row = (gtk_list_box_row_get_index (selected_row) == count - 1);

  g_list_store_append (G_LIST_STORE (priv->recordings), recording);

  if (should_select_new_row)
    {
      gtk_list_box_select_row (GTK_LIST_BOX (priv->recordings_list),
                               gtk_list_box_get_row_at_index (GTK_LIST_BOX (priv->recordings_list), count));
    }
}

void
gtk_inspector_recorder_set_recording (GtkInspectorRecorder *recorder,
                                      gboolean              recording)
{
  GtkInspectorRecorderPrivate *priv = gtk_inspector_recorder_get_instance_private (recorder);

  if (gtk_inspector_recorder_is_recording (recorder) == recording)
    return;

  if (recording)
    {
      priv->recording = gtk_inspector_start_recording_new ();
      gtk_inspector_recorder_add_recording (recorder, priv->recording);
    }
  else
    {
      g_clear_object (&priv->recording);
    }

  g_object_notify_by_pspec (G_OBJECT (recorder), props[PROP_RECORDING]);
}

gboolean
gtk_inspector_recorder_is_recording (GtkInspectorRecorder *recorder)
{
  GtkInspectorRecorderPrivate *priv = gtk_inspector_recorder_get_instance_private (recorder);

  return priv->recording != NULL;
}

void
gtk_inspector_recorder_record_render (GtkInspectorRecorder *recorder,
                                      GtkWidget            *widget,
                                      GskRenderer          *renderer,
                                      GdkWindow            *window,
                                      const cairo_region_t *region,
                                      GskRenderNode        *node)
{
  GtkInspectorRecording *recording;
  GdkFrameClock *frame_clock;

  if (!gtk_inspector_recorder_is_recording (recorder))
    return;

  frame_clock = gtk_widget_get_frame_clock (widget);

  recording = gtk_inspector_render_recording_new (gdk_frame_clock_get_frame_time (frame_clock),
                                                  gsk_renderer_get_profiler (renderer),
                                                  &(GdkRectangle) { 0, 0,
                                                    gdk_window_get_width (window),
                                                    gdk_window_get_height (window) },
                                                  region,
                                                  node);
  gtk_inspector_recorder_add_recording (recorder, recording);
  g_object_unref (recording);
}

// vim: set et sw=2 ts=2:
