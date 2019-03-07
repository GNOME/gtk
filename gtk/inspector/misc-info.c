/*
 * Copyright (c) 2014 Red Hat, Inc.
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

#include "misc-info.h"
#include "window.h"
#include "object-tree.h"

#include "gtktypebuiltins.h"
#include "gtktreeview.h"
#include "gtkbuildable.h"
#include "gtklabel.h"
#include "gtkframe.h"
#include "gtkbutton.h"
#include "gtkwidgetprivate.h"


struct _GtkInspectorMiscInfoPrivate {
  GtkInspectorObjectTree *object_tree;

  GObject *object;

  GtkWidget *address;
  GtkWidget *refcount_row;
  GtkWidget *refcount;
  GtkWidget *state_row;
  GtkWidget *state;
  GtkWidget *buildable_id_row;
  GtkWidget *buildable_id;
  GtkWidget *default_widget_row;
  GtkWidget *default_widget;
  GtkWidget *default_widget_button;
  GtkWidget *mnemonic_label_row;
  GtkWidget *mnemonic_label;
  GtkWidget *request_mode_row;
  GtkWidget *request_mode;
  GtkWidget *allocated_size_row;
  GtkWidget *allocated_size;
  GtkWidget *baseline_row;
  GtkWidget *baseline;
  GtkWidget *frame_clock_row;
  GtkWidget *frame_clock;
  GtkWidget *frame_clock_button;
  GtkWidget *tick_callback_row;
  GtkWidget *tick_callback;
  GtkWidget *framerate_row;
  GtkWidget *framerate;
  GtkWidget *framecount_row;
  GtkWidget *framecount;
  GtkWidget *accessible_role_row;
  GtkWidget *accessible_role;
  GtkWidget *accessible_name_row;
  GtkWidget *accessible_name;
  GtkWidget *accessible_description_row;
  GtkWidget *accessible_description;
  GtkWidget *mapped_row;
  GtkWidget *mapped;
  GtkWidget *realized_row;
  GtkWidget *realized;
  GtkWidget *is_toplevel_row;
  GtkWidget *is_toplevel;
  GtkWidget *child_visible_row;
  GtkWidget *child_visible;

  guint update_source_id;
  gint64 last_frame;
};

enum
{
  PROP_0,
  PROP_OBJECT_TREE
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkInspectorMiscInfo, gtk_inspector_misc_info, GTK_TYPE_SCROLLED_WINDOW)

static gchar *
format_state_flags (GtkStateFlags state)
{
  GFlagsClass *fclass;
  GString *str;
  gint i;

  str = g_string_new ("");

  if (state)
    {
      fclass = g_type_class_ref (GTK_TYPE_STATE_FLAGS);
      for (i = 0; i < fclass->n_values; i++)
        {
          if (state & fclass->values[i].value)
            {
              if (str->len)
                g_string_append (str, " | ");
              g_string_append (str, fclass->values[i].value_nick);
            }
        } 
      g_type_class_unref (fclass);
    }
  else
    g_string_append (str, "normal");

  return g_string_free (str, FALSE);
}

static void
state_flags_changed (GtkWidget *w, GtkStateFlags old_flags, GtkInspectorMiscInfo *sl)
{
  gchar *s;

  s = format_state_flags (gtk_widget_get_state_flags (w));
  gtk_label_set_label (GTK_LABEL (sl->priv->state), s);
  g_free (s);
}

static void
allocation_changed (GtkWidget    *w,
                    int           width,
                    int           height,
                    int           baseline,
                    GtkInspectorMiscInfo *sl)
{
  GtkAllocation alloc;
  gchar *size_label;
  GEnumClass *class;
  GEnumValue *value;

  gtk_widget_get_allocation (w, &alloc);
  size_label = g_strdup_printf ("%d × %d +%d +%d",
                                alloc.width, alloc.height,
                                alloc.x, alloc.y);

  gtk_label_set_label (GTK_LABEL (sl->priv->allocated_size), size_label);
  g_free (size_label);

  size_label = g_strdup_printf ("%d", gtk_widget_get_allocated_baseline (w));
  gtk_label_set_label (GTK_LABEL (sl->priv->baseline), size_label);
  g_free (size_label);

  class = G_ENUM_CLASS (g_type_class_ref (GTK_TYPE_SIZE_REQUEST_MODE));
  value = g_enum_get_value (class, gtk_widget_get_request_mode (w));
  gtk_label_set_label (GTK_LABEL (sl->priv->request_mode), value->value_nick);
  g_type_class_unref (class);
}

static void
disconnect_each_other (gpointer  still_alive,
                       GObject  *for_science)
{
  if (GTK_INSPECTOR_IS_MISC_INFO (still_alive))
    {
      GtkInspectorMiscInfo *self = GTK_INSPECTOR_MISC_INFO (still_alive);
      self->priv->object = NULL;
    }

  g_signal_handlers_disconnect_matched (still_alive, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, for_science);
  g_object_weak_unref (still_alive, disconnect_each_other, for_science);
}

static void
show_object (GtkInspectorMiscInfo *sl,
             GObject              *object,
             const gchar          *tab)
{
  g_object_set_data_full (G_OBJECT (sl->priv->object_tree), "next-tab", g_strdup (tab), g_free);
  gtk_inspector_object_tree_select_object (sl->priv->object_tree, object);
}

static void
update_default_widget (GtkInspectorMiscInfo *sl)
{
  GtkWidget *widget;

  widget = gtk_window_get_default_widget (GTK_WINDOW (sl->priv->object));
  if (widget)
    {
      gchar *tmp;
      tmp = g_strdup_printf ("%p", widget);
      gtk_label_set_label (GTK_LABEL (sl->priv->default_widget), tmp);
      g_free (tmp);
      gtk_widget_set_sensitive (sl->priv->default_widget_button, TRUE);
    }
  else
    {
      gtk_label_set_label (GTK_LABEL (sl->priv->default_widget), "NULL");   
      gtk_widget_set_sensitive (sl->priv->default_widget_button, FALSE);
    }
}

static void
show_default_widget (GtkWidget *button, GtkInspectorMiscInfo *sl)
{
  GtkWidget *widget;

  update_default_widget (sl);
  widget = gtk_window_get_default_widget (GTK_WINDOW (sl->priv->object));
  if (widget)
    show_object (sl, G_OBJECT (widget), "properties"); 
}

static void
show_mnemonic_label (GtkWidget *button, GtkInspectorMiscInfo *sl)
{
  GtkWidget *widget;

  widget = g_object_get_data (G_OBJECT (button), "mnemonic-label");
  if (widget)
    show_object (sl, G_OBJECT (widget), "properties");
}

static void
show_frame_clock (GtkWidget *button, GtkInspectorMiscInfo *sl)
{
  GObject *clock;

  clock = (GObject *)gtk_widget_get_frame_clock (GTK_WIDGET (sl->priv->object));
  if (clock)
    show_object (sl, G_OBJECT (clock), "properties");
}

static void
update_frame_clock (GtkInspectorMiscInfo *sl)
{
  GObject *clock;

  clock = (GObject *)gtk_widget_get_frame_clock (GTK_WIDGET (sl->priv->object));
  if (clock)
    {
      gchar *tmp;
      tmp = g_strdup_printf ("%p", clock);
      gtk_label_set_label (GTK_LABEL (sl->priv->frame_clock), tmp);
      g_free (tmp);
      gtk_widget_set_sensitive (sl->priv->frame_clock_button, TRUE);
    }
  else
    {
      gtk_label_set_label (GTK_LABEL (sl->priv->frame_clock), "NULL");
      gtk_widget_set_sensitive (sl->priv->frame_clock_button, FALSE);
    }
}

static gboolean
update_info (gpointer data)
{
  GtkInspectorMiscInfo *sl = data;
  gchar *tmp;

  tmp = g_strdup_printf ("%p", sl->priv->object);
  gtk_label_set_text (GTK_LABEL (sl->priv->address), tmp);
  g_free (tmp);

  if (G_IS_OBJECT (sl->priv->object))
    {
      tmp = g_strdup_printf ("%d", sl->priv->object->ref_count);
      gtk_label_set_text (GTK_LABEL (sl->priv->refcount), tmp);
      g_free (tmp);
    }

  if (GTK_IS_WIDGET (sl->priv->object))
    {
      AtkObject *accessible;
      AtkRole role;
      GList *list, *l;

      gtk_container_forall (GTK_CONTAINER (sl->priv->mnemonic_label), (GtkCallback)gtk_widget_destroy, NULL);
      list = gtk_widget_list_mnemonic_labels (GTK_WIDGET (sl->priv->object));
      for (l = list; l; l = l->next)
        {
          GtkWidget *button;

          tmp = g_strdup_printf ("%p (%s)", l->data, g_type_name_from_instance ((GTypeInstance*)l->data));
          button = gtk_button_new_with_label (tmp);
          g_free (tmp);
          gtk_widget_show (button);
          gtk_container_add (GTK_CONTAINER (sl->priv->mnemonic_label), button);
          g_object_set_data (G_OBJECT (button), "mnemonic-label", l->data);
          g_signal_connect (button, "clicked", G_CALLBACK (show_mnemonic_label), sl);
        }
      g_list_free (list);

      gtk_widget_set_visible (sl->priv->tick_callback, gtk_widget_has_tick_callback (GTK_WIDGET (sl->priv->object)));

      accessible = ATK_OBJECT (gtk_widget_get_accessible (GTK_WIDGET (sl->priv->object)));
      role = atk_object_get_role (accessible);
      gtk_label_set_text (GTK_LABEL (sl->priv->accessible_role), atk_role_get_name (role));
      gtk_label_set_text (GTK_LABEL (sl->priv->accessible_name), atk_object_get_name (accessible));
      gtk_label_set_text (GTK_LABEL (sl->priv->accessible_description), atk_object_get_description (accessible));
      gtk_widget_set_visible (sl->priv->mapped, gtk_widget_get_mapped (GTK_WIDGET (sl->priv->object)));
      gtk_widget_set_visible (sl->priv->realized, gtk_widget_get_realized (GTK_WIDGET (sl->priv->object)));
      gtk_widget_set_visible (sl->priv->is_toplevel, gtk_widget_is_toplevel (GTK_WIDGET (sl->priv->object)));
      gtk_widget_set_visible (sl->priv->child_visible, gtk_widget_get_child_visible (GTK_WIDGET (sl->priv->object)));

      update_frame_clock (sl);
    }

  if (GTK_IS_BUILDABLE (sl->priv->object))
    {
      gtk_label_set_text (GTK_LABEL (sl->priv->buildable_id),
                          gtk_buildable_get_name (GTK_BUILDABLE (sl->priv->object)));
    }

  if (GTK_IS_WINDOW (sl->priv->object))
    {
      update_default_widget (sl);
    }

  if (GDK_IS_FRAME_CLOCK (sl->priv->object))
    {
      GdkFrameClock *clock;
      gint64 frame;
      gint64 frame_time;
      gint64 history_start;
      gint64 history_len;
      gint64 previous_frame_time;
      GdkFrameTimings *previous_timings;

      clock = GDK_FRAME_CLOCK (sl->priv->object);
      frame = gdk_frame_clock_get_frame_counter (clock);
      frame_time = gdk_frame_clock_get_frame_time (clock);

      tmp = g_strdup_printf ("%"G_GINT64_FORMAT, frame);
      gtk_label_set_label (GTK_LABEL (sl->priv->framecount), tmp);
      g_free (tmp);

      history_start = gdk_frame_clock_get_history_start (clock);
      history_len = frame - history_start;

      if (history_len > 0 && sl->priv->last_frame != frame)
        {
          previous_timings = gdk_frame_clock_get_timings (clock, history_start);
          previous_frame_time = gdk_frame_timings_get_frame_time (previous_timings);
          tmp = g_strdup_printf ("%4.1f ⁄ s", (G_USEC_PER_SEC * history_len) / (double) (frame_time - previous_frame_time));
          gtk_label_set_label (GTK_LABEL (sl->priv->framerate), tmp);
          g_free (tmp);
        }
      else
        {
          gtk_label_set_label (GTK_LABEL (sl->priv->framerate), "—");
        }

      sl->priv->last_frame = frame;
    }

  return G_SOURCE_CONTINUE;
}

void
gtk_inspector_misc_info_set_object (GtkInspectorMiscInfo *sl,
                                    GObject              *object)
{
  if (sl->priv->object)
    {
      g_signal_handlers_disconnect_by_func (sl->priv->object, state_flags_changed, sl);
      g_signal_handlers_disconnect_by_func (sl->priv->object, allocation_changed, sl);
      disconnect_each_other (sl->priv->object, G_OBJECT (sl));
      disconnect_each_other (sl, sl->priv->object);
      sl->priv->object = NULL;
    }

  gtk_widget_show (GTK_WIDGET (sl));

  sl->priv->object = object;
  g_object_weak_ref (G_OBJECT (sl), disconnect_each_other, object);
  g_object_weak_ref (object, disconnect_each_other, sl);

  if (GTK_IS_WIDGET (object))
    {
      gtk_widget_show (sl->priv->refcount_row);
      gtk_widget_show (sl->priv->state_row);
      gtk_widget_show (sl->priv->request_mode_row);
      gtk_widget_show (sl->priv->allocated_size_row);
      gtk_widget_show (sl->priv->baseline_row);
      gtk_widget_show (sl->priv->mnemonic_label_row);
      gtk_widget_show (sl->priv->tick_callback_row);
      gtk_widget_show (sl->priv->accessible_role_row);
      gtk_widget_show (sl->priv->accessible_name_row);
      gtk_widget_show (sl->priv->accessible_description_row);
      gtk_widget_show (sl->priv->mapped_row);
      gtk_widget_show (sl->priv->realized_row);
      gtk_widget_show (sl->priv->is_toplevel_row);
      gtk_widget_show (sl->priv->is_toplevel_row);
      gtk_widget_show (sl->priv->frame_clock_row);

      g_signal_connect_object (object, "state-flags-changed", G_CALLBACK (state_flags_changed), sl, 0);
      state_flags_changed (GTK_WIDGET (sl->priv->object), 0, sl);

      g_signal_connect_object (object, "size-allocate", G_CALLBACK (allocation_changed), sl, 0);
      allocation_changed (GTK_WIDGET (sl->priv->object), 0, 0, -1, sl);
    }
  else
    {
      gtk_widget_hide (sl->priv->state_row);
      gtk_widget_hide (sl->priv->request_mode_row);
      gtk_widget_hide (sl->priv->mnemonic_label_row);
      gtk_widget_hide (sl->priv->allocated_size_row);
      gtk_widget_hide (sl->priv->baseline_row);
      gtk_widget_hide (sl->priv->tick_callback_row);
      gtk_widget_hide (sl->priv->accessible_role_row);
      gtk_widget_hide (sl->priv->accessible_name_row);
      gtk_widget_hide (sl->priv->accessible_description_row);
      gtk_widget_hide (sl->priv->mapped_row);
      gtk_widget_hide (sl->priv->realized_row);
      gtk_widget_hide (sl->priv->is_toplevel_row);
      gtk_widget_hide (sl->priv->child_visible_row);
      gtk_widget_hide (sl->priv->frame_clock_row);
    }

  if (GTK_IS_BUILDABLE (object))
    {
      gtk_widget_show (sl->priv->buildable_id_row);
    }
  else
    {
      gtk_widget_hide (sl->priv->buildable_id_row);
    }

  if (GTK_IS_WINDOW (object))
    {
      gtk_widget_show (sl->priv->default_widget_row);
    }
  else
    {
      gtk_widget_hide (sl->priv->default_widget_row);
    }

  if (GDK_IS_FRAME_CLOCK (object))
    {
      gtk_widget_show (sl->priv->framecount_row);
      gtk_widget_show (sl->priv->framerate_row);
    }
  else
    {
      gtk_widget_hide (sl->priv->framecount_row);
      gtk_widget_hide (sl->priv->framerate_row);
    }

  update_info (sl);
}

static void
gtk_inspector_misc_info_init (GtkInspectorMiscInfo *sl)
{
  sl->priv = gtk_inspector_misc_info_get_instance_private (sl);
  gtk_widget_init_template (GTK_WIDGET (sl));
}

static void
map (GtkWidget *widget)
{
  GtkInspectorMiscInfo *sl = GTK_INSPECTOR_MISC_INFO (widget);

  GTK_WIDGET_CLASS (gtk_inspector_misc_info_parent_class)->map (widget);

  sl->priv->update_source_id = g_timeout_add_seconds (1, update_info, sl);
  update_info (sl);
}

static void
unmap (GtkWidget *widget)
{
  GtkInspectorMiscInfo *sl = GTK_INSPECTOR_MISC_INFO (widget);

  g_source_remove (sl->priv->update_source_id);
  sl->priv->update_source_id = 0;

  GTK_WIDGET_CLASS (gtk_inspector_misc_info_parent_class)->unmap (widget);
}

static void
get_property (GObject    *object,
              guint       param_id,
              GValue     *value,
              GParamSpec *pspec)
{
  GtkInspectorMiscInfo *sl = GTK_INSPECTOR_MISC_INFO (object);

  switch (param_id)
    {
      case PROP_OBJECT_TREE:
        g_value_take_object (value, sl->priv->object_tree);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
set_property (GObject      *object,
              guint         param_id,
              const GValue *value,
              GParamSpec   *pspec)
{
  GtkInspectorMiscInfo *sl = GTK_INSPECTOR_MISC_INFO (object);

  switch (param_id)
    {
      case PROP_OBJECT_TREE:
        sl->priv->object_tree = g_value_get_object (value);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
gtk_inspector_misc_info_class_init (GtkInspectorMiscInfoClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = get_property;
  object_class->set_property = set_property;

  widget_class->map = map;
  widget_class->unmap = unmap;

  g_object_class_install_property (object_class, PROP_OBJECT_TREE,
      g_param_spec_object ("object-tree", "Object Tree", "Object tree",
                           GTK_TYPE_WIDGET, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/libgtk/inspector/misc-info.ui");
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorMiscInfo, address);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorMiscInfo, refcount_row);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorMiscInfo, refcount);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorMiscInfo, state_row);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorMiscInfo, state);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorMiscInfo, buildable_id_row);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorMiscInfo, buildable_id);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorMiscInfo, default_widget_row);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorMiscInfo, default_widget);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorMiscInfo, default_widget_button);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorMiscInfo, mnemonic_label_row);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorMiscInfo, mnemonic_label);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorMiscInfo, request_mode_row);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorMiscInfo, request_mode);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorMiscInfo, allocated_size_row);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorMiscInfo, allocated_size);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorMiscInfo, baseline_row);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorMiscInfo, baseline);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorMiscInfo, frame_clock_row);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorMiscInfo, frame_clock);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorMiscInfo, frame_clock_button);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorMiscInfo, tick_callback_row);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorMiscInfo, tick_callback);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorMiscInfo, framecount_row);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorMiscInfo, framecount);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorMiscInfo, framerate_row);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorMiscInfo, framerate);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorMiscInfo, accessible_role_row);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorMiscInfo, accessible_role);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorMiscInfo, accessible_name_row);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorMiscInfo, accessible_name);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorMiscInfo, accessible_description_row);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorMiscInfo, accessible_description);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorMiscInfo, mapped_row);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorMiscInfo, mapped);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorMiscInfo, realized_row);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorMiscInfo, realized);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorMiscInfo, is_toplevel_row);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorMiscInfo, is_toplevel);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorMiscInfo, child_visible_row);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorMiscInfo, child_visible);

  gtk_widget_class_bind_template_callback (widget_class, show_default_widget);
  gtk_widget_class_bind_template_callback (widget_class, show_frame_clock);
}

// vim: set et sw=2 ts=2:

