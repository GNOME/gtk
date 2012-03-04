/* GTK - The GIMP Toolkit
 * Copyright (C) 2012 Red Hat, Inc.
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

#include "gdk.h"
#include "gtkpressandholdprivate.h"
#include "gtkintl.h"
#include "gtkprivate.h"

struct _GtkPressAndHoldPrivate
{
  gint hold_time;
  gint drag_threshold;

  GdkEventSequence *sequence;
  guint timeout;
  gint start_x;
  gint start_y;
  gint x;
  gint y;
};

enum
{
  PROP_ZERO,
  PROP_HOLD_TIME,
  PROP_DRAG_THRESHOLD
};

enum
{
  HOLD,
  TAP,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (GtkPressAndHold, gtk_press_and_hold, G_TYPE_OBJECT)

static void
gtk_press_and_hold_init (GtkPressAndHold *pah)
{
  pah->priv = G_TYPE_INSTANCE_GET_PRIVATE (pah,
                                           GTK_TYPE_PRESS_AND_HOLD,
                                           GtkPressAndHoldPrivate);

  pah->priv->hold_time = 1000;
  pah->priv->drag_threshold = 8;
}

static void
press_and_hold_finalize (GObject *object)
{
  GtkPressAndHold *pah = GTK_PRESS_AND_HOLD (object);

  if (pah->priv->timeout)
    g_source_remove (pah->priv->timeout);

  G_OBJECT_CLASS (gtk_press_and_hold_parent_class)->finalize (object);
}

static void
press_and_hold_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  GtkPressAndHold *pah = GTK_PRESS_AND_HOLD (object);

  switch (prop_id)
    {
    case PROP_HOLD_TIME:
      g_value_set_int (value, pah->priv->hold_time);
      break;
    case PROP_DRAG_THRESHOLD:
      g_value_set_int (value, pah->priv->drag_threshold);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
press_and_hold_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  GtkPressAndHold *pah = GTK_PRESS_AND_HOLD (object);

  switch (prop_id)
    {
    case PROP_HOLD_TIME:
      pah->priv->hold_time = g_value_get_int (value);
      break;
    case PROP_DRAG_THRESHOLD:
      pah->priv->hold_time = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_press_and_hold_class_init (GtkPressAndHoldClass *class)
{
  GObjectClass *object_class = (GObjectClass *)class;

  object_class->get_property = press_and_hold_get_property;
  object_class->set_property = press_and_hold_set_property;
  object_class->finalize = press_and_hold_finalize;

  signals[HOLD] =
    g_signal_new ("hold",
                  GTK_TYPE_PRESS_AND_HOLD,
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkPressAndHoldClass, hold),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 2, G_TYPE_INT, G_TYPE_INT);

  signals[TAP] =
    g_signal_new ("tap",
                  GTK_TYPE_PRESS_AND_HOLD,
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkPressAndHoldClass, tap),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 2, G_TYPE_INT, G_TYPE_INT);

  g_object_class_install_property (object_class, PROP_HOLD_TIME,
      g_param_spec_int ("hold-time", P_("Hold Time"), P_("Hold Time (in milliseconds)"),
                        0, G_MAXINT, 1000, GTK_PARAM_READWRITE));

  g_object_class_install_property (object_class, PROP_DRAG_THRESHOLD,
      g_param_spec_int ("drag-threshold", P_("Drag Threshold"), P_("Drag Threshold (in pixels)"),
                        1, G_MAXINT, 8, GTK_PARAM_READWRITE));

  g_type_class_add_private (object_class, sizeof (GtkPressAndHoldPrivate));
}

static void
press_and_hold_cancel (GtkPressAndHold *pah)
{
  GtkPressAndHoldPrivate *priv = pah->priv;

  if (priv->timeout)
    g_source_remove (priv->timeout);

  priv->timeout = 0;
  priv->sequence = NULL;
}

static gboolean
hold_action (gpointer data)
{
  GtkPressAndHold *pah = data;
  GtkPressAndHoldPrivate *priv = pah->priv;

  press_and_hold_cancel (pah);

  g_signal_emit (pah, signals[HOLD], 0, priv->x, priv->y);

  return G_SOURCE_REMOVE;
}

void
gtk_press_and_hold_process_event (GtkPressAndHold *pah,
                                  GdkEvent        *event)
{
  GtkPressAndHoldPrivate *priv = pah->priv;

  /* We're already tracking a different touch, ignore */
  if ((event->type == GDK_TOUCH_BEGIN && priv->sequence != NULL) ||
      (event->type != GDK_TOUCH_BEGIN && priv->sequence != event->touch.sequence))
    return;

  priv->x = event->touch.x;
  priv->y = event->touch.y;

  if (event->type == GDK_TOUCH_BEGIN)
    {
      priv->sequence = event->touch.sequence;
      priv->start_x = priv->x;
      priv->start_y = priv->y;
      priv->timeout =
          gdk_threads_add_timeout (priv->hold_time, hold_action, pah);
    }
  else if (event->type == GDK_TOUCH_UPDATE)
    {
      if (ABS (priv->x - priv->start_x) > priv->drag_threshold ||
          ABS (priv->y - priv->start_y) > priv->drag_threshold)
        press_and_hold_cancel (pah);
    }
  else if (event->type == GDK_TOUCH_END)
    {
      press_and_hold_cancel (pah);
      g_signal_emit (pah, signals[TAP], 0, priv->x, priv->y);
    }
  else if (event->type == GDK_TOUCH_CANCEL)
    {
      press_and_hold_cancel (pah);
    }
}

GtkPressAndHold *
gtk_press_and_hold_new (void)
{
  return (GtkPressAndHold *) g_object_new (GTK_TYPE_PRESS_AND_HOLD, NULL);
}
