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

#include <string.h>

#include "gtkprogressbar.h"
#include "gtkorientableprivate.h"
#include "gtkwidgetprivate.h"
#include "gtkprivate.h"
#include "gtkintl.h"
#include "gtkcssshadowsvalueprivate.h"
#include "gtkstylecontextprivate.h"
#include "gtkcssnodeprivate.h"
#include "gtkcssstylepropertyprivate.h"
#include "gtkcsscustomgadgetprivate.h"
#include "gtkcssnumbervalueprivate.h"
#include "gtkprogresstrackerprivate.h"

#include "a11y/gtkprogressbaraccessible.h"

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

#define MIN_HORIZONTAL_BAR_WIDTH   150
#define MIN_HORIZONTAL_BAR_HEIGHT  6
#define MIN_VERTICAL_BAR_WIDTH     7
#define MIN_VERTICAL_BAR_HEIGHT    80

#define DEFAULT_PULSE_DURATION     250000000

struct _GtkProgressBarPrivate
{
  gchar         *text;

  GtkCssGadget  *gadget;
  GtkCssGadget  *text_gadget;
  GtkCssGadget  *trough_gadget;
  GtkCssGadget  *progress_gadget;

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
};

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
static void gtk_progress_bar_size_allocate        (GtkWidget      *widget,
                                                   GtkAllocation  *allocation);
static void gtk_progress_bar_get_preferred_width  (GtkWidget      *widget,
                                                   gint           *minimum,
                                                   gint           *natural);
static void gtk_progress_bar_get_preferred_height (GtkWidget      *widget,
                                                   gint           *minimum,
                                                   gint           *natural);

static gboolean gtk_progress_bar_draw             (GtkWidget      *widget,
                                                   cairo_t        *cr);
static void     gtk_progress_bar_act_mode_enter   (GtkProgressBar *progress);
static void     gtk_progress_bar_act_mode_leave   (GtkProgressBar *progress);
static void     gtk_progress_bar_finalize         (GObject        *object);
static void     gtk_progress_bar_set_orientation  (GtkProgressBar *progress,
                                                   GtkOrientation  orientation);
static void     gtk_progress_bar_direction_changed (GtkWidget        *widget,
                                                    GtkTextDirection  previous_dir);
static void     gtk_progress_bar_state_flags_changed (GtkWidget      *widget,
                                                      GtkStateFlags   previous_state);

static void     gtk_progress_bar_measure           (GtkCssGadget        *gadget,
                                                    GtkOrientation       orientation,
                                                    gint                 for_size,
                                                    gint                *minimum,
                                                    gint                *natural,
                                                    gint                *minimum_baseline,
                                                    gint                *natural_baseline,
                                                    gpointer             data);
static void     gtk_progress_bar_allocate          (GtkCssGadget        *gadget,
                                                    const GtkAllocation *allocation,
                                                    gint                 baseline,
                                                    GtkAllocation       *out_clip,
                                                    gpointer             data);
static gboolean gtk_progress_bar_render            (GtkCssGadget        *gadget,
                                                    cairo_t             *cr,
                                                    gint                 x,
                                                    gint                 y,
                                                    gint                 width,
                                                    gint                 height,
                                                    gpointer             data);
static void     gtk_progress_bar_allocate_trough   (GtkCssGadget        *gadget,
                                                    const GtkAllocation *allocation,
                                                    gint                 baseline,
                                                    GtkAllocation       *out_clip,
                                                    gpointer             data);
static void     gtk_progress_bar_measure_trough    (GtkCssGadget        *gadget,
                                                    GtkOrientation       orientation,
                                                    gint                 for_size,
                                                    gint                *minimum,
                                                    gint                *natural,
                                                    gint                *minimum_baseline,
                                                    gint                *natural_baseline,
                                                    gpointer             data);
static gboolean gtk_progress_bar_render_trough     (GtkCssGadget        *gadget,
                                                    cairo_t             *cr,
                                                    gint                 x,
                                                    gint                 y,
                                                    gint                 width,
                                                    gint                 height,
                                                    gpointer             data);
static void     gtk_progress_bar_measure_progress  (GtkCssGadget        *gadget,
                                                    GtkOrientation       orientation,
                                                    gint                 for_size,
                                                    gint                *minimum,
                                                    gint                *natural,
                                                    gint                *minimum_baseline,
                                                    gint                *natural_baseline,
                                                    gpointer             data);
static void     gtk_progress_bar_measure_text      (GtkCssGadget        *gadget,
                                                    GtkOrientation       orientation,
                                                    gint                 for_size,
                                                    gint                *minimum,
                                                    gint                *natural,
                                                    gint                *minimum_baseline,
                                                    gint                *natural_baseline,
                                                    gpointer             data);
