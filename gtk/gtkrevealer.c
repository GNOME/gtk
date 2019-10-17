/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * Copyright 2013, 2015 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Author: Alexander Larsson <alexl@redhat.com>
 *         Carlos Soriano <csoriano@gnome.org>
 */

#include "config.h"

#include "gtkrevealer.h"

#include "gtkintl.h"
#include "gtkprivate.h"
#include "gtkprogresstrackerprivate.h"
#include "gtksettingsprivate.h"
#include "gtktypebuiltins.h"
#include "gtkwidgetprivate.h"

#include "fallback-c89.c"

/**
 * SECTION:gtkrevealer
 * @Short_description: Hide and show with animation
 * @Title: GtkRevealer
 * @See_also: #GtkExpander
 *
 * The GtkRevealer widget is a container which animates
 * the transition of its child from invisible to visible.
 *
 * The style of transition can be controlled with
 * gtk_revealer_set_transition_type().
 *
 * These animations respect the #GtkSettings:gtk-enable-animations
 * setting.
 *
 * # CSS nodes
 *
 * GtkRevealer has a single CSS node with name revealer.
 * When styling #GtkRevealer using CSS, remember that it only hides its contents,
 * not itself. That means applied margin, padding and borders will be
 * visible even when the #GtkRevealer:reveal-child property is set to %FALSE.
 *
 * The GtkRevealer widget was added in GTK+ 3.10.
 */

/**
 * GtkRevealerTransitionType:
 * @GTK_REVEALER_TRANSITION_TYPE_NONE: No transition
 * @GTK_REVEALER_TRANSITION_TYPE_CROSSFADE: Fade in
 * @GTK_REVEALER_TRANSITION_TYPE_SLIDE_RIGHT: Slide in from the left
 * @GTK_REVEALER_TRANSITION_TYPE_SLIDE_LEFT: Slide in from the right
 * @GTK_REVEALER_TRANSITION_TYPE_SLIDE_UP: Slide in from the bottom
 * @GTK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN: Slide in from the top
 * @GTK_REVEALER_TRANSITION_TYPE_SWING_RIGHT: Floop in from the left
 * @GTK_REVEALER_TRANSITION_TYPE_SWING_LEFT: Floop in from the right
 * @GTK_REVEALER_TRANSITION_TYPE_SWING_UP: Floop in from the bottom
 * @GTK_REVEALER_TRANSITION_TYPE_SWING_DOWN: Floop in from the top
 *
 * These enumeration values describe the possible transitions
 * when the child of a #GtkRevealer widget is shown or hidden.
 */

enum  {
  PROP_0,
  PROP_TRANSITION_TYPE,
  PROP_TRANSITION_DURATION,
  PROP_REVEAL_CHILD,
  PROP_CHILD_REVEALED,
  LAST_PROP
};

typedef struct _GtkRevealerClass GtkRevealerClass;

struct _GtkRevealer {
  GtkBin parent_instance;
};

struct _GtkRevealerClass {
  GtkBinClass parent_class;
};

typedef struct {
  GtkRevealerTransitionType transition_type;
  guint transition_duration;

  gdouble current_pos;
  gdouble source_pos;
  gdouble target_pos;

  guint tick_id;
  GtkProgressTracker tracker;
} GtkRevealerPrivate;

static GParamSpec *props[LAST_PROP] = { NULL, };

static void     gtk_revealer_real_add                            (GtkContainer  *widget,
                                                                  GtkWidget     *child);
static void     gtk_revealer_real_size_allocate                  (GtkWidget     *widget,
                                                                  int            width,
                                                                  int            height,
                                                                  int            baseline);
static void gtk_revealer_measure (GtkWidget      *widget,
                                  GtkOrientation  orientation,
                                  int             for_size,
                                  int            *minimum,
                                  int            *natural,
                                  int            *minimum_baseline,
                                  int            *natural_baseline);

static void     gtk_revealer_set_position (GtkRevealer *revealer,
                                           gdouble      pos);

G_DEFINE_TYPE_WITH_PRIVATE (GtkRevealer, gtk_revealer, GTK_TYPE_BIN)

