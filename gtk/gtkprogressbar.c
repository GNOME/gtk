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

#include <math.h>
#include <string.h>

#include "gtkprogressbar.h"
#include "gtkorientableprivate.h"
#include "gtkprivate.h"
#include "gtkintl.h"
#include "gtkwidgetprivate.h"

#include "a11y/gtkprogressbaraccessible.h"

#include "actors/gtkbinlayoutprivate.h"
#include "actors/gtkcssboxprivate.h"
#include "actors/gtkcsstextprivate.h"

/**
 * SECTION:gtkprogressbar
 * @Short_description: A widget which indicates progress visually
 * @Title: GtkProgressBar
 *
 * The #GtkProgressBar is typically used to display the progress of a long
 * running operation.  It provides a visual clue that processing
 * is underway.  The #GtkProgressBar can be used in two different
 * modes: percentage mode and activity mode.
 *
 * When an application can determine how much work needs to take place
 * (e.g. read a fixed number of bytes from a file) and can monitor its
 * progress, it can use the #GtkProgressBar in percentage mode and the user
 * sees a growing bar indicating the percentage of the work that has
 * been completed.  In this mode, the application is required to call
 * gtk_progress_bar_set_fraction() periodically to update the progress bar.
 *
 * When an application has no accurate way of knowing the amount of work
 * to do, it can use the #GtkProgressBar in activity mode, which shows
 * activity by a block moving back and forth within the progress area. In
 * this mode, the application is required to call gtk_progress_bar_pulse()
 * periodically to update the progress bar.
 *
 * There is quite a bit of flexibility provided to control the appearance
 * of the #GtkProgressBar.  Functions are provided to control the
 * orientation of the bar, optional text can be displayed along with
 * the bar, and the step size used in activity mode can be set.
 */

#define MIN_HORIZONTAL_BAR_WIDTH   150
#define MIN_HORIZONTAL_BAR_HEIGHT  20
#define MIN_VERTICAL_BAR_WIDTH     22
#define MIN_VERTICAL_BAR_HEIGHT    80


struct _GtkProgressBarPrivate
{
  GtkActor      *slider;
  GtkActor      *text;

  gdouble        fraction;
  gdouble        pulse_fraction;

  double         activity_pos;
  guint          activity_blocks;

  GtkOrientation orientation;

  guint          activity_dir  : 1;
  guint          activity_mode : 1;
  guint          inverted      : 1;
  guint          autotext      : 1;
};

enum {
  PROP_0,
  PROP_FRACTION,
  PROP_PULSE_STEP,
  PROP_ORIENTATION,
  PROP_INVERTED,
  PROP_TEXT,
  PROP_SHOW_TEXT,
  PROP_ELLIPSIZE
};

static void gtk_progress_bar_set_property         (GObject        *object,
                                                   guint           prop_id,
                                                   const GValue   *value,
                                                   GParamSpec     *pspec);
static void gtk_progress_bar_get_property         (GObject        *object,
                                                   guint           prop_id,
                                                   GValue         *value,
                                                   GParamSpec     *pspec);

static void     gtk_progress_bar_real_update      (GtkProgressBar *progress);
static gboolean gtk_progress_bar_draw             (GtkWidget      *widget,
                                                   cairo_t        *cr);
static void     gtk_progress_bar_act_mode_enter   (GtkProgressBar *progress);
static void     gtk_progress_bar_set_orientation  (GtkProgressBar *progress,
                                                   GtkOrientation  orientation);

G_DEFINE_TYPE_WITH_CODE (GtkProgressBar, gtk_progress_bar, GTK_TYPE_WIDGET,
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

  widget_class->draw = gtk_progress_bar_draw;

  g_object_class_override_property (gobject_class, PROP_ORIENTATION, "orientation");

  g_object_class_install_property (gobject_class,
                                   PROP_INVERTED,
                                   g_param_spec_boolean ("inverted",
                                                         P_("Inverted"),
                                                         P_("Invert the direction in which the progress bar grows"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_FRACTION,
                                   g_param_spec_double ("fraction",
                                                        P_("Fraction"),
                                                        P_("The fraction of total work that has been completed"),
                                                        0.0, 1.0, 0.0,
                                                        GTK_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_PULSE_STEP,
                                   g_param_spec_double ("pulse-step",
                                                        P_("Pulse Step"),
                                                        P_("The fraction of total progress to move the bouncing block when pulsed"),
                                                        0.0, 1.0, 0.1,
                                                        GTK_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_TEXT,
                                   g_param_spec_string ("text",
                                                        P_("Text"),
                                                        P_("Text to be displayed in the progress bar"),
                                                        NULL,
                                                        GTK_PARAM_READWRITE));

  /**
   * GtkProgressBar:show-text:
   *
   * Sets whether the progress bar will show text superimposed
   * over the bar. The shown text is either the value of
   * the #GtkProgressBar:text property or, if that is %NULL,
   * the #GtkProgressBar:fraction value, as a percentage.
   *
   * To make a progress bar that is styled and sized suitably for containing
   * text (even if the actual text is blank), set #GtkProgressBar:show-text to
   * %TRUE and #GtkProgressBar:text to the empty string (not %NULL).
   *
   * Since: 3.0
   */
  g_object_class_install_property (gobject_class,
                                   PROP_SHOW_TEXT,
                                   g_param_spec_boolean ("show-text",
                                                         P_("Show text"),
                                                         P_("Whether the progress is shown as text."),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE));

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
   *
   * Since: 2.6
   */
  g_object_class_install_property (gobject_class,
                                   PROP_ELLIPSIZE,
                                   g_param_spec_enum ("ellipsize",
                                                      P_("Ellipsize"),
                                                      P_("The preferred place to ellipsize the string, if the progress bar "
                                                         "does not have enough room to display the entire string, if at all."),
                                                      PANGO_TYPE_ELLIPSIZE_MODE,
                                                      PANGO_ELLIPSIZE_NONE,
                                                      GTK_PARAM_READWRITE));
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_int ("xspacing",
                                                             P_("X spacing"),
                                                             P_("Extra spacing applied to the width of a progress bar."),
                                                             0, G_MAXINT, 7,
                                                             G_PARAM_READWRITE));
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_int ("yspacing",
                                                             P_("Y spacing"),
                                                             P_("Extra spacing applied to the height of a progress bar."),
                                                             0, G_MAXINT, 7,
                                                             G_PARAM_READWRITE));

  /**
   * GtkProgressBar:min-horizontal-bar-width:
   *
   * The minimum horizontal width of the progress bar.
   *
   * Since: 2.14
   */
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_int ("min-horizontal-bar-width",
                                                             P_("Minimum horizontal bar width"),
                                                             P_("The minimum horizontal width of the progress bar"),
                                                             1, G_MAXINT, MIN_HORIZONTAL_BAR_WIDTH,
                                                             G_PARAM_READWRITE));
  /**
   * GtkProgressBar:min-horizontal-bar-height:
   *
   * Minimum horizontal height of the progress bar.
   *
   * Since: 2.14
   */
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_int ("min-horizontal-bar-height",
                                                             P_("Minimum horizontal bar height"),
                                                             P_("Minimum horizontal height of the progress bar"),
                                                             1, G_MAXINT, MIN_HORIZONTAL_BAR_HEIGHT,
                                                             G_PARAM_READWRITE));
  /**
   * GtkProgressBar:min-vertical-bar-width:
   *
   * The minimum vertical width of the progress bar.
   *
   * Since: 2.14
   */
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_int ("min-vertical-bar-width",
                                                             P_("Minimum vertical bar width"),
                                                             P_("The minimum vertical width of the progress bar"),
                                                             1, G_MAXINT, MIN_VERTICAL_BAR_WIDTH,
                                                             G_PARAM_READWRITE));
  /**
   * GtkProgressBar:min-vertical-bar-height:
   *
   * The minimum vertical height of the progress bar.
   *
   * Since: 2.14
   */
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_int ("min-vertical-bar-height",
                                                             P_("Minimum vertical bar height"),
                                                             P_("The minimum vertical height of the progress bar"),
                                                             1, G_MAXINT, MIN_VERTICAL_BAR_HEIGHT,
                                                             G_PARAM_READWRITE));

  g_type_class_add_private (class, sizeof (GtkProgressBarPrivate));

  gtk_widget_class_set_accessible_type (widget_class, GTK_TYPE_PROGRESS_BAR_ACCESSIBLE);
}

static void
gtk_progress_bar_init (GtkProgressBar *pbar)
{
  GtkProgressBarPrivate *priv;
  GtkActor *trough;

  pbar->priv = G_TYPE_INSTANCE_GET_PRIVATE (pbar,
                                            GTK_TYPE_PROGRESS_BAR,
                                            GtkProgressBarPrivate);
  priv = pbar->priv;

  priv->orientation = GTK_ORIENTATION_HORIZONTAL;
  priv->inverted = FALSE;
  priv->pulse_fraction = 0.1;
  priv->activity_pos = 0;
  priv->activity_dir = 1;
  priv->activity_blocks = 5;

  priv->fraction = 0.0;

  gtk_widget_set_has_window (GTK_WIDGET (pbar), FALSE);

  trough = _gtk_widget_get_actor (GTK_WIDGET (pbar));
  _gtk_actor_set_layout_manager (trough, _gtk_bin_layout_new ());
  _gtk_css_box_add_class (GTK_CSS_BOX (trough), GTK_STYLE_CLASS_TROUGH);
  _gtk_css_box_add_class (GTK_CSS_BOX (trough), GTK_STYLE_CLASS_HORIZONTAL);
  priv->slider = _gtk_css_box_new ();
  _gtk_css_box_add_class (GTK_CSS_BOX (priv->slider), GTK_STYLE_CLASS_PROGRESSBAR);
  _gtk_actor_add_child (trough, priv->slider);
  priv->text = _gtk_css_text_new ();
  _gtk_css_text_set_text_alignment (GTK_CSS_TEXT (priv->text), PANGO_ALIGN_CENTER);
  _gtk_actor_hide (priv->text);
  _gtk_actor_add_child (trough, priv->text);
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
  GtkProgressBarPrivate* priv = pbar->priv;

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
      g_value_set_string (value, _gtk_css_text_get_text (GTK_CSS_TEXT (priv->text)));
      break;
    case PROP_SHOW_TEXT:
      g_value_set_boolean (value, _gtk_actor_get_visible (priv->text));
      break;
    case PROP_ELLIPSIZE:
      g_value_set_enum (value, _gtk_css_text_get_ellipsize (GTK_CSS_TEXT (priv->text)));
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
gtk_progress_bar_update_position (GtkProgressBar *pbar)
{
  GtkProgressBarPrivate *priv;
  GtkLayoutManager *layout;
  double scale, align;
  gboolean inverted;

  priv = pbar->priv;
  layout = _gtk_actor_get_layout_manager (_gtk_widget_get_actor (GTK_WIDGET (pbar)));
      
  inverted = priv->inverted;
  if (priv->orientation == GTK_ORIENTATION_HORIZONTAL &&
      gtk_widget_get_direction (GTK_WIDGET (pbar)) == GTK_TEXT_DIR_RTL)
    inverted = !inverted;

  if (priv->activity_mode)
    {
      align = priv->activity_pos;
      scale = 1.0 / priv->activity_blocks;
    }
  else
    {
      align = inverted ? 1.0 : 0.0;
      scale = priv->fraction;
    }

  if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      _gtk_bin_layout_set_child_alignment (GTK_BIN_LAYOUT (layout),
                                           priv->slider,
                                           align,
                                           0.0,
                                           scale,
                                           1.0);
    }
  else
    {
      _gtk_bin_layout_set_child_alignment (GTK_BIN_LAYOUT (layout),
                                           priv->slider,
                                           0.0,
                                           align,
                                           1.0,
                                           scale);
    }
}

static void
gtk_progress_bar_update_text (GtkProgressBar *pbar)
{
  GtkProgressBarPrivate *priv = pbar->priv;
  char *s;

  if (!priv->autotext)
    return;

  s = g_strdup_printf ("%.0f %%", priv->fraction * 100.0);
  _gtk_css_text_set_text (GTK_CSS_TEXT (priv->text), s);
  g_free (s);
}

static void
gtk_progress_bar_real_update (GtkProgressBar *pbar)
{
  GtkProgressBarPrivate *priv;

  g_return_if_fail (GTK_IS_PROGRESS_BAR (pbar));

  priv = pbar->priv;

  if (priv->activity_mode)
    {
      /* advance the block */
      if (priv->activity_dir == 0)
        {
          priv->activity_pos += priv->pulse_fraction;
          if (priv->activity_pos > 1.0)
            {
              priv->activity_pos = 1.0;
              priv->activity_dir = 1;
            }
        }
      else
        {
          priv->activity_pos -= priv->pulse_fraction;
          if (priv->activity_pos <= 0)
            {
              priv->activity_pos = 0;
              priv->activity_dir = 0;
            }
        }
    }

  gtk_progress_bar_update_position (pbar);
}

static void
gtk_progress_bar_act_mode_enter (GtkProgressBar *pbar)
{
  GtkProgressBarPrivate *priv = pbar->priv;
  GtkWidget *widget = GTK_WIDGET (pbar);
  GtkOrientation orientation;
  gboolean inverted;

  orientation = priv->orientation;
  inverted = priv->inverted;
  if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
    {
      if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
        inverted = !inverted;
    }

  /* calculate start pos */

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
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
    }
  else
    {
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
    }
}

static gboolean
gtk_progress_bar_draw (GtkWidget      *widget,
                       cairo_t        *cr)
{
  cairo_reset_clip (cr);
  _gtk_actor_draw (_gtk_widget_get_actor (widget), cr);
  return FALSE;
}

static void
gtk_progress_bar_set_activity_mode (GtkProgressBar *pbar,
                                    gboolean        activity_mode)
{
  GtkProgressBarPrivate *priv = pbar->priv;

  activity_mode = !!activity_mode;

  if (priv->activity_mode != activity_mode)
    {
      priv->activity_mode = activity_mode;

      if (priv->activity_mode)
        gtk_progress_bar_act_mode_enter (pbar);

      gtk_progress_bar_update_position (pbar);
    }
}

/**
 * gtk_progress_bar_set_fraction:
 * @pbar: a #GtkProgressBar
 * @fraction: fraction of the task that's been completed
 *
 * Causes the progress bar to "fill in" the given fraction
 * of the bar. The fraction should be between 0.0 and 1.0,
 * inclusive.
 */
void
gtk_progress_bar_set_fraction (GtkProgressBar *pbar,
                               gdouble         fraction)
{
  GtkProgressBarPrivate* priv;

  g_return_if_fail (GTK_IS_PROGRESS_BAR (pbar));

  priv = pbar->priv;

  priv->fraction = CLAMP(fraction, 0.0, 1.0);
  gtk_progress_bar_set_activity_mode (pbar, FALSE);
  gtk_progress_bar_update_text (pbar);
  gtk_progress_bar_update_position (pbar);

  g_object_notify (G_OBJECT (pbar), "fraction");
}

/**
 * gtk_progress_bar_pulse:
 * @pbar: a #GtkProgressBar
 *
 * Indicates that some progress has been made, but you don't know how much.
 * Causes the progress bar to enter "activity mode," where a block
 * bounces back and forth. Each call to gtk_progress_bar_pulse()
 * causes the block to move by a little bit (the amount of movement
 * per pulse is determined by gtk_progress_bar_set_pulse_step()).
 */
void
gtk_progress_bar_pulse (GtkProgressBar *pbar)
{
  g_return_if_fail (GTK_IS_PROGRESS_BAR (pbar));

  gtk_progress_bar_set_activity_mode (pbar, TRUE);
  gtk_progress_bar_real_update (pbar);
}

/**
 * gtk_progress_bar_set_text:
 * @pbar: a #GtkProgressBar
 * @text: (allow-none): a UTF-8 string, or %NULL
 *
 * Causes the given @text to appear superimposed on the progress bar.
 *
 * If @text is %NULL and #GtkProgressBar:show-text is %TRUE, the current
 * value of #GtkProgressBar:fraction will be displayed as a percentage.
 *
 * If @text is non-%NULL and #GtkProgressBar:show-text is %TRUE, the text will
 * be displayed. In this case, it will not display the progress percentage.
 * If @text is the empty string, the progress bar will still be styled and sized
 * suitably for containing text, as long as #GtkProgressBar:show-text is %TRUE.
 */
void
gtk_progress_bar_set_text (GtkProgressBar *pbar,
                           const gchar    *text)
{
  GtkProgressBarPrivate *priv;

  g_return_if_fail (GTK_IS_PROGRESS_BAR (pbar));

  priv = pbar->priv;

  /* Don't notify again if nothing's changed. */
  if (g_strcmp0 (_gtk_css_text_get_text (GTK_CSS_TEXT (priv->text)), text) == 0)
    return;

  if (text)
    {
      _gtk_css_text_set_text (GTK_CSS_TEXT (priv->text), text);
      priv->autotext = FALSE;
    }
  else
    {
      priv->autotext = TRUE;
      gtk_progress_bar_update_text (pbar);
    }

  g_object_notify (G_OBJECT (pbar), "text");
}

/**
 * gtk_progress_bar_set_show_text:
 * @pbar: a #GtkProgressBar
 * @show_text: whether to show superimposed text
 *
 * Sets whether the progress bar will show text superimposed
 * over the bar. The shown text is either the value of
 * the #GtkProgressBar:text property or, if that is %NULL,
 * the #GtkProgressBar:fraction value, as a percentage.
 *
 * To make a progress bar that is styled and sized suitably for containing
 * text (even if the actual text is blank), set #GtkProgressBar:show-text to
 * %TRUE and #GtkProgressBar:text to the empty string (not %NULL).
 *
 * Since: 3.0
 */
void
gtk_progress_bar_set_show_text (GtkProgressBar *pbar,
                                gboolean        show_text)
{
  GtkProgressBarPrivate *priv;

  g_return_if_fail (GTK_IS_PROGRESS_BAR (pbar));

  priv = pbar->priv;

  show_text = !!show_text;

  if (_gtk_actor_get_visible (priv->text) != show_text)
    {
      _gtk_actor_set_visible (priv->text, show_text);

      g_object_notify (G_OBJECT (pbar), "show-text");
    }
}

/**
 * gtk_progress_bar_get_show_text:
 * @pbar: a #GtkProgressBar
 *
 * Gets the value of the #GtkProgressBar:show-text property.
 * See gtk_progress_bar_set_show_text().
 *
 * Returns: %TRUE if text is shown in the progress bar
 *
 * Since: 3.0
 */
gboolean
gtk_progress_bar_get_show_text (GtkProgressBar *pbar)
{
  g_return_val_if_fail (GTK_IS_PROGRESS_BAR (pbar), FALSE);

  return _gtk_actor_get_visible (pbar->priv->text);
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
  GtkProgressBarPrivate *priv;

  g_return_if_fail (GTK_IS_PROGRESS_BAR (pbar));

  priv = pbar->priv;

  priv->pulse_fraction = fraction;

  g_object_notify (G_OBJECT (pbar), "pulse-step");
}

static void
gtk_progress_bar_set_orientation (GtkProgressBar *pbar,
                                  GtkOrientation  orientation)
{
  GtkProgressBarPrivate *priv = pbar->priv;

  if (priv->orientation != orientation)
    {
      GtkActor *actor = _gtk_widget_get_actor (GTK_WIDGET (pbar));

      priv->orientation = orientation;
      _gtk_orientable_set_style_classes (GTK_ORIENTABLE (pbar));
      if (orientation == GTK_ORIENTATION_HORIZONTAL)
        {
          _gtk_css_box_add_class (GTK_CSS_BOX (actor), GTK_STYLE_CLASS_HORIZONTAL);
          _gtk_css_box_remove_class (GTK_CSS_BOX (actor), GTK_STYLE_CLASS_VERTICAL);
        }
      else
        {
          _gtk_css_box_add_class (GTK_CSS_BOX (actor), GTK_STYLE_CLASS_VERTICAL);
          _gtk_css_box_remove_class (GTK_CSS_BOX (actor), GTK_STYLE_CLASS_HORIZONTAL);
        }

      gtk_widget_queue_resize (GTK_WIDGET (pbar));
    }
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
  GtkProgressBarPrivate *priv;

  g_return_if_fail (GTK_IS_PROGRESS_BAR (pbar));

  priv = pbar->priv;

  if (priv->inverted != inverted)
    {
      priv->inverted = inverted;

      gtk_progress_bar_update_position (pbar);

      g_object_notify (G_OBJECT (pbar), "inverted");
    }
}

