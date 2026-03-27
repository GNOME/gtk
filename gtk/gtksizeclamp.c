#include "config.h"

#include "gtksizeclamp.h"
#include "gtkprivate.h"
#include "gtkbuildable.h"

/**
 * GtkSizeClamp:
 *
 * A helper widget that limits or otherwise adjusts its child widget's
 * size request in various ways.
 *
 * The most straightforward way to use this widget is to set
 * [property@SizeClamp:max-width] and/or [property@SizeClamp:max-height],
 * which will prevent the child widget from asking for more space than
 * specified. Note that it's still possible that the widget ends up
 * _allocated_ more space than the specified limit, if it turns out that
 * more space is available and there is no better use for that space.
 * You can control what happens in that case with
 * [property@Widget:halign] and [property@Widget:valign].
 *
 * ### Constant-size mode
 *
 * `GtkSizeClamp` can be put into _constant-size mode_ by setting the
 * [property@SizeClamp:constant-size] flag. In this mode, the clamp will
 * report [enum@Gtk.SizeRequestMode.CONSTANT_SIZE] as its
 * [size request mode](class@Widget#height-for-width-geometry-management).
 * This will stop the GTK layout machinery from trying to compute the
 * widget's height based on its width; instead the width and height will
 * be computed independently.
 *
 * Note that this does not necessarily mean that the widget's size can
 * not change, this only prevents its width and height from depending on
 * each other.
 *
 * In general, constant-size widgets are much easier for the GTK layout
 * machinery to handle, and result in faster, more stable, and less
 * brittle layouts. For this reason, it's beneficial to use the
 * constant-size mode of `GtkSizeClamp`. However, this is not
 * appropriate for all layouts either.
 *
 * The inteded use case for the constant-size mode of `GtkSizeClamp` is
 * to "isolate" height-for-width geometry management happening in some
 * part of a layout from "leaking out" to the outer larger layout. The
 * `GtkSizeClamp`, as seen from the outside, can be constant-size, while
 * still allowing for things like wrappable labels somewhere inside of
 * the clamp.
 *
 * Using constant-size mode can result in the clamp requesting an overly
 * large minimum size. Somewhat counterintuitively, to prevent this you
 * should ensure that the child widget has a *large* enough minimum size
 * in the *opposite* orientation, perhaps by using
 * [method@Gtk.Widget.set_size_request].
 *
 * ### Always requesting natural size
 *
 * If [property@SizeClamp:request-natural-width] and/or
 * [property@SizeClamp:request-natural-height] are set, the clamp will
 * report its minimum size in that orientation as being equal to its
 * natural size. This will ensure that its child always receives its
 * natural size, and not any less, even if the child says it can cope
 * with less.
 *
 * The opposite effect, reporting natural size equal to the minimum
 * size, can be achieved by setting [property@SizeClamp:max-width]
 * and/or [property@SizeClamp:max-height] to 0 (or any other value not
 * exceeding the minimum size).
 *
 * ::: warning
 *     This mode can result in inconsistent measurements and broken
 *     layouts. This is because, in general, natural sizes don't follow
 *     the rules that minimum sizes must follow. In particular, this
 *     mode must not be used with widgets that try to keep their aspect
 *     ratio, such as [class@Picture] or [class@AspectFrame], unless
 *     [property@SizeClamp:constant-size] is also set.
 *
 * Since: 4.24
 */

struct _GtkSizeClamp
{
  GtkWidget parent_instance;

  int max_width;
  int max_height;
  gboolean constant_size : 1;
  gboolean request_natural_width : 1;
  gboolean request_natural_height : 1;
};

enum
{
  PROP_CHILD = 1,
  PROP_MAX_WIDTH,
  PROP_MAX_HEIGHT,
  PROP_CONSTANT_SIZE,
  PROP_REQUEST_NATURAL_WIDTH,
  PROP_REQUEST_NATURAL_HEIGHT,
  N_PROPS
};

static GParamSpec *props[N_PROPS];

