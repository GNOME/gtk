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

#include <gtk/gtkbox.h>
#include <gtk/gtkfilechooserdialog.h>
#include <gtk/gtklabel.h>
#include <gtk/gtklistbox.h>
#include <gtk/gtkmessagedialog.h>
#include <gtk/gtktogglebutton.h>
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
  GtkWidget *render_node_save_button;
  GtkWidget *node_property_tree;
  GtkTreeModel *render_node_properties;

  GtkInspectorRecording *recording; /* start recording if recording or NULL if not */
};

enum {
  COLUMN_NODE_NAME,
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
      gtk_render_node_view_set_render_region (GTK_RENDER_NODE_VIEW (priv->render_node_view),
                                              gtk_inspector_render_recording_get_render_region (GTK_INSPECTOR_RENDER_RECORDING (recording)));
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

    default:
      g_assert_not_reached ();
      break;
    }
}

static const char *
node_type_name (GskRenderNodeType type)
{
  switch (type)
    {
    case GSK_NOT_A_RENDER_NODE:
    default:
      g_assert_not_reached ();
      return "Unknown";
    case GSK_CONTAINER_NODE:
      return "Container";
    case GSK_CAIRO_NODE:
      return "Cairo";
    case GSK_COLOR_NODE:
      return "Color";
    case GSK_LINEAR_GRADIENT_NODE:
      return "Linear Gradient";
    case GSK_REPEATING_LINEAR_GRADIENT_NODE:
      return "Repeating Linear Gradient";
    case GSK_BORDER_NODE:
      return "Border";
    case GSK_TEXTURE_NODE:
      return "Texture";
    case GSK_INSET_SHADOW_NODE:
      return "Inset Shadow";
    case GSK_OUTSET_SHADOW_NODE:
      return "Outset Shadow";
    case GSK_TRANSFORM_NODE:
      return "Transform";
    case GSK_OPACITY_NODE:
      return "Opacity";
    case GSK_COLOR_MATRIX_NODE:
      return "Color Matrix";
    case GSK_REPEAT_NODE:
      return "Repeat";
    case GSK_CLIP_NODE:
      return "Clip";
    case GSK_ROUNDED_CLIP_NODE:
      return "Rounded Clip";
    case GSK_SHADOW_NODE:
      return "Shadow";
    case GSK_BLEND_NODE:
      return "Blend";
    case GSK_CROSS_FADE_NODE:
      return "CrossFade";
    }
}

static void
populate_render_node_properties (GtkListStore  *store,
                                 GskRenderNode *node)
{
  graphene_rect_t bounds;
  char *tmp;

  gtk_list_store_clear (store);

  gsk_render_node_get_bounds (node, &bounds);

  gtk_list_store_insert_with_values (store, NULL, -1,
                                     0, "Type",
                                     1, node_type_name (gsk_render_node_get_node_type (node)),
                                     -1);

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

  gtk_list_store_insert_with_values (store, NULL, -1,
                                     0, "Has Surface",
                                     1, gsk_render_node_get_node_type (node) == GSK_CAIRO_NODE ? "TRUE" : "FALSE",
                                     -1);

  gtk_list_store_insert_with_values (store, NULL, -1,
                                     0, "Has Texture",
                                     1, gsk_render_node_get_node_type (node) == GSK_TEXTURE_NODE ? "TRUE" : "FALSE",
                                     -1);
}

static void
render_node_list_selection_changed (GtkTreeSelection     *selection,
                                    GtkInspectorRecorder *recorder)
{
  GtkInspectorRecorderPrivate *priv = gtk_inspector_recorder_get_instance_private (recorder);
  GskRenderNode *node;
  GtkTreeIter iter;

  if (!gtk_tree_selection_get_selected (selection, NULL, &iter))
    {
      gtk_widget_set_sensitive (priv->render_node_save_button, FALSE);
      return;
    }

  gtk_widget_set_sensitive (priv->render_node_save_button, TRUE);
  node = gtk_tree_model_render_node_get_node_from_iter (GTK_TREE_MODEL_RENDER_NODE (priv->render_node_model), &iter);
  gtk_render_node_view_set_render_node (GTK_RENDER_NODE_VIEW (priv->render_node_view), node);

  populate_render_node_properties (GTK_LIST_STORE (priv->render_node_properties), node);
}