/**
 * gtk_progress_bar_get_text:
 * @pbar: a #GtkProgressBar
 *
 * Retrieves the text displayed superimposed on the progress bar,
 * if any, otherwise %NULL. The return value is a reference
 * to the text, not a copy of it, so will become invalid
 * if you change the text in the progress bar.
 *
 * Return value: text, or %NULL; this string is owned by the widget
 * and should not be modified or freed.
 */
const gchar*
gtk_progress_bar_get_text (GtkProgressBar *pbar)
{
  g_return_val_if_fail (GTK_IS_PROGRESS_BAR (pbar), NULL);

  return _gtk_css_text_get_text (GTK_CSS_TEXT (pbar->priv->text));
}

/**
 * gtk_progress_bar_get_fraction:
 * @pbar: a #GtkProgressBar
 *
 * Returns the current fraction of the task that's been completed.
 *
 * Return value: a fraction from 0.0 to 1.0
 */
gdouble
gtk_progress_bar_get_fraction (GtkProgressBar *pbar)
{
  g_return_val_if_fail (GTK_IS_PROGRESS_BAR (pbar), 0);

  return pbar->priv->fraction;
}

/**
 * gtk_progress_bar_get_pulse_step:
 * @pbar: a #GtkProgressBar
 *
 * Retrieves the pulse step set with gtk_progress_bar_set_pulse_step().
 *
 * Return value: a fraction from 0.0 to 1.0
 */
gdouble
gtk_progress_bar_get_pulse_step (GtkProgressBar *pbar)
{
  g_return_val_if_fail (GTK_IS_PROGRESS_BAR (pbar), 0);

  return pbar->priv->pulse_fraction;
}

/**
 * gtk_progress_bar_get_inverted:
 * @pbar: a #GtkProgressBar
 *
 * Gets the value set by gtk_progress_bar_set_inverted().
 *
 * Return value: %TRUE if the progress bar is inverted
 */
gboolean
gtk_progress_bar_get_inverted (GtkProgressBar *pbar)
{
  g_return_val_if_fail (GTK_IS_PROGRESS_BAR (pbar), FALSE);

  return pbar->priv->inverted;
}

/**
 * gtk_progress_bar_set_ellipsize:
 * @pbar: a #GtkProgressBar
 * @mode: a #PangoEllipsizeMode
 *
 * Sets the mode used to ellipsize (add an ellipsis: "...") the text
 * if there is not enough space to render the entire string.
 *
 * Since: 2.6
 */
void
gtk_progress_bar_set_ellipsize (GtkProgressBar     *pbar,
                                PangoEllipsizeMode  mode)
{
  GtkProgressBarPrivate *priv;

  g_return_if_fail (GTK_IS_PROGRESS_BAR (pbar));
  g_return_if_fail (mode >= PANGO_ELLIPSIZE_NONE &&
                    mode <= PANGO_ELLIPSIZE_END);

  priv = pbar->priv;

  if (_gtk_css_text_get_ellipsize (GTK_CSS_TEXT (priv->text)) != mode)
    {
      _gtk_css_text_set_ellipsize (GTK_CSS_TEXT (priv->text), mode);

      g_object_notify (G_OBJECT (pbar), "ellipsize");
    }
}

/**
 * gtk_progress_bar_get_ellipsize:
 * @pbar: a #GtkProgressBar
 *
 * Returns the ellipsizing position of the progress bar.
 * See gtk_progress_bar_set_ellipsize().
 *
 * Return value: #PangoEllipsizeMode
 *
 * Since: 2.6
 */
PangoEllipsizeMode
gtk_progress_bar_get_ellipsize (GtkProgressBar *pbar)
{
  g_return_val_if_fail (GTK_IS_PROGRESS_BAR (pbar), PANGO_ELLIPSIZE_NONE);

  return _gtk_css_text_get_ellipsize (GTK_CSS_TEXT (pbar->priv->text));
}