static void gtk_size_clamp_buildable_init (GtkBuildableIface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (GtkSizeClamp, gtk_size_clamp, GTK_TYPE_WIDGET,
                               G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                                      gtk_size_clamp_buildable_init))

static void
gtk_size_clamp_dispose (GObject *object)
{
  GtkWidget *widget = GTK_WIDGET (object);

  GtkWidget *child = gtk_widget_get_first_child (widget);
  if (child)
    gtk_widget_unparent (child);

  G_OBJECT_CLASS (gtk_size_clamp_parent_class)->dispose (object);
}

static GtkSizeRequestMode
gtk_size_clamp_get_request_mode (GtkWidget *widget)
{
  GtkSizeClamp *self = GTK_SIZE_CLAMP (widget);
  GtkWidget *child;

  if (self->constant_size)
    return GTK_SIZE_REQUEST_CONSTANT_SIZE;
  child = gtk_widget_get_first_child (widget);
  if (!child)
    return GTK_SIZE_REQUEST_CONSTANT_SIZE;
  return gtk_widget_get_request_mode (child);
}

static void
gtk_size_clamp_measure (GtkWidget     *widget,
                        GtkOrientation orientation,
                        int            for_size,
                        int           *minimum,
                        int           *natural,
                        int           *minimum_baseline,
                        int           *natural_baseline)
{
  GtkSizeClamp *self = GTK_SIZE_CLAMP (widget);
  GtkWidget *child = gtk_widget_get_first_child (widget);
  int limit;
  gboolean remeasure_global_natural = FALSE;

  if (!child)
    {
      *minimum = *natural = 0;
      *minimum_baseline = *natural_baseline = -1;
      return;
    }

  if (self->constant_size)
    {
      /* When measuring in one orientation, we report the child's
       * overall minimum and natural sizes as our sizes. For the
       * other orientation, we measure the child for this picked
       * size in this orientation.
       */
      gboolean is_preferred_orientation;
      if (gtk_widget_get_request_mode (child) == GTK_SIZE_REQUEST_WIDTH_FOR_HEIGHT)
        is_preferred_orientation = orientation == GTK_ORIENTATION_HORIZONTAL;
      else
        is_preferred_orientation = orientation == GTK_ORIENTATION_VERTICAL;

      if (!is_preferred_orientation)
        {
          /* Measure the child's overall minimum and natural sizes */
          for_size = -1;
        }
      else
        {
          /* Measure the child for that minimum size we measured in
           * the opposite orientation. Remember to measure the overall
           * natural size.
           */
          int child_min, child_nat;

          remeasure_global_natural = TRUE;

          gtk_widget_measure (child, OPPOSITE_ORIENTATION (orientation),
                              -1, &child_min, &child_nat, NULL, NULL);
          if (self->request_natural_width && orientation == GTK_ORIENTATION_VERTICAL)
            {
              for_size = child_nat;
              if (self->max_width != -1)
                for_size = MAX (child_min, MIN (child_nat, self->max_width));
            }
          else if (self->request_natural_height && orientation == GTK_ORIENTATION_HORIZONTAL)
            {
              for_size = child_nat;
              if (self->max_height != -1)
                for_size = MAX (child_min, MIN (child_nat, self->max_height));
            }
          else
            for_size = child_min;
        }
    }

  gtk_widget_measure (child, orientation, for_size, minimum, natural,
                      minimum_baseline, natural_baseline);
  if (remeasure_global_natural)
    gtk_widget_measure (child, orientation, -1, NULL,
                        natural, NULL, natural_baseline);

  if (orientation == GTK_ORIENTATION_VERTICAL)
    limit = self->max_height;
  else
    limit = self->max_width;

  if (limit != -1 && *natural > limit)
    {
      /* Carefully clamp the natural size */
      if (*minimum > limit)
        {
          *natural = *minimum;
          *natural_baseline = *minimum_baseline;
        }
      else
        {
          if (orientation == GTK_ORIENTATION_VERTICAL && *natural_baseline != -1)
            {
              /* Adjust the baseline by interpolating */
              g_assert (*natural > *minimum);
              *natural_baseline = *minimum_baseline + (
                ((double) (*natural_baseline - *minimum_baseline))
                  / (*natural - *minimum)
                  * (limit - *minimum));
            }
          *natural = limit;
        }
    }

  if ((orientation == GTK_ORIENTATION_VERTICAL && self->request_natural_height) ||
      (orientation == GTK_ORIENTATION_HORIZONTAL && self->request_natural_width))
    {
      *minimum = *natural;
      *minimum_baseline = *natural_baseline;
    }
}

