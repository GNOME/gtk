/* GTK - The GIMP Toolkit
 * Copyright (C) 2007 Carlos Garnacho <carlos@imendio.com>
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

#include <gtk/gtktimeline.h>
#include <gtk/gtktypebuiltins.h>
#include <gtk/gtksettings.h>
#include <math.h>

typedef struct GtkTimelinePriv GtkTimelinePriv;

struct GtkTimelinePriv
{
  guint duration;

  guint64 last_time;
  gdouble elapsed_time;

  gdouble progress;
  gdouble last_progress;

  GtkWidget *widget;
  GdkFrameClock *frame_clock;
  GdkScreen *screen;

  guint update_id;

  GtkTimelineProgressType progress_type;

  guint animations_enabled : 1;
  guint loop               : 1;
  guint direction          : 1;
  guint running            : 1;
};

enum {
  PROP_0,
  PROP_DURATION,
  PROP_LOOP,
  PROP_DIRECTION,
  PROP_FRAME_CLOCK,
  PROP_PROGRESS_TYPE,
  PROP_SCREEN,
  PROP_WIDGET
};

enum {
  STARTED,
  PAUSED,
  FINISHED,
  FRAME,
  LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0, };


static void gtk_timeline_start_running (GtkTimeline *timeline);
static void gtk_timeline_stop_running (GtkTimeline *timeline);

static void frame_clock_target_iface_init (GdkFrameClockTargetInterface *target);

static void  gtk_timeline_set_property  (GObject         *object,
                                         guint            prop_id,
                                         const GValue    *value,
                                         GParamSpec      *pspec);
static void  gtk_timeline_get_property  (GObject         *object,
                                         guint            prop_id,
                                         GValue          *value,
                                         GParamSpec      *pspec);
static void  gtk_timeline_finalize      (GObject *object);

static void  gtk_timeline_set_clock     (GdkFrameClockTarget *target,
                                         GdkFrameClock       *frame_clock);

G_DEFINE_TYPE_WITH_CODE (GtkTimeline, gtk_timeline, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GDK_TYPE_FRAME_CLOCK_TARGET, frame_clock_target_iface_init))

static void
gtk_timeline_class_init (GtkTimelineClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = gtk_timeline_set_property;
  object_class->get_property = gtk_timeline_get_property;
  object_class->finalize = gtk_timeline_finalize;

  g_object_class_install_property (object_class,
                                   PROP_DURATION,
                                   g_param_spec_uint ("duration",
                                                      "Animation Duration",
                                                      "Animation Duration",
                                                      0, G_MAXUINT,
                                                      0,
                                                      G_PARAM_READWRITE));
  g_object_class_install_property (object_class,
                                   PROP_LOOP,
                                   g_param_spec_boolean ("loop",
                                                         "Loop",
                                                         "Whether the timeline loops or not",
                                                         FALSE,
                                                         G_PARAM_READWRITE));
  g_object_class_install_property (object_class,
                                   PROP_FRAME_CLOCK,
                                   g_param_spec_object ("paint-clock",
                                                        "Frame Clock",
                                                        "clock used for timing the animation (not needed if :widget is set)",
                                                        GDK_TYPE_FRAME_CLOCK,
                                                        G_PARAM_READWRITE));
  g_object_class_install_property (object_class,
                                   PROP_PROGRESS_TYPE,
                                   g_param_spec_enum ("progress-type",
                                                      "Progress Type",
                                                      "Easing function for animation progress",
                                                      GTK_TYPE_TIMELINE_PROGRESS_TYPE,
                                                      GTK_TIMELINE_PROGRESS_EASE_OUT,
                                                      G_PARAM_READWRITE));
  g_object_class_install_property (object_class,
                                   PROP_SCREEN,
                                   g_param_spec_object ("screen",
                                                        "Screen",
                                                        "Screen to get the settings from (not needed if :widget is set)",
                                                        GDK_TYPE_SCREEN,
                                                        G_PARAM_READWRITE));
  g_object_class_install_property (object_class,
                                   PROP_WIDGET,
                                   g_param_spec_object ("widget",
                                                        "Widget",
                                                        "Widget the timeline will be used with",
                                                        GTK_TYPE_WIDGET,
                                                        G_PARAM_READWRITE));

  signals[STARTED] =
    g_signal_new ("started",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkTimelineClass, started),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  signals[PAUSED] =
    g_signal_new ("paused",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkTimelineClass, paused),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  signals[FINISHED] =
    g_signal_new ("finished",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkTimelineClass, finished),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  signals[FRAME] =
    g_signal_new ("frame",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkTimelineClass, frame),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__DOUBLE,
                  G_TYPE_NONE, 1,
                  G_TYPE_DOUBLE);

  g_type_class_add_private (klass, sizeof (GtkTimelinePriv));
}

static void
frame_clock_target_iface_init (GdkFrameClockTargetInterface *iface)
{
  iface->set_clock = gtk_timeline_set_clock;
}

static void
gtk_timeline_init (GtkTimeline *timeline)
{
  GtkTimelinePriv *priv;

  priv = timeline->priv = G_TYPE_INSTANCE_GET_PRIVATE (timeline,
                                                       GTK_TYPE_TIMELINE,
                                                       GtkTimelinePriv);

  priv->duration = 0.0;
  priv->direction = GTK_TIMELINE_DIRECTION_FORWARD;
  priv->progress_type = GTK_TIMELINE_PROGRESS_EASE_OUT;
  priv->screen = gdk_screen_get_default ();

  priv->last_progress = 0;
}

static void
gtk_timeline_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  GtkTimeline *timeline;

  timeline = GTK_TIMELINE (object);

  switch (prop_id)
    {
    case PROP_DURATION:
      gtk_timeline_set_duration (timeline, g_value_get_uint (value));
      break;
    case PROP_LOOP:
      gtk_timeline_set_loop (timeline, g_value_get_boolean (value));
      break;
    case PROP_DIRECTION:
      gtk_timeline_set_direction (timeline, g_value_get_enum (value));
      break;
    case PROP_FRAME_CLOCK:
      gtk_timeline_set_frame_clock (timeline,
                                    GDK_FRAME_CLOCK (g_value_get_object (value)));
      break;
    case PROP_PROGRESS_TYPE:
      gtk_timeline_set_progress_type (timeline,
                                      g_value_get_enum (value));
      break;
    case PROP_SCREEN:
      gtk_timeline_set_screen (timeline,
                               GDK_SCREEN (g_value_get_object (value)));
      break;
    case PROP_WIDGET:
      gtk_timeline_set_widget (timeline,
                               GTK_WIDGET (g_value_get_object (value)));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_timeline_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  GtkTimeline *timeline;
  GtkTimelinePriv *priv;

  timeline = GTK_TIMELINE (object);
  priv = timeline->priv;

  switch (prop_id)
    {
    case PROP_DURATION:
      g_value_set_uint (value, priv->duration);
      break;
    case PROP_LOOP:
      g_value_set_boolean (value, priv->loop);
      break;
    case PROP_DIRECTION:
      g_value_set_enum (value, priv->direction);
      break;
    case PROP_FRAME_CLOCK:
      g_value_set_object (value, priv->frame_clock);
      break;
    case PROP_PROGRESS_TYPE:
      g_value_set_enum (value, priv->progress_type);
      break;
    case PROP_SCREEN:
      g_value_set_object (value, priv->screen);
      break;
    case PROP_WIDGET:
      g_value_set_object (value, priv->widget);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_timeline_finalize (GObject *object)
{
  GtkTimelinePriv *priv;
  GtkTimeline *timeline;

  timeline = (GtkTimeline *) object;
  priv = timeline->priv;

  if (priv->running)
    {
      gtk_timeline_stop_running (timeline);
      priv->running = FALSE;
    }

  G_OBJECT_CLASS (gtk_timeline_parent_class)->finalize (object);
}

/* Implementation of GdkFrameClockTarget method */
static void
gtk_timeline_set_clock (GdkFrameClockTarget *target,
                        GdkFrameClock       *frame_clock)
{
  gtk_timeline_set_frame_clock (GTK_TIMELINE (target),
                                frame_clock);
}

static gdouble
calculate_progress (gdouble                 linear_progress,
                    GtkTimelineProgressType progress_type)
{
  gdouble progress;

  progress = linear_progress;

  switch (progress_type)
    {
    case GTK_TIMELINE_PROGRESS_LINEAR:
      break;
    case GTK_TIMELINE_PROGRESS_EASE_IN_OUT:
      progress *= 2;

      if (progress < 1)
        progress = pow (progress, 3) / 2;
      else
        progress = (pow (progress - 2, 3) + 2) / 2;

      break;
    case GTK_TIMELINE_PROGRESS_EASE:
      progress = (sin ((progress - 0.5) * G_PI) + 1) / 2;
      break;
    case GTK_TIMELINE_PROGRESS_EASE_IN:
      progress = pow (progress, 3);
      break;
    case GTK_TIMELINE_PROGRESS_EASE_OUT:
      progress = pow (progress - 1, 3) + 1;
      break;
    default:
      g_warning ("Timeline progress type not implemented");
    }

  return progress;
}

static void
gtk_timeline_on_update (GdkFrameClock *clock,
                        GtkTimeline   *timeline)
{
  GtkTimelinePriv *priv;
  gdouble delta_progress, progress, adjust;
  guint64 now;

  /* the user may unref us during the signals, so save ourselves */
  g_object_ref (timeline);

  priv = timeline->priv;

  now = gdk_frame_clock_get_frame_time (clock);
  priv->elapsed_time = (now - priv->last_time) / 1000;
  priv->last_time = now;

  if (priv->animations_enabled)
    {
      delta_progress = (gdouble) priv->elapsed_time / priv->duration;
      progress = priv->last_progress;

      if (priv->direction == GTK_TIMELINE_DIRECTION_BACKWARD)
        progress -= delta_progress;
      else
        progress += delta_progress;

      priv->last_progress = progress;

      /* When looping, if we go past the end, start that much into the
       * next cycle */
      if (progress < 0.0)
        {
          adjust = progress - ceil(progress);
          progress = 0.0;
        }
      else if (progress > 1.0)
        {
          adjust = progress - floor(progress);
          progress = 1.0;
        }
      else
        adjust = 0.0;

      progress = CLAMP (progress, 0., 1.);
    }
  else
    progress = (priv->direction == GTK_TIMELINE_DIRECTION_FORWARD) ? 1.0 : 0.0;

  priv->progress = progress;
  g_signal_emit (timeline, signals [FRAME], 0,
                 calculate_progress (progress, priv->progress_type));

  if ((priv->direction == GTK_TIMELINE_DIRECTION_FORWARD && progress == 1.0) ||
      (priv->direction == GTK_TIMELINE_DIRECTION_BACKWARD && progress == 0.0))
    {
      gboolean loop;

      loop = priv->loop && priv->animations_enabled;

      if (loop)
        {
          gtk_timeline_rewind (timeline);
          priv->progress += adjust;
        }
      else
        {
          gtk_timeline_stop_running (timeline);
          priv->running = FALSE;

          g_signal_emit (timeline, signals [FINISHED], 0);
          g_object_unref (timeline);
          return;
        }
    }

  g_object_unref (timeline);
  gdk_frame_clock_request_phase (clock, GDK_FRAME_CLOCK_PHASE_UPDATE);
}

