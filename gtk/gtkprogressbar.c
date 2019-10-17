/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"

#include "gtkprogressbar.h"

#include "gtkcssnodeprivate.h"
#include "gtkcssnumbervalueprivate.h"
#include "gtkcssshadowsvalueprivate.h"
#include "gtkcssstylepropertyprivate.h"
#include "gtkgizmoprivate.h"
#include "gtkintl.h"
#include "gtklabel.h"
#include "gtkorientableprivate.h"
#include "gtkprogresstrackerprivate.h"
#include "gtkprivate.h"
#include "gtkstylecontextprivate.h"
#include "gtkwidgetprivate.h"
#include "gtkboxlayout.h"

#include "a11y/gtkprogressbaraccessible.h"

#include <string.h>

#include "fallback-c89.c"

/**
 * SECTION:gtkprogressbar
 * @Short_description: A widget which indicates progress visually
 * @Title: GtkProgressBar
 *
 * The #GtkProgressBar is typically used to display the progress of a long
 * running operation. It provides a visual clue that processing is underway.
 * The GtkProgressBar can be used in two different modes: percentage mode
 * and activity mode.
 *
 * When an application can determine how much work needs to take place
 * (e.g. read a fixed number of bytes from a file) and can monitor its
 * progress, it can use the GtkProgressBar in percentage mode and the
 * user sees a growing bar indicating the percentage of the work that
 * has been completed. In this mode, the application is required to call
 * gtk_progress_bar_set_fraction() periodically to update the progress bar.
 *
 * When an application has no accurate way of knowing the amount of work
 * to do, it can use the #GtkProgressBar in activity mode, which shows
 * activity by a block moving back and forth within the progress area. In
 * this mode, the application is required to call gtk_progress_bar_pulse()
 * periodically to update the progress bar.
 *
 * There is quite a bit of flexibility provided to control the appearance
 * of the #GtkProgressBar. Functions are provided to control the orientation
 * of the bar, optional text can be displayed along with the bar, and the
 * step size used in activity mode can be set.
 *
 * # CSS nodes
 *
 * |[<!-- language="plain" -->
 * progressbar[.osd]
 * ├── [text]
 * ╰── trough[.empty][.full]
 *     ╰── progress[.pulse]
 * ]|
 *
 * GtkProgressBar has a main CSS node with name progressbar and subnodes with
 * names text and trough, of which the latter has a subnode named progress. The
 * text subnode is only present if text is shown. The progress subnode has the
 * style class .pulse when in activity mode. It gets the style classes .left,
 * .right, .top or .bottom added when the progress 'touches' the corresponding
 * end of the GtkProgressBar. The .osd class on the progressbar node is for use
 * in overlays like the one Epiphany has for page loading progress.
 */

typedef struct _GtkProgressBarClass         GtkProgressBarClass;

struct _GtkProgressBar
{
  GtkWidget parent_instance;
};

struct _GtkProgressBarClass
{
  GtkWidgetClass parent_class;
};


typedef struct
{
  gchar         *text;

  GtkWidget     *label;
  GtkWidget     *trough_widget;
  GtkWidget     *progress_widget;

  gdouble        fraction;
  gdouble        pulse_fraction;

  double         activity_pos;
  guint          activity_blocks;

  GtkOrientation orientation;

  guint              tick_id;
  GtkProgressTracker tracker;
  gint64             pulse1;
  gint64             pulse2;
  gdouble            last_iteration;

  guint          activity_dir  : 1;
  guint          activity_mode : 1;
  guint          ellipsize     : 3;
  guint          show_text     : 1;
  guint          inverted      : 1;
} GtkProgressBarPrivate;

enum {
  PROP_0,
  PROP_FRACTION,
  PROP_PULSE_STEP,
  PROP_INVERTED,
  PROP_TEXT,
  PROP_SHOW_TEXT,
  PROP_ELLIPSIZE,
  PROP_ORIENTATION,
  NUM_PROPERTIES = PROP_ORIENTATION
};

static GParamSpec *progress_props[NUM_PROPERTIES] = { NULL, };

static void gtk_progress_bar_set_property         (GObject        *object,
                                                   guint           prop_id,
                                                   const GValue   *value,
                                                   GParamSpec     *pspec);
static void gtk_progress_bar_get_property         (GObject        *object,
                                                   guint           prop_id,
                                                   GValue         *value,
                                                   GParamSpec     *pspec);
static void     gtk_progress_bar_act_mode_enter   (GtkProgressBar *progress);
static void     gtk_progress_bar_act_mode_leave   (GtkProgressBar *progress);
static void     gtk_progress_bar_finalize         (GObject        *object);
static void     gtk_progress_bar_set_orientation  (GtkProgressBar *progress,
                                                   GtkOrientation  orientation);