static void
gtk_size_clamp_size_allocate (GtkWidget *widget,
                              int        width,
                              int        height,
                              int        baseline)
{
  GtkWidget *child = gtk_widget_get_first_child (widget);
  if (!child)
    return;

  /* We propagate our complete allocation to the child, even if it
   * exceeds max-width/max-height. If you never want the child to
   * grow larger than the set limit, set halign/valign on the clamp.
   */

  gtk_widget_allocate (child, width, height, baseline, NULL);
}

static void
gtk_size_clamp_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  GtkSizeClamp *self = GTK_SIZE_CLAMP (object);

  switch (prop_id)
    {
    case PROP_CHILD:
      g_value_set_object (value, gtk_size_clamp_get_child (self));
      break;
    case PROP_MAX_WIDTH:
      g_value_set_int (value, self->max_width);
      break;
    case PROP_MAX_HEIGHT:
      g_value_set_int (value, self->max_height);
      break;
    case PROP_CONSTANT_SIZE:
      g_value_set_boolean (value, self->constant_size);
      break;
    case PROP_REQUEST_NATURAL_WIDTH:
      g_value_set_boolean (value, self->request_natural_width);
      break;
    case PROP_REQUEST_NATURAL_HEIGHT:
      g_value_set_boolean (value, self->request_natural_height);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_size_clamp_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  GtkSizeClamp *self = GTK_SIZE_CLAMP (object);

  switch (prop_id)
    {
    case PROP_CHILD:
      gtk_size_clamp_set_child (self, g_value_get_object (value));
      break;
    case PROP_MAX_WIDTH:
      gtk_size_clamp_set_max_width (self, g_value_get_int (value));
      break;
    case PROP_MAX_HEIGHT:
      gtk_size_clamp_set_max_height (self, g_value_get_int (value));
      break;
    case PROP_CONSTANT_SIZE:
      gtk_size_clamp_set_constant_size (self, g_value_get_boolean (value));
      break;
    case PROP_REQUEST_NATURAL_WIDTH:
      gtk_size_clamp_set_request_natural_width (self, g_value_get_boolean (value));
      break;
    case PROP_REQUEST_NATURAL_HEIGHT:
      gtk_size_clamp_set_request_natural_height (self, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_size_clamp_init (GtkSizeClamp *self)
{
  self->max_width = self->max_height = -1;
}

static void
gtk_size_clamp_class_init (GtkSizeClampClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = gtk_size_clamp_dispose;
  object_class->get_property = gtk_size_clamp_get_property;
  object_class->set_property = gtk_size_clamp_set_property;

  widget_class->get_request_mode = gtk_size_clamp_get_request_mode;
  widget_class->measure = gtk_size_clamp_measure;
  widget_class->size_allocate = gtk_size_clamp_size_allocate;

  /**
   * GtkSizeClamp:child:
   *
   * The child widget of the clamp.
   *
   * Since: 4.24
   */
  props[PROP_CHILD] =
      g_param_spec_object ("child", NULL, NULL,
                           GTK_TYPE_WIDGET,
                           GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkSizeClamp:max-width:
   *
   * The maximum width the clamp will request, or -1 for no limit.
   *
   * Since: 4.24
   */
  props[PROP_MAX_WIDTH] =
      g_param_spec_int ("max-width", NULL, NULL,
                        -1, G_MAXINT, -1,
                        GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkSizeClamp:max-height:
   *
   * The mazximum height the clamp will request, or -1 for no limit.
   *
   * Since: 4.24
   */
  props[PROP_MAX_HEIGHT] =
      g_param_spec_int ("max-height", NULL, NULL,
                        -1, G_MAXINT, -1,
                        GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkSizeClamp:constant-size:
   *
   * Whether the clamp should be put into
   * [constant-size mode](class@SizeClamp#constant-size-mode).
   *
   * Since: 4.24
   */
  props[PROP_CONSTANT_SIZE] =
      g_param_spec_boolean ("constant-size", NULL, NULL,
                            FALSE,
                            GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkSizeClamp:request-natural-width:
   *
   * Whether the clamp should ensure its child always receives its
   * natural width.
   *
   * Since: 4.24
   */
  props[PROP_REQUEST_NATURAL_WIDTH] =
      g_param_spec_boolean ("request-natural-width", NULL, NULL,
                            FALSE,
                            GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkSizeClamp:request-natural-height:
   *
   * Whether the clamp should ensure its child always receives its
   * natural height.
   *
   * Since: 4.24
   */
  props[PROP_REQUEST_NATURAL_HEIGHT] =
      g_param_spec_boolean ("request-natural-height", NULL, NULL,
                            FALSE,
                            GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, N_PROPS, props);
}

static GtkBuildableIface *parent_buildable_iface;

static void
gtk_size_clamp_buildable_add_child (GtkBuildable *buildable,
                                    GtkBuilder   *builder,
                                    GObject      *child,
                                    const char   *type)
{
  if (GTK_IS_WIDGET (child))
    gtk_size_clamp_set_child (GTK_SIZE_CLAMP (buildable), GTK_WIDGET (child));
  else
    parent_buildable_iface->add_child (buildable, builder, child, type);
}

static void
gtk_size_clamp_buildable_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);

  iface->add_child = gtk_size_clamp_buildable_add_child;
}

/**
 * gtk_size_clamp_new:
 * @max_width: maximum width the clamp will request, or -1 for no limit
 * @max_height: maximum height the clamp will request, or -1 for no limit
 *
 * Creates a new size clamp widget with the given size limits.
 *
 * Returns: (transfer floating): the newly created size clamp
 *
 * Since: 4.24
 */
GtkWidget *
gtk_size_clamp_new (int max_width,
                    int max_height)
{
  g_return_val_if_fail (max_width >= -1, NULL);
  g_return_val_if_fail (max_height >= -1, NULL);

  return g_object_new (GTK_TYPE_SIZE_CLAMP,
                       "max-width", max_width,
                       "max-height", max_height,
                       NULL);
}

/**
 * gtk_size_clamp_get_child:
 * @self: a size clamp
 *
 * Returns: (transfer none) (nullable): the child widget
 *
 * Since: 4.24
 */
GtkWidget *
gtk_size_clamp_get_child (GtkSizeClamp *self)
{
  g_return_val_if_fail (GTK_IS_SIZE_CLAMP (self), NULL);

  return gtk_widget_get_first_child (GTK_WIDGET (self));
}

/**
 * gtk_size_clamp_set_child:
 * @self: a size clamp
 * @child: (nullable) (transfer floating): the new child
 *
 * Since: 4.24
 */
void
gtk_size_clamp_set_child (GtkSizeClamp *self,
                          GtkWidget    *child)
{
  g_return_if_fail (GTK_IS_SIZE_CLAMP (self));
  g_return_if_fail (child == NULL || GTK_IS_WIDGET (child));

  GtkWidget *old_child = gtk_widget_get_first_child (GTK_WIDGET (self));
  if (old_child == child)
    return;
  else if (old_child)
    gtk_widget_unparent (old_child);

  if (child)
    gtk_widget_set_parent (child, GTK_WIDGET (self));

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_CHILD]);
}

/**
 * gtk_size_clamp_get_max_width:
 *
 * Returns: the width limit, or -1 if no limit
 *
 * Since: 4.24
 */
int
gtk_size_clamp_get_max_width (GtkSizeClamp *self)
{
  g_return_val_if_fail (GTK_IS_SIZE_CLAMP (self), -1);

  return self->max_width;
}

/**
 * gtk_size_clamp_set_max_width:
 *
 * Since: 4.24
 */
void
gtk_size_clamp_set_max_width (GtkSizeClamp *self,
                              int           max_width)
{
  g_return_if_fail (GTK_IS_SIZE_CLAMP (self));
  g_return_if_fail (max_width >= -1);

  if (self->max_width == max_width)
    return;
  self->max_width = max_width;

  gtk_widget_queue_resize (GTK_WIDGET (self));
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_MAX_WIDTH]);
}

/**
 * gtk_size_clamp_get_max_height:
 *
 * Returns: the height limit, or -1 if no limit
 *
 * Since: 4.24
 */
int
gtk_size_clamp_get_max_height (GtkSizeClamp *self)
{
  g_return_val_if_fail (GTK_IS_SIZE_CLAMP (self), -1);

  return self->max_height;
}

/**
 * gtk_size_clamp_set_max_height:
 *
 * Since: 4.24
 */
void
gtk_size_clamp_set_max_height (GtkSizeClamp *self,
                               int           max_height)
{
  g_return_if_fail (GTK_IS_SIZE_CLAMP (self));
  g_return_if_fail (max_height >= -1);

  if (self->max_height == max_height)
    return;
  self->max_height = max_height;

  gtk_widget_queue_resize (GTK_WIDGET (self));
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_MAX_HEIGHT]);
}

/**
 * gtk_size_clamp_get_constant_size:
 *
 * Since: 4.24
 */
gboolean
gtk_size_clamp_get_constant_size (GtkSizeClamp *self)
{
  g_return_val_if_fail (GTK_IS_SIZE_CLAMP (self), FALSE);

  return self->constant_size;
}

/**
 * gtk_size_clamp_set_constant_size:
 *
 * Since: 4.24
 */
void
gtk_size_clamp_set_constant_size (GtkSizeClamp *self,
                                  gboolean      constant_size)
{
  g_return_if_fail (GTK_IS_SIZE_CLAMP (self));

  constant_size = !!constant_size;
  if (self->constant_size == constant_size)
    return;
  self->constant_size = constant_size;

  gtk_widget_queue_resize (GTK_WIDGET (self));
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_CONSTANT_SIZE]);
}