static void
gtk_timeline_start_updating (GtkTimeline *timeline)
{
  GtkTimelinePriv *priv = timeline->priv;

  g_assert (priv->running && priv->frame_clock && priv->update_id == 0);

  priv->update_id = g_signal_connect (priv->frame_clock,
                                      "update",
                                      G_CALLBACK (gtk_timeline_on_update),
                                      timeline);

  gdk_frame_clock_request_phase (priv->frame_clock, GDK_FRAME_CLOCK_PHASE_UPDATE);
  priv->last_time = gdk_frame_clock_get_frame_time (priv->frame_clock);
}

static void
gtk_timeline_stop_updating (GtkTimeline *timeline)
{
  GtkTimelinePriv *priv = timeline->priv;

  g_assert (priv->running && priv->frame_clock && priv->update_id != 0);

  g_signal_handler_disconnect (priv->frame_clock,
                               priv->update_id);
  priv->update_id = 0;
}

static void
gtk_timeline_start_running (GtkTimeline *timeline)
{
  GtkTimelinePriv *priv = timeline->priv;

  g_assert (priv->running);

  if (priv->widget)
    gtk_widget_add_frame_clock_target (priv->widget,
                                       GDK_FRAME_CLOCK_TARGET (timeline));
  else if (priv->frame_clock)
    gtk_timeline_start_updating (timeline);
}