static gboolean gtk_progress_bar_render_text       (GtkCssGadget        *gadget,
                                                    cairo_t             *cr,
                                                    gint                 x,
                                                    gint                 y,
                                                    gint                 width,
                                                    gint                 height,
                                                    gpointer             data);

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

  widget_class->draw = gtk_progress_bar_draw;
  widget_class->size_allocate = gtk_progress_bar_size_allocate;
  widget_class->get_preferred_width = gtk_progress_bar_get_preferred_width;
  widget_class->get_preferred_height = gtk_progress_bar_get_preferred_height;
  widget_class->direction_changed = gtk_progress_bar_direction_changed;
  widget_class->state_flags_changed = gtk_progress_bar_state_flags_changed;

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
   *
   * Since: 3.0
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
   *
   * Since: 2.6
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

  /**
   * GtkProgressBar:xspacing:
   *
   * Extra spacing applied to the width of a progress bar.
   *
   * Deprecated: 3.20: Use the standard CSS padding and margins; the
   *     value of this style property is ignored.
   */
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_int ("xspacing",
                                                             P_("X spacing"),
                                                             P_("Extra spacing applied to the width of a progress bar."),
                                                             0, G_MAXINT, 2,
                                                             G_PARAM_READWRITE|G_PARAM_DEPRECATED));

  /**
   * GtkProgressBar:yspacing:
   *
   * Extra spacing applied to the height of a progress bar.
   *
   * Deprecated: 3.20: Use the standard CSS padding and margins; the
   *     value of this style property is ignored.
   */
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_int ("yspacing",
                                                             P_("Y spacing"),
                                                             P_("Extra spacing applied to the height of a progress bar."),
                                                             0, G_MAXINT, 2,
                                                             G_PARAM_READWRITE|G_PARAM_DEPRECATED));

  /**
   * GtkProgressBar:min-horizontal-bar-width:
   *
   * The minimum horizontal width of the progress bar.
   *
   * Since: 2.14
   *
   * Deprecated: 3.20: Use the standard CSS property min-width.
   */
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_int ("min-horizontal-bar-width",
                                                             P_("Minimum horizontal bar width"),
                                                             P_("The minimum horizontal width of the progress bar"),
                                                             1, G_MAXINT, MIN_HORIZONTAL_BAR_WIDTH,
                                                             G_PARAM_READWRITE|G_PARAM_DEPRECATED));
  /**
   * GtkProgressBar:min-horizontal-bar-height:
   *
   * Minimum horizontal height of the progress bar.
   *
   * Since: 2.14
   *
   * Deprecated: 3.20: Use the standard CSS property min-height.
   */
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_int ("min-horizontal-bar-height",
                                                             P_("Minimum horizontal bar height"),
                                                             P_("Minimum horizontal height of the progress bar"),
                                                             1, G_MAXINT, MIN_HORIZONTAL_BAR_HEIGHT,
                                                             G_PARAM_READWRITE|G_PARAM_DEPRECATED));
  /**
   * GtkProgressBar:min-vertical-bar-width:
   *
   * The minimum vertical width of the progress bar.
   *
   * Since: 2.14
   *
   * Deprecated: 3.20: Use the standard CSS property min-width.
   */
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_int ("min-vertical-bar-width",
                                                             P_("Minimum vertical bar width"),
                                                             P_("The minimum vertical width of the progress bar"),
                                                             1, G_MAXINT, MIN_VERTICAL_BAR_WIDTH,
                                                             G_PARAM_READWRITE|G_PARAM_DEPRECATED));
  /**
   * GtkProgressBar:min-vertical-bar-height:
   *
   * The minimum vertical height of the progress bar.
   *
   * Since: 2.14
   *
   * Deprecated: 3.20: Use the standard CSS property min-height.
   */
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_int ("min-vertical-bar-height",
                                                             P_("Minimum vertical bar height"),
                                                             P_("The minimum vertical height of the progress bar"),
                                                             1, G_MAXINT, MIN_VERTICAL_BAR_HEIGHT,
                                                             G_PARAM_READWRITE|G_PARAM_DEPRECATED));

  gtk_widget_class_set_accessible_type (widget_class, GTK_TYPE_PROGRESS_BAR_ACCESSIBLE);
  gtk_widget_class_set_css_name (widget_class, "progressbar");
}