static void
render_node_save_response (GtkWidget     *dialog,
                           gint           response,
                           GskRenderNode *node)
{
  gtk_widget_hide (dialog);

  if (response == GTK_RESPONSE_ACCEPT)
    {
      GBytes *bytes = gsk_render_node_serialize (node);
      GError *error = NULL;

      if (!g_file_replace_contents (gtk_file_chooser_get_file (GTK_FILE_CHOOSER (dialog)),
                                    g_bytes_get_data (bytes, NULL),
                                    g_bytes_get_size (bytes),
                                    NULL,
                                    FALSE,
                                    0,
                                    NULL,
                                    NULL,
                                    &error))
        {
          GtkWidget *message_dialog;

          message_dialog = gtk_message_dialog_new (GTK_WINDOW (gtk_window_get_transient_for (GTK_WINDOW (dialog))),
                                                   GTK_DIALOG_MODAL|GTK_DIALOG_DESTROY_WITH_PARENT,
                                                   GTK_MESSAGE_INFO,
                                                   GTK_BUTTONS_OK,
                                                   _("Saving RenderNode failed"));
          gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (message_dialog),
                                                    "%s", error->message);
          g_signal_connect (message_dialog, "response", G_CALLBACK (gtk_widget_destroy), NULL);
          gtk_widget_show (message_dialog);
          g_error_free (error);
        }

      g_bytes_unref (bytes);
    }

  gtk_widget_destroy (dialog);
}

static void
render_node_save (GtkButton            *button,
                  GtkInspectorRecorder *recorder)
{
  GtkInspectorRecorderPrivate *priv = gtk_inspector_recorder_get_instance_private (recorder);
  GskRenderNode *node;
  GtkTreeIter iter;
  GtkWidget *dialog;
  char *filename;

  if (!gtk_tree_selection_get_selected (gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->render_node_tree)), NULL, &iter))
    return;

  node = gtk_tree_model_render_node_get_node_from_iter (GTK_TREE_MODEL_RENDER_NODE (priv->render_node_model), &iter);

  dialog = gtk_file_chooser_dialog_new ("",
                                        GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (recorder))),
                                        GTK_FILE_CHOOSER_ACTION_SAVE,
                                        _("_Cancel"), GTK_RESPONSE_CANCEL,
                                        _("_Save"), GTK_RESPONSE_ACCEPT,
                                        NULL);
  filename = g_strdup_printf ("%s.node", gsk_render_node_get_name (node) ? gsk_render_node_get_name (node)
                                                                         : node_type_name (gsk_render_node_get_node_type (node)));
  gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (dialog), filename);
  g_free (filename);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);
  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
  gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (dialog), TRUE);
  g_signal_connect (dialog, "response", G_CALLBACK (render_node_save_response), node);
  gtk_widget_show (dialog);
}

static char *
format_timespan (gint64 timespan)
{
  if (ABS (timespan) < G_TIME_SPAN_MILLISECOND)
    return g_strdup_printf ("%fus", (double) timespan);
  else if (ABS (timespan) < 10 * G_TIME_SPAN_MILLISECOND)
    return g_strdup_printf ("%.1fs", (double) timespan / G_TIME_SPAN_MILLISECOND);
  else if (ABS (timespan) < G_TIME_SPAN_SECOND)
    return g_strdup_printf ("%.0fms", (double) timespan / G_TIME_SPAN_MILLISECOND);
  else if (ABS (timespan) < 10 * G_TIME_SPAN_SECOND)
    return g_strdup_printf ("%.1fs", (double) timespan / G_TIME_SPAN_SECOND);
  else
    return g_strdup_printf ("%.0fs", (double) timespan / G_TIME_SPAN_SECOND);
}

