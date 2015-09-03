/* GTK - The GIMP Toolkit
 *
 * Copyright (C) 2015 Red Hat, Inc.
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

#include "gtkdatechooserdayprivate.h"
#include "gtklabel.h"
#include "gtkgesturemultipress.h"
#include "gtkrender.h"
#include "gtkwidgetprivate.h"
#include "gtkselection.h"
#include "gtkdnd.h"

#include "gtkintl.h"
#include "gtkprivate.h"

#include <stdlib.h>
#include <langinfo.h>

enum {
  SELECTED,
  LAST_DAY_SIGNAL
};

static guint day_signals[LAST_DAY_SIGNAL] = { 0, };

struct _GtkDateChooserDay
{
  GtkBin parent;
  GtkWidget *label;
  guint day;
  guint month;
  guint year;
  GdkWindow *event_window;
  GtkGesture *multipress_gesture;
};

struct _GtkDateChooserDayClass
{
  GtkBinClass parent_class;
};

G_DEFINE_TYPE (GtkDateChooserDay, gtk_date_chooser_day, GTK_TYPE_BIN)

static void
day_pressed (GtkGestureMultiPress *gesture,
             gint                  n_press,
             gdouble               x,
             gdouble               y,
             GtkDateChooserDay    *day)
{
  gint button;

  button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));

  if (button == GDK_BUTTON_PRIMARY)
    {
      if (n_press == 1)
        g_signal_emit (day, day_signals[SELECTED], 0);
    }
}

static gboolean
gtk_date_chooser_day_key_press (GtkWidget   *widget,
                                GdkEventKey *event)
{
  GtkDateChooserDay *day = GTK_DATE_CHOOSER_DAY (widget);

  if (event->keyval == GDK_KEY_space ||
      event->keyval == GDK_KEY_Return ||
      event->keyval == GDK_KEY_ISO_Enter||
      event->keyval == GDK_KEY_KP_Enter ||
      event->keyval == GDK_KEY_KP_Space)
    {
      g_signal_emit (day, day_signals[SELECTED], 0);
      return TRUE;
    }

  if (GTK_WIDGET_CLASS (gtk_date_chooser_day_parent_class)->key_press_event (widget, event))
    return TRUE;

 return FALSE;
}

static void
gtk_date_chooser_day_init (GtkDateChooserDay *day)
{
  GtkWidget *widget = GTK_WIDGET (day);

  gtk_widget_set_can_focus (widget, TRUE);
  gtk_style_context_add_class (gtk_widget_get_style_context (widget), "day");

  day->label = gtk_label_new ("");
  gtk_widget_show (day->label);
  gtk_widget_set_halign (day->label, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (day->label, GTK_ALIGN_CENTER);
  gtk_widget_set_hexpand (day->label, TRUE);
  gtk_widget_set_vexpand (day->label, TRUE);
  gtk_label_set_xalign (GTK_LABEL (day->label), 1.0);

  gtk_container_add (GTK_CONTAINER (day), day->label);

  day->multipress_gesture = gtk_gesture_multi_press_new (widget);
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (day->multipress_gesture), 0);
  g_signal_connect (day->multipress_gesture, "pressed",
                    G_CALLBACK (day_pressed), day);
}

static void
gtk_date_chooser_day_dispose (GObject *object)
{
  GtkDateChooserDay *day = GTK_DATE_CHOOSER_DAY (object);

  g_clear_object (&day->multipress_gesture);

  G_OBJECT_CLASS (gtk_date_chooser_day_parent_class)->dispose (object);
}

static gboolean
gtk_date_chooser_day_draw (GtkWidget *widget,
                           cairo_t   *cr)
{
  GtkStyleContext *context;
  GtkStateFlags state;
  gint x, y, width, height;

  context = gtk_widget_get_style_context (widget);
  state = gtk_widget_get_state_flags (widget);

  x = 0;
  y = 0;
  width = gtk_widget_get_allocated_width (widget);
  height = gtk_widget_get_allocated_height (widget);

  gtk_render_background (context, cr, x, y, width, height);
  gtk_render_frame (context, cr, x, y, width, height);

  GTK_WIDGET_CLASS (gtk_date_chooser_day_parent_class)->draw (widget, cr);

  if (gtk_widget_has_visible_focus (widget))
    {
      GtkBorder border;

      gtk_style_context_get_border (context, state, &border);
      gtk_render_focus (context, cr, border.left, border.top,
                        gtk_widget_get_allocated_width (widget) - border.left - border.right,
                        gtk_widget_get_allocated_height (widget) - border.top - border.bottom);
    }

  return FALSE;
}

static void
gtk_date_chooser_day_map (GtkWidget *widget)
{
  GtkDateChooserDay *day = GTK_DATE_CHOOSER_DAY (widget);

  GTK_WIDGET_CLASS (gtk_date_chooser_day_parent_class)->map (widget);

  gdk_window_show (day->event_window);
}

static void
gtk_date_chooser_day_unmap (GtkWidget *widget)
{
  GtkDateChooserDay *day = GTK_DATE_CHOOSER_DAY (widget);

  gdk_window_hide (day->event_window);

  GTK_WIDGET_CLASS (gtk_date_chooser_day_parent_class)->unmap (widget);
}

static void
gtk_date_chooser_day_realize (GtkWidget *widget)
{
  GtkDateChooserDay *day = GTK_DATE_CHOOSER_DAY (widget);
  GtkAllocation allocation;
  GdkWindow *window;
  GdkWindowAttr attributes;
  gint attributes_mask;

  gtk_widget_get_allocation (widget, &allocation);
  gtk_widget_set_realized (widget, TRUE);

  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.x = allocation.x;
  attributes.y = allocation.y;
  attributes.width = allocation.width;
  attributes.height = allocation.height;
  attributes.wclass = GDK_INPUT_ONLY;
  attributes.event_mask = gtk_widget_get_events (widget);
  attributes.event_mask |= GDK_BUTTON_PRESS_MASK
                           | GDK_BUTTON_RELEASE_MASK
                           | GDK_TOUCH_MASK;

  attributes_mask = GDK_WA_X | GDK_WA_Y;

  window = gtk_widget_get_parent_window (widget);
  gtk_widget_set_window (widget, window);
  g_object_ref (window);

  day->event_window = gdk_window_new (window, &attributes, attributes_mask);
  gtk_widget_register_window (widget, day->event_window);
}

static void
gtk_date_chooser_day_unrealize (GtkWidget *widget)
{
  GtkDateChooserDay *day = GTK_DATE_CHOOSER_DAY (widget);

  if (day->event_window)
    {
      gtk_widget_unregister_window (widget, day->event_window);
      gdk_window_destroy (day->event_window);
      day->event_window = NULL;
    }

  GTK_WIDGET_CLASS (gtk_date_chooser_day_parent_class)->unrealize (widget);
}

static void
gtk_date_chooser_day_size_allocate (GtkWidget     *widget,
                                    GtkAllocation *allocation)
{
  GtkDateChooserDay *day = GTK_DATE_CHOOSER_DAY (widget);

  GTK_WIDGET_CLASS (gtk_date_chooser_day_parent_class)->size_allocate (widget, allocation);

  if (gtk_widget_get_realized (widget))
    gdk_window_move_resize (day->event_window,
                            allocation->x,
                            allocation->y,
                            allocation->width,
                            allocation->height);
}

static void
gtk_date_chooser_day_drag_data_get (GtkWidget        *widget,
                                    GdkDragContext   *context,
                                    GtkSelectionData *selection_data,
                                    guint             info,
                                    guint             time)
{
  GtkDateChooserDay *day = GTK_DATE_CHOOSER_DAY (widget);
  GDateTime *dt;
  gchar *text;

  dt = g_date_time_new_local (day->year, day->month + 1 , day->day, 1, 1, 1);
  text = g_date_time_format (dt, "%x");
  gtk_selection_data_set_text (selection_data, text, -1);
  g_free (text);
  g_date_time_unref (dt);
}

static void
gtk_date_chooser_day_class_init (GtkDateChooserDayClass *class)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = gtk_date_chooser_day_dispose;

  widget_class->draw = gtk_date_chooser_day_draw;
  widget_class->realize = gtk_date_chooser_day_realize;
  widget_class->unrealize = gtk_date_chooser_day_unrealize;
  widget_class->map = gtk_date_chooser_day_map;
  widget_class->unmap = gtk_date_chooser_day_unmap;
  widget_class->key_press_event = gtk_date_chooser_day_key_press;
  widget_class->size_allocate = gtk_date_chooser_day_size_allocate;
  widget_class->drag_data_get = gtk_date_chooser_day_drag_data_get;

  day_signals[SELECTED] = g_signal_new (I_("selected"),
                                        G_OBJECT_CLASS_TYPE (object_class),
                                        G_SIGNAL_RUN_FIRST,
                                        0,
                                        NULL, NULL,
                                        NULL,
                                        G_TYPE_NONE, 0);
}

GtkWidget *
gtk_date_chooser_day_new (void)
{
  return GTK_WIDGET (g_object_new (GTK_TYPE_DATE_CHOOSER_DAY, NULL));
}

void
gtk_date_chooser_day_set_date (GtkDateChooserDay *day,
                               guint              y,
                               guint              m,
                               guint              d)
{
  gchar *text;

  day->year = y;
  day->month = m;
  day->day = d;

  text = g_strdup_printf ("%d", day->day);
  gtk_label_set_label (GTK_LABEL (day->label), text);
  g_free (text);
}

void
gtk_date_chooser_day_get_date (GtkDateChooserDay *day,
                               guint             *y,
                               guint             *m,
                               guint             *d)
{
  *y = day->year;
  *m = day->month;
  *d = day->day;
}

void
gtk_date_chooser_day_set_other_month (GtkDateChooserDay *day,
                                      gboolean           other_month)
{
  GtkStyleContext *context;

  context = gtk_widget_get_style_context (GTK_WIDGET (day));
  if (other_month)
    {
      gtk_style_context_add_class (context, GTK_STYLE_CLASS_DIM_LABEL);
      gtk_drag_source_unset (GTK_WIDGET (day));
    }
  else
    {
      gtk_style_context_remove_class (context, GTK_STYLE_CLASS_DIM_LABEL);
      gtk_drag_source_set (GTK_WIDGET (day),
                           GDK_BUTTON1_MASK | GDK_BUTTON3_MASK,
                           NULL, 0,
                           GDK_ACTION_COPY);
      gtk_drag_source_add_text_targets (GTK_WIDGET (day));
    }
}

void
gtk_date_chooser_day_set_selected (GtkDateChooserDay *day,
                                   gboolean           selected)
{
  if (selected)
    gtk_widget_set_state_flags (GTK_WIDGET (day), GTK_STATE_FLAG_SELECTED, FALSE);
  else
    gtk_widget_unset_state_flags (GTK_WIDGET (day), GTK_STATE_FLAG_SELECTED);
}