static void
update_fraction_classes (GtkProgressBar *pbar)
{
  GtkProgressBarPrivate *priv = pbar->priv;
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

  if (empty)
    gtk_css_gadget_add_class (priv->trough_gadget, "empty");
  else
    gtk_css_gadget_remove_class (priv->trough_gadget, "empty");

  if (full)
    gtk_css_gadget_add_class (priv->trough_gadget, "full");
  else
    gtk_css_gadget_remove_class (priv->trough_gadget, "full");
}

static void
update_node_classes (GtkProgressBar *pbar)
{
  GtkProgressBarPrivate *priv = pbar->priv;
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

  if (left)
    gtk_css_gadget_add_class (priv->progress_gadget, GTK_STYLE_CLASS_LEFT);
  else
    gtk_css_gadget_remove_class (priv->progress_gadget, GTK_STYLE_CLASS_LEFT);

  if (right)
    gtk_css_gadget_add_class (priv->progress_gadget, GTK_STYLE_CLASS_RIGHT);
  else
    gtk_css_gadget_remove_class (priv->progress_gadget, GTK_STYLE_CLASS_RIGHT);

  if (top)
    gtk_css_gadget_add_class (priv->progress_gadget, GTK_STYLE_CLASS_TOP);
  else
    gtk_css_gadget_remove_class (priv->progress_gadget, GTK_STYLE_CLASS_TOP);

  if (bottom)
    gtk_css_gadget_add_class (priv->progress_gadget, GTK_STYLE_CLASS_BOTTOM);
  else
    gtk_css_gadget_remove_class (priv->progress_gadget, GTK_STYLE_CLASS_BOTTOM);

  update_fraction_classes (pbar);
}

static void
update_node_state (GtkProgressBar *pbar)
{
  GtkProgressBarPrivate *priv = pbar->priv;
  GtkStateFlags state;

  state = gtk_widget_get_state_flags (GTK_WIDGET (pbar));

  gtk_css_gadget_set_state (priv->gadget, state);
  gtk_css_gadget_set_state (priv->trough_gadget, state);
  gtk_css_gadget_set_state (priv->progress_gadget, state);
  if (priv->text_gadget)
    gtk_css_gadget_set_state (priv->text_gadget, state);
}

static void
gtk_progress_bar_init (GtkProgressBar *pbar)
{
  GtkProgressBarPrivate *priv;
  GtkCssNode *widget_node;

  pbar->priv = gtk_progress_bar_get_instance_private (pbar);
  priv = pbar->priv;

  priv->orientation = GTK_ORIENTATION_HORIZONTAL;
  priv->inverted = FALSE;
  priv->pulse_fraction = 0.1;
  priv->activity_pos = 0;
  priv->activity_dir = 1;
  priv->activity_blocks = 5;
  priv->ellipsize = PANGO_ELLIPSIZE_NONE;
  priv->show_text = FALSE;

  priv->text = NULL;
  priv->fraction = 0.0;

  gtk_widget_set_has_window (GTK_WIDGET (pbar), FALSE);

  _gtk_orientable_set_style_classes (GTK_ORIENTABLE (pbar));

  widget_node = gtk_widget_get_css_node (GTK_WIDGET (pbar));
  priv->gadget = gtk_css_custom_gadget_new_for_node (widget_node,
                                                     GTK_WIDGET (pbar),
                                                     gtk_progress_bar_measure,
                                                     gtk_progress_bar_allocate,
                                                     gtk_progress_bar_render,
                                                     NULL,
                                                     NULL);

  priv->trough_gadget = gtk_css_custom_gadget_new ("trough",
                                                   GTK_WIDGET (pbar),
                                                   priv->gadget,
                                                   NULL,
                                                   gtk_progress_bar_measure_trough,
                                                   gtk_progress_bar_allocate_trough,
                                                   gtk_progress_bar_render_trough,
                                                   NULL,
                                                   NULL);

  priv->progress_gadget = gtk_css_custom_gadget_new ("progress",
                                                     GTK_WIDGET (pbar),
                                                     priv->trough_gadget,
                                                     NULL,
                                                     gtk_progress_bar_measure_progress,
                                                     NULL,
                                                     NULL,
                                                     NULL,
                                                     NULL);

  update_node_state (pbar);
  update_node_classes (pbar);
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
  GtkProgressBarPrivate *priv = pbar->priv;

  if (priv->activity_mode)
    gtk_progress_bar_act_mode_leave (pbar);

  g_free (priv->text);

  g_clear_object (&priv->text_gadget);
  g_clear_object (&priv->progress_gadget);
  g_clear_object (&priv->trough_gadget);
  g_clear_object (&priv->gadget);

  G_OBJECT_CLASS (gtk_progress_bar_parent_class)->finalize (object);
}

