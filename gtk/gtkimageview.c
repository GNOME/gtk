/*  Copyright 2018 Timm Bäder
 *
 * GTK+ is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * GLib is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with GTK+; see the file COPYING.  If not,
 * see <http://www.gnu.org/licenses/>.
 */

/**
 * SECTION:gtkimageview
 * @Short_description: A widget for displaying content images to users
 * @Title: GtkImageView
 *
 * #GtkImageView is a widget intended to be used to display "content images"
 * to users. What we refer to as "content images" in the documentation could
 * be characterized as "images the user is deeply interested in". You should
 * use #GtkImageView whenever you want to actually present an image instead
 * of just using an icon.
 *
 * Since: 4.0
 */

#include "config.h"
#include "gtkimageview.h"
#include "gtkintl.h"
#include "gtkprivate.h"
#include "gtkgesturerotate.h"
#include "gtkgesturezoom.h"
#include "gtkscrollable.h"
#include "gtkadjustment.h"
#include "gtkprogresstrackerprivate.h"
#include "gtkeventcontrollerscroll.h"
#include "gtkeventcontrollermotion.h"
#include "gtksnapshot.h"
#include "gtkwidgetprivate.h"
#include <math.h>

#define DEG_TO_RAD(x) (((x) / 360.0) * (2 * M_PI))
#define RAD_TO_DEG(x) (((x) / (2.0 * M_PI) * 360.0))

#define TRANSITION_DURATION (150.0 * 1000.0)
#define ANGLE_TRANSITION_MIN_DELTA (1.0)
#define SCALE_TRANSITION_MIN_DELTA (0.01)


typedef struct
{
  double hupper;
  double vupper;
  double hvalue;
  double vvalue;
  double angle;
  double scale;
} State;

typedef struct
{
  GdkPaintable *paintable;

  double   scale;
  double   angle;

  guint fit_allocation      : 1;
  guint scale_set           : 1;
  guint snap_angle          : 1;
  guint rotatable           : 1;
  guint zoomable            : 1;
  guint in_rotate           : 1;
  guint in_zoom             : 1;
  guint transitions_enabled : 1;
  guint in_angle_transition : 1;
  guint in_scale_transition : 1;

  GtkProgressTracker scale_tracker;
  GtkProgressTracker angle_tracker;

  GtkGesture *rotate_gesture;
  double      gesture_start_angle;
  double      visible_angle;

  GtkGesture *zoom_gesture;
  GtkEventController *zoom_controller;
  double      gesture_start_scale;
  double      visible_scale;

  GtkEventController *motion_controller;

  /* Current anchor point, or -1/-1.
   * In widget coordinates. */
  double      anchor_x;
  double      anchor_y;

  /* GtkScrollable stuff */
  GtkAdjustment       *hadjustment;
  GtkAdjustment       *vadjustment;
  GtkScrollablePolicy  hscroll_policy : 1;
  GtkScrollablePolicy  vscroll_policy : 1;

  /* Transitions */
  double transition_start_angle;
  guint  angle_transition_id;

  double transition_start_scale;
  guint  scale_transition_id;

  /* Event state */
  double mouse_x;
  double mouse_y;
} GtkImageViewPrivate;

enum
{
  PROP_SCALE = 1,
  PROP_SCALE_SET,
  PROP_ANGLE,
  PROP_ROTATABLE,
  PROP_ZOOMABLE,
  PROP_SNAP_ANGLE,
  PROP_FIT_ALLOCATION,
  PROP_TRANSITIONS_ENABLED,

  LAST_WIDGET_PROPERTY,
  PROP_HADJUSTMENT,
  PROP_VADJUSTMENT,
  PROP_HSCROLL_POLICY,
  PROP_VSCROLL_POLICY,

  LAST_PROPERTY
};

static GParamSpec *widget_props[LAST_WIDGET_PROPERTY] = { NULL, };


G_DEFINE_TYPE_WITH_CODE (GtkImageView, gtk_image_view, GTK_TYPE_WIDGET,
                         G_ADD_PRIVATE (GtkImageView)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_SCROLLABLE, NULL))

static void adjustment_value_changed_cb (GtkAdjustment *adjustment,
                                         gpointer       user_data);

static void gtk_image_view_update_adjustments (GtkImageView *self);

static void gtk_image_view_compute_bounding_box (GtkImageView *self,
                                                 double       *bb_width_out,
                                                 double       *bb_height_out,
                                                 double       *paintable_width_out,
                                                 double       *paintable_height_out);
static void gtk_image_view_ensure_gestures (GtkImageView *self);

static inline void gtk_image_view_restrict_adjustment (GtkAdjustment *adjustment);
static void gtk_image_view_fix_anchor (GtkImageView *self,
                                       double        anchor_x,
                                       double        anchor_y,
                                       State        *old_state);

static void gtk_image_view_fix_anchor2 (GtkImageView *self,
                                        double        anchor_x,
                                        double        anchor_y,
                                        const State  *old_state);


static inline void
gtk_image_view_invalidate (GtkImageView *self)
{
  GtkImageViewPrivate *priv = gtk_image_view_get_instance_private (self);

  if (priv->paintable == NULL)
    return;

  if (priv->fit_allocation)
    gtk_widget_queue_draw (GTK_WIDGET (self));
  else
    gtk_widget_queue_resize (GTK_WIDGET (self));
}

static inline double
gtk_image_view_get_real_scale (GtkImageView *self)
{
  GtkImageViewPrivate *priv = gtk_image_view_get_instance_private (self);

  if (priv->in_zoom || priv->in_scale_transition)
    return priv->visible_scale;
  else
    return priv->scale;
}

static inline double
gtk_image_view_get_real_angle (GtkImageView *self)
{
  GtkImageViewPrivate *priv = gtk_image_view_get_instance_private (self);

  if (priv->in_rotate || priv->in_angle_transition)
    return priv->visible_angle;
  else
    return priv->angle;
}

static inline double
gtk_image_view_clamp_angle (double angle)
{
  double new_angle = angle;

  if (angle > 360.0)
    new_angle -= (int)(angle / 360.0) * 360;
  else if (angle < 0.0)
    new_angle += 360 - ((int)(angle  /360) * 360.0);

  g_assert (new_angle >= 0.0);
  g_assert (new_angle <= 360.0);

  return new_angle;
}

static inline int
gtk_image_view_get_snapped_angle (double angle)
{
  return (int) ((angle + 45.0) / 90.0) * 90;
}

static void
gtk_image_view_get_current_state (GtkImageView *self,
                                  State        *state)
{
  GtkImageViewPrivate *priv = gtk_image_view_get_instance_private (self);

  if (priv->hadjustment != NULL && priv->vadjustment != NULL)
    {
      state->hvalue = gtk_adjustment_get_value (priv->hadjustment);
      state->vvalue = gtk_adjustment_get_value (priv->vadjustment);
      state->hupper = gtk_adjustment_get_upper (priv->hadjustment);
      state->vupper = gtk_adjustment_get_upper (priv->vadjustment);
    }
  state->angle = gtk_image_view_get_real_angle (self);
  state->scale = gtk_image_view_get_real_scale (self);
}

static gboolean
gtk_image_view_transitions_enabled (GtkImageView *self)
{
  GtkImageViewPrivate *priv = gtk_image_view_get_instance_private (self);
  gboolean animations_enabled;

  g_object_get (gtk_widget_get_settings (GTK_WIDGET (self)),
                "gtk-enable-animations", &animations_enabled,
                NULL);

  return priv->transitions_enabled &&
         animations_enabled &&
         priv->paintable &&
         gtk_widget_get_mapped (GTK_WIDGET (self));
}