/**
 * gtk_size_clamp_get_request_natural_width:
 *
 * Since: 4.24
 */
gboolean
gtk_size_clamp_get_request_natural_width (GtkSizeClamp *self)
{
  g_return_val_if_fail (GTK_IS_SIZE_CLAMP (self), FALSE);

  return self->request_natural_width;
}

/**
 * gtk_size_clamp_set_request_natural_width:
 *
 * Since: 4.24
 */
void
gtk_size_clamp_set_request_natural_width (GtkSizeClamp *self,
                                          gboolean      request_natural_width)
{
  g_return_if_fail (GTK_IS_SIZE_CLAMP (self));

  request_natural_width = !!request_natural_width;
  if (self->request_natural_width == request_natural_width)
    return;
  self->request_natural_width = request_natural_width;

  gtk_widget_queue_resize (GTK_WIDGET (self));
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_REQUEST_NATURAL_WIDTH]);
}

/**
 * gtk_size_clamp_get_request_natural_height:
 *
 * Since: 4.24
 */
gboolean
gtk_size_clamp_get_request_natural_height (GtkSizeClamp *self)
{
  g_return_val_if_fail (GTK_IS_SIZE_CLAMP (self), FALSE);

  return self->request_natural_height;
}

/**
 * gtk_size_clamp_set_request_natural_height:
 *
 * Since: 4.24
 */
void
gtk_size_clamp_set_request_natural_height (GtkSizeClamp *self,
                                           gboolean      request_natural_height)
{
  g_return_if_fail (GTK_IS_SIZE_CLAMP (self));

  request_natural_height = !!request_natural_height;
  if (self->request_natural_height == request_natural_height)
    return;
  self->request_natural_height = request_natural_height;

  gtk_widget_queue_resize (GTK_WIDGET (self));
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_REQUEST_NATURAL_HEIGHT]);
}