static gchar *
get_current_text (GtkProgressBar *pbar)
{
  GtkProgressBarPrivate *priv = pbar->priv;

  if (priv->text)
    return g_strdup (priv->text);
  else
    return g_strdup_printf (C_("progress bar label", "%.0f %%"), priv->fraction * 100.0);
}

static void
gtk_progress_bar_measure (GtkCssGadget   *gadget,
                          GtkOrientation  orientation,
                          int             for_size,
                          int            *minimum,
                          int            *natural,
                          int            *minimum_baseline,
                          int            *natural_baseline,
                          gpointer        data)
{
  GtkWidget *widget;
  GtkProgressBar *pbar;
  GtkProgressBarPrivate *priv;
  gint text_minimum, text_natural;
  gint trough_minimum, trough_natural;

  widget = gtk_css_gadget_get_owner (gadget);
  pbar = GTK_PROGRESS_BAR (widget);
  priv = pbar->priv;

  if (priv->show_text)
    gtk_css_gadget_get_preferred_size (priv->text_gadget,
                                       orientation,
                                       -1,
                                       &text_minimum, &text_natural,
                                       NULL, NULL);
  else
    text_minimum = text_natural = 0;

  gtk_css_gadget_get_preferred_size (priv->trough_gadget,
                                     orientation,
                                     -1,
                                     &trough_minimum, &trough_natural,
                                     NULL, NULL);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
        {
          *minimum = MAX (text_minimum, trough_minimum);
          *natural = MAX (text_natural, trough_natural);
        }
      else
        {
          *minimum = text_minimum + trough_minimum;
          *natural = text_natural + trough_natural;
        }
    }
  else
    {
      if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
        {
          *minimum = text_minimum + trough_minimum;
          *natural = text_natural + trough_natural;
        }
      else
        {
          *minimum = MAX (text_minimum, trough_minimum);
          *natural = MAX (text_natural, trough_natural);
        }
    }
}

static PangoLayout *
gtk_progress_bar_get_layout (GtkProgressBar *pbar)
{
  PangoLayout *layout;
  gchar *buf;
  GtkCssStyle *style;
  PangoAttrList *attrs;
  PangoFontDescription *desc;

  buf = get_current_text (pbar);
  layout = gtk_widget_create_pango_layout (GTK_WIDGET (pbar), buf);

  style = gtk_css_node_get_style (gtk_css_gadget_get_node (pbar->priv->text_gadget));

  attrs = gtk_css_style_get_pango_attributes (style);
  desc = gtk_css_style_get_pango_font (style);

  pango_layout_set_attributes (layout, attrs);
  pango_layout_set_font_description (layout, desc);

  if (attrs)
    pango_attr_list_unref (attrs);
  pango_font_description_free (desc);

  g_free (buf);

  return layout;
}

static void
gtk_progress_bar_measure_text (GtkCssGadget   *gadget,
                               GtkOrientation  orientation,
                               int             for_size,
                               int            *minimum,
                               int            *natural,
                               int            *minimum_baseline,
                               int            *natural_baseline,
                               gpointer        data)
{
  GtkWidget *widget;
  GtkProgressBar *pbar;
  GtkProgressBarPrivate *priv;
  PangoLayout *layout;
  PangoRectangle logical_rect;

  widget = gtk_css_gadget_get_owner (gadget);
  pbar = GTK_PROGRESS_BAR (widget);
  priv = pbar->priv;

  layout = gtk_progress_bar_get_layout (pbar);

  pango_layout_get_pixel_extents (layout, NULL, &logical_rect);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      if (priv->ellipsize)
        {
          PangoContext *context;
          PangoFontMetrics *metrics;
          gint char_width;

          /* The minimum size for ellipsized text is ~ 3 chars */
          context = pango_layout_get_context (layout);
          metrics = pango_context_get_metrics (context,
                                               pango_layout_get_font_description (layout),
                                               pango_context_get_language (context));

          char_width = pango_font_metrics_get_approximate_char_width (metrics);
          pango_font_metrics_unref (metrics);

          *minimum = PANGO_PIXELS (char_width) * 3;
        }
      else
        *minimum = logical_rect.width;

      *natural = MAX (*minimum, logical_rect.width);
    }
  else
    *minimum = *natural = logical_rect.height;

  g_object_unref (layout);
}

static gint
get_number (GtkCssStyle *style,
            guint        property)
{
  double d = _gtk_css_number_value_get (gtk_css_style_get_value (style, property), 100.0);

  if (d < 1)
    return ceil (d);
  else
    return floor (d);
}