static void
gtk_timeline_stop_running (GtkTimeline *timeline)
{
  GtkTimelinePriv *priv = timeline->priv;

  g_assert (priv->running);

  if (priv->widget)
    gtk_widget_remove_frame_clock_target (priv->widget,
                                          GDK_FRAME_CLOCK_TARGET (timeline));
  else if (priv->frame_clock)
    gtk_timeline_stop_updating (timeline);
}

/**
 * gtk_timeline_new:
 * @widget: a widget the timeline will be used with
 * @duration: duration in milliseconds for the timeline
 *
 * Creates a new #GtkTimeline with the specified duration
 *
 * Return Value: the newly created #GtkTimeline
 */
GtkTimeline *
gtk_timeline_new (GtkWidget *widget,
                  guint      duration)
{
  return g_object_new (GTK_TYPE_TIMELINE,
                       "widget", widget,
                       "duration", duration,
                       NULL);
}

/**
 * gtk_timeline_start:
 * @timeline: A #GtkTimeline
 *
 * Runs the timeline from the current frame.
 */
void
gtk_timeline_start (GtkTimeline *timeline)
{
  GtkTimelinePriv *priv;
  GtkSettings *settings;
  gboolean enable_animations = FALSE;

  g_return_if_fail (GTK_IS_TIMELINE (timeline));

  priv = timeline->priv;

  if (!priv->running)
    {
      if (priv->screen)
        {
          settings = gtk_settings_get_for_screen (priv->screen);
          g_object_get (settings, "gtk-enable-animations", &enable_animations, NULL);
        }

      priv->animations_enabled = enable_animations;

      priv->running = TRUE;
      gtk_timeline_start_running (timeline);

      g_signal_emit (timeline, signals [STARTED], 0);
    }
}

/**
 * gtk_timeline_pause:
 * @timeline: A #GtkTimeline
 *
 * Pauses the timeline.
 */
void
gtk_timeline_pause (GtkTimeline *timeline)
{
  GtkTimelinePriv *priv;

  g_return_if_fail (GTK_IS_TIMELINE (timeline));

  priv = timeline->priv;

  if (priv->running)
    {
      gtk_timeline_stop_running (timeline);
      priv->running = FALSE;

      g_signal_emit (timeline, signals [PAUSED], 0);
    }
}

/**
 * gtk_timeline_rewind:
 * @timeline: A #GtkTimeline
 *
 * Rewinds the timeline.
 */
void
gtk_timeline_rewind (GtkTimeline *timeline)
{
  GtkTimelinePriv *priv;

  g_return_if_fail (GTK_IS_TIMELINE (timeline));

  priv = timeline->priv;

  if (gtk_timeline_get_direction (timeline) != GTK_TIMELINE_DIRECTION_FORWARD)
    priv->progress = priv->last_progress = 1.;
  else
    priv->progress = priv->last_progress = 0.;

  if (priv->running && priv->frame_clock)
    priv->last_time = gdk_frame_clock_get_frame_time (priv->frame_clock);
}

/**
 * gtk_timeline_is_running:
 * @timeline: A #GtkTimeline
 *
 * Returns whether the timeline is running or not.
 *
 * Return Value: %TRUE if the timeline is running
 */
gboolean
gtk_timeline_is_running (GtkTimeline *timeline)
{
  GtkTimelinePriv *priv;

  g_return_val_if_fail (GTK_IS_TIMELINE (timeline), FALSE);

  priv = timeline->priv;

  return priv->running;
}

/**
 * gtk_timeline_get_elapsed_time:
 * @timeline: A #GtkTimeline
 *
 * Returns the elapsed time since the last GtkTimeline::frame signal
 *
 * Return Value: elapsed time in milliseconds since the last frame
 */
guint
gtk_timeline_get_elapsed_time (GtkTimeline *timeline)
{
  GtkTimelinePriv *priv;

  g_return_val_if_fail (GTK_IS_TIMELINE (timeline), 0);

  priv = timeline->priv;
  return priv->elapsed_time;
}

/**
 * gtk_timeline_get_loop:
 * @timeline: A #GtkTimeline
 *
 * Returns whether the timeline loops to the
 * beginning when it has reached the end.
 *
 * Return Value: %TRUE if the timeline loops
 */
gboolean
gtk_timeline_get_loop (GtkTimeline *timeline)
{
  GtkTimelinePriv *priv;

  g_return_val_if_fail (GTK_IS_TIMELINE (timeline), FALSE);

  priv = timeline->priv;
  return priv->loop;
}

/**
 * gtk_timeline_set_loop:
 * @timeline: A #GtkTimeline
 * @loop: %TRUE to make the timeline loop
 *
 * Sets whether the timeline loops to the beginning
 * when it has reached the end.
 */
void
gtk_timeline_set_loop (GtkTimeline *timeline,
                        gboolean     loop)
{
  GtkTimelinePriv *priv;

  g_return_if_fail (GTK_IS_TIMELINE (timeline));

  priv = timeline->priv;

  if (loop != priv->loop)
    {
      priv->loop = loop;
      g_object_notify (G_OBJECT (timeline), "loop");
    }
}

void
gtk_timeline_set_duration (GtkTimeline *timeline,
                            guint        duration)
{
  GtkTimelinePriv *priv;

  g_return_if_fail (GTK_IS_TIMELINE (timeline));

  priv = timeline->priv;

  if (duration != priv->duration)
    {
      priv->duration = duration;
      g_object_notify (G_OBJECT (timeline), "duration");
    }
}

guint
gtk_timeline_get_duration (GtkTimeline *timeline)
{
  GtkTimelinePriv *priv;

  g_return_val_if_fail (GTK_IS_TIMELINE (timeline), 0);

  priv = timeline->priv;

  return priv->duration;
}

/**
 * gtk_timeline_set_direction:
 * @timeline: A #GtkTimeline
 * @direction: direction
 *
 * Sets the direction of the timeline.
 */
void
gtk_timeline_set_direction (GtkTimeline          *timeline,
                             GtkTimelineDirection  direction)
{
  GtkTimelinePriv *priv;

  g_return_if_fail (GTK_IS_TIMELINE (timeline));

  priv = timeline->priv;
  priv->direction = direction;
}

/*
 * gtk_timeline_get_direction:
 * @timeline: A #GtkTimeline
 *
 * Returns the direction of the timeline.
 *
 * Return Value: direction
 */
GtkTimelineDirection
gtk_timeline_get_direction (GtkTimeline *timeline)
{
  GtkTimelinePriv *priv;

  g_return_val_if_fail (GTK_IS_TIMELINE (timeline), GTK_TIMELINE_DIRECTION_FORWARD);

  priv = timeline->priv;
  return priv->direction;
}

void
gtk_timeline_set_frame_clock (GtkTimeline   *timeline,
                              GdkFrameClock *frame_clock)
{
  GtkTimelinePriv *priv;

  g_return_if_fail (GTK_IS_TIMELINE (timeline));
  g_return_if_fail (frame_clock == NULL || GDK_IS_FRAME_CLOCK (frame_clock));

  priv = timeline->priv;

  if (frame_clock == priv->frame_clock)
    return;

  if (priv->running && priv->frame_clock)
    gtk_timeline_stop_updating (timeline);

  if (priv->frame_clock)
    g_object_unref (priv->frame_clock);

  priv->frame_clock = frame_clock;

  if (priv->frame_clock)
    g_object_ref (priv->frame_clock);

  if (priv->running && priv->frame_clock)
    gtk_timeline_start_updating (timeline);

  g_object_notify (G_OBJECT (timeline), "paint-clock");
}

/**
 * gtk_timeline_get_frame_clock:
 *
 * Returns: (transfer none): 
 */
GdkFrameClock *
gtk_timeline_get_frame_clock (GtkTimeline *timeline)
{
  GtkTimelinePriv *priv;

  g_return_val_if_fail (GTK_IS_TIMELINE (timeline), NULL);

  priv = timeline->priv;
  return priv->frame_clock;
}

void
gtk_timeline_set_screen (GtkTimeline *timeline,
                         GdkScreen   *screen)
{
  GtkTimelinePriv *priv;

  g_return_if_fail (GTK_IS_TIMELINE (timeline));
  g_return_if_fail (screen == NULL || GDK_IS_SCREEN (screen));

  priv = timeline->priv;

  if (screen == priv->screen)
    return;

  if (priv->screen)
    g_object_unref (priv->screen);

  priv->screen = screen;

  if (priv->screen)
    g_object_ref (priv->screen);

  g_object_notify (G_OBJECT (timeline), "screen");
}

/**
 * gtk_timeline_get_screen:
 *
 * Returns: (transfer none): 
 */
GdkScreen *
gtk_timeline_get_screen (GtkTimeline *timeline)
{
  GtkTimelinePriv *priv;

  g_return_val_if_fail (GTK_IS_TIMELINE (timeline), NULL);

  priv = timeline->priv;
  return priv->screen;
}
void
gtk_timeline_set_widget (GtkTimeline *timeline,
                         GtkWidget   *widget)
{
  GtkTimelinePriv *priv;

  g_return_if_fail (GTK_IS_TIMELINE (timeline));
  g_return_if_fail (widget == NULL || GTK_IS_WIDGET (widget));

  priv = timeline->priv;

  if (widget == priv->widget)
    return;

  if (priv->running)
    gtk_timeline_stop_running (timeline);

  if (priv->widget)
    g_object_unref (priv->widget);

  priv->widget = widget;

  if (priv->widget)
    g_object_ref (widget);

  if (priv->running)
    gtk_timeline_start_running (timeline);

  g_object_notify (G_OBJECT (timeline), "widget");
}

/**
 * gtk_timeline_get_widget:
 *
 * Returns: (transfer none): 
 */
GtkWidget *
gtk_timeline_get_widget (GtkTimeline *timeline)
{
  GtkTimelinePriv *priv;

  g_return_val_if_fail (GTK_IS_TIMELINE (timeline), NULL);

  priv = timeline->priv;
  return priv->widget;
}

gdouble
gtk_timeline_get_progress (GtkTimeline *timeline)
{
  GtkTimelinePriv *priv;

  g_return_val_if_fail (GTK_IS_TIMELINE (timeline), 0.);

  priv = timeline->priv;
  return calculate_progress (priv->progress, priv->progress_type);
}

GtkTimelineProgressType
gtk_timeline_get_progress_type (GtkTimeline *timeline)
{
  GtkTimelinePriv *priv;

  g_return_val_if_fail (GTK_IS_TIMELINE (timeline), GTK_TIMELINE_PROGRESS_LINEAR);

  priv = timeline->priv;
  return priv->progress_type;
}

void
gtk_timeline_set_progress_type (GtkTimeline             *timeline,
                                GtkTimelineProgressType  progress_type)
{
  GtkTimelinePriv *priv;

  g_return_if_fail (GTK_IS_TIMELINE (timeline));

  priv = timeline->priv;
  priv->progress_type = progress_type;
}