static void
paintable_contents_changed_cb (GdkPaintable *paintable,
                               gpointer      user_data)
{
  GtkImageView *self = user_data;

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static gboolean
scale_frameclock_cb (GtkWidget     *widget,
                     GdkFrameClock *frame_clock,
                     gpointer       user_data)
{
  GtkImageView *self = GTK_IMAGE_VIEW (widget);
  GtkImageViewPrivate *priv = gtk_image_view_get_instance_private (self);
  State state;
  double t;
  double new_scale;

  gtk_progress_tracker_advance_frame (&priv->scale_tracker, gdk_frame_clock_get_frame_time (frame_clock));
  t = gtk_progress_tracker_get_ease_out_cubic (&priv->scale_tracker, FALSE);

  new_scale = (priv->scale - priv->transition_start_scale) * t;

  gtk_image_view_get_current_state (self, &state);

  priv->visible_scale = priv->transition_start_scale + new_scale;

  if (t >= 1.0)
    priv->in_scale_transition = FALSE;

  if (priv->hadjustment && priv->vadjustment)
    {
      gtk_image_view_update_adjustments (self);

      gtk_image_view_fix_anchor (self,
                                 gtk_widget_get_width (widget)  / 2,
                                 gtk_widget_get_height (widget) / 2,
                                 &state);
    }

  gtk_image_view_invalidate (self);

  if (gtk_progress_tracker_get_state (&priv->scale_tracker) == GTK_PROGRESS_STATE_AFTER)
    {
      priv->scale_transition_id = 0;
      return G_SOURCE_REMOVE;
    }

  return G_SOURCE_CONTINUE;
}

static void
gtk_image_view_animate_to_scale (GtkImageView *self)
{
  GtkImageViewPrivate *priv = gtk_image_view_get_instance_private (self);

  if (priv->scale_transition_id != 0)
    gtk_widget_remove_tick_callback (GTK_WIDGET (self), priv->scale_transition_id);

  /* Target scale is priv->scale */
  priv->in_scale_transition = TRUE;
  priv->visible_scale = priv->scale;
  priv->transition_start_scale = priv->scale;

  gtk_progress_tracker_start (&priv->scale_tracker, TRANSITION_DURATION, 0, 1.0);
  priv->scale_transition_id = gtk_widget_add_tick_callback (GTK_WIDGET (self),
                                                            scale_frameclock_cb,
                                                            NULL, NULL);
}

static gboolean
angle_frameclock_cb (GtkWidget     *widget,
                     GdkFrameClock *frame_clock,
                     gpointer       user_data)
{
  GtkImageView *self = GTK_IMAGE_VIEW (widget);
  GtkImageViewPrivate *priv = gtk_image_view_get_instance_private (self);
  int direction = GPOINTER_TO_INT (user_data);
  double new_angle;
  double t;
  State state;
  double target_angle = priv->angle;

  if (direction == 1 && target_angle < priv->transition_start_angle)
    target_angle += 360.0;
  else if (direction == 0 && target_angle > priv->transition_start_angle)
    target_angle -= 360.0;

  gtk_progress_tracker_advance_frame (&priv->angle_tracker, gdk_frame_clock_get_frame_time (frame_clock));
  t = gtk_progress_tracker_get_ease_out_cubic (&priv->angle_tracker, FALSE);
  new_angle = (target_angle - priv->transition_start_angle) * t;

  gtk_image_view_get_current_state (self, &state);

  priv->visible_angle = priv->transition_start_angle + new_angle;

  if (t >= 1.0)
    priv->in_angle_transition = FALSE;

  if (priv->hadjustment && priv->vadjustment)
    {
      gtk_image_view_update_adjustments (self);

      gtk_image_view_fix_anchor (self,
                                 gtk_widget_get_width (widget)  / 2,
                                 gtk_widget_get_height (widget) / 2,
                                 &state);

    }

  gtk_image_view_invalidate (self);

  if (gtk_progress_tracker_get_state (&priv->angle_tracker) == GTK_PROGRESS_STATE_AFTER)
    {
      priv->angle_transition_id = 0;
      return G_SOURCE_REMOVE;
    }

  return G_SOURCE_CONTINUE;
}

static void
gtk_image_view_animate_to_angle (GtkImageView *self,
                                 int           direction)
{
  GtkImageViewPrivate *priv = gtk_image_view_get_instance_private (self);

  if (priv->angle_transition_id != 0)
    {
      gtk_widget_remove_tick_callback (GTK_WIDGET (self), priv->angle_transition_id);
      priv->angle_transition_id = 0;
    }

  /* Target angle is priv->angle */
  priv->in_angle_transition = TRUE;
  priv->visible_angle = priv->angle;
  priv->transition_start_angle = priv->angle;

  gtk_progress_tracker_start (&priv->angle_tracker, TRANSITION_DURATION, 0, 1.0);
  priv->angle_transition_id = gtk_widget_add_tick_callback (GTK_WIDGET (self),
                                                            angle_frameclock_cb,
                                                            GINT_TO_POINTER (direction),
                                                            NULL);
}

static void
gtk_image_view_do_snapping (GtkImageView *self)
{
  GtkImageViewPrivate *priv = gtk_image_view_get_instance_private (self);
  double new_angle = gtk_image_view_get_snapped_angle (priv->angle);

  g_assert (priv->snap_angle);

  if (gtk_image_view_transitions_enabled (self))
    gtk_image_view_animate_to_angle (self, new_angle > priv->angle);

  priv->angle = new_angle;

  /* Don't notify! */
}

static void
to_rotate_coords (GtkImageView *self,
                  const State  *state,
                  double        in_x,
                  double        in_y,
                  double       *out_x,
                  double       *out_y)
{
  double cx = state->hupper / 2.0 - state->hvalue;
  double cy = state->vupper / 2.0 - state->vvalue;

  *out_x = in_x - cx;
  *out_y = in_y - cy;
}


/*
 * The anchor here is given in widget coordinates.
 *
 * The task now is to...
 *   1) Calculate the position of the anchor on the untransformed image.
 *      That means we have to remove both scaling and rotation from the image
 *      and calculate where the anchor point would be now.
 *      angle and scale for this are saved in @old_state.
 *   2) Now transform the anchor point by the new state, i.e. apply both
 *      scale and angle to it.
 */
static void
gtk_image_view_fix_anchor2 (GtkImageView *self,
                            double        anchor_x,
                            double        anchor_y,
                            const State  *old_state)
{
  GtkImageViewPrivate *priv = gtk_image_view_get_instance_private (self);
  graphene_matrix_t old_transform;
  graphene_matrix_t old_transform_inverse;
  graphene_matrix_t new_transform;
  graphene_point_t old_anchor_point;
  graphene_point_t anchor_point;

  g_assert (!priv->fit_allocation);
  g_assert (old_state->hupper >= 0);
  g_assert (old_state->vupper >= 0);
  g_assert (priv->hadjustment);
  g_assert (priv->vadjustment);
  g_assert (anchor_x >= 0);
  g_assert (anchor_y >= 0);
  g_assert (anchor_x < gtk_widget_get_width (GTK_WIDGET (self)));
  g_assert (anchor_y < gtk_widget_get_height (GTK_WIDGET (self)));

  /*
   * Plan:
   *   1) Transform given anchor intoun-scaled, un-rotated,
   *      un-translated (due to scrolling) image, coordinates.
   */

  g_message ("Anchor: %f/%f", anchor_x, anchor_y);
  g_message ("Scale: %f -> %f", old_state->scale, priv->scale);

  /* 1) Transform the anchor point back into untransformed paintable coordinates */
  graphene_point_init (&old_anchor_point,
                       anchor_x + old_state->hvalue,
                       anchor_y + old_state->vvalue);
  graphene_point_init_from_point (&anchor_point, &old_anchor_point);

  graphene_matrix_init_scale (&old_transform, old_state->scale, old_state->scale, 1);
  graphene_matrix_rotate (&old_transform, old_state->angle, graphene_vec3_z_axis ());
  g_assert (graphene_matrix_inverse (&old_transform, &old_transform_inverse));
  graphene_matrix_transform_point (&old_transform_inverse, &anchor_point, &anchor_point);

  g_message ("Transformed anchor: %f/%f", anchor_point.x, anchor_point.y);


  graphene_matrix_init_scale (&new_transform, priv->scale, priv->scale, 1);
  graphene_matrix_rotate (&new_transform, priv->angle, graphene_vec3_z_axis ());
  graphene_matrix_transform_point (&new_transform, &anchor_point, &anchor_point);

  g_message ("Anchor now: %f×%f", anchor_point.x, anchor_point.y);

  anchor_point.x += old_state->hvalue;
  anchor_point.y += old_state->vvalue;

  double diff_x = anchor_point.x - old_anchor_point.x;
  double diff_y = anchor_point.y - old_anchor_point.y;

  g_message ("Diff: %f/%f", diff_x, diff_y);

  gtk_adjustment_set_value (priv->hadjustment,
                            /*gtk_adjustment_get_value (priv->hadjustment) + */
                            diff_x);

  gtk_adjustment_set_value (priv->vadjustment,
                            /*gtk_adjustment_get_value (priv->vadjustment) + */
                            diff_y);


  // XXX XXX XXX XXX XXX XXX XXX XXX XXX XXX
  //
  // We can probably do this by taking a point and transforming it  (scale + rotate),
  // then taking the difference betwen before and after. Done. */
  //


}


static void
gtk_image_view_fix_anchor (GtkImageView *self,
                           double        anchor_x,
                           double        anchor_y,
                           State        *old_state)
{
  GtkImageViewPrivate *priv = gtk_image_view_get_instance_private (self);
  double hupper_delta = gtk_adjustment_get_upper (priv->hadjustment)
                        - old_state->hupper;
  double vupper_delta = gtk_adjustment_get_upper (priv->vadjustment)
                        - old_state->vupper;
  double hupper_delta_scale, vupper_delta_scale;
  double hupper_delta_angle, vupper_delta_angle;
  double cur_scale = gtk_image_view_get_real_scale (self);

  g_assert (old_state->hupper >= 0);
  g_assert (old_state->vupper >= 0);
  g_assert (priv->hadjustment);
  g_assert (priv->vadjustment);
  g_assert (anchor_x >= 0);
  g_assert (anchor_y >= 0);
  g_assert (anchor_x < gtk_widget_get_allocated_width (GTK_WIDGET (self)));
  g_assert (anchor_y < gtk_widget_get_allocated_height (GTK_WIDGET (self)));

  /*
   * Plan:
   *   1) Transform given anchor intoun-scaled, un-rotated,
   *      un-translated (due to scrolling) image, coordinates.
   */

  // XXX XXX XXX XXX XXX XXX XXX XXX XXX XXX
  //
  // We can probably do this by taking a point and transforming it  (scale + rotate),
  // then taking the difference betwen before and after. Done. */
  //

  /* Amount of upper change caused by scale */
  hupper_delta_scale = ((old_state->hupper / old_state->scale) * cur_scale)
                       - old_state->hupper;
  vupper_delta_scale = ((old_state->vupper / old_state->scale) * cur_scale)
                       - old_state->vupper;

  /* Amount of upper change caused by angle */
  hupper_delta_angle = hupper_delta - hupper_delta_scale;
  vupper_delta_angle = vupper_delta - vupper_delta_scale;

  /* As a first step, fix the anchor point with regard to the
   * updated scale
   */
  {
    double hvalue = gtk_adjustment_get_value (priv->hadjustment);
    double vvalue = gtk_adjustment_get_value (priv->vadjustment);

    double px = anchor_x + hvalue;
    double py = anchor_y + vvalue;

    double px_after = (px / old_state->scale) * cur_scale;
    double py_after = (py / old_state->scale) * cur_scale;

    gtk_adjustment_set_value (priv->hadjustment,
                              hvalue + px_after - px);
    gtk_adjustment_set_value (priv->vadjustment,
                              vvalue + py_after - py);
  }

  gtk_adjustment_set_value (priv->hadjustment,
                            gtk_adjustment_get_value (priv->hadjustment) + hupper_delta_angle / 2.0);

  gtk_adjustment_set_value (priv->vadjustment,
                            gtk_adjustment_get_value (priv->vadjustment) + vupper_delta_angle / 2.0);

  if (0){
    double rotate_anchor_x;
    double rotate_anchor_y;
    double anchor_angle;
    double anchor_length;
    double new_anchor_x, new_anchor_y;
    double delta_x, delta_y;

    /* Calculate the angle of the given anchor point relative to the
     * bounding box center and the OLD state */
    to_rotate_coords (self, old_state,
                      anchor_x, anchor_y,
                      &rotate_anchor_x, &rotate_anchor_y);
    anchor_angle = atan2 (rotate_anchor_y, rotate_anchor_x);
    anchor_length = sqrt ((rotate_anchor_x * rotate_anchor_x) +
                          (rotate_anchor_y * rotate_anchor_y));

    /* The angle of the anchor point NOW is the old angle plus
     * the difference between old surface angle and new surface angle */
    anchor_angle += DEG_TO_RAD (gtk_image_view_get_real_angle (self)
                                - old_state->angle);

    /* Calculate the position of the new anchor point, relative
     * to the bounding box center */
    new_anchor_x = cos (anchor_angle) * anchor_length;
    new_anchor_y = sin (anchor_angle) * anchor_length;

    /* The difference between old anchor and new anchor
     * is what we care about... */
    delta_x = rotate_anchor_x - new_anchor_x;
    delta_y = rotate_anchor_y - new_anchor_y;

    /* At last, make the old anchor match the new anchor */
    gtk_adjustment_set_value (priv->hadjustment,
                              gtk_adjustment_get_value (priv->hadjustment) - delta_x);
    gtk_adjustment_set_value (priv->vadjustment,
                              gtk_adjustment_get_value (priv->vadjustment) - delta_y);

  }

  gtk_widget_queue_draw (GTK_WIDGET (self));
}


/*
 * In here, we need to differenciate between the actual bounding box size,
 * i.e. the box around the scaled, rotated paintable -- and the size
 * we need to pass to gdk_paintable_snapshot to achieve the correct rendering.
 *
 * The size we assign to @paintable_width_out and @paintable_height_out is
 * the one we really need to pass directly to gdk_paintable_snapshot. No
 * scaling will have to be applied before.
 * This especially means that we don't explicitly scale the paintable at all,
 * we just increase the size we pass to gdk_paintable_snapshot() and let the
 * paintable handle it. This way, e.g. SVGs will scale.
 */
static void
gtk_image_view_compute_bounding_box (GtkImageView *self,
                                     double       *bb_width_out,
                                     double       *bb_height_out,
                                     double       *paintable_width_out,
                                     double       *paintable_height_out)
{
  GtkImageViewPrivate *priv = gtk_image_view_get_instance_private (self);
  int widget_width, widget_height;
  double image_width;
  double image_height;
  double bb_width;
  double bb_height;
  double scale;
  double angle;

  g_assert (bb_width_out != NULL);
  g_assert (bb_height_out != NULL);

  if (!priv->paintable)
    {
      *bb_width_out  = 0;
      *bb_height_out = 0;

      if (paintable_width_out)
        *paintable_width_out = 0;

      if (paintable_height_out)
        *paintable_height_out = 0;
      return;
    }

  widget_width = gtk_widget_get_width (GTK_WIDGET (self));
  widget_height = gtk_widget_get_height (GTK_WIDGET (self));
  angle = gtk_image_view_get_real_angle (self);

  /* XXX Passing the widget size as default size is probably wrong if
   *     priv->fit_allocation is FALSE? */
  gdk_paintable_compute_concrete_size (priv->paintable,
                                       0, 0,
                                       widget_width,
                                       widget_height,
                                       &image_width,
                                       &image_height);

  /* Calculate the bounding box of the rotated image */
  {
    graphene_rect_t bounds;
    graphene_matrix_t transform;

    graphene_rect_init (&bounds, 0, 0, image_width, image_height);
    graphene_matrix_init_translate (&transform,
                                    &(graphene_point3d_t){
                                      - image_width  / 2.0f,
                                      - image_height / 2.0f,
                                      0});
    graphene_matrix_rotate (&transform, angle, graphene_vec3_z_axis ());
    graphene_matrix_init_translate (&transform,
                                    &(graphene_point3d_t){
                                      + image_width  / 2.0f,
                                      + image_height / 2.0f,
                                      0});

    graphene_matrix_transform_bounds (&transform, &bounds, &bounds);

    bb_width = bounds.size.width;
    bb_height = bounds.size.height;

    /*g_message ("%f×%f rotated by %.2fdef: %f×%f",*/
               /*image_width, image_height, angle, bb_width, bb_height);*/
  }

  if (priv->fit_allocation)
    {
      double scale_x = (double)widget_width / (double)bb_width;
      double scale_y = (double)widget_height / (double)bb_height;

      scale = MIN (MIN (scale_x, scale_y), 1.0);
    }
  else
    {
      scale = gtk_image_view_get_real_scale (self);
    }


  /* TODO: Doing this here seem *very* wrong. */
  if (priv->fit_allocation)
    {
      g_assert (!priv->scale_set);
      if (priv->scale != scale)
        {
          priv->scale = scale;
          g_object_notify_by_pspec (G_OBJECT (self),
                                    widget_props[PROP_SCALE]);
        }
    }

  *bb_width_out  = bb_width  * scale;
  *bb_height_out = bb_height * scale;

  if (paintable_width_out)
    *paintable_width_out = image_width * scale;

  if (paintable_height_out)
    *paintable_height_out = image_height * scale;
}

static inline void
gtk_image_view_restrict_adjustment (GtkAdjustment *adjustment)
{
  double value     = gtk_adjustment_get_value (adjustment);
  double upper     = gtk_adjustment_get_upper (adjustment);
  double page_size = gtk_adjustment_get_page_size (adjustment);

  value = gtk_adjustment_get_value (adjustment);
  upper = gtk_adjustment_get_upper (adjustment);

  if (value > upper - page_size)
    gtk_adjustment_set_value (adjustment, upper - page_size);
  else if (value < 0)
    gtk_adjustment_set_value (adjustment, 0);
}

static void
gtk_image_view_update_adjustments (GtkImageView *self)
{
  GtkImageViewPrivate *priv = gtk_image_view_get_instance_private (self);
  double bb_width, bb_height;
  int widget_width  = gtk_widget_get_width  (GTK_WIDGET (self));
  int widget_height = gtk_widget_get_height (GTK_WIDGET (self));

  if (!priv->hadjustment && !priv->vadjustment)
    return;

  if (!priv->paintable)
    {
      if (priv->hadjustment)
        gtk_adjustment_configure (priv->hadjustment, 0, 0, 1, 0, 0, 1);

      if (priv->vadjustment)
        gtk_adjustment_configure (priv->vadjustment, 0, 0, 1, 0, 0, 1);

      return;
    }

  gtk_image_view_compute_bounding_box (self,
                                       &bb_width,
                                       &bb_height,
                                       NULL,
                                       NULL);

  /* compute_bounding_box makes sure that the bounding box is never bigger than
   * the widget allocation if fit-allocation is set.
   * We cast width/height to int anyway to avoid tiny differences in size */
  if (priv->hadjustment)
    {
      gtk_adjustment_set_upper (priv->hadjustment, MAX ((int)bb_width,  widget_width));
      gtk_adjustment_set_page_size (priv->hadjustment, widget_width);
      gtk_image_view_restrict_adjustment (priv->hadjustment);
    }

  if (priv->vadjustment)
    {
      gtk_adjustment_set_upper (priv->vadjustment, MAX ((int)bb_height, widget_height));
      gtk_adjustment_set_page_size (priv->vadjustment, widget_height);
      gtk_image_view_restrict_adjustment (priv->vadjustment);
    }
}

static void
gtk_image_view_set_scale_internal (GtkImageView *self,
                                   double        scale)
{
  GtkImageViewPrivate *priv = gtk_image_view_get_instance_private (self);
  scale = MAX (0, scale);

  priv->scale = scale;
  g_object_notify_by_pspec (G_OBJECT (self),
                            widget_props[PROP_SCALE]);

  if (priv->scale_set)
    {
      priv->scale_set = FALSE;
      g_object_notify_by_pspec (G_OBJECT (self),
                                widget_props[PROP_SCALE_SET]);
    }

  if (priv->fit_allocation)
    {
      priv->fit_allocation = FALSE;
      g_object_notify_by_pspec (G_OBJECT (self),
                                widget_props[PROP_FIT_ALLOCATION]);
    }

  gtk_image_view_update_adjustments (self);

  gtk_image_view_invalidate (self);
}

static void
gesture_zoom_begin_cb (GtkGesture       *gesture,
                       GdkEventSequence *sequence,
                       gpointer          user_data)
{
  GtkImageViewPrivate *priv = gtk_image_view_get_instance_private (user_data);

  if (!priv->zoomable ||
      !priv->paintable)
    {
      gtk_gesture_set_state (gesture, GTK_EVENT_SEQUENCE_DENIED);
      return;
    }

  if (priv->anchor_x == -1 && priv->anchor_y == -1)
    {
      gtk_gesture_get_bounding_box_center (gesture,
                                           &priv->anchor_x,
                                           &priv->anchor_y);
    }
}

static void
gesture_zoom_end_cb (GtkGesture       *gesture,
                     GdkEventSequence *sequence,
                     gpointer          self)
{
  GtkImageViewPrivate *priv = gtk_image_view_get_instance_private (self);

  gtk_image_view_set_scale_internal (self, priv->visible_scale);

  priv->in_zoom = FALSE;
  priv->anchor_x = -1;
  priv->anchor_y = -1;
}

static void
gesture_zoom_cancel_cb (GtkGesture       *gesture,
                        GdkEventSequence *sequence,
                        gpointer          user_data)
{
  GtkImageViewPrivate *priv = gtk_image_view_get_instance_private (user_data);

  if (priv->in_zoom)
    gtk_image_view_set_scale (user_data, priv->gesture_start_scale);

  priv->in_zoom = FALSE;
  priv->anchor_x = -1;
  priv->anchor_y = -1;
}

static void
gesture_zoom_changed_cb (GtkGestureZoom *gesture,
                          double          delta,
                          GtkWidget      *widget)
{
  GtkImageView *self = GTK_IMAGE_VIEW (widget);
  GtkImageViewPrivate *priv = gtk_image_view_get_instance_private (self);
  State state;
  double new_scale;

  if (!priv->in_zoom)
    {
      priv->in_zoom = TRUE;
      priv->gesture_start_scale = priv->scale;
    }

  if (priv->fit_allocation)
    {
      priv->fit_allocation = FALSE;
      g_object_notify_by_pspec (G_OBJECT (widget),
                                widget_props[PROP_FIT_ALLOCATION]);
    }

  new_scale = priv->gesture_start_scale * delta;
  gtk_image_view_get_current_state (self, &state);

  priv->visible_scale = new_scale;

  gtk_image_view_update_adjustments (self);

  if (priv->hadjustment != NULL && priv->vadjustment != NULL)
    {
      gtk_image_view_fix_anchor2 (self,
                                  priv->anchor_x,
                                  priv->anchor_y,
                                  &state);
    }

  gtk_image_view_invalidate (self);
}

static void
gesture_rotate_begin_cb (GtkGesture       *gesture,
                         GdkEventSequence *sequence,
                         gpointer          user_data)
{
  GtkImageViewPrivate *priv = gtk_image_view_get_instance_private (user_data);

  if (!priv->rotatable ||
      !priv->paintable)
    {
      gtk_gesture_set_state (gesture, GTK_EVENT_SEQUENCE_DENIED);
      return;
    }

  if (priv->anchor_x == -1 && priv->anchor_y == -1)
    {
      gtk_gesture_get_bounding_box_center (gesture,
                                           &priv->anchor_x,
                                           &priv->anchor_y);
    }
}

static void
gesture_rotate_end_cb (GtkGesture       *gesture,
                       GdkEventSequence *sequence,
                       gpointer          self)
{
  GtkImageViewPrivate *priv = gtk_image_view_get_instance_private (self);

  priv->angle = gtk_image_view_clamp_angle (priv->visible_angle);

  if (priv->snap_angle)
    {
      /* Will update priv->angle */
      gtk_image_view_do_snapping (self);
    }
  g_object_notify_by_pspec (self,
                            widget_props[PROP_ANGLE]);

  priv->in_rotate = FALSE;
  priv->anchor_x = -1;
  priv->anchor_y = -1;
}

static void
gesture_rotate_cancel_cb (GtkGesture       *gesture,
                          GdkEventSequence *sequence,
                          gpointer          self)
{
  GtkImageViewPrivate *priv = gtk_image_view_get_instance_private (self);

  gtk_image_view_update_adjustments (self);

  priv->in_rotate = FALSE;
  priv->anchor_x = -1;
  priv->anchor_y = -1;
}

static void
gesture_rotate_changed_cb (GtkGestureRotate *gesture,
                          double            angle,
                          double            delta,
                          GtkWidget        *widget)
{
  GtkImageView *self = GTK_IMAGE_VIEW (widget);
  GtkImageViewPrivate *priv = gtk_image_view_get_instance_private (self);
  State old_state;
  double new_angle;

  if (!priv->in_rotate)
    {
      priv->in_rotate = TRUE;
      priv->gesture_start_angle = priv->angle;
    }

  new_angle = priv->gesture_start_angle + RAD_TO_DEG (delta);
  gtk_image_view_get_current_state (self, &old_state);

  priv->visible_angle = new_angle;
  gtk_image_view_update_adjustments (self);

  if (priv->hadjustment && priv->vadjustment && !priv->fit_allocation)
    gtk_image_view_fix_anchor2 (self,
                               priv->anchor_x,
                               priv->anchor_y,
                               &old_state);

  gtk_image_view_invalidate (self);
}

static void
scroll_controller_scroll_cb (GtkEventControllerScroll *controller,
                             double                    dx,
                             double                    dy,
                             gpointer                  user_data)
{
  GtkImageView *self = user_data;
  GtkImageViewPrivate *priv = gtk_image_view_get_instance_private (self);
  double new_scale;
  State state;

  new_scale = MAX (0, priv->scale - (0.02 * dy));

  if (!priv->paintable || !priv->zoomable)
    return;

  /* TODO: We should probably use the step increment or something from the
   *       adjustment to control the scaling granularity? */

  /* TODO: We might want to only conditionally zoom in here, e.g. when
   * CTRL or SHIFT are pressed. */

  gtk_image_view_get_current_state (self, &state);

  gtk_image_view_set_scale_internal (self, new_scale);

  if (priv->hadjustment && priv->vadjustment)
    {
      /*priv->mouse_x = 100;*/
      /*priv->mouse_y = 200;*/
      gtk_image_view_fix_anchor2 (self, priv->mouse_x, priv->mouse_y, &state);
    }
}

static void
motion_controller_motion_cb (GtkEventControllerMotion *controller,
                             double                    x,
                             double                    y,
                             gpointer                  user_data)
{
  GtkImageView *self = user_data;
  GtkImageViewPrivate *priv = gtk_image_view_get_instance_private (self);

  priv->mouse_x = x;
  priv->mouse_y = y;
}

static void
gtk_image_view_ensure_gestures (GtkImageView *self)
{
  GtkImageViewPrivate *priv = gtk_image_view_get_instance_private (self);

  if (priv->zoomable && priv->zoom_gesture == NULL)
    {
      priv->zoom_gesture = gtk_gesture_zoom_new ();
      g_signal_connect (priv->zoom_gesture, "scale-changed",
                        (GCallback)gesture_zoom_changed_cb, self);
      g_signal_connect (priv->zoom_gesture, "begin",
                        (GCallback)gesture_zoom_begin_cb, self);
      g_signal_connect (priv->zoom_gesture, "end",
                        (GCallback)gesture_zoom_end_cb, self);
      g_signal_connect (priv->zoom_gesture, "cancel",
                        (GCallback)gesture_zoom_cancel_cb, self);
      gtk_widget_add_controller (GTK_WIDGET (self),
                                 GTK_EVENT_CONTROLLER (priv->zoom_gesture));

      priv->zoom_controller = gtk_event_controller_scroll_new (GTK_EVENT_CONTROLLER_SCROLL_BOTH_AXES);
      g_signal_connect (priv->zoom_controller, "scroll",
                        (GCallback)scroll_controller_scroll_cb, self);
      gtk_widget_add_controller (GTK_WIDGET (self),
                                 priv->zoom_controller);


      /* We only need this one for scrolling, to know the mouse position at the time of
       * a ::scroll emission */
      priv->motion_controller = gtk_event_controller_motion_new ();
      g_signal_connect (priv->motion_controller, "motion", (GCallback)motion_controller_motion_cb, self);
      gtk_widget_add_controller (GTK_WIDGET (self), priv->motion_controller);
    }
  else if (!priv->zoomable && priv->zoom_gesture != NULL)
    {
      gtk_widget_remove_controller (GTK_WIDGET (self),
                                    GTK_EVENT_CONTROLLER (priv->zoom_gesture));
      priv->zoom_gesture = NULL;
    }

  if (priv->rotatable && priv->rotate_gesture == NULL)
    {
      priv->rotate_gesture = gtk_gesture_rotate_new ();
      g_signal_connect (priv->rotate_gesture, "angle-changed", (GCallback)gesture_rotate_changed_cb, self);
      g_signal_connect (priv->rotate_gesture, "begin", (GCallback)gesture_rotate_begin_cb, self);
      g_signal_connect (priv->rotate_gesture, "end", (GCallback)gesture_rotate_end_cb, self);
      g_signal_connect (priv->rotate_gesture, "cancel", (GCallback)gesture_rotate_cancel_cb, self);

      gtk_widget_add_controller (GTK_WIDGET (self),
                                 GTK_EVENT_CONTROLLER (priv->rotate_gesture));

    }
  else if (!priv->rotatable && priv->rotate_gesture != NULL)
    {
      gtk_widget_remove_controller (GTK_WIDGET (self),
                                    GTK_EVENT_CONTROLLER (priv->rotate_gesture));
      priv->rotate_gesture = NULL;
    }

  if (priv->zoom_gesture && priv->rotate_gesture)
    gtk_gesture_group (priv->zoom_gesture,
                       priv->rotate_gesture);
}

static void
gtk_image_view_init (GtkImageView *self)
{
  GtkImageViewPrivate *priv = gtk_image_view_get_instance_private (self);
  GtkWidget *widget = GTK_WIDGET (self);

  gtk_widget_set_has_surface (widget, FALSE);

  priv->scale = 1.0;
  priv->angle = 0.0;
  priv->visible_scale = 1.0;
  priv->visible_angle = 0.0;
  priv->snap_angle = FALSE;
  priv->fit_allocation = FALSE;
  priv->scale_set = FALSE;
  priv->anchor_x = -1;
  priv->anchor_y = -1;
  priv->rotatable = TRUE;
  priv->zoomable = TRUE;
  priv->transitions_enabled = TRUE;
  priv->angle_transition_id = 0;
  priv->scale_transition_id = 0;

  gtk_image_view_ensure_gestures (self);
}

static void
gtk_image_view_snapshot (GtkWidget   *widget,
                         GtkSnapshot *snapshot)
{
  GtkImageView *self = GTK_IMAGE_VIEW (widget);
  GtkImageViewPrivate *priv = gtk_image_view_get_instance_private (self);
  graphene_matrix_t transform;
  int widget_width;
  int widget_height;
  double draw_x = 0;
  double draw_y = 0;
  double paintable_width;
  double paintable_height;
  double bb_width, bb_height;

  if (!priv->paintable)
    return;

  gtk_image_view_compute_bounding_box (self,
                                       &bb_width,
                                       &bb_height,
                                       &paintable_width,
                                       &paintable_height);

  if (paintable_width <= 0 || paintable_height <= 0)
    return;

  widget_width = gtk_widget_get_width (widget);
  widget_height = gtk_widget_get_height (widget);

  if (priv->fit_allocation)
    {
      draw_x = (widget_width  - paintable_width)  / 2;
      draw_y = (widget_height - paintable_height) / 2;
    }
  else
    {
      /* If the image we draw is smaller than the widget size, we
       * center it anyway */
      if (bb_width <= widget_width)
        {
          draw_x = (widget_width - paintable_width)  / 2.0;
        }
      else if (priv->hadjustment)
        {
          draw_x = -gtk_adjustment_get_value (priv->hadjustment);
        }

      if (bb_height <= widget_height)
        {
          draw_y = (widget_height - paintable_height)  / 2.0;
        }
      else if (priv->vadjustment)
        {
          draw_y = -gtk_adjustment_get_value (priv->vadjustment);
        }
    }
  /*g_message ("Draw pos: %f/%f", draw_x, draw_y);*/

  /* Rotate around the center */
  graphene_matrix_init_identity (&transform);

  gtk_snapshot_push_clip (snapshot,
                          &GRAPHENE_RECT_INIT (0, 0, widget_width, widget_height));

  graphene_matrix_translate (&transform,
                             &GRAPHENE_POINT3D_INIT (-(paintable_width  / 2.0f),
                                                     -(paintable_height / 2.0f),
                                                     0));

  graphene_matrix_rotate (&transform,
                          gtk_image_view_get_real_angle (self),
                          graphene_vec3_z_axis ());

  graphene_matrix_translate (&transform,
                             &GRAPHENE_POINT3D_INIT((paintable_width  / 2.0f) + draw_x,
                                                    (paintable_height / 2.0f) + draw_y,
                                                    0));
  gtk_snapshot_push_transform (snapshot, &transform);

  gdk_paintable_snapshot (priv->paintable, snapshot, paintable_width, paintable_height);

  gtk_snapshot_pop (snapshot); /* Transform */
  gtk_snapshot_pop (snapshot); /* Clip */
}

static void
gtk_image_view_set_hadjustment (GtkImageView  *self,
                                GtkAdjustment *hadjustment)
{
  GtkImageViewPrivate *priv = gtk_image_view_get_instance_private (self);

  if (priv->hadjustment && priv->hadjustment == hadjustment)
    return;

  if (priv->hadjustment)
    {
      g_signal_handlers_disconnect_by_func (priv->hadjustment, adjustment_value_changed_cb, self);
      g_object_unref (priv->hadjustment);
    }

  if (hadjustment)
    {
      g_signal_connect (G_OBJECT (hadjustment), "value-changed",
                        G_CALLBACK (adjustment_value_changed_cb), self);
      priv->hadjustment = g_object_ref_sink (hadjustment);
    }
  else
    {
      priv->hadjustment = hadjustment;
    }

  g_object_notify (G_OBJECT (self), "hadjustment");

  gtk_image_view_update_adjustments (self);

  gtk_image_view_invalidate (self);
}

static void
gtk_image_view_set_vadjustment (GtkImageView  *self,
                                GtkAdjustment *vadjustment)
{
  GtkImageViewPrivate *priv = gtk_image_view_get_instance_private (self);

  if (priv->vadjustment == vadjustment)
    return;

  if (priv->vadjustment)
    {
      g_signal_handlers_disconnect_by_func (priv->vadjustment, adjustment_value_changed_cb, self);
      g_object_unref (priv->vadjustment);
    }

  if (vadjustment)
    {
      g_signal_connect (G_OBJECT (vadjustment), "value-changed",
                        G_CALLBACK (adjustment_value_changed_cb), self);
      priv->vadjustment = g_object_ref_sink (vadjustment);
    }
  else
    {
      priv->vadjustment = vadjustment;
    }

  g_object_notify (G_OBJECT (self), "vadjustment");

  gtk_image_view_update_adjustments (self);

  gtk_image_view_invalidate (self);
}

static void
gtk_image_view_set_hscroll_policy (GtkImageView        *self,
                                   GtkScrollablePolicy  hscroll_policy)
{
  GtkImageViewPrivate *priv = gtk_image_view_get_instance_private (self);

  if (priv->hscroll_policy == hscroll_policy)
    return;

  priv->hscroll_policy = hscroll_policy;
  g_object_notify (G_OBJECT (self), "hscroll-policy");
}

static void
gtk_image_view_set_vscroll_policy (GtkImageView        *self,
                                   GtkScrollablePolicy  vscroll_policy)
{
  GtkImageViewPrivate *priv = gtk_image_view_get_instance_private (self);

  if (priv->vscroll_policy == vscroll_policy)
    return;

  priv->vscroll_policy = vscroll_policy;
  g_object_notify (G_OBJECT (self), "vscroll-policy");
}

/**
 * gtk_image_view_set_scale:
 * @self: A #GtkImageView instance
 * @scale: The new scale value
 *
 * Sets the value of the #scale property. This will cause the
 * #scale-set property to be set to #FALSE as well
 *
 * If #GtkImageView:fit-allocation is %TRUE, it will be set to %FALSE, and @self
 * will be resized to the image's current size, taking the new scale into
 * account.
 *
 * If #GtkImageView:transitions-enabled is set to %TRUE, the internal scale value will be
 * interpolated between the old and the new scale, gtk_image_view_get_scale()
 * will report the one passed to gtk_image_view_set_scale() however.
 *
 * When calling this function, #GtkImageView will try to keep the currently centered
 * point of the image where it is, so visually it will "zoom" into the current
 * center of the widget. Note that #GtkImageView is a #GtkScrollable, so the center
 * of the image is also the center of the scrolled window in case it is packed into
 * a #GtkScrolledWindow.
 *
 * Since: 4.0
 */
void
gtk_image_view_set_scale (GtkImageView *self,
                          double        scale)
{
  GtkImageViewPrivate *priv = gtk_image_view_get_instance_private (self);
  State state;

  g_return_if_fail (GTK_IS_IMAGE_VIEW (self));
  g_return_if_fail (scale > 0.0);

  if (scale == priv->scale)
    return;

  gtk_image_view_get_current_state (self, &state);

  priv->scale = scale;
  g_object_notify_by_pspec (G_OBJECT (self),
                            widget_props[PROP_SCALE]);

  if (gtk_image_view_transitions_enabled (self))
    gtk_image_view_animate_to_scale (self);

  if (priv->scale_set)
    {
      priv->scale_set = FALSE;
      g_object_notify_by_pspec (G_OBJECT (self),
                                widget_props[PROP_SCALE_SET]);
    }

  if (priv->fit_allocation)
    {
      priv->fit_allocation = FALSE;
      g_object_notify_by_pspec (G_OBJECT (self),
                                widget_props[PROP_FIT_ALLOCATION]);
    }


  if (!priv->paintable)
    return;

  if (priv->hadjustment != NULL && priv->vadjustment != NULL)
    {
      gtk_image_view_fix_anchor2 (self,
                                 gtk_widget_get_width (GTK_WIDGET (self)) / 2,
                                 gtk_widget_get_height (GTK_WIDGET (self)) / 2,
                                 &state);
    }

  gtk_image_view_update_adjustments (self);

  gtk_image_view_invalidate (self);
}

/**
 * gtk_image_view_get_scale:
 * @self: A #GtkImageView instance
 *
 * Returns: The current value of the #GtkImageView:scale property.
 *
 * Since: 4.0
 */
double
gtk_image_view_get_scale (GtkImageView *self)
{
  GtkImageViewPrivate *priv = gtk_image_view_get_instance_private (self);
  g_return_val_if_fail (GTK_IS_IMAGE_VIEW (self), 0.0);

  return priv->scale;
}

/**
 * gtk_image_view_set_angle:
 * @self: A #GtkImageView instance
 * @angle: The angle to rotate the image about, in
 *   degrees. If this is < 0 or > 360, the value will
 *   be wrapped. So e.g. setting this to 362 will result in a
 *   angle of 2, setting it to -2 will result in 358.
 *   Both 0 and 360 are possible.
 *
 * Sets the value of the #GtkImageView:angle property. When calling this function,
 * #GtkImageView will try to keep the currently centered point of the image where it is,
 * so visually the image will not be rotated around its center, but around the current
 * center of the widget. Note that #GtkImageView is a #GtkScrollable, so the center
 * of the image is also the center of the scrolled window in case it is packed into
 * a #GtkScrolledWindow.
 *
 * Since: 4.0
 */
void
gtk_image_view_set_angle (GtkImageView *self,
                          double        angle)
{
  GtkImageViewPrivate *priv = gtk_image_view_get_instance_private (self);
  State state;

  g_return_if_fail (GTK_IS_IMAGE_VIEW (self));

  if (angle == priv->angle)
    return;

  gtk_image_view_get_current_state (self, &state);

  angle = gtk_image_view_clamp_angle (angle);

  if (priv->snap_angle)
    priv->angle = gtk_image_view_get_snapped_angle (angle);
  else
    priv->angle = angle;

  if (gtk_image_view_transitions_enabled (self) &&
      ABS(gtk_image_view_clamp_angle (angle) - priv->angle) > ANGLE_TRANSITION_MIN_DELTA)
    {
      gtk_image_view_animate_to_angle (self, angle > priv->angle);
    }

  g_object_notify_by_pspec (G_OBJECT (self),
                            widget_props[PROP_ANGLE]);

  if (!priv->paintable)
    return;

  if (priv->hadjustment && priv->vadjustment && !priv->fit_allocation)
    {
      gtk_image_view_fix_anchor2 (self,
                                 gtk_widget_get_width (GTK_WIDGET (self)) / 2,
                                 gtk_widget_get_height (GTK_WIDGET (self)) / 2,
                                 &state);
    }

  gtk_image_view_invalidate (self);

  gtk_image_view_update_adjustments (self);
}

/**
 * gtk_image_view_get_angle:
 * @self: A #GtkImageView instance
 *
 * Returns: The current angle value.
 *
 * Since: 4.0
 */
double
gtk_image_view_get_angle (GtkImageView *self)
{
  GtkImageViewPrivate *priv = gtk_image_view_get_instance_private (self);
  g_return_val_if_fail (GTK_IS_IMAGE_VIEW (self), 0.0);

  return priv->angle;
}

/**
 * gtk_image_view_set_snap_angle:
 * @self: A #GtkImageView instance
 * @snap_angle: The new value of the #GtkImageView:snap-angle property
 *
 * Setting #snap-angle to %TRUE will cause @self's  angle to
 * be snapped to 90° steps. Setting the #GtkImageView:angle property will cause it to
 * be set to the closest 90° step, so e.g. using an angle of 40 will result
 * in an angle of 0, using 240 will result in 270, etc.
 *
 * Since: 4.0
 */
void
gtk_image_view_set_snap_angle (GtkImageView *self,
                               gboolean     snap_angle)
{
  GtkImageViewPrivate *priv = gtk_image_view_get_instance_private (self);

  g_return_if_fail (GTK_IS_IMAGE_VIEW (self));

  snap_angle = !!snap_angle;

  if (snap_angle == priv->snap_angle)
    return;

  priv->snap_angle = snap_angle;
  g_object_notify_by_pspec (G_OBJECT (self),
                            widget_props[PROP_SNAP_ANGLE]);

  if (priv->snap_angle)
    {
      gtk_image_view_do_snapping (self);
      g_object_notify_by_pspec (G_OBJECT (self),
                                widget_props[PROP_ANGLE]);
    }
}

/**
 * gtk_image_view_get_snap_angle:
 * @self: A #GtkImageView instance
 *
 * Returns: The current value of the #GtkImageView:snap-angle property.
 *
 * Since: 4.0
 */
gboolean
gtk_image_view_get_snap_angle (GtkImageView *self)
{
  GtkImageViewPrivate *priv = gtk_image_view_get_instance_private (self);
  g_return_val_if_fail (GTK_IS_IMAGE_VIEW (self), FALSE);

  return priv->snap_angle;
}

/**
 * gtk_image_view_set_fit_allocation:
 * @self: A #GtkImageView instance
 * @fit_allocation: The new value of the #GtkImageView:fit-allocation property.
 *
 * Setting #GtkImageView:fit-allocation to %TRUE will cause the image to be scaled
 * to the widget's allocation, unless it would cause the image to be
 * scaled up.
 *
 * Setting #GtkImageView:fit-allocation will have the side effect of setting
 * #scale-set set to %FALSE, thus giving the #GtkImageView the control
 * over the image's scale. Additionally, if the new #GtkImageView:fit-allocation
 * value is %FALSE, the scale will be reset to 1.0 and the #GtkImageView
 * will be resized to take at least the image's real size.
 *
 * Since: 4.0
 */
void
gtk_image_view_set_fit_allocation (GtkImageView *self,
                                   gboolean      fit_allocation)
{
  GtkImageViewPrivate *priv = gtk_image_view_get_instance_private (self);
  g_return_if_fail (GTK_IS_IMAGE_VIEW (self));

  fit_allocation = !!fit_allocation;

  if (fit_allocation == priv->fit_allocation)
    return;

  priv->fit_allocation = fit_allocation;
  g_object_notify_by_pspec (G_OBJECT (self),
                            widget_props[PROP_FIT_ALLOCATION]);

  priv->scale_set = FALSE;
  g_object_notify_by_pspec (G_OBJECT (self),
                            widget_props[PROP_SCALE_SET]);

  if (!priv->fit_allocation)
    {
      priv->scale = 1.0;
      g_object_notify_by_pspec (G_OBJECT (self),
                                widget_props[PROP_SCALE]);
    }

  gtk_image_view_update_adjustments (self);

  gtk_image_view_invalidate (self);
}

/**
 * gtk_image_view_get_fit_allocation:
 * @self: A #GtkImageView instance
 *
 * Returns: The current value of the #GtkImageView:fit-allocation property.
 *
 * Since: 4.0
 */
gboolean
gtk_image_view_get_fit_allocation (GtkImageView *self)
{
  GtkImageViewPrivate *priv = gtk_image_view_get_instance_private (self);
  g_return_val_if_fail (GTK_IS_IMAGE_VIEW (self), FALSE);

  return priv->fit_allocation;
}

/**
 * gtk_image_view_set_rotatable:
 * @self: A #GtkImageView instance
 * @rotatable: The new value of the #GtkImageView:rotatable property
 *
 * Sets the value of the #GtkImageView:rotatable property to @rotatable. This controls whether
 * the user can change the angle of the displayed image using a two-finger gesture.
 *
 * Since: 4.0
 */
void
gtk_image_view_set_rotatable (GtkImageView *self,
                              gboolean      rotatable)
{
  GtkImageViewPrivate *priv = gtk_image_view_get_instance_private (self);
  g_return_if_fail (GTK_IS_IMAGE_VIEW (self));

  rotatable = !!rotatable;

  if (priv->rotatable != rotatable)
    {
      priv->rotatable = rotatable;
      gtk_image_view_ensure_gestures (self);
      g_object_notify_by_pspec (G_OBJECT (self),
                                widget_props[PROP_ROTATABLE]);
    }
}

/**
 * gtk_image_view_get_rotatable:
 * @self: A #GtkImageView instance
 *
 * Returns: The current value of the #GtkImageView:rotatable property
 *
 * Since: 4.0
 */
gboolean
gtk_image_view_get_rotatable (GtkImageView *self)
{
  GtkImageViewPrivate *priv = gtk_image_view_get_instance_private (self);
  g_return_val_if_fail (GTK_IS_IMAGE_VIEW (self), FALSE);

  return priv->rotatable;
}

/**
 * gtk_image_view_set_zoomable:
 * @self: A #GtkImageView instance
 * @zoomable: The new value of the #GtkImageView:zoomable property
 *
 * Sets the new value of the #GtkImageView:zoomable property. This controls whether the user can
 * change the #GtkImageView:scale property using a two-finger gesture.
 *
 * Since: 4.0
 */
void
gtk_image_view_set_zoomable (GtkImageView *self,
                             gboolean      zoomable)
{
  GtkImageViewPrivate *priv = gtk_image_view_get_instance_private (self);
  g_return_if_fail (GTK_IS_IMAGE_VIEW (self));

  zoomable = !!zoomable;

  if (zoomable != priv->zoomable)
    {
      priv->zoomable = zoomable;
      gtk_image_view_ensure_gestures (self);
      g_object_notify_by_pspec (G_OBJECT (self),
                                widget_props[PROP_ZOOMABLE]);
    }
}

/**
 * gtk_image_view_get_zoomable:
 * @self: A #GtkImageView instance
 *
 * Returns: The current value of the #GtkImageView:zoomable property.
 *
 * Since: 4.0
 */
gboolean
gtk_image_view_get_zoomable (GtkImageView *self)
{
  GtkImageViewPrivate *priv = gtk_image_view_get_instance_private (self);
  g_return_val_if_fail (GTK_IS_IMAGE_VIEW (self), FALSE);

  return priv->zoomable;
}

/**
 * gtk_image_view_set_transitions_enabled:
 * @self: A #GtkImageView instance
 * @transitions_enabled: The new value of the #GtkImageView:transitions-enabled property
 *
 * Sets the new value of the #GtkImageView:transitions-enabled property.
 * Note that even if #GtkImageView:transitions-enabled is %TRUE, transitions will
 * not be used if #GtkSettings:gtk-enable-animations is %FALSE.
 *
 * Since: 4.0
 */
void
gtk_image_view_set_transitions_enabled (GtkImageView *self,
                                        gboolean      transitions_enabled)
{
  GtkImageViewPrivate *priv = gtk_image_view_get_instance_private (self);
  g_return_if_fail (GTK_IS_IMAGE_VIEW (self));

  transitions_enabled = !!transitions_enabled;

  if (transitions_enabled != priv->transitions_enabled)
    {
      priv->transitions_enabled = transitions_enabled;
      g_object_notify_by_pspec (G_OBJECT (self),
                                widget_props[PROP_TRANSITIONS_ENABLED]);
    }
}

/**
 * gtk_image_view_get_transitions_enabled:
 * @self: A #GtkImageView instance
 *
 * Returns: the current value of the #GtkImageView:transitions-enabled property.
 *
 * Since: 4.0
 */
gboolean
gtk_image_view_get_transitions_enabled (GtkImageView *self)
{
  GtkImageViewPrivate *priv = gtk_image_view_get_instance_private (self);
  g_return_val_if_fail (GTK_IS_IMAGE_VIEW (self), FALSE);

  return priv->transitions_enabled;
}

/**
 * gtk_image_view_get_scale_set:
 * @self: A #GtkImageView instance
 *
 * Returns: the current value of the #GtkImageView:scale-set property.
 *
 * Since: 4.0
 */
gboolean
gtk_image_view_get_scale_set (GtkImageView *self)
{
  GtkImageViewPrivate *priv = gtk_image_view_get_instance_private (self);
  g_return_val_if_fail (GTK_IS_IMAGE_VIEW (self), FALSE);

  return priv->scale_set;
}

static void
gtk_image_view_size_allocate (GtkWidget           *widget,
                              const GtkAllocation *allocation,
                              int                  baseline)
{
  GtkImageView *self = GTK_IMAGE_VIEW (widget);

  gtk_image_view_update_adjustments (self);
}

static void
adjustment_value_changed_cb (GtkAdjustment *adjustment,
                             gpointer       user_data)
{
  GtkImageView *self = user_data;

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
gtk_image_view_measure (GtkWidget      *widget,
                        GtkOrientation  orientation,
                        int             for_size,
                        int            *minimum,
                        int            *natural,
                        int            *minimum_baseline,
                        int            *natural_baselien)
{
  GtkImageView *self  = GTK_IMAGE_VIEW (widget);
  GtkImageViewPrivate *priv = gtk_image_view_get_instance_private (self);

  if (priv->fit_allocation && priv->paintable)
    {
      *minimum = 0;
      // XXX We should probably also call compute_concrete_size in here?

      if (orientation == GTK_ORIENTATION_HORIZONTAL)
        {
          *natural = gdk_paintable_get_intrinsic_width (priv->paintable);
        }
      else /* VERTICAL */
        {
          *natural = gdk_paintable_get_intrinsic_height (priv->paintable);
        }
    }
  else
    {
      double width, height;

      gtk_image_view_compute_bounding_box (self,
                                           &width,
                                           &height,
                                           NULL,
                                           NULL);

      if (orientation == GTK_ORIENTATION_HORIZONTAL)
        {
          *minimum = (int)ceil (width);
          *natural = (int)ceil (width);
        }
      else /* VERTICAL */
        {
          *minimum = (int)ceil (height);
          *natural = (int)ceil (height);
        }
    }
}

static void
gtk_image_view_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)

{
  GtkImageView *self = (GtkImageView *) object;

  switch (prop_id)
    {
      case PROP_SCALE:
        gtk_image_view_set_scale (self, g_value_get_double (value));
        break;
      case PROP_ANGLE:
        gtk_image_view_set_angle (self, g_value_get_double (value));
        break;
      case PROP_SNAP_ANGLE:
        gtk_image_view_set_snap_angle (self, g_value_get_boolean (value));
        break;
      case PROP_FIT_ALLOCATION:
        gtk_image_view_set_fit_allocation (self, g_value_get_boolean (value));
        break;
      case PROP_ROTATABLE:
        gtk_image_view_set_rotatable (self, g_value_get_boolean (value));
        break;
      case PROP_ZOOMABLE:
        gtk_image_view_set_zoomable (self, g_value_get_boolean (value));
        break;
      case PROP_TRANSITIONS_ENABLED:
        gtk_image_view_set_transitions_enabled (self, g_value_get_boolean (value));
        break;
      case PROP_HADJUSTMENT:
        gtk_image_view_set_hadjustment (self, g_value_get_object (value));
        break;
       case PROP_VADJUSTMENT:
        gtk_image_view_set_vadjustment (self, g_value_get_object (value));
        break;
      case PROP_HSCROLL_POLICY:
        gtk_image_view_set_hscroll_policy (self, g_value_get_enum (value));
        break;
      case PROP_VSCROLL_POLICY:
        gtk_image_view_set_vscroll_policy (self, g_value_get_enum (value));
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_image_view_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  GtkImageView *self  = (GtkImageView *)object;
  GtkImageViewPrivate *priv = gtk_image_view_get_instance_private (self);

  switch (prop_id)
    {
      case PROP_SCALE:
        g_value_set_double (value, priv->scale);
        break;
      case PROP_SCALE_SET:
        g_value_set_boolean (value, priv->scale_set);
        break;
      case PROP_ANGLE:
        g_value_set_double (value, priv->angle);
        break;
      case PROP_SNAP_ANGLE:
        g_value_set_boolean (value, priv->snap_angle);
        break;
      case PROP_FIT_ALLOCATION:
        g_value_set_boolean (value, priv->fit_allocation);
        break;
      case PROP_ROTATABLE:
        g_value_set_boolean (value, priv->rotatable);
        break;
      case PROP_ZOOMABLE:
        g_value_set_boolean (value, priv->zoomable);
        break;
      case PROP_TRANSITIONS_ENABLED:
        g_value_set_boolean (value, priv->transitions_enabled);
        break;
      case PROP_HADJUSTMENT:
        g_value_set_object (value, priv->hadjustment);
        break;
      case PROP_VADJUSTMENT:
        g_value_set_object (value, priv->vadjustment);
        break;
      case PROP_HSCROLL_POLICY:
        g_value_set_enum (value, priv->hscroll_policy);
        break;
      case PROP_VSCROLL_POLICY:
        g_value_set_enum (value, priv->vscroll_policy);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_image_view_finalize (GObject *object)
{
  GtkImageView *self  = (GtkImageView *)object;
  GtkImageViewPrivate *priv = gtk_image_view_get_instance_private (self);

  g_clear_object (&priv->rotate_gesture);
  g_clear_object (&priv->zoom_gesture);
  g_clear_object (&priv->zoom_controller);

  g_clear_object (&priv->hadjustment);
  g_clear_object (&priv->vadjustment);

  if (priv->paintable)
    {
      g_signal_handlers_disconnect_by_func (priv->paintable,
                                            G_CALLBACK (paintable_contents_changed_cb), self);
      g_clear_object (&priv->paintable);
    }

  G_OBJECT_CLASS (gtk_image_view_parent_class)->finalize (object);
}

static void
gtk_image_view_class_init (GtkImageViewClass *view_class)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (view_class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (view_class);

  object_class->set_property = gtk_image_view_set_property;
  object_class->get_property = gtk_image_view_get_property;
  object_class->finalize     = gtk_image_view_finalize;

  widget_class->measure       = gtk_image_view_measure;
  widget_class->size_allocate = gtk_image_view_size_allocate;
  widget_class->snapshot      = gtk_image_view_snapshot;

  /**
   * GtkImageView:scale:
   * The scale the internal surface gets drawn with.
   *
   * Since: 4.0
   */
  widget_props[PROP_SCALE] = g_param_spec_double ("scale",
                                                  P_("Scale"),
                                                  P_("The scale the internal surface gets drawn with"),
                                                  0.0,
                                                  G_MAXDOUBLE,
                                                  1.0,
                                                  GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);
  /**
   * GtkImageView:scale-set:
   * Whether or not the current value of the scale property was set by the user.
   * This is to distringuish between scale values set by the #GtkImageView itself,
   * e.g. when #GtkImageView:fit-allocation is true, which will change the scale
   * depending on the widget allocation.
   *
   * Since: 4.0
   */
  widget_props[PROP_SCALE_SET] = g_param_spec_boolean ("scale-set",
                                                       P_("Scale Set"),
                                                       P_("Wheter the scale property has been set by the user or by GtkImageView itself"),
                                                       FALSE,
                                                       GTK_PARAM_READABLE|G_PARAM_EXPLICIT_NOTIFY);
  /**
   * GtkImageView:angle:
   * The angle the surface gets rotated about.
   * This is in degrees and we rotate clock-wise.
   *
   * Since: 4.0
   */
  widget_props[PROP_ANGLE] = g_param_spec_double ("angle",
                                                  P_("Angle"),
                                                  P_("The angle the internal surface gets rotated about"),
                                                  0.0,
                                                  360.0,
                                                  0.0,
                                                  GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);
  /**
   * GtkImageView:rotatable:
   * Whether or not the image can be rotated using a two-finger rotate gesture.
   *
   * Since: 4.0
   */
  widget_props[PROP_ROTATABLE] = g_param_spec_boolean ("rotatable",
                                                       P_("Rotatable"),
                                                       P_("Controls user-rotatability"),
                                                       TRUE,
                                                       GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);
/**
   * GtkImageView:zoomable:
   * Whether or not the image can be scaled using a two-finger zoom gesture, as well as
   * scrolling on the #GtkImageView.
   *
   * Since: 4.0
   */
  widget_props[PROP_ZOOMABLE] = g_param_spec_boolean ("zoomable",
                                                      P_("Zoomable"),
                                                      P_("Controls user-zoomability"),
                                                      TRUE,
                                                      GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);
/**
   * GtkImageView:snap-angle:
   * Whether or not the angle property snaps to 90° steps. If this is enabled
   * and the angle property gets set to a non-90° step, the new value will be
   * set to the closest 90° step. If #GtkImageView:transitions-enabled is %TRUE,
   * the angle change from the current angle to the new angle will be interpolated.
   *
   * Since: 4.0
   */
  widget_props[PROP_SNAP_ANGLE] = g_param_spec_boolean ("snap-angle",
                                                        P_("Snap Angle"),
                                                        P_("Snap angle to 90° steps"),
                                                        FALSE,
                                                        GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkImageView:fit-allocation:
   * If this is %TRUE, the scale the image will be drawn in will depend on the current
   * widget allocation. The image will be scaled down to fit into the widget allocation,
   * but never scaled up. The aspect ratio of the image will be kept at all times.
   *
   * Since: 4.0
   */
  widget_props[PROP_FIT_ALLOCATION] = g_param_spec_boolean ("fit-allocation",
                                                            P_("Fit Allocation"),
                                                            P_("Scale the image down to fit into the widget allocation"),
                                                            FALSE,
                                                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   *  GtkImageView:transitions-enabled
   *
   *  Whether or not certain property changes will be interpolated. This affects a variety
   *  of function calls on a #GtkImageView instance, e.g. setting the angle property, the
   *  scale property, but also the angle snapping in case #GtkImageView:snap-angle is set.
   *
   *  Note that the transitions in #GtkImageView never apply to the actual property values
   *  set and instead interpolate between the visual angle/scale, so you cannot depend on
   *  getting 60 notify signal emissions per second.
   *
   *  Since: 4.0
   */
  widget_props[PROP_TRANSITIONS_ENABLED] = g_param_spec_boolean ("transitions-enabled",
                                                                 P_("Transitions Enabled"),
                                                                 P_("Whether scale and angle changes get interpolated"),
                                                                 TRUE,
                                                                 GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_WIDGET_PROPERTY, widget_props);

  g_object_class_override_property (object_class, PROP_HADJUSTMENT,    "hadjustment");
  g_object_class_override_property (object_class, PROP_VADJUSTMENT,    "vadjustment");
  g_object_class_override_property (object_class, PROP_HSCROLL_POLICY, "hscroll-policy");
  g_object_class_override_property (object_class, PROP_VSCROLL_POLICY, "vscroll-policy");

  gtk_widget_class_set_css_name (widget_class, "imageview");
}

/**
 * gtk_image_view_new:
 *
 * Returns: A newly created #GtkImageView instance.
 *
 * Since: 4.0
 */
GtkWidget *
gtk_image_view_new (void)
{
  return g_object_new (GTK_TYPE_IMAGE_VIEW, NULL);
}

static void
gtk_image_view_replace_paintable (GtkImageView *self,
                                  GdkPaintable *paintable)
{
  GtkImageViewPrivate *priv = gtk_image_view_get_instance_private (self);

  if (priv->paintable)
    {
      g_signal_handlers_disconnect_by_func (priv->paintable, G_CALLBACK (paintable_contents_changed_cb), self);
      g_object_unref (priv->paintable);
    }

  priv->paintable = paintable;

  if (priv->paintable)
    {
      g_object_ref (priv->paintable);
      g_signal_connect (priv->paintable, "invalidate-contents",
                        G_CALLBACK (paintable_contents_changed_cb), self);
    }

  gtk_image_view_update_adjustments (self);

  gtk_image_view_invalidate (self);
}

static GdkPaintable *
gtk_image_view_load_image_from_stream (GtkImageView *self,
                                       GInputStream *input_stream,
                                       GCancellable *cancellable,
                                       GError       *error)
{
  GdkPixbufAnimation *result;
  GdkPaintable *paintable = NULL;

  g_assert (error == NULL);
  result = gdk_pixbuf_animation_new_from_stream (input_stream,
                                                 cancellable,
                                                 &error);

  if (!error)
    {
      GdkPixbuf *frame = gdk_pixbuf_animation_get_static_image (result);

      paintable = GDK_PAINTABLE (gdk_texture_new_for_pixbuf (frame));
      g_object_unref (result);
    }

  g_input_stream_close (input_stream, NULL, NULL);
  g_object_unref (input_stream);

  return paintable;
}

/* CALLED FROM ANOTHER THREAD */
static void
gtk_image_view_load_image_contents (GTask        *task,
                                    gpointer      source_object,
                                    gpointer      task_data,
                                    GCancellable *cancellable)
{
  GtkImageView *self = source_object;
  GFile *file = G_FILE (task_data);
  GFileInputStream *in_stream;
  GdkPaintable *paintable;
  GError *error = NULL;

  in_stream = g_file_read (file, cancellable, &error);

  if (error)
    {
      /* in_stream is NULL */
      g_task_return_error (task, error);
      return;
    }

  /* Closes and unrefs the input stream */
  paintable = gtk_image_view_load_image_from_stream (self,
                                                     G_INPUT_STREAM (in_stream),
                                                     cancellable,
                                                     error);

  if (error)
    g_task_return_error (task, error);
  else
    g_task_return_pointer (task, paintable, NULL);
}

/* CALLED FROM ANOTHER THREAD */
static void
gtk_image_view_load_from_input_stream (GTask        *task,
                                       gpointer      source_object,
                                       gpointer      task_data,
                                       GCancellable *cancellable)
{
  GtkImageView *self = source_object;
  GInputStream *in_stream = task_data;
  GdkPaintable *paintable;
  GError *error = NULL;

  /* Closes and unrefs the input stream */
  paintable = gtk_image_view_load_image_from_stream (self,
                                                     in_stream,
                                                     cancellable,
                                                     error);

  if (error)
    g_task_return_error (task, error);
  else
    g_task_return_pointer (task, paintable, NULL);
}

/**
 * gtk_image_view_load_from_file_async:
 * @self: A #GtkImageView instance
 * @file: (transfer full): The file to read from
 * @cancellable: (nullable): A #GCancellable that can be used to
 *   cancel the loading operation
 * @callback: (scope async): Callback to call once the operation finished
 * @user_data: (closure): Data to pass to @callback
 *
 * Asynchronously loads an image from the given file.
 *
 * Since: 4.0
 */
void
gtk_image_view_load_from_file_async (GtkImageView        *self,
                                     GFile               *file,
                                     GCancellable        *cancellable,
                                     GAsyncReadyCallback  callback,
                                     gpointer             user_data)
{
  GTask *task;

  g_return_if_fail (GTK_IS_IMAGE_VIEW (self));
  g_return_if_fail (G_IS_FILE (file));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_task_data (task, g_object_ref (file), g_object_unref);
  g_task_run_in_thread (task, gtk_image_view_load_image_contents);

  g_object_unref (task);
}

/**
 * gtk_image_view_load_from_file_finish:
 * @self: A #GtkImageView instance
 * @result: A #GAsyncResult
 * @error: (nullable): Location to store error information in case the operation fails
 *
 * Finished an asynchronous operation started with gtk_image_view_load_from_file_async().
 *
 * Returns: %TRUE if the operation succeeded, %FALSE otherwise,
 * in which case @error will be set.
 *
 * Since: 4.0
 */
gboolean
gtk_image_view_load_from_file_finish (GtkImageView  *self,
                                      GAsyncResult  *result,
                                      GError       **error)
{
  GdkPaintable *paintable;
  g_return_val_if_fail (g_task_is_valid (result, self), FALSE);

  paintable = g_task_propagate_pointer (G_TASK (result), error);
  gtk_image_view_set_paintable (self, paintable);

  return paintable != NULL;
}

/**
 * gtk_image_view_load_from_stream_async:
 * @self: A #GtkImageView instance
 * @input_stream: (transfer full): Input stream to read from
 * @cancellable: (nullable): The #GCancellable used to cancel the operation
 * @callback: (scope async): A #GAsyncReadyCallback invoked when the operation finishes
 * @user_data: (closure): The data to pass to @callback
 *
 * Asynchronously loads an image from the given input stream.
 *
 * Since: 4.0
 */
void
gtk_image_view_load_from_stream_async (GtkImageView        *self,
                                       GInputStream        *input_stream,
                                       GCancellable        *cancellable,
                                       GAsyncReadyCallback  callback,
                                       gpointer             user_data)
{
  GTask *task;

  g_return_if_fail (GTK_IS_IMAGE_VIEW (self));
  g_return_if_fail (G_IS_INPUT_STREAM (input_stream));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_task_data (task, g_object_ref (input_stream), g_object_unref);
  g_task_run_in_thread (task, gtk_image_view_load_from_input_stream);

  g_object_unref (task);
}

/**
 * gtk_image_view_load_from_stream_finish:
 * @self: A #GtkImageView instance
 * @result: A #GAsyncResult
 * @error: (nullable): Location to store error information on failure
 *
 * Finishes an asynchronous operation started by gtk_image_view_load_from_stream_async().
 *
 * Returns: %TRUE if the operation finished successfully, %FALSE otherwise.
 *
 * Since: 4.0
 */
gboolean
gtk_image_view_load_from_stream_finish (GtkImageView  *self,
                                        GAsyncResult  *result,
                                        GError       **error)
{
  GdkPaintable *paintable;

  g_return_val_if_fail (g_task_is_valid (result, self), FALSE);

  paintable = g_task_propagate_pointer (G_TASK (result), error);
  gtk_image_view_set_paintable (self, paintable);

  return paintable != NULL;
}

/**
 * gtk_image_view_set_paintable:
 * @self: A #GtkImageView instance
 * @paintable: (nullable): The #GdkPaintable to set, or %NULL
 *   to unset any currently set one
 *
 * Since: 4.0
 */
void
gtk_image_view_set_paintable (GtkImageView *self,
                              GdkPaintable *paintable)
{
  g_return_if_fail (GTK_IS_IMAGE_VIEW (self));
  g_return_if_fail (paintable == NULL || GDK_IS_PAINTABLE (paintable));

  gtk_image_view_replace_paintable (self, paintable);
}