static void
gtk_progress_bar_measure_trough (GtkCssGadget   *gadget,
                                 GtkOrientation  orientation,
                                 int             for_size,
                                 int            *minimum,
                                 int            *natural,
                                 int            *minimum_baseline,
                                 int            *natural_baseline,
                                 gpointer        data)
{
  GtkWidget *widget;
  GtkProgressBarPrivate *priv;
  GtkCssStyle *style;

  widget = gtk_css_gadget_get_owner (gadget);
  priv = GTK_PROGRESS_BAR (widget)->priv;

  style = gtk_css_gadget_get_style (gadget);
  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      gdouble min_width;

      min_width = _gtk_css_number_value_get (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_MIN_WIDTH), 100.0);

      if (min_width > 0.0)
        *minimum = 0;
      else if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
        gtk_widget_style_get (widget, "min-horizontal-bar-width", minimum, NULL);
      else
        gtk_widget_style_get (widget, "min-vertical-bar-width", minimum, NULL);
    }
  else
    {
      gdouble min_height;

      min_height = _gtk_css_number_value_get (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_MIN_HEIGHT), 100.0);

      if (min_height > 0.0)
        *minimum = 0;
      else if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
        gtk_widget_style_get (widget, "min-horizontal-bar-height", minimum, NULL);
      else
        gtk_widget_style_get (widget, "min-vertical-bar-height", minimum, NULL);
    }

  *natural = *minimum;

  if (minimum_baseline)
    *minimum_baseline = -1;
  if (natural_baseline)
    *natural_baseline = -1;
}

static void
gtk_progress_bar_measure_progress (GtkCssGadget   *gadget,
                                   GtkOrientation  orientation,
                                   int             for_size,
                                   int            *minimum,
                                   int            *natural,
                                   int            *minimum_baseline,
                                   int            *natural_baseline,
                                   gpointer        data)
{
  GtkWidget *widget;
  GtkProgressBar *pbar;
  GtkProgressBarPrivate *priv;
  GtkCssStyle *style;

  widget = gtk_css_gadget_get_owner (gadget);
  pbar = GTK_PROGRESS_BAR (widget);
  priv = pbar->priv;

  style = gtk_css_gadget_get_style (gadget);
  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      gint min_width;

      min_width = get_number (style, GTK_CSS_PROPERTY_MIN_WIDTH);

      if (min_width != 0)
        *minimum = min_width;
      else if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
        *minimum = 0;
      else
        gtk_widget_style_get (widget, "min-vertical-bar-width", minimum, NULL);
    }
  else
    {
      gint min_height;

      min_height = get_number (style, GTK_CSS_PROPERTY_MIN_HEIGHT);

      if (min_height != 0)
        *minimum = min_height;
      else if (priv->orientation == GTK_ORIENTATION_VERTICAL)
        *minimum = 0;
      else
        gtk_widget_style_get (widget, "min-horizontal-bar-height", minimum, NULL);
    }

  *natural = *minimum;

  if (minimum_baseline)
    *minimum_baseline = -1;
  if (natural_baseline)
    *natural_baseline = -1;
}

static void
gtk_progress_bar_size_allocate (GtkWidget     *widget,
                                GtkAllocation *allocation)
{
  GtkAllocation clip;

  gtk_widget_set_allocation (widget, allocation);

  gtk_css_gadget_allocate (GTK_PROGRESS_BAR (widget)->priv->gadget,
                           allocation,
                           gtk_widget_get_allocated_baseline (widget),
                           &clip);

  gtk_widget_set_clip (widget, &clip);
}

static void
gtk_progress_bar_allocate (GtkCssGadget        *gadget,
                           const GtkAllocation *allocation,
                           int                  baseline,
                           GtkAllocation       *out_clip,
                           gpointer             data)
{
  GtkWidget *widget;
  GtkProgressBarPrivate *priv;
  gint bar_width, bar_height;
  gint text_width, text_height, text_min, text_nat;
  GtkAllocation alloc;
  GtkAllocation text_clip;

  widget = gtk_css_gadget_get_owner (gadget);
  priv = GTK_PROGRESS_BAR (widget)->priv;

  if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      gtk_css_gadget_get_preferred_size (priv->trough_gadget,
                                         GTK_ORIENTATION_VERTICAL,
                                         -1,
                                         &bar_height, NULL,
                                         NULL, NULL);
      bar_width = allocation->width;
    }
  else
    {
      gtk_css_gadget_get_preferred_size (priv->trough_gadget,
                                         GTK_ORIENTATION_HORIZONTAL,
                                         -1,
                                         &bar_width, NULL,
                                         NULL, NULL);
      bar_height = allocation->height;
    }

  alloc.x = allocation->x + allocation->width - bar_width;
  alloc.y = allocation->y + allocation->height - bar_height;
  alloc.width = bar_width;
  alloc.height = bar_height;

  gtk_css_gadget_allocate (priv->trough_gadget, &alloc, -1, out_clip);

  if (!priv->show_text)
    return;

  gtk_css_gadget_get_preferred_size (priv->text_gadget,
                                     GTK_ORIENTATION_HORIZONTAL,
                                     -1,
                                     &text_min, &text_nat,
                                     NULL, NULL);
  gtk_css_gadget_get_preferred_size (priv->text_gadget,
                                     GTK_ORIENTATION_VERTICAL,
                                     -1,
                                     &text_height, NULL,
                                     NULL, NULL);

  text_width = CLAMP (text_nat, text_min, allocation->width);

  if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      alloc.x = allocation->x + (allocation->width - text_width) / 2;
      alloc.y = allocation->y;
      alloc.width = text_width;
      alloc.height = text_height;
    }
  else
    {
      alloc.x = allocation->x + allocation->width - text_width;
      alloc.y = allocation->y + (allocation->height - text_height) / 2;
      alloc.width = text_width;
      alloc.height = text_height;
    }

  gtk_css_gadget_allocate (priv->text_gadget, &alloc, -1, &text_clip);

  gdk_rectangle_union (out_clip, &text_clip, out_clip);
}