static GtkWidget *
gtk_inspector_recorder_recordings_list_create_widget (gpointer item,
                                                      gpointer user_data)
{
  GtkInspectorRecorder *recorder = GTK_INSPECTOR_RECORDER (user_data);
  GtkInspectorRecorderPrivate *priv = gtk_inspector_recorder_get_instance_private (recorder);
  GtkInspectorRecording *recording = GTK_INSPECTOR_RECORDING (item);
  GtkWidget *widget;

  if (GTK_INSPECTOR_IS_RENDER_RECORDING (recording))
    {
      GtkInspectorRecording *previous = NULL;
      char *time_str, *str;
      const char *render_str;
      cairo_region_t *region;
      GtkWidget *hbox, *label, *button;
      guint i;

      widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
      gtk_box_pack_start (GTK_BOX (widget), hbox);

      for (i = 0; i < g_list_model_get_n_items (priv->recordings); i++)
        {
          GtkInspectorRecording *r = g_list_model_get_item (priv->recordings, i);

          if (r == recording)
            break;

          if (GTK_INSPECTOR_IS_RENDER_RECORDING (r))
            previous = r;
          else if (GTK_INSPECTOR_IS_START_RECORDING (r))
            previous = NULL;
        }

      region = cairo_region_create_rectangle (
                   gtk_inspector_render_recording_get_area (GTK_INSPECTOR_RENDER_RECORDING (recording)));
      cairo_region_subtract (region,
                             gtk_inspector_render_recording_get_clip_region (GTK_INSPECTOR_RENDER_RECORDING (recording)));
      if (cairo_region_is_empty (region))
        render_str = "Full Render";
      else
        render_str = "Partial Render";
      cairo_region_destroy (region);

      if (previous)
        {
          time_str = format_timespan (gtk_inspector_recording_get_timestamp (recording) -
                                      gtk_inspector_recording_get_timestamp (previous));
          str = g_strdup_printf ("<b>%s</b>\n+%s", render_str, time_str);
          g_free (time_str);
        }
      else
        {
          str = g_strdup_printf ("<b>%s</b>\n", render_str);
        }
      label = gtk_label_new (str);
      gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
      g_free (str);
      gtk_box_pack_start (GTK_BOX (hbox), label);

      button = gtk_toggle_button_new ();
      gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
      gtk_button_set_icon_name (GTK_BUTTON (button), "view-more-symbolic");

      gtk_box_pack_end (GTK_BOX (hbox), button);

      label = gtk_label_new (gtk_inspector_render_recording_get_profiler_info (GTK_INSPECTOR_RENDER_RECORDING (recording)));
      gtk_widget_hide (label);
      gtk_box_pack_end (GTK_BOX (widget), label);
      g_object_bind_property (button, "active", label, "visible", 0);
    }
  else
    {
      widget = gtk_label_new ("<b>Start of Recording</b>");
      gtk_label_set_use_markup (GTK_LABEL (widget), TRUE);
    }

  g_object_set (widget, "margin", 6, NULL); /* Seriously? g_object_set() needed for that? */

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
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorRecorder, render_node_save_button);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorRecorder, node_property_tree);

  gtk_widget_class_bind_template_callback (widget_class, recordings_clear_all);
  gtk_widget_class_bind_template_callback (widget_class, recordings_list_row_selected);
  gtk_widget_class_bind_template_callback (widget_class, render_node_list_selection_changed);
  gtk_widget_class_bind_template_callback (widget_class, render_node_save);
}

static void
gtk_inspector_recorder_init (GtkInspectorRecorder *recorder)
{
  GtkInspectorRecorderPrivate *priv = gtk_inspector_recorder_get_instance_private (recorder);

  gtk_widget_init_template (GTK_WIDGET (recorder));

  gtk_list_box_bind_model (GTK_LIST_BOX (priv->recordings_list),
                           priv->recordings,
                           gtk_inspector_recorder_recordings_list_create_widget,
                           recorder,
                           NULL);

  priv->render_node_model = gtk_tree_model_render_node_new (render_node_list_get_value,
                                                            N_NODE_COLUMNS,
                                                            G_TYPE_STRING);
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
                                      GdkDrawingContext    *context,
                                      GskRenderNode        *node)
{
  GtkInspectorRecording *recording;
  GdkFrameClock *frame_clock;
  cairo_region_t *clip;

  if (!gtk_inspector_recorder_is_recording (recorder))
    return;

  frame_clock = gtk_widget_get_frame_clock (widget);
  clip = gdk_drawing_context_get_clip (context);

  recording = gtk_inspector_render_recording_new (gdk_frame_clock_get_frame_time (frame_clock),
                                                  gsk_renderer_get_profiler (renderer),
                                                  &(GdkRectangle) { 0, 0,
                                                    gdk_window_get_width (window),
                                                    gdk_window_get_height (window) },
                                                  region,
                                                  clip,
                                                  node);
  gtk_inspector_recorder_add_recording (recorder, recording);
  g_object_unref (recording);
  cairo_region_destroy (clip);
}

// vim: set et sw=2 ts=2:
