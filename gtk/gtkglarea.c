#include "config.h"

#include "config.h"
#include "gtkglarea.h"
#include "gtkintl.h"
#include "gtkstylecontext.h"
#include "gtkmarshalers.h"
#include "gtkprivate.h"

/**
 * SECTION:gtkglarea
 * @Title: GtkGLArea
 * @Short_description: A widget for custom drawing with OpenGL
 *
 * #GtkGLArea is a widget that allows drawing with OpenGL.
 *
 * #GtkGLArea can set up its own #GdkGLContext using a provided
 * #GdkGLPixelFormat, or can use a given #GdkGLContext.
 *
 * In order to draw, you have to connect to the #GtkGLArea::draw-with-context
 * signal, or subclass #GtkGLArea and override the @GtkGLAreaClass.draw_with_context
 * virtual function.
 *
 * The #GtkGLArea widget ensures that the #GdkGLContext is associated with the
 * widget's drawing area, and it is kept updated when the size and position of
 * the drawing area changes.
 */

typedef struct {
  GdkGLPixelFormat *pixel_format;
  GdkGLContext *context;
} GtkGLAreaPrivate;

enum {
  PROP_0,

  PROP_PIXEL_FORMAT,
  PROP_CONTEXT,

  LAST_PROP
};

static GParamSpec *obj_props[LAST_PROP] = { NULL, };

enum {
  RENDER,
  CREATE_CONTEXT,

  LAST_SIGNAL
};

static guint area_signals[LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE_WITH_PRIVATE (GtkGLArea, gtk_gl_area, GTK_TYPE_WIDGET)

static void
gtk_gl_area_dispose (GObject *gobject)
{
  GtkGLArea *self = GTK_GL_AREA (gobject);
  GtkGLAreaPrivate *priv = gtk_gl_area_get_instance_private (self);

  g_clear_object (&priv->pixel_format);
  g_clear_object (&priv->context);

  G_OBJECT_CLASS (gtk_gl_area_parent_class)->dispose (gobject);
}

static void
gtk_gl_area_set_property (GObject      *gobject,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  GtkGLArea *self = GTK_GL_AREA (gobject);
  GtkGLAreaPrivate *priv = gtk_gl_area_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_PIXEL_FORMAT:
      priv->pixel_format = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
gtk_gl_area_get_property (GObject    *gobject,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  GtkGLAreaPrivate *priv = gtk_gl_area_get_instance_private (GTK_GL_AREA (gobject));

  switch (prop_id)
    {
    case PROP_PIXEL_FORMAT:
      g_value_set_object (value, priv->pixel_format);
      break;

    case PROP_CONTEXT:
      g_value_set_object (value, priv->context);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static GdkGLContext *
gtk_gl_area_create_context (GtkGLArea *area)
{
  GtkGLAreaPrivate *priv = gtk_gl_area_get_instance_private (area);
  GdkGLContext *context;

  if (priv->context != NULL)
    return priv->context;

  context = NULL;
  g_signal_emit (area, area_signals[CREATE_CONTEXT], 0,
                 priv->pixel_format,
                 &context);

  if (context == NULL)
    {
      g_critical ("No GL context was created for the widget");
      return NULL;
    }

  gtk_widget_set_visual (GTK_WIDGET (area), gdk_gl_context_get_visual (context));

  priv->context = context;

  return priv->context;
}

static void
gtk_gl_area_realize (GtkWidget *widget)
{
  GdkGLContext *context;
  GtkAllocation allocation;
  GdkWindow *window;
  GdkWindowAttr attributes;
  gint attributes_mask;

  gtk_widget_set_realized (widget, TRUE);
  gtk_widget_get_allocation (widget, &allocation);

  context = gtk_gl_area_create_context (GTK_GL_AREA (widget));

  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.x = allocation.x;
  attributes.y = allocation.y;
  attributes.width = allocation.width;
  attributes.height = allocation.height;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.event_mask = gtk_widget_get_events (widget) | GDK_EXPOSURE_MASK;

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL;

  window = gdk_window_new (gtk_widget_get_parent_window (widget),
                           &attributes,
                           attributes_mask);
  gtk_widget_register_window (widget, window);
  gtk_widget_set_window (widget, window);

  if (context != NULL)
    {
      gdk_gl_context_set_window (context, gtk_widget_get_window (widget));
      gdk_gl_context_update (context);
    }
}

static void
gtk_gl_area_unrealize (GtkWidget *widget)
{
  GtkGLAreaPrivate *priv = gtk_gl_area_get_instance_private ((GtkGLArea *) widget);

  if (priv->context != NULL)
    gdk_gl_context_set_window (priv->context, NULL);

  GTK_WIDGET_CLASS (gtk_gl_area_parent_class)->unrealize (widget);
}

static void
gtk_gl_area_size_allocate (GtkWidget     *widget,
                           GtkAllocation *allocation)
{
  GtkGLAreaPrivate *priv = gtk_gl_area_get_instance_private ((GtkGLArea *) widget);
  GtkAllocation old_alloc;
  gboolean same_size;

  gtk_widget_get_allocation (widget, &old_alloc);
  gtk_widget_set_allocation (widget, allocation);

  if (!gtk_widget_get_realized (widget))
    return;

  same_size = old_alloc.width == allocation->width &&
              old_alloc.height == allocation->height;

  gdk_window_move_resize (gtk_widget_get_window (widget),
                          allocation->x,
                          allocation->y,
                          allocation->width,
                          allocation->height);

  /* no need to reset the viewport if the size of the window did not change */
  if (priv->context != NULL && !same_size)
    gdk_gl_context_update (priv->context);
}

static gboolean
gtk_gl_area_draw (GtkWidget *widget,
                  cairo_t   *cr)
{
  GtkGLArea *self = GTK_GL_AREA (widget);
  GtkGLAreaPrivate *priv = gtk_gl_area_get_instance_private (self);
  gboolean unused;

  if (priv->context == NULL)
    return FALSE;

  if (!gtk_gl_area_make_current (self))
    return FALSE;

  g_signal_emit (self, area_signals[RENDER], 0, priv->context, &unused);

  gtk_gl_area_flush_buffer (self);

  return TRUE;
}

static void
gtk_gl_area_screen_changed (GtkWidget *widget,
                            GdkScreen *old_screen)
{
  GtkGLArea *self = GTK_GL_AREA (widget);
  GtkGLAreaPrivate *priv = gtk_gl_area_get_instance_private (self);

  /* this will cause the context to be recreated on realize */
  g_clear_object (&priv->context);
}

static GdkGLContext *
gtk_gl_area_real_create_context (GtkGLArea        *area,
                                 GdkGLPixelFormat *format)
{
  GdkDisplay *display = gtk_widget_get_display (GTK_WIDGET (area));
  GdkGLContext *retval;
  GError *error = NULL;

  if (display == NULL)
    display = gdk_display_get_default ();

  retval = gdk_display_get_gl_context (display, format, NULL, &error);
  if (error != NULL)
    {
      g_critical ("Unable to create a GdkGLContext: %s", error->message);
      g_error_free (error);
      return NULL;
    }

  return retval;
}

static gboolean
create_context_accumulator (GSignalInvocationHint *ihint,
                            GValue                *return_accu,
                            const GValue          *handler_return,
                            gpointer               data)
{
  g_value_copy (handler_return, return_accu);

  /* stop after the first handler returning a valid object */
  return g_value_get_object (handler_return) == NULL;
}

static void
gtk_gl_area_class_init (GtkGLAreaClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  klass->create_context = gtk_gl_area_real_create_context;

  widget_class->screen_changed = gtk_gl_area_screen_changed;
  widget_class->realize = gtk_gl_area_realize;
  widget_class->unrealize = gtk_gl_area_unrealize;
  widget_class->size_allocate = gtk_gl_area_size_allocate;
  widget_class->draw = gtk_gl_area_draw;

  gtk_widget_class_set_accessible_role (widget_class, ATK_ROLE_DRAWING_AREA);

  obj_props[PROP_PIXEL_FORMAT] =
    g_param_spec_object ("pixel-format",
                         P_("Pixel Format"),
                         P_("The GDK pixel format for creating the GL context"),
                         GDK_TYPE_GL_PIXEL_FORMAT,
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS);

  obj_props[PROP_CONTEXT] =
    g_param_spec_object ("context",
                         P_("Context"),
                         P_("The GL context"),
                         GDK_TYPE_GL_CONTEXT,
                         G_PARAM_READABLE |
                         G_PARAM_STATIC_STRINGS);

  gobject_class->set_property = gtk_gl_area_set_property;
  gobject_class->get_property = gtk_gl_area_get_property;
  gobject_class->dispose = gtk_gl_area_dispose;

  g_object_class_install_properties (gobject_class, LAST_PROP, obj_props);

  area_signals[CREATE_CONTEXT] =
    g_signal_new (I_("create-context"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkGLAreaClass, create_context),
                  create_context_accumulator, NULL,
                  _gtk_marshal_OBJECT__OBJECT,
                  GDK_TYPE_GL_CONTEXT, 1,
                  GDK_TYPE_GL_PIXEL_FORMAT);

  /**
   * GtkGLArea::render:
   * @area: the #GtkGLArea that emitted the signal
   * @context: the #GdkGLContext used by @area
   *
   * The ::render signal is emitted every time the contents
   * of the #GtkGLArea should be redrawn.
   *
   * The @context is bound to the @area prior to emitting this function,
   * and the buffers are flushed once the emission terminates.
   *
   * Returns: %TRUE to stop other handlers from being invoked for the event.
   *   %FALSE to propagate the event further.
   *
   * Since: 3.14
   */
  area_signals[RENDER] =
    g_signal_new (I_("render"),
		   G_TYPE_FROM_CLASS (gobject_class),
		   G_SIGNAL_RUN_LAST,
		   G_STRUCT_OFFSET (GtkGLAreaClass, render),
                   _gtk_boolean_handled_accumulator, NULL,
                   NULL,
		   G_TYPE_BOOLEAN, 1,
		   GDK_TYPE_GL_CONTEXT);
}

static void
gtk_gl_area_init (GtkGLArea *self)
{
  gtk_widget_set_has_window (GTK_WIDGET (self), TRUE);
}

/**
 * gtk_gl_area_new:
 * @pixel_format: a #GdkGLPixelFormat
 *
 * Creates a new #GtkGLArea widget using the given @pixel_format to
 * configure a #GdkGLContext.
 *
 * Returns: (transfer full): the newly created #GtkGLArea
 *
 * Since: 3.14
 */
GtkWidget *
gtk_gl_area_new (GdkGLPixelFormat *pixel_format)
{
  g_return_val_if_fail (GDK_IS_GL_PIXEL_FORMAT (pixel_format), NULL);

  return g_object_new (GTK_TYPE_GL_AREA,
                       "pixel-format", pixel_format,
                       NULL);
}

/**
 * gtk_gl_area_get_pixel_format:
 * @area: a #GtkGLArea
 *
 * Retrieves the #GdkGLPixelFormat used by @area.
 *
 * Returns: (transfer none): the #GdkGLPixelFormat
 *
 * Since: 3.14
 */
GdkGLPixelFormat *
gtk_gl_area_get_pixel_format (GtkGLArea *area)
{
  GtkGLAreaPrivate *priv = gtk_gl_area_get_instance_private (area);

  g_return_val_if_fail (GTK_IS_GL_AREA (area), NULL);

  return priv->pixel_format;
}

/**
 * gtk_gl_area_get_context:
 * @area: a #GtkGLArea
 *
 * Retrieves the #GdkGLContext used by @area.
 *
 * Returns: (transfer none): the #GdkGLContext
 *
 * Since: 3.14
 */
GdkGLContext *
gtk_gl_area_get_context (GtkGLArea *area)
{
  GtkGLAreaPrivate *priv = gtk_gl_area_get_instance_private (area);

  g_return_val_if_fail (GTK_IS_GL_AREA (area), NULL);

  return priv->context;
}

/**
 * gtk_gl_area_make_current:
 * @area: a #GtkGLArea
 *
 * Ensures that the #GdkGLContext used by @area is associated with
 * the #GtkGLArea.
 *
 * This function is automatically called before emitting the
 * #GtkGLArea::draw-with-context signal, and should not be called
 * by application code.
 *
 * Returns: %TRUE if the context was associated successfully with
 *  the widget
 *
 * Since: 3.14
 */
gboolean
gtk_gl_area_make_current (GtkGLArea *area)
{
  GtkGLAreaPrivate *priv = gtk_gl_area_get_instance_private (area);
  GtkWidget *widget;

  g_return_val_if_fail (GTK_IS_GL_AREA (area), FALSE);

  widget = GTK_WIDGET (area);
  g_return_val_if_fail (gtk_widget_get_realized (widget), FALSE);

  if (priv->context == NULL)
    return FALSE;

  gdk_gl_context_set_window (priv->context, gtk_widget_get_window (widget));

  return gdk_gl_context_make_current (priv->context);
}

/**
 * gtk_gl_area_flush_buffer:
 * @area: a #GtkGLArea
 *
 * Flushes the buffer associated with @area.
 *
 * This function is automatically called after emitting
 * the #GtkGLArea::draw-with-context signal, and should not
 * be called by application code.
 *
 * Since: 3.14
 */
void
gtk_gl_area_flush_buffer (GtkGLArea *area)
{
  GtkGLAreaPrivate *priv = gtk_gl_area_get_instance_private (area);

  g_return_if_fail (GTK_IS_GL_AREA (area));
  g_return_if_fail (gtk_widget_get_realized (GTK_WIDGET (area)));

  if (priv->context == NULL)
    return;

  gdk_gl_context_flush_buffer (priv->context);
}