static void
gtk_progress_bar_allocate_trough (GtkCssGadget        *gadget,
                                  const GtkAllocation *allocation,
                                  int                  baseline,
                                  GtkAllocation       *out_clip,
                                  gpointer             data)
{
  GtkWidget *widget;
  GtkProgressBarPrivate *priv;
  GtkAllocation alloc;
  gint width, height;
  gboolean inverted;

  widget = gtk_css_gadget_get_owner (gadget);
  priv = GTK_PROGRESS_BAR (widget)->priv;

  inverted = priv->inverted;
  if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
    {
      if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
        inverted = !inverted;
    }

  gtk_css_gadget_get_preferred_size (priv->progress_gadget,
                                     GTK_ORIENTATION_VERTICAL,
                                     -1,
                                     &height, NULL,
                                     NULL, NULL);
  gtk_css_gadget_get_preferred_size (priv->progress_gadget,
                                     GTK_ORIENTATION_HORIZONTAL,
                                     -1,
                                     &width, NULL,
                                     NULL, NULL);

  if (priv->activity_mode)
    {
      if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
        {
          alloc.width = width + (allocation->width - width) / priv->activity_blocks;
          alloc.x = allocation->x + priv->activity_pos * (allocation->width - alloc.width);
          alloc.y = allocation->y + (allocation->height - height) / 2;
          alloc.height = height;
        }
      else
        {

          alloc.height = height + (allocation->height - height) / priv->activity_blocks;
          alloc.y = allocation->y + priv->activity_pos * (allocation->height - alloc.height);
          alloc.x = allocation->x + (allocation->width - width) / 2;
          alloc.width = width;
        }
    }
  else
    {
      if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
        {
          alloc.width = width + (allocation->width - width) * priv->fraction;
          alloc.height = height;
          alloc.y = allocation->y + (allocation->height - height) / 2;

          if (!inverted)
            alloc.x = allocation->x;
          else
            alloc.x = allocation->x + allocation->width - alloc.width;
        }
      else
        {
          alloc.width = width;
          alloc.height = height + (allocation->height - height) * priv->fraction;
          alloc.x = allocation->x + (allocation->width - width) / 2;

          if (!inverted)
            alloc.y = allocation->y;
          else
            alloc.y = allocation->y + allocation->height - alloc.height;
        }
    }

  gtk_css_gadget_allocate (priv->progress_gadget, &alloc, -1, out_clip);
}

static void
gtk_progress_bar_get_preferred_width (GtkWidget *widget,
                                      gint      *minimum,
                                      gint      *natural)
{
  gtk_css_gadget_get_preferred_size (GTK_PROGRESS_BAR (widget)->priv->gadget,
                                     GTK_ORIENTATION_HORIZONTAL,
                                     -1,
                                     minimum, natural,
                                     NULL, NULL);
}

static void
gtk_progress_bar_get_preferred_height (GtkWidget *widget,
                                       gint      *minimum,
                                       gint      *natural)
{
  gtk_css_gadget_get_preferred_size (GTK_PROGRESS_BAR (widget)->priv->gadget,
                                     GTK_ORIENTATION_VERTICAL,
                                     -1,
                                     minimum, natural,
                                     NULL, NULL);
}