static void     gtk_progress_bar_direction_changed (GtkWidget        *widget,
                                                    GtkTextDirection  previous_dir);

G_DEFINE_TYPE_WITH_CODE (GtkProgressBar, gtk_progress_bar, GTK_TYPE_WIDGET,
                         G_ADD_PRIVATE (GtkProgressBar)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ORIENTABLE, NULL))

static void
gtk_progress_bar_class_init (GtkProgressBarClass *class)
{
  GObjectClass *gobject_class;
  GtkWidgetClass *widget_class;

  gobject_class = G_OBJECT_CLASS (class);
  widget_class = (GtkWidgetClass *) class;

  gobject_class->set_property = gtk_progress_bar_set_property;
  gobject_class->get_property = gtk_progress_bar_get_property;
  gobject_class->finalize = gtk_progress_bar_finalize;

  widget_class->direction_changed = gtk_progress_bar_direction_changed;

  g_object_class_override_property (gobject_class, PROP_ORIENTATION, "orientation");

  progress_props[PROP_INVERTED] =
      g_param_spec_boolean ("inverted",
                            P_("Inverted"),
                            P_("Invert the direction in which the progress bar grows"),
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  progress_props[PROP_FRACTION] =
      g_param_spec_double ("fraction",
                           P_("Fraction"),
                           P_("The fraction of total work that has been completed"),
                           0.0, 1.0,
                           0.0,
                           GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  progress_props[PROP_PULSE_STEP] =
      g_param_spec_double ("pulse-step",
                           P_("Pulse Step"),
                           P_("The fraction of total progress to move the bouncing block when pulsed"),
                           0.0, 1.0,
                           0.1,
                           GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  progress_props[PROP_TEXT] =
      g_param_spec_string ("text",
                           P_("Text"),
                           P_("Text to be displayed in the progress bar"),
                           NULL,
                           GTK_PARAM_READWRITE);

  /**
   * GtkProgressBar:show-text:
   *
   * Sets whether the progress bar will show a text in addition
   * to the bar itself. The shown text is either the value of
   * the #GtkProgressBar:text property or, if that is %NULL,
   * the #GtkProgressBar:fraction value, as a percentage.
   *
   * To make a progress bar that is styled and sized suitably for
   * showing text (even if the actual text is blank), set
   * #GtkProgressBar:show-text to %TRUE and #GtkProgressBar:text
   * to the empty string (not %NULL).
   */
  progress_props[PROP_SHOW_TEXT] =
      g_param_spec_boolean ("show-text",
                            P_("Show text"),
                            P_("Whether the progress is shown as text."),
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkProgressBar:ellipsize:
   *
   * The preferred place to ellipsize the string, if the progress bar does
   * not have enough room to display the entire string, specified as a
   * #PangoEllipsizeMode.
   *
   * Note that setting this property to a value other than
   * %PANGO_ELLIPSIZE_NONE has the side-effect that the progress bar requests
   * only enough space to display the ellipsis ("..."). Another means to set a
   * progress bar's width is gtk_widget_set_size_request().
   */
  progress_props[PROP_ELLIPSIZE] =
      g_param_spec_enum ("ellipsize",
                         P_("Ellipsize"),
                         P_("The preferred place to ellipsize the string, if the progress bar "
                            "does not have enough room to display the entire string, if at all."),
                         PANGO_TYPE_ELLIPSIZE_MODE,
                         PANGO_ELLIPSIZE_NONE,
                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, NUM_PROPERTIES, progress_props);

  gtk_widget_class_set_accessible_type (widget_class, GTK_TYPE_PROGRESS_BAR_ACCESSIBLE);
  gtk_widget_class_set_css_name (widget_class, I_("progressbar"));
  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BOX_LAYOUT);
}

static void
update_fraction_classes (GtkProgressBar *pbar)
{
  GtkProgressBarPrivate *priv = gtk_progress_bar_get_instance_private (pbar);
  GtkStyleContext *context;
  gboolean empty = FALSE;
  gboolean full = FALSE;

  /* Here we set classes based on fill-level unless we're in activity-mode.
   */

  if (!priv->activity_mode)
    {
      if (priv->fraction <= 0.0)
        empty = TRUE;
      else if (priv->fraction >= 1.0)
        full = TRUE;
    }

  context = gtk_widget_get_style_context (priv->trough_widget);

  if (empty)
    gtk_style_context_add_class (context, "empty");
  else
    gtk_style_context_remove_class (context, "empty");

  if (full)
    gtk_style_context_add_class (context, "full");
  else
    gtk_style_context_remove_class (context, "full");
}

static void
update_node_classes (GtkProgressBar *pbar)
{
  GtkProgressBarPrivate *priv = gtk_progress_bar_get_instance_private (pbar);
  GtkStyleContext *context;
  gboolean left = FALSE;
  gboolean right = FALSE;
  gboolean top = FALSE;
  gboolean bottom = FALSE;

  /* Here we set positional classes, depending on which end of the
   * progressbar the progress touches.
   */

  if (priv->activity_mode)
    {
      if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
        {
          left = priv->activity_pos <= 0.0;
          right = priv->activity_pos >= 1.0;
        }
      else
        {
          top = priv->activity_pos <= 0.0;
          bottom = priv->activity_pos >= 1.0;
        }
    }
  else /* continuous */
    {
      gboolean inverted;

      inverted = priv->inverted;
      if (gtk_widget_get_direction (GTK_WIDGET (pbar)) == GTK_TEXT_DIR_RTL)
        {
          if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
            inverted = !inverted;
        }

      if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
        {
          left = !inverted || priv->fraction >= 1.0;
          right = inverted || priv->fraction >= 1.0;
        }
      else
        {
          top = !inverted || priv->fraction >= 1.0;
          bottom = inverted || priv->fraction >= 1.0;
        }
    }

  context = gtk_widget_get_style_context (priv->progress_widget);

  if (left)
    gtk_style_context_add_class (context, GTK_STYLE_CLASS_LEFT);
  else
    gtk_style_context_remove_class (context, GTK_STYLE_CLASS_LEFT);

  if (right)
    gtk_style_context_add_class (context, GTK_STYLE_CLASS_RIGHT);
  else
    gtk_style_context_remove_class (context, GTK_STYLE_CLASS_RIGHT);

  if (top)
    gtk_style_context_add_class (context, GTK_STYLE_CLASS_TOP);
  else
    gtk_style_context_remove_class (context, GTK_STYLE_CLASS_TOP);

  if (bottom)
    gtk_style_context_add_class (context, GTK_STYLE_CLASS_BOTTOM);
  else
    gtk_style_context_remove_class (context, GTK_STYLE_CLASS_BOTTOM);

  update_fraction_classes (pbar);
}

static void
allocate_trough (GtkGizmo *gizmo,
                 int       width,
                 int       height,
                 int       baseline)

{
  GtkProgressBar *pbar = GTK_PROGRESS_BAR (gtk_widget_get_parent (GTK_WIDGET (gizmo)));
  GtkProgressBarPrivate *priv = gtk_progress_bar_get_instance_private (pbar);
  GtkAllocation alloc;
  int progress_width, progress_height;
  gboolean inverted;

  inverted = priv->inverted;
  if (gtk_widget_get_direction (GTK_WIDGET (pbar)) == GTK_TEXT_DIR_RTL)
    {
      if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
        inverted = !inverted;
    }

  gtk_widget_measure (priv->progress_widget, GTK_ORIENTATION_VERTICAL, -1,
                      &progress_height, NULL,
                      NULL, NULL);

  gtk_widget_measure (priv->progress_widget, GTK_ORIENTATION_HORIZONTAL, -1,
                      &progress_width, NULL,
                      NULL, NULL);

  if (priv->activity_mode)
    {
      if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
        {
          alloc.width = progress_width + (width - progress_width) / priv->activity_blocks;
          alloc.x = priv->activity_pos * (width - alloc.width);
          alloc.y = (height - progress_height) / 2;
          alloc.height = progress_height;
        }
      else
        {

          alloc.height = progress_height + (height - progress_height) / priv->activity_blocks;
          alloc.y = priv->activity_pos * (height - alloc.height);
          alloc.x = (width - progress_width) / 2;
          alloc.width = progress_width;
        }
    }
  else
    {
      if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
        {
          alloc.width = progress_width + (width - progress_width) * priv->fraction;
          alloc.height = progress_height;
          alloc.y = (height - progress_height) / 2;

          if (!inverted)
            alloc.x = 0;
          else
            alloc.x = width - alloc.width;
        }
      else
        {
          alloc.width = progress_width;
          alloc.height = progress_height + (height - progress_height) * priv->fraction;
          alloc.x = (width - progress_width) / 2;

          if (!inverted)
            alloc.y = 0;
          else
            alloc.y = height - alloc.height;
        }
    }

  gtk_widget_size_allocate (priv->progress_widget, &alloc, -1);

}

static void
snapshot_trough (GtkGizmo    *gizmo,
                 GtkSnapshot *snapshot)

{
  GtkProgressBar *pbar = GTK_PROGRESS_BAR (gtk_widget_get_parent (GTK_WIDGET (gizmo)));
  GtkProgressBarPrivate *priv = gtk_progress_bar_get_instance_private (pbar);

  gtk_widget_snapshot_child (GTK_WIDGET (gizmo), priv->progress_widget, snapshot);
}

static void
gtk_progress_bar_init (GtkProgressBar *pbar)
{
  GtkProgressBarPrivate *priv = gtk_progress_bar_get_instance_private (pbar);

  priv->inverted = FALSE;
  priv->pulse_fraction = 0.1;
  priv->activity_pos = 0;
  priv->activity_dir = 1;
  priv->activity_blocks = 5;
  priv->ellipsize = PANGO_ELLIPSIZE_NONE;
  priv->show_text = FALSE;

  priv->text = NULL;
  priv->fraction = 0.0;

  priv->trough_widget = gtk_gizmo_new ("trough",
                                       NULL,
                                       allocate_trough,
                                       snapshot_trough,
                                       NULL);
  gtk_widget_set_parent (priv->trough_widget, GTK_WIDGET (pbar));

  priv->progress_widget = gtk_gizmo_new ("progress", NULL, NULL, NULL, NULL);
  gtk_widget_set_parent (priv->progress_widget, priv->trough_widget);

  /* horizontal is default */
  priv->orientation = GTK_ORIENTATION_VERTICAL; /* Just to force an update... */
  gtk_progress_bar_set_orientation (pbar, GTK_ORIENTATION_HORIZONTAL);
  _gtk_orientable_set_style_classes (GTK_ORIENTABLE (pbar));
}

static void
gtk_progress_bar_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  GtkProgressBar *pbar;

  pbar = GTK_PROGRESS_BAR (object);

  switch (prop_id)
    {
    case PROP_ORIENTATION:
      gtk_progress_bar_set_orientation (pbar, g_value_get_enum (value));
      break;
    case PROP_INVERTED:
      gtk_progress_bar_set_inverted (pbar, g_value_get_boolean (value));
      break;
    case PROP_FRACTION:
      gtk_progress_bar_set_fraction (pbar, g_value_get_double (value));
      break;
    case PROP_PULSE_STEP:
      gtk_progress_bar_set_pulse_step (pbar, g_value_get_double (value));
      break;
    case PROP_TEXT:
      gtk_progress_bar_set_text (pbar, g_value_get_string (value));
      break;
    case PROP_SHOW_TEXT:
      gtk_progress_bar_set_show_text (pbar, g_value_get_boolean (value));
      break;
    case PROP_ELLIPSIZE:
      gtk_progress_bar_set_ellipsize (pbar, g_value_get_enum (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_progress_bar_get_property (GObject      *object,
                               guint         prop_id,
                               GValue       *value,
                               GParamSpec   *pspec)
{
  GtkProgressBar *pbar = GTK_PROGRESS_BAR (object);
  GtkProgressBarPrivate *priv = gtk_progress_bar_get_instance_private (pbar);

  switch (prop_id)
    {
    case PROP_ORIENTATION:
      g_value_set_enum (value, priv->orientation);
      break;
    case PROP_INVERTED:
      g_value_set_boolean (value, priv->inverted);
      break;
    case PROP_FRACTION:
      g_value_set_double (value, priv->fraction);
      break;
    case PROP_PULSE_STEP:
      g_value_set_double (value, priv->pulse_fraction);
      break;
    case PROP_TEXT:
      g_value_set_string (value, priv->text);
      break;
    case PROP_SHOW_TEXT:
      g_value_set_boolean (value, priv->show_text);
      break;
    case PROP_ELLIPSIZE:
      g_value_set_enum (value, priv->ellipsize);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/**
 * gtk_progress_bar_new:
 *
 * Creates a new #GtkProgressBar.
 *
 * Returns: a #GtkProgressBar.
 */
GtkWidget*
gtk_progress_bar_new (void)
{
  GtkWidget *pbar;

  pbar = g_object_new (GTK_TYPE_PROGRESS_BAR, NULL);

  return pbar;
}

static void
gtk_progress_bar_finalize (GObject *object)
{
  GtkProgressBar *pbar = GTK_PROGRESS_BAR (object);
  GtkProgressBarPrivate *priv = gtk_progress_bar_get_instance_private (pbar);

  if (priv->activity_mode)
    gtk_progress_bar_act_mode_leave (pbar);

  g_free (priv->text);

  g_clear_pointer (&priv->label, gtk_widget_unparent);

  gtk_widget_unparent (priv->progress_widget);
  gtk_widget_unparent (priv->trough_widget);

  G_OBJECT_CLASS (gtk_progress_bar_parent_class)->finalize (object);
}

static gchar *
get_current_text (GtkProgressBar *pbar)
{
  GtkProgressBarPrivate *priv = gtk_progress_bar_get_instance_private (pbar);

  if (priv->text)
    return g_strdup (priv->text);
  else
    return g_strdup_printf (C_("progress bar label", "%.0f %%"), priv->fraction * 100.0);
}

static gboolean
tick_cb (GtkWidget     *widget,
         GdkFrameClock *frame_clock,
         gpointer       user_data)
{
  GtkProgressBar *pbar = GTK_PROGRESS_BAR (widget);
  GtkProgressBarPrivate *priv = gtk_progress_bar_get_instance_private (pbar);
  gint64 frame_time;
  gdouble iteration, pulse_iterations, current_iterations, fraction;

  if (priv->pulse2 == 0 && priv->pulse1 == 0)
    return G_SOURCE_CONTINUE;

  frame_time = gdk_frame_clock_get_frame_time (frame_clock);
  gtk_progress_tracker_advance_frame (&priv->tracker, frame_time);

  g_assert (priv->pulse2 > priv->pulse1);

  pulse_iterations = (priv->pulse2 - priv->pulse1) / (gdouble) G_USEC_PER_SEC;
  current_iterations = (frame_time - priv->pulse1) / (gdouble) G_USEC_PER_SEC;

  iteration = gtk_progress_tracker_get_iteration (&priv->tracker);
  /* Determine the fraction to move the block from one frame
   * to the next when pulse_fraction is how far the block should
   * move between two calls to gtk_progress_bar_pulse().
   */
  fraction = priv->pulse_fraction * (iteration - priv->last_iteration) / MAX (pulse_iterations, current_iterations);
  priv->last_iteration = iteration;

  if (current_iterations > 3 * pulse_iterations)
    return G_SOURCE_CONTINUE;

  /* advance the block */
  if (priv->activity_dir == 0)
    {
      priv->activity_pos += fraction;
      if (priv->activity_pos > 1.0)
        {
          priv->activity_pos = 1.0;
          priv->activity_dir = 1;
        }
    }
  else
    {
      priv->activity_pos -= fraction;
      if (priv->activity_pos <= 0)
        {
          priv->activity_pos = 0;
          priv->activity_dir = 0;
        }
    }

  update_node_classes (pbar);

  gtk_widget_queue_allocate (priv->trough_widget);

  return G_SOURCE_CONTINUE;
}

static void
gtk_progress_bar_act_mode_enter (GtkProgressBar *pbar)
{
  GtkProgressBarPrivate *priv = gtk_progress_bar_get_instance_private (pbar);
  GtkWidget *widget = GTK_WIDGET (pbar);
  gboolean inverted;

  gtk_style_context_add_class (gtk_widget_get_style_context (priv->progress_widget), GTK_STYLE_CLASS_PULSE);

  inverted = priv->inverted;
  if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
    {
      if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
        inverted = !inverted;
    }

  /* calculate start pos */
  if (!inverted)
    {
      priv->activity_pos = 0.0;
      priv->activity_dir = 0;
    }
  else
    {
      priv->activity_pos = 1.0;
      priv->activity_dir = 1;
    }

  update_node_classes (pbar);
  /* No fixed schedule for pulses, will adapt after calls to update_pulse. Just
   * start the tracker to repeat forever with iterations every second.*/
  gtk_progress_tracker_start (&priv->tracker, G_USEC_PER_SEC, 0, INFINITY);
  priv->tick_id = gtk_widget_add_tick_callback (widget, tick_cb, NULL, NULL);
  priv->pulse2 = 0;
  priv->pulse1 = 0;
  priv->last_iteration = 0;
}

static void
gtk_progress_bar_act_mode_leave (GtkProgressBar *pbar)
{
  GtkProgressBarPrivate *priv = gtk_progress_bar_get_instance_private (pbar);

  if (priv->tick_id)
    gtk_widget_remove_tick_callback (GTK_WIDGET (pbar), priv->tick_id);
  priv->tick_id = 0;

  gtk_style_context_remove_class (gtk_widget_get_style_context (priv->progress_widget),
                                  GTK_STYLE_CLASS_PULSE);
  update_node_classes (pbar);
}

static void
gtk_progress_bar_set_activity_mode (GtkProgressBar *pbar,
                                    gboolean        activity_mode)
{
  GtkProgressBarPrivate *priv = gtk_progress_bar_get_instance_private (pbar);

  activity_mode = !!activity_mode;

  if (priv->activity_mode != activity_mode)
    {
      priv->activity_mode = activity_mode;

      if (priv->activity_mode)
        gtk_progress_bar_act_mode_enter (pbar);
      else
        gtk_progress_bar_act_mode_leave (pbar);

      gtk_widget_queue_resize (GTK_WIDGET (pbar));
    }
}

/**
 * gtk_progress_bar_set_fraction:
 * @pbar: a #GtkProgressBar
 * @fraction: fraction of the task that’s been completed
 *
 * Causes the progress bar to “fill in” the given fraction
 * of the bar. The fraction should be between 0.0 and 1.0,
 * inclusive.
 */
void
gtk_progress_bar_set_fraction (GtkProgressBar *pbar,
                               gdouble         fraction)
{
  GtkProgressBarPrivate *priv = gtk_progress_bar_get_instance_private (pbar);

  g_return_if_fail (GTK_IS_PROGRESS_BAR (pbar));

  priv->fraction = CLAMP (fraction, 0.0, 1.0);

  if (priv->label)
    {
      char *text = get_current_text (pbar);
      gtk_label_set_label (GTK_LABEL (priv->label), text);

      g_free (text);
    }

  gtk_progress_bar_set_activity_mode (pbar, FALSE);
  gtk_widget_queue_allocate (priv->trough_widget);
  update_fraction_classes (pbar);

  g_object_notify_by_pspec (G_OBJECT (pbar), progress_props[PROP_FRACTION]);
}

static void
gtk_progress_bar_update_pulse (GtkProgressBar *pbar)
{
  GtkProgressBarPrivate *priv = gtk_progress_bar_get_instance_private (pbar);
  gint64 pulse_time = g_get_monotonic_time ();

  if (priv->pulse2 == pulse_time)
    return;

  priv->pulse1 = priv->pulse2;
  priv->pulse2 = pulse_time;
}

/**
 * gtk_progress_bar_pulse:
 * @pbar: a #GtkProgressBar
 *
 * Indicates that some progress has been made, but you don’t know how much.
 * Causes the progress bar to enter “activity mode,” where a block
 * bounces back and forth. Each call to gtk_progress_bar_pulse()
 * causes the block to move by a little bit (the amount of movement
 * per pulse is determined by gtk_progress_bar_set_pulse_step()).
 */
void
gtk_progress_bar_pulse (GtkProgressBar *pbar)
{
  g_return_if_fail (GTK_IS_PROGRESS_BAR (pbar));

  gtk_progress_bar_set_activity_mode (pbar, TRUE);
  gtk_progress_bar_update_pulse (pbar);
}

/**
 * gtk_progress_bar_set_text:
 * @pbar: a #GtkProgressBar
 * @text: (allow-none): a UTF-8 string, or %NULL
 *
 * Causes the given @text to appear next to the progress bar.
 *
 * If @text is %NULL and #GtkProgressBar:show-text is %TRUE, the current
 * value of #GtkProgressBar:fraction will be displayed as a percentage.
 *
 * If @text is non-%NULL and #GtkProgressBar:show-text is %TRUE, the text
 * will be displayed. In this case, it will not display the progress
 * percentage. If @text is the empty string, the progress bar will still
 * be styled and sized suitably for containing text, as long as
 * #GtkProgressBar:show-text is %TRUE.
 */
void
gtk_progress_bar_set_text (GtkProgressBar *pbar,
                           const gchar    *text)
{
  GtkProgressBarPrivate *priv = gtk_progress_bar_get_instance_private (pbar);

  g_return_if_fail (GTK_IS_PROGRESS_BAR (pbar));

  /* Don't notify again if nothing's changed. */
  if (g_strcmp0 (priv->text, text) == 0)
    return;

  g_free (priv->text);
  priv->text = g_strdup (text);

  if (priv->label)
    gtk_label_set_label (GTK_LABEL (priv->label), text);

  gtk_widget_queue_resize (GTK_WIDGET (pbar));

  g_object_notify_by_pspec (G_OBJECT (pbar), progress_props[PROP_TEXT]);
}

/**
 * gtk_progress_bar_set_show_text:
 * @pbar: a #GtkProgressBar
 * @show_text: whether to show text
 *
 * Sets whether the progress bar will show text next to the bar.
 * The shown text is either the value of the #GtkProgressBar:text
 * property or, if that is %NULL, the #GtkProgressBar:fraction value,
 * as a percentage.
 *
 * To make a progress bar that is styled and sized suitably for containing
 * text (even if the actual text is blank), set #GtkProgressBar:show-text to
 * %TRUE and #GtkProgressBar:text to the empty string (not %NULL).
 */
void
gtk_progress_bar_set_show_text (GtkProgressBar *pbar,
                                gboolean        show_text)
{
  GtkProgressBarPrivate *priv = gtk_progress_bar_get_instance_private (pbar);

  g_return_if_fail (GTK_IS_PROGRESS_BAR (pbar));

  show_text = !!show_text;

  if (priv->show_text == show_text)
    return;

  priv->show_text = show_text;

  if (show_text)
    {
      char *text = get_current_text (pbar);

      priv->label = g_object_new (GTK_TYPE_LABEL,
                                  "css-name", "text",
                                  "label", text,
                                  "ellipsize", priv->ellipsize,
                                  NULL);
      gtk_widget_insert_after (priv->label, GTK_WIDGET (pbar), NULL);

      g_free (text);
    }
  else
    {
      g_clear_pointer (&priv->label, gtk_widget_unparent);
    }

  gtk_widget_queue_resize (GTK_WIDGET (pbar));

  g_object_notify_by_pspec (G_OBJECT (pbar), progress_props[PROP_SHOW_TEXT]);
}

/**
 * gtk_progress_bar_get_show_text:
 * @pbar: a #GtkProgressBar
 *
 * Gets the value of the #GtkProgressBar:show-text property.
 * See gtk_progress_bar_set_show_text().
 *
 * Returns: %TRUE if text is shown in the progress bar
 */
gboolean
gtk_progress_bar_get_show_text (GtkProgressBar *pbar)
{
  GtkProgressBarPrivate *priv = gtk_progress_bar_get_instance_private (pbar);

  g_return_val_if_fail (GTK_IS_PROGRESS_BAR (pbar), FALSE);

  return priv->show_text;
}

/**
 * gtk_progress_bar_set_pulse_step:
 * @pbar: a #GtkProgressBar
 * @fraction: fraction between 0.0 and 1.0
 *
 * Sets the fraction of total progress bar length to move the
 * bouncing block for each call to gtk_progress_bar_pulse().
 */
void
gtk_progress_bar_set_pulse_step (GtkProgressBar *pbar,
                                 gdouble         fraction)
{
  GtkProgressBarPrivate *priv = gtk_progress_bar_get_instance_private (pbar);

  g_return_if_fail (GTK_IS_PROGRESS_BAR (pbar));

  priv->pulse_fraction = fraction;

  g_object_notify_by_pspec (G_OBJECT (pbar), progress_props[PROP_PULSE_STEP]);
}

static void
gtk_progress_bar_direction_changed (GtkWidget        *widget,
                                    GtkTextDirection  previous_dir)
{
  GtkProgressBar *pbar = GTK_PROGRESS_BAR (widget);

  update_node_classes (pbar);

  GTK_WIDGET_CLASS (gtk_progress_bar_parent_class)->direction_changed (widget, previous_dir);
}

static void
gtk_progress_bar_set_orientation (GtkProgressBar *pbar,
                                  GtkOrientation  orientation)
{
  GtkProgressBarPrivate *priv = gtk_progress_bar_get_instance_private (pbar);
  GtkBoxLayout *layout;

  if (priv->orientation == orientation)
    return;

  priv->orientation = orientation;

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      gtk_widget_set_vexpand (priv->trough_widget, FALSE);
      gtk_widget_set_hexpand (priv->trough_widget, TRUE);
      gtk_widget_set_halign (priv->trough_widget, GTK_ALIGN_FILL);
      gtk_widget_set_valign (priv->trough_widget, GTK_ALIGN_CENTER);
    }
  else
    {
      gtk_widget_set_vexpand (priv->trough_widget, TRUE);
      gtk_widget_set_hexpand (priv->trough_widget, FALSE);
      gtk_widget_set_halign (priv->trough_widget, GTK_ALIGN_CENTER);
      gtk_widget_set_valign (priv->trough_widget, GTK_ALIGN_FILL);
    }

  _gtk_orientable_set_style_classes (GTK_ORIENTABLE (pbar));
  update_node_classes (pbar);
  gtk_widget_queue_resize (GTK_WIDGET (pbar));

  layout = GTK_BOX_LAYOUT (gtk_widget_get_layout_manager (GTK_WIDGET (pbar)));
  gtk_orientable_set_orientation (GTK_ORIENTABLE (layout), GTK_ORIENTATION_VERTICAL);

  g_object_notify (G_OBJECT (pbar), "orientation");
}

/**
 * gtk_progress_bar_set_inverted:
 * @pbar: a #GtkProgressBar
 * @inverted: %TRUE to invert the progress bar
 *
 * Progress bars normally grow from top to bottom or left to right.
 * Inverted progress bars grow in the opposite direction.
 */
void
gtk_progress_bar_set_inverted (GtkProgressBar *pbar,
                               gboolean        inverted)
{
  GtkProgressBarPrivate *priv = gtk_progress_bar_get_instance_private (pbar);

  g_return_if_fail (GTK_IS_PROGRESS_BAR (pbar));

  if (priv->inverted == inverted)
    return;

  priv->inverted = inverted;

  update_node_classes (pbar);
  gtk_widget_queue_resize (GTK_WIDGET (pbar));

  g_object_notify_by_pspec (G_OBJECT (pbar), progress_props[PROP_INVERTED]);
}

/**
 * gtk_progress_bar_get_text:
 * @pbar: a #GtkProgressBar
 *
 * Retrieves the text that is displayed with the progress bar,
 * if any, otherwise %NULL. The return value is a reference
 * to the text, not a copy of it, so will become invalid
 * if you change the text in the progress bar.
 *
 * Returns: (nullable): text, or %NULL; this string is owned by the widget
 * and should not be modified or freed.
 */
const gchar*
gtk_progress_bar_get_text (GtkProgressBar *pbar)
{
  GtkProgressBarPrivate *priv = gtk_progress_bar_get_instance_private (pbar);

  g_return_val_if_fail (GTK_IS_PROGRESS_BAR (pbar), NULL);

  return priv->text;
}

/**
 * gtk_progress_bar_get_fraction:
 * @pbar: a #GtkProgressBar
 *
 * Returns the current fraction of the task that’s been completed.
 *
 * Returns: a fraction from 0.0 to 1.0
 */
gdouble
gtk_progress_bar_get_fraction (GtkProgressBar *pbar)
{
  GtkProgressBarPrivate *priv = gtk_progress_bar_get_instance_private (pbar);

  g_return_val_if_fail (GTK_IS_PROGRESS_BAR (pbar), 0);

  return priv->fraction;
}

/**
 * gtk_progress_bar_get_pulse_step:
 * @pbar: a #GtkProgressBar
 *
 * Retrieves the pulse step set with gtk_progress_bar_set_pulse_step().
 *
 * Returns: a fraction from 0.0 to 1.0
 */
gdouble
gtk_progress_bar_get_pulse_step (GtkProgressBar *pbar)
{
  GtkProgressBarPrivate *priv = gtk_progress_bar_get_instance_private (pbar);

  g_return_val_if_fail (GTK_IS_PROGRESS_BAR (pbar), 0);

  return priv->pulse_fraction;
}

/**
 * gtk_progress_bar_get_inverted:
 * @pbar: a #GtkProgressBar
 *
 * Gets the value set by gtk_progress_bar_set_inverted().
 *
 * Returns: %TRUE if the progress bar is inverted
 */
gboolean
gtk_progress_bar_get_inverted (GtkProgressBar *pbar)
{
  GtkProgressBarPrivate *priv = gtk_progress_bar_get_instance_private (pbar);

  g_return_val_if_fail (GTK_IS_PROGRESS_BAR (pbar), FALSE);

  return priv->inverted;
}

/**
 * gtk_progress_bar_set_ellipsize:
 * @pbar: a #GtkProgressBar
 * @mode: a #PangoEllipsizeMode
 *
 * Sets the mode used to ellipsize (add an ellipsis: "...") the
 * text if there is not enough space to render the entire string.
 */
void
gtk_progress_bar_set_ellipsize (GtkProgressBar     *pbar,
                                PangoEllipsizeMode  mode)
{
  GtkProgressBarPrivate *priv = gtk_progress_bar_get_instance_private (pbar);

  g_return_if_fail (GTK_IS_PROGRESS_BAR (pbar));
  g_return_if_fail (mode >= PANGO_ELLIPSIZE_NONE &&
                    mode <= PANGO_ELLIPSIZE_END);

  if ((PangoEllipsizeMode)priv->ellipsize != mode)
    {
      priv->ellipsize = mode;

      if (priv->label)
        gtk_label_set_ellipsize (GTK_LABEL (priv->label), mode);

      g_object_notify_by_pspec (G_OBJECT (pbar), progress_props[PROP_ELLIPSIZE]);
      gtk_widget_queue_resize (GTK_WIDGET (pbar));
    }
}

/**
 * gtk_progress_bar_get_ellipsize:
 * @pbar: a #GtkProgressBar
 *
 * Returns the ellipsizing position of the progress bar.
 * See gtk_progress_bar_set_ellipsize().
 *
 * Returns: #PangoEllipsizeMode
 */
PangoEllipsizeMode
gtk_progress_bar_get_ellipsize (GtkProgressBar *pbar)
{
  GtkProgressBarPrivate *priv = gtk_progress_bar_get_instance_private (pbar);

  g_return_val_if_fail (GTK_IS_PROGRESS_BAR (pbar), PANGO_ELLIPSIZE_NONE);

  return priv->ellipsize;
}