static void
gtk_revealer_init (GtkRevealer *revealer)
{
  GtkRevealerPrivate *priv = gtk_revealer_get_instance_private (revealer);

  priv->transition_type = GTK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN;
  priv->transition_duration = 250;
  priv->current_pos = 0.0;
  priv->target_pos = 0.0;

  gtk_widget_set_overflow (GTK_WIDGET (revealer), GTK_OVERFLOW_HIDDEN);
}

static void
gtk_revealer_finalize (GObject *obj)
{
  GtkRevealer *revealer = GTK_REVEALER (obj);
  GtkRevealerPrivate *priv = gtk_revealer_get_instance_private (revealer);

  if (priv->tick_id != 0)
    gtk_widget_remove_tick_callback (GTK_WIDGET (revealer), priv->tick_id);
  priv->tick_id = 0;

  G_OBJECT_CLASS (gtk_revealer_parent_class)->finalize (obj);
}

static void
gtk_revealer_get_property (GObject    *object,
                           guint       property_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  GtkRevealer *revealer = GTK_REVEALER (object);

  switch (property_id)
   {
    case PROP_TRANSITION_TYPE:
      g_value_set_enum (value, gtk_revealer_get_transition_type (revealer));
      break;
    case PROP_TRANSITION_DURATION:
      g_value_set_uint (value, gtk_revealer_get_transition_duration (revealer));
      break;
    case PROP_REVEAL_CHILD:
      g_value_set_boolean (value, gtk_revealer_get_reveal_child (revealer));
      break;
    case PROP_CHILD_REVEALED:
      g_value_set_boolean (value, gtk_revealer_get_child_revealed (revealer));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_revealer_set_property (GObject      *object,
                           guint         property_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  GtkRevealer *revealer = GTK_REVEALER (object);

  switch (property_id)
    {
    case PROP_TRANSITION_TYPE:
      gtk_revealer_set_transition_type (revealer, g_value_get_enum (value));
      break;
    case PROP_TRANSITION_DURATION:
      gtk_revealer_set_transition_duration (revealer, g_value_get_uint (value));
      break;
    case PROP_REVEAL_CHILD:
      gtk_revealer_set_reveal_child (revealer, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_revealer_unmap (GtkWidget *widget)
{
  GtkRevealer *revealer = GTK_REVEALER (widget);
  GtkRevealerPrivate *priv = gtk_revealer_get_instance_private (revealer);

  GTK_WIDGET_CLASS (gtk_revealer_parent_class)->unmap (widget);

  /* Finish & stop the animation */
  if (priv->current_pos != priv->target_pos)
    gtk_revealer_set_position (revealer, priv->target_pos);

  if (priv->tick_id != 0)
    {
      gtk_widget_remove_tick_callback (GTK_WIDGET (revealer), priv->tick_id);
      priv->tick_id = 0;
    }
}

static void
gtk_revealer_class_init (GtkRevealerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);

  object_class->get_property = gtk_revealer_get_property;
  object_class->set_property = gtk_revealer_set_property;
  object_class->finalize = gtk_revealer_finalize;

  widget_class->unmap = gtk_revealer_unmap;
  widget_class->size_allocate = gtk_revealer_real_size_allocate;
  widget_class->measure = gtk_revealer_measure;

  container_class->add = gtk_revealer_real_add;

  props[PROP_TRANSITION_TYPE] =
    g_param_spec_enum ("transition-type",
                       P_("Transition type"),
                       P_("The type of animation used to transition"),
                       GTK_TYPE_REVEALER_TRANSITION_TYPE,
                       GTK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN,
                       GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_TRANSITION_DURATION] =
    g_param_spec_uint ("transition-duration",
                       P_("Transition duration"),
                       P_("The animation duration, in milliseconds"),
                       0, G_MAXUINT, 250,
                       GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_REVEAL_CHILD] =
    g_param_spec_boolean ("reveal-child",
                          P_("Reveal Child"),
                          P_("Whether the container should reveal the child"),
                          FALSE,
                          GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_CHILD_REVEALED] =
    g_param_spec_boolean ("child-revealed",
                          P_("Child Revealed"),
                          P_("Whether the child is revealed and the animation target reached"),
                          FALSE,
                          GTK_PARAM_READABLE);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  gtk_widget_class_set_css_name (widget_class, I_("revealer"));
}

/**
 * gtk_revealer_new:
 *
 * Creates a new #GtkRevealer.
 *
 * Returns: a newly created #GtkRevealer
 */
GtkWidget *
gtk_revealer_new (void)
{
  return g_object_new (GTK_TYPE_REVEALER, NULL);
}

static GtkRevealerTransitionType
effective_transition (GtkRevealer *revealer)
{
  GtkRevealerPrivate *priv = gtk_revealer_get_instance_private (revealer);

  if (gtk_widget_get_direction (GTK_WIDGET (revealer)) == GTK_TEXT_DIR_RTL)
    {
      if (priv->transition_type == GTK_REVEALER_TRANSITION_TYPE_SLIDE_LEFT)
        return GTK_REVEALER_TRANSITION_TYPE_SLIDE_RIGHT;
      else if (priv->transition_type == GTK_REVEALER_TRANSITION_TYPE_SLIDE_RIGHT)
        return GTK_REVEALER_TRANSITION_TYPE_SLIDE_LEFT;
      if (priv->transition_type == GTK_REVEALER_TRANSITION_TYPE_SWING_LEFT)
        return GTK_REVEALER_TRANSITION_TYPE_SWING_RIGHT;
      else if (priv->transition_type == GTK_REVEALER_TRANSITION_TYPE_SWING_RIGHT)
        return GTK_REVEALER_TRANSITION_TYPE_SWING_LEFT;
    }

  return priv->transition_type;
}

static void
gtk_revealer_real_add (GtkContainer *container,
                       GtkWidget    *child)
{
  GtkRevealer *revealer = GTK_REVEALER (container);
  GtkRevealerPrivate *priv = gtk_revealer_get_instance_private (revealer);

  g_return_if_fail (child != NULL);

  gtk_widget_set_child_visible (child, priv->current_pos != 0.0);

  GTK_CONTAINER_CLASS (gtk_revealer_parent_class)->add (container, child);
}

static double
get_child_size_scale (GtkRevealer    *revealer,
                      GtkOrientation  orientation)
{
  GtkRevealerPrivate *priv = gtk_revealer_get_instance_private (revealer);

  switch (effective_transition (revealer))
    {
    case GTK_REVEALER_TRANSITION_TYPE_SLIDE_RIGHT:
    case GTK_REVEALER_TRANSITION_TYPE_SLIDE_LEFT:
      if (orientation == GTK_ORIENTATION_HORIZONTAL)
        return priv->current_pos;
      else
        return 1.0;

    case GTK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN:
    case GTK_REVEALER_TRANSITION_TYPE_SLIDE_UP:
      if (orientation == GTK_ORIENTATION_VERTICAL)
        return priv->current_pos;
      else
        return 1.0;

    case GTK_REVEALER_TRANSITION_TYPE_SWING_RIGHT:
    case GTK_REVEALER_TRANSITION_TYPE_SWING_LEFT:
      if (orientation == GTK_ORIENTATION_HORIZONTAL)
        return sin (G_PI * priv->current_pos / 2);
      else
        return 1.0;

    case GTK_REVEALER_TRANSITION_TYPE_SWING_DOWN:
    case GTK_REVEALER_TRANSITION_TYPE_SWING_UP:
      if (orientation == GTK_ORIENTATION_VERTICAL)
        return sin (G_PI * priv->current_pos / 2);
      else
        return 1.0;

    case GTK_REVEALER_TRANSITION_TYPE_NONE:
    case GTK_REVEALER_TRANSITION_TYPE_CROSSFADE:
    default:
      return 1.0;
    }
}

static void
gtk_revealer_real_size_allocate (GtkWidget *widget,
                                 int        width,
                                 int        height,
                                 int        baseline)
{
  GtkRevealer *revealer = GTK_REVEALER (widget);
  GtkRevealerPrivate *priv = gtk_revealer_get_instance_private (revealer);
  GtkWidget *child;
  GskTransform *transform;
  double hscale, vscale;
  int child_width, child_height;

  child = gtk_bin_get_child (GTK_BIN (revealer));
  if (child == NULL || !gtk_widget_get_visible (child))
    return;

  if (priv->current_pos >= 1.0)
    {
      gtk_widget_allocate (child, width, height, baseline, NULL);
      return;
    }

  child_width = width;
  child_height = height;
  hscale = get_child_size_scale (revealer, GTK_ORIENTATION_HORIZONTAL);
  vscale = get_child_size_scale (revealer, GTK_ORIENTATION_VERTICAL);
  if (hscale <= 0 || vscale <= 0)
    {
      /* don't allocate anything, the child is invisible and the numbers
       * don't make sense. */
      return;
    }

  if (hscale < 1.0)
    {
      g_assert (vscale == 1.0);
      child_width = MIN (G_MAXINT, ceil (width / hscale));
    }
  else if (vscale < 1.0)
    {
      child_height = MIN (G_MAXINT, ceil (height / vscale));
    }

  transform = NULL;
  switch (effective_transition (revealer))
    {
    case GTK_REVEALER_TRANSITION_TYPE_SLIDE_RIGHT:
      transform = gsk_transform_translate (transform, &GRAPHENE_POINT_INIT (width - child_width, 0));
      break;

    case GTK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN:
      transform = gsk_transform_translate (transform, &GRAPHENE_POINT_INIT (0, height - child_height));
      break;

    case GTK_REVEALER_TRANSITION_TYPE_SWING_LEFT:
      transform = gsk_transform_translate (transform, &GRAPHENE_POINT_INIT (width, height / 2));
      transform = gsk_transform_perspective (transform, 2 * MAX (width, height));
      transform = gsk_transform_rotate_3d (transform, -90 * (1.0 - priv->current_pos), graphene_vec3_y_axis ());
      transform = gsk_transform_translate (transform, &GRAPHENE_POINT_INIT (- child_width, - child_height / 2));
      break;

    case GTK_REVEALER_TRANSITION_TYPE_SWING_RIGHT:
      transform = gsk_transform_translate (transform, &GRAPHENE_POINT_INIT (0, height / 2));
      transform = gsk_transform_perspective (transform, 2 * MAX (width, height));
      transform = gsk_transform_rotate_3d (transform, 90 * (1.0 - priv->current_pos), graphene_vec3_y_axis ());
      transform = gsk_transform_translate (transform, &GRAPHENE_POINT_INIT (0, - child_height / 2));
      break;

    case GTK_REVEALER_TRANSITION_TYPE_SWING_DOWN:
      transform = gsk_transform_translate (transform, &GRAPHENE_POINT_INIT (width / 2, 0));
      transform = gsk_transform_perspective (transform, 2 * MAX (width, height));
      transform = gsk_transform_rotate_3d (transform, -90 * (1.0 - priv->current_pos), graphene_vec3_x_axis ());
      transform = gsk_transform_translate (transform, &GRAPHENE_POINT_INIT (- child_width / 2, 0));
      break;

    case GTK_REVEALER_TRANSITION_TYPE_SWING_UP:
      transform = gsk_transform_translate (transform, &GRAPHENE_POINT_INIT (width / 2, height));
      transform = gsk_transform_perspective (transform, 2 * MAX (width, height));
      transform = gsk_transform_rotate_3d (transform, 90 * (1.0 - priv->current_pos), graphene_vec3_x_axis ());
      transform = gsk_transform_translate (transform, &GRAPHENE_POINT_INIT (- child_width / 2, - child_height));
      break;

    case GTK_REVEALER_TRANSITION_TYPE_NONE:
    case GTK_REVEALER_TRANSITION_TYPE_CROSSFADE:
    case GTK_REVEALER_TRANSITION_TYPE_SLIDE_LEFT:
    case GTK_REVEALER_TRANSITION_TYPE_SLIDE_UP:
    default:
      break;
    }

  gtk_widget_allocate (child, child_width, child_height, -1, transform);
}

static void
gtk_revealer_set_position (GtkRevealer *revealer,
                           gdouble      pos)
{
  GtkRevealerPrivate *priv = gtk_revealer_get_instance_private (revealer);
  gboolean new_visible;
  GtkWidget *child;
  GtkRevealerTransitionType transition;

  priv->current_pos = pos;

  new_visible = priv->current_pos != 0.0;

  child = gtk_bin_get_child (GTK_BIN (revealer));
  if (child != NULL &&
      new_visible != gtk_widget_get_child_visible (child))
    {
      gtk_widget_set_child_visible (child, new_visible);
      gtk_widget_queue_resize (GTK_WIDGET (revealer));
    }

  transition = effective_transition (revealer);
  if (transition == GTK_REVEALER_TRANSITION_TYPE_NONE)
    {
      gtk_widget_queue_draw (GTK_WIDGET (revealer));
    }
  else if (transition == GTK_REVEALER_TRANSITION_TYPE_CROSSFADE)
    {
      gtk_widget_set_opacity (GTK_WIDGET (revealer), priv->current_pos);
      gtk_widget_queue_draw (GTK_WIDGET (revealer));
    }
  else
    {
      gtk_widget_queue_resize (GTK_WIDGET (revealer));
    }

  if (priv->current_pos == priv->target_pos)
    g_object_notify_by_pspec (G_OBJECT (revealer), props[PROP_CHILD_REVEALED]);
}

static gboolean
gtk_revealer_animate_cb (GtkWidget     *widget,
                         GdkFrameClock *frame_clock,
                         gpointer       user_data)
{
  GtkRevealer *revealer = GTK_REVEALER (widget);
  GtkRevealerPrivate *priv = gtk_revealer_get_instance_private (revealer);
  gdouble ease;

  gtk_progress_tracker_advance_frame (&priv->tracker,
                                      gdk_frame_clock_get_frame_time (frame_clock));
  ease = gtk_progress_tracker_get_ease_out_cubic (&priv->tracker, FALSE);
  gtk_revealer_set_position (revealer,
                             priv->source_pos + (ease * (priv->target_pos - priv->source_pos)));

  if (gtk_progress_tracker_get_state (&priv->tracker) == GTK_PROGRESS_STATE_AFTER)
    {
      priv->tick_id = 0;
      return FALSE;
    }

  return TRUE;
}

static void
gtk_revealer_start_animation (GtkRevealer *revealer,
                              gdouble      target)
{
  GtkRevealerPrivate *priv = gtk_revealer_get_instance_private (revealer);
  GtkWidget *widget = GTK_WIDGET (revealer);
  GtkRevealerTransitionType transition;

  if (priv->target_pos == target)
    return;

  priv->target_pos = target;
  g_object_notify_by_pspec (G_OBJECT (revealer), props[PROP_REVEAL_CHILD]);

  transition = effective_transition (revealer);
  if (gtk_widget_get_mapped (widget) &&
      priv->transition_duration != 0 &&
      transition != GTK_REVEALER_TRANSITION_TYPE_NONE &&
      gtk_settings_get_enable_animations (gtk_widget_get_settings (widget)))
    {
      priv->source_pos = priv->current_pos;
      if (priv->tick_id == 0)
        priv->tick_id =
          gtk_widget_add_tick_callback (widget, gtk_revealer_animate_cb, revealer, NULL);
      gtk_progress_tracker_start (&priv->tracker,
                                  priv->transition_duration * 1000,
                                  0,
                                  1.0);
    }
  else
    {
      gtk_revealer_set_position (revealer, target);
    }
}

/**
 * gtk_revealer_set_reveal_child:
 * @revealer: a #GtkRevealer
 * @reveal_child: %TRUE to reveal the child
 *
 * Tells the #GtkRevealer to reveal or conceal its child.
 *
 * The transition will be animated with the current
 * transition type of @revealer.
 */
void
gtk_revealer_set_reveal_child (GtkRevealer *revealer,
                               gboolean     reveal_child)
{
  g_return_if_fail (GTK_IS_REVEALER (revealer));

  if (reveal_child)
    gtk_revealer_start_animation (revealer, 1.0);
  else
    gtk_revealer_start_animation (revealer, 0.0);
}

/**
 * gtk_revealer_get_reveal_child:
 * @revealer: a #GtkRevealer
 *
 * Returns whether the child is currently
 * revealed. See gtk_revealer_set_reveal_child().
 *
 * This function returns %TRUE as soon as the transition
 * is to the revealed state is started. To learn whether
 * the child is fully revealed (ie the transition is completed),
 * use gtk_revealer_get_child_revealed().
 *
 * Returns: %TRUE if the child is revealed.
 */
gboolean
gtk_revealer_get_reveal_child (GtkRevealer *revealer)
{
  GtkRevealerPrivate *priv = gtk_revealer_get_instance_private (revealer);

  g_return_val_if_fail (GTK_IS_REVEALER (revealer), FALSE);

  return priv->target_pos != 0.0;
}

/**
 * gtk_revealer_get_child_revealed:
 * @revealer: a #GtkRevealer
 *
 * Returns whether the child is fully revealed, in other words whether
 * the transition to the revealed state is completed.
 *
 * Returns: %TRUE if the child is fully revealed
 */
gboolean
gtk_revealer_get_child_revealed (GtkRevealer *revealer)
{
  GtkRevealerPrivate *priv = gtk_revealer_get_instance_private (revealer);
  gboolean animation_finished = (priv->target_pos == priv->current_pos);
  gboolean reveal_child = gtk_revealer_get_reveal_child (revealer);

  if (animation_finished)
    return reveal_child;
  else
    return !reveal_child;
}

static void
gtk_revealer_measure (GtkWidget      *widget,
                      GtkOrientation  orientation,
                      int             for_size,
                      int            *minimum,
                      int            *natural,
                      int            *minimum_baseline,
                      int            *natural_baseline)
{
  GtkRevealer *self = GTK_REVEALER (widget);
  double scale;

  scale = get_child_size_scale (self, OPPOSITE_ORIENTATION (orientation));

  if (for_size >= 0)
    {
      if (scale == 0)
        return;
      else
        for_size = MIN (G_MAXINT, ceil (for_size / scale));
    }

  GTK_WIDGET_CLASS (gtk_revealer_parent_class)->measure (widget,
                                                         orientation,
                                                         for_size,
                                                         minimum, natural,
                                                         NULL, NULL);

  scale = get_child_size_scale (self, orientation);
  *minimum = ceil (*minimum * scale);
  *natural = ceil (*natural * scale);
}

/**
 * gtk_revealer_get_transition_duration:
 * @revealer: a #GtkRevealer
 *
 * Returns the amount of time (in milliseconds) that
 * transitions will take.
 *
 * Returns: the transition duration
 */
guint
gtk_revealer_get_transition_duration (GtkRevealer *revealer)
{
  GtkRevealerPrivate *priv = gtk_revealer_get_instance_private (revealer);

  g_return_val_if_fail (GTK_IS_REVEALER (revealer), 0);

  return priv->transition_duration;
}

/**
 * gtk_revealer_set_transition_duration:
 * @revealer: a #GtkRevealer
 * @duration: the new duration, in milliseconds
 *
 * Sets the duration that transitions will take.
 */
void
gtk_revealer_set_transition_duration (GtkRevealer *revealer,
                                      guint        value)
{
  GtkRevealerPrivate *priv = gtk_revealer_get_instance_private (revealer);

  g_return_if_fail (GTK_IS_REVEALER (revealer));

  if (priv->transition_duration == value)
    return;

  priv->transition_duration = value;
  g_object_notify_by_pspec (G_OBJECT (revealer), props[PROP_TRANSITION_DURATION]);
}

/**
 * gtk_revealer_get_transition_type:
 * @revealer: a #GtkRevealer
 *
 * Gets the type of animation that will be used
 * for transitions in @revealer.
 *
 * Returns: the current transition type of @revealer
 */
GtkRevealerTransitionType
gtk_revealer_get_transition_type (GtkRevealer *revealer)
{
  GtkRevealerPrivate *priv = gtk_revealer_get_instance_private (revealer);

  g_return_val_if_fail (GTK_IS_REVEALER (revealer), GTK_REVEALER_TRANSITION_TYPE_NONE);

  return priv->transition_type;
}

/**
 * gtk_revealer_set_transition_type:
 * @revealer: a #GtkRevealer
 * @transition: the new transition type
 *
 * Sets the type of animation that will be used for
 * transitions in @revealer. Available types include
 * various kinds of fades and slides.
 */
void
gtk_revealer_set_transition_type (GtkRevealer               *revealer,
                                  GtkRevealerTransitionType  transition)
{
  GtkRevealerPrivate *priv = gtk_revealer_get_instance_private (revealer);

  g_return_if_fail (GTK_IS_REVEALER (revealer));

  if (priv->transition_type == transition)
    return;

  priv->transition_type = transition;
  gtk_widget_queue_resize (GTK_WIDGET (revealer));
  g_object_notify_by_pspec (G_OBJECT (revealer), props[PROP_TRANSITION_TYPE]);
}