static gboolean
tick_cb (GtkWidget     *widget,
         GdkFrameClock *frame_clock,
         gpointer       user_data)
{
  GtkProgressBar *pbar = GTK_PROGRESS_BAR (widget);
  GtkProgressBarPrivate *priv = pbar->priv;
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
    {
      priv->pulse1 = 0;
      return G_SOURCE_CONTINUE;
    }

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

  gtk_widget_queue_allocate (widget);

  return G_SOURCE_CONTINUE;
}

static void
gtk_progress_bar_act_mode_enter (GtkProgressBar *pbar)
{
  GtkProgressBarPrivate *priv = pbar->priv;
  GtkWidget *widget = GTK_WIDGET (pbar);
  gboolean inverted;

  gtk_css_gadget_add_class (priv->progress_gadget, GTK_STYLE_CLASS_PULSE);

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
  GtkProgressBarPrivate *priv = pbar->priv;

  if (priv->tick_id)
    gtk_widget_remove_tick_callback (GTK_WIDGET (pbar), priv->tick_id);
  priv->tick_id = 0;

  gtk_css_gadget_remove_class (priv->progress_gadget, GTK_STYLE_CLASS_PULSE);
  update_node_classes (pbar);
}

static gboolean
gtk_progress_bar_render_text (GtkCssGadget *gadget,
                              cairo_t      *cr,
                              int           x,
                              int           y,
                              int           width,
                              int           height,
                              gpointer      data)
{
  GtkWidget *widget;
  GtkProgressBar *pbar;
  GtkProgressBarPrivate *priv;
  GtkStyleContext *context;
  PangoLayout *layout;

  widget = gtk_css_gadget_get_owner (gadget);
  pbar = GTK_PROGRESS_BAR (widget);
  priv = pbar->priv;

  context = gtk_widget_get_style_context (widget);
  gtk_style_context_save_to_node (context, gtk_css_gadget_get_node (gadget));

  layout = gtk_progress_bar_get_layout (pbar);
  pango_layout_set_ellipsize (layout, priv->ellipsize);
  if (priv->ellipsize)
    pango_layout_set_width (layout, width * PANGO_SCALE);

  gtk_render_layout (context, cr, x, y, layout);

  g_object_unref (layout);

  gtk_style_context_restore (context);

  return FALSE;
}

static gboolean
gtk_progress_bar_render_trough (GtkCssGadget *gadget,
                                cairo_t      *cr,
                                int           x,
                                int           y,
                                int           width,
                                int           height,
                                gpointer      data)
{
  GtkWidget *widget;
  GtkProgressBarPrivate *priv;

  widget = gtk_css_gadget_get_owner (gadget);
  priv = GTK_PROGRESS_BAR (widget)->priv;

  gtk_css_gadget_draw (priv->progress_gadget, cr);

  return FALSE;
}

static gboolean
gtk_progress_bar_render (GtkCssGadget *gadget,
                         cairo_t      *cr,
                         int           x,
                         int           y,
                         int           width,
                         int           height,
                         gpointer      data)
{
  GtkWidget *widget;
  GtkProgressBarPrivate *priv;

  widget = gtk_css_gadget_get_owner (gadget);
  priv = GTK_PROGRESS_BAR (widget)->priv;

  gtk_css_gadget_draw (priv->trough_gadget, cr);
  if (priv->show_text)
    gtk_css_gadget_draw (priv->text_gadget, cr);

  return FALSE;
}

static gboolean
gtk_progress_bar_draw (GtkWidget *widget,
                       cairo_t   *cr)
{
  GtkProgressBar *pbar = GTK_PROGRESS_BAR (widget);
  GtkProgressBarPrivate *priv = pbar->priv;

  gtk_css_gadget_draw (priv->gadget, cr);

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
  GtkProgressBarPrivate* priv;

  g_return_if_fail (GTK_IS_PROGRESS_BAR (pbar));

  priv = pbar->priv;

  priv->fraction = CLAMP (fraction, 0.0, 1.0);
  gtk_progress_bar_set_activity_mode (pbar, FALSE);
  gtk_widget_queue_allocate (GTK_WIDGET (pbar));
  update_fraction_classes (pbar);

  g_object_notify_by_pspec (G_OBJECT (pbar), progress_props[PROP_FRACTION]);
}

static void
gtk_progress_bar_update_pulse (GtkProgressBar *pbar)
{
  GtkProgressBarPrivate *priv = pbar->priv;
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
  GtkProgressBarPrivate *priv;

  g_return_if_fail (GTK_IS_PROGRESS_BAR (pbar));

  priv = pbar->priv;

  /* Don't notify again if nothing's changed. */
  if (g_strcmp0 (priv->text, text) == 0)
    return;

  g_free (priv->text);
  priv->text = g_strdup (text);

  gtk_widget_queue_resize (GTK_WIDGET (pbar));

  g_object_notify_by_pspec (G_OBJECT (pbar), progress_props[PROP_TEXT]);
}

static void
gtk_progress_bar_text_style_changed (GtkCssNode        *node,
                                     GtkCssStyleChange *change,
                                     GtkProgressBar    *pbar)
{
  if (change == NULL ||
      gtk_css_style_change_affects (change, GTK_CSS_AFFECTS_TEXT_ATTRS) ||
      gtk_css_style_change_affects (change, GTK_CSS_AFFECTS_FONT))
    {
      gtk_widget_queue_resize (GTK_WIDGET (pbar));
    }
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

  if (priv->show_text == show_text)
    return;

  priv->show_text = show_text;

  if (show_text)
    {
      priv->text_gadget = gtk_css_custom_gadget_new ("text",
                                                     GTK_WIDGET (pbar),
                                                     priv->gadget,
                                                     priv->trough_gadget,
                                                     gtk_progress_bar_measure_text,
                                                     NULL,
                                                     gtk_progress_bar_render_text,
                                                     NULL,
                                                     NULL);
      g_signal_connect (gtk_css_gadget_get_node (priv->text_gadget), "style-changed",
                        G_CALLBACK (gtk_progress_bar_text_style_changed), pbar);

      update_node_state (pbar);
    }
  else
    {
      if (priv->text_gadget)
        gtk_css_node_set_parent (gtk_css_gadget_get_node (priv->text_gadget), NULL);
      g_clear_object (&priv->text_gadget);
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
 *
 * Since: 3.0
 */
gboolean
gtk_progress_bar_get_show_text (GtkProgressBar *pbar)
{
  g_return_val_if_fail (GTK_IS_PROGRESS_BAR (pbar), FALSE);

  return pbar->priv->show_text;
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

  g_object_notify_by_pspec (G_OBJECT (pbar), progress_props[PROP_PULSE_STEP]);
}

static void
gtk_progress_bar_direction_changed (GtkWidget        *widget,
                                    GtkTextDirection  previous_dir)
{
  GtkProgressBar *pbar = GTK_PROGRESS_BAR (widget);

  update_node_classes (pbar);
  update_node_state (pbar);

  GTK_WIDGET_CLASS (gtk_progress_bar_parent_class)->direction_changed (widget, previous_dir);
}

static void
gtk_progress_bar_state_flags_changed (GtkWidget     *widget,
                                      GtkStateFlags  previous_state)
{
  GtkProgressBar *pbar = GTK_PROGRESS_BAR (widget);

  update_node_state (pbar);

  GTK_WIDGET_CLASS (gtk_progress_bar_parent_class)->state_flags_changed (widget, previous_state);
}

static void
gtk_progress_bar_set_orientation (GtkProgressBar *pbar,
                                  GtkOrientation  orientation)
{
  GtkProgressBarPrivate *priv = pbar->priv;

  if (priv->orientation == orientation)
    return;

  priv->orientation = orientation;

  _gtk_orientable_set_style_classes (GTK_ORIENTABLE (pbar));
  update_node_classes (pbar);
  gtk_widget_queue_resize (GTK_WIDGET (pbar));

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
  GtkProgressBarPrivate *priv;

  g_return_if_fail (GTK_IS_PROGRESS_BAR (pbar));

  priv = pbar->priv;

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
  g_return_val_if_fail (GTK_IS_PROGRESS_BAR (pbar), NULL);

  return pbar->priv->text;
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
  g_return_val_if_fail (GTK_IS_PROGRESS_BAR (pbar), 0);

  return pbar->priv->fraction;
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
  g_return_val_if_fail (GTK_IS_PROGRESS_BAR (pbar), 0);

  return pbar->priv->pulse_fraction;
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
  g_return_val_if_fail (GTK_IS_PROGRESS_BAR (pbar), FALSE);

  return pbar->priv->inverted;
}

/**
 * gtk_progress_bar_set_ellipsize:
 * @pbar: a #GtkProgressBar
 * @mode: a #PangoEllipsizeMode
 *
 * Sets the mode used to ellipsize (add an ellipsis: "...") the
 * text if there is not enough space to render the entire string.
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

  if ((PangoEllipsizeMode)priv->ellipsize != mode)
    {
      priv->ellipsize = mode;

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
 *
 * Since: 2.6
 */
PangoEllipsizeMode
gtk_progress_bar_get_ellipsize (GtkProgressBar *pbar)
{
  g_return_val_if_fail (GTK_IS_PROGRESS_BAR (pbar), PANGO_ELLIPSIZE_NONE);

  return pbar->priv->ellipsize;
}
