/* GTK - The GIMP Toolkit
 *
 * gtkglarea.c: A GL drawing area
 *
 * Copyright © 2014  Emmanuele Bassi
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

#include "config.h"
#include "gtkglarea.h"
#include "gtkintl.h"
#include "gtkstylecontext.h"
#include "gtkmarshalers.h"
#include "gtkprivate.h"
#include "gtkrender.h"

#include <epoxy/gl.h>

/**
 * SECTION:gtkglarea
 * @Title: GtkGLArea
 * @Short_description: A widget for custom drawing with OpenGL
 *
 * #GtkGLArea is a widget that allows drawing with OpenGL.
 *
 * #GtkGLArea sets up its own #GdkGLContext for the window it creates, and
 * creates a custom GL framebuffer that the widget will do GL rendering onto.
 * It also ensures that this framebuffer is the default GL rendering target
 * when rendering.
 *
 * In order to draw, you have to connect to the #GtkGLArea::render signal,
 * or subclass #GtkGLArea and override the @GtkGLAreaClass.render() virtual
 * function.
 *
 * The #GtkGLArea widget ensures that the #GdkGLContext is associated with
 * the widget's drawing area, and it is kept updated when the size and
 * position of the drawing area changes.
 *
 * ## Drawing with GtkGLArea ##
 *
 * The simplest way to draw using OpenGL commands in a #GtkGLArea is to
 * create a widget instance and connect to the #GtkGLArea::render signal:
 *
 * |[<!-- language="C" -->
 *   // create a GtkGLArea instance
 *   GtkWidget *gl_area = gtk_gl_area_new ();
 *
 *   // connect to the "render" signal
 *   g_signal_connect (gl_area, "render", G_CALLBACK (render), NULL);
 * ]|
 *
 * The `render()` function will be called when the #GtkGLArea is ready
 * for you to draw its content:
 *
 * |[<!-- language="C" -->
 *   static gboolean
 *   render (GtkGLArea *area, GdkGLContext *context)
 *   {
 *     // inside this function it's safe to use GL; the given
 *     // #GdkGLContext has been made current to the drawable
 *     // surface used by the #GtkGLArea and the viewport has
 *     // already been set to be the size of the allocation
 *
 *     // we can start by clearing the buffer
 *     glClearColor (0, 0, 0, 0);
 *     glClear (GL_COLOR_BUFFER_BIT);
 *
 *     // draw your object
 *     draw_an_object ();
 *
 *     // we completed our drawing; the draw commands will be
 *     // flushed at the end of the signal emission chain, and
 *     // the buffers will be drawn on the window
 *     return TRUE;
 *   }
 * ]|
 *
 * The `draw_an_object()` function draws a 2D, gold-colored
 * triangle:
 *
 * |[<!-- language="C" -->
 *   static void
 *   draw_an_object (void)
 *   {
 *     // set the color
 *     glColor3f (1.0f, 0.85f, 0.35f);
 *
 *     // draw our triangle
 *     glBegin (GL_TRIANGLES);
 *     {
 *       glVertex3f ( 0.0f,  0.6f,  0.0f);
 *       glVertex3f (-0.2f, -0.3f,  0.0f);
 *       glVertex3f ( 0.2f, -0.3f,  0.0f);
 *     }
 *     glEnd ();
 *   }
 * ]|
 *
 * This is an extremely simple example; in a real-world application you
 * would probably replace the immediate mode drawing with persistent
 * geometry primitives, like a Vertex Buffer Object, and only redraw what
 * changed in your scene.
 *
 */

typedef struct {
  GdkGLContext *context;
  GError *error;

  gboolean have_buffers;

  guint frame_buffer;
  guint render_buffer;
  guint texture;
  guint depth_stencil_buffer;

  gboolean has_alpha;
  gboolean has_depth_buffer;
  gboolean has_stencil_buffer;

  gboolean needs_render;
  gboolean auto_render;
} GtkGLAreaPrivate;

enum {
  PROP_0,

  PROP_CONTEXT,
  PROP_HAS_ALPHA,
  PROP_HAS_DEPTH_BUFFER,
  PROP_HAS_STENCIL_BUFFER,

  PROP_AUTO_RENDER,
  LAST_PROP
};

static GParamSpec *obj_props[LAST_PROP] = { NULL, };

enum {
  RENDER,

  LAST_SIGNAL
};

static void gtk_gl_area_allocate_buffers (GtkGLArea *area, int width, int height);

static guint area_signals[LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE_WITH_PRIVATE (GtkGLArea, gtk_gl_area, GTK_TYPE_WIDGET)

static void
gtk_gl_area_dispose (GObject *gobject)
{
  GtkGLArea *area = GTK_GL_AREA (gobject);
  GtkGLAreaPrivate *priv = gtk_gl_area_get_instance_private (area);

  g_clear_object (&priv->context);

  G_OBJECT_CLASS (gtk_gl_area_parent_class)->dispose (gobject);
}

static void
gtk_gl_area_set_property (GObject      *gobject,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  switch (prop_id)
    {
    case PROP_AUTO_RENDER:
      gtk_gl_area_set_auto_render (GTK_GL_AREA(gobject),
				   g_value_get_boolean (value));
      break;

    case PROP_HAS_ALPHA:
      gtk_gl_area_set_has_alpha (GTK_GL_AREA(gobject),
                                 g_value_get_boolean (value));
      break;

    case PROP_HAS_DEPTH_BUFFER:
      gtk_gl_area_set_has_depth_buffer (GTK_GL_AREA(gobject),
                                        g_value_get_boolean (value));
      break;

    case PROP_HAS_STENCIL_BUFFER:
      gtk_gl_area_set_has_stencil_buffer (GTK_GL_AREA(gobject),
                                        g_value_get_boolean (value));
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
    case PROP_AUTO_RENDER:
      g_value_set_boolean (value, priv->auto_render);
      break;

    case PROP_HAS_ALPHA:
      g_value_set_boolean (value, priv->has_alpha);
      break;

    case PROP_HAS_DEPTH_BUFFER:
      g_value_set_boolean (value, priv->has_depth_buffer);
      break;

    case PROP_HAS_STENCIL_BUFFER:
      g_value_set_boolean (value, priv->has_stencil_buffer);
      break;

    case PROP_CONTEXT:
      g_value_set_object (value, priv->context);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
gtk_gl_area_realize (GtkWidget *widget)
{
  GtkGLAreaPrivate *priv = gtk_gl_area_get_instance_private ((GtkGLArea *) widget);
  GdkWindow *window;

  GTK_WIDGET_CLASS (gtk_gl_area_parent_class)->realize (widget);

  window = gtk_widget_get_window (widget);
  priv->context = gdk_window_create_gl_context (window,
                                                GDK_GL_PROFILE_DEFAULT,
                                                &priv->error);
}

/*
 * Creates all the buffer objects needed for rendering the scene
 */
static void
gtk_gl_area_create_buffers (GtkGLArea *area)
{
  GtkGLAreaPrivate *priv = gtk_gl_area_get_instance_private (area);
  GtkWidget *widget = GTK_WIDGET (area);
  int scale;

  gtk_widget_realize (widget);

  if (priv->context == NULL)
    return;

  if (priv->have_buffers)
    return;

  priv->have_buffers = TRUE;

  glGenFramebuffersEXT (1, &priv->frame_buffer);

  if (priv->has_alpha)
    /* For alpha we use textures as that is required for blending to work */
    glGenTextures (1, &priv->texture);
  else
    /* For non-alpha we use render buffers so we can blit instead of texture the result */
    glGenRenderbuffersEXT (1, &priv->render_buffer);

  if (priv->has_depth_buffer || priv->has_stencil_buffer)
    glGenRenderbuffersEXT (1, &priv->depth_stencil_buffer);

  scale = gtk_widget_get_scale_factor (widget);
  gtk_gl_area_allocate_buffers (area,
				gtk_widget_get_allocated_width (widget) * scale,
				gtk_widget_get_allocated_height (widget) * scale);
}

/*
 * Allocates space of the right type and size for all the buffers
 */
static void
gtk_gl_area_allocate_buffers (GtkGLArea *area, int width, int height)
{
  GtkGLAreaPrivate *priv = gtk_gl_area_get_instance_private (area);

  if (priv->context == NULL)
    return;

  if (priv->texture)
    {
      glBindTexture (GL_TEXTURE_2D, priv->texture);
      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_BGRA, GL_UNSIGNED_BYTE, NULL);
    }

  if (priv->render_buffer)
    {
      glBindRenderbufferEXT (GL_RENDERBUFFER_EXT, priv->render_buffer);
      glRenderbufferStorageEXT (GL_RENDERBUFFER_EXT, GL_RGB8, width, height);
    }

  if (priv->has_depth_buffer || priv->has_stencil_buffer)
    {
      glBindRenderbufferEXT (GL_RENDERBUFFER_EXT, priv->depth_stencil_buffer);
      if (priv->has_stencil_buffer)
	glRenderbufferStorageEXT (GL_RENDERBUFFER_EXT, GL_DEPTH24_STENCIL8_EXT, width, height);
      else
	glRenderbufferStorageEXT (GL_RENDERBUFFER_EXT, GL_DEPTH_COMPONENT24, width, height);
    }
}

static void
gtk_gl_area_maybe_allocate_buffers (GtkGLArea *area)
{
  GtkGLAreaPrivate *priv = gtk_gl_area_get_instance_private (area);
  GtkWidget *widget = GTK_WIDGET (area);
  int scale;

  if (!priv->have_buffers)
    return;

  gtk_gl_area_make_current (area);
  scale = gtk_widget_get_scale_factor (widget);
  gtk_gl_area_allocate_buffers (area,
				gtk_widget_get_allocated_width (widget) * scale,
				gtk_widget_get_allocated_height (widget) * scale);
}

/**
 * gtk_gl_area_attach_buffers:
 * @area: a #GtkGLArea
 *
 * Ensures that the @area framebuffer object is made the current draw
 * and read target, and that all the required buffers for the @area
 * are created and bound to the frambuffer.
 *
 * This function is automatically called before emitting the
 * #GtkGLArea::render signal, and doesn't normally need to be called
 * by application code.
 *
 * Since: 3.16
 */
void
gtk_gl_area_attach_buffers (GtkGLArea *area)
{
  GtkGLAreaPrivate *priv = gtk_gl_area_get_instance_private (area);

  gtk_gl_area_make_current (area);

  if (!priv->have_buffers)
    gtk_gl_area_create_buffers (area);

  glBindFramebufferEXT (GL_FRAMEBUFFER_EXT, priv->frame_buffer);

  if (priv->texture)
    glFramebufferTexture2D (GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
			    GL_TEXTURE_2D, priv->texture, 0);
  else if (priv->render_buffer)
    glFramebufferRenderbufferEXT (GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
				  GL_RENDERBUFFER_EXT, priv->render_buffer);

  if (priv->depth_stencil_buffer)
    {
      if (priv->has_depth_buffer)
	glFramebufferRenderbufferEXT (GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT,
				      GL_RENDERBUFFER_EXT, priv->depth_stencil_buffer);
      if (priv->has_stencil_buffer)
	glFramebufferRenderbufferEXT (GL_FRAMEBUFFER_EXT, GL_STENCIL_ATTACHMENT_EXT,
				      GL_RENDERBUFFER_EXT, priv->depth_stencil_buffer);
    }
}

static void
gtk_gl_area_delete_buffers (GtkGLArea *area)
{
  GtkGLAreaPrivate *priv = gtk_gl_area_get_instance_private (area);

  if (priv->context == NULL)
    return;

  if (!priv->have_buffers)
    return;

  priv->have_buffers = FALSE;

  if (priv->render_buffer != 0)
    {
      glDeleteRenderbuffersEXT(1, &priv->render_buffer);
      priv->render_buffer = 0;
    }

  if (priv->texture != 0)
    {
      glDeleteTextures(1, &priv->texture);
      priv->texture = 0;
    }

  if (priv->depth_stencil_buffer != 0)
    {
      glDeleteRenderbuffersEXT(1, &priv->depth_stencil_buffer);

      glBindFramebufferEXT (GL_FRAMEBUFFER_EXT, 0);
      glDeleteFramebuffersEXT (1, &priv->frame_buffer);
      priv->frame_buffer = 0;
    }
}

static void
gtk_gl_area_post_render (GtkGLArea *area)
{
  GtkGLAreaPrivate *priv = gtk_gl_area_get_instance_private (area);
  GLenum invalidate[4];
  int i = 0;

  if (priv->has_depth_buffer)
    invalidate[i++] = GL_DEPTH_ATTACHMENT;

  if (priv->has_stencil_buffer)
    invalidate[i++] = GL_STENCIL_ATTACHMENT;

  if (priv->auto_render)
    invalidate[i++] = GL_COLOR_ATTACHMENT0;

  glInvalidateFramebuffer (GL_FRAMEBUFFER, i, invalidate);
}


static void
gtk_gl_area_unrealize (GtkWidget *widget)
{
  GtkGLArea *area = GTK_GL_AREA (widget);
  GtkGLAreaPrivate *priv = gtk_gl_area_get_instance_private (area);

  if (priv->context != NULL)
    {
      if (priv->have_buffers)
	{
	  gtk_gl_area_make_current (area);
	  gtk_gl_area_delete_buffers (area);
	}

      /* Make sure to destroy if current */
      g_object_run_dispose (G_OBJECT (priv->context));
      g_object_unref (priv->context);
      priv->context = NULL;
    }

  g_clear_error (&priv->error);

  GTK_WIDGET_CLASS (gtk_gl_area_parent_class)->unrealize (widget);
}

static void
gtk_gl_area_size_allocate (GtkWidget     *widget,
                           GtkAllocation *allocation)
{
  GtkGLArea *area = GTK_GL_AREA (widget);
  GtkGLAreaPrivate *priv = gtk_gl_area_get_instance_private (area);

  GTK_WIDGET_CLASS (gtk_gl_area_parent_class)->size_allocate (widget, allocation);

  priv->needs_render = TRUE;
  gtk_gl_area_maybe_allocate_buffers (area);
}

static void
gtk_gl_area_draw_error_screen (GtkGLArea *area,
                               cairo_t   *cr,
                               gint       width,
                               gint       height)
{
  GtkGLAreaPrivate *priv = gtk_gl_area_get_instance_private (area);
  PangoLayout *layout;
  int layout_height;

  layout = gtk_widget_create_pango_layout (GTK_WIDGET (area),
                                           priv->error->message);
  pango_layout_set_width (layout, width * PANGO_SCALE);
  pango_layout_set_alignment (layout, PANGO_ALIGN_CENTER);
  pango_layout_get_pixel_size (layout, NULL, &layout_height);
  gtk_render_layout (gtk_widget_get_style_context (GTK_WIDGET (area)),
                     cr,
                     0, (height - layout_height) / 2,
                     layout);

  g_object_unref (layout);
}

static gboolean
gtk_gl_area_draw (GtkWidget *widget,
                  cairo_t   *cr)
{
  GtkGLArea *area = GTK_GL_AREA (widget);
  GtkGLAreaPrivate *priv = gtk_gl_area_get_instance_private (area);
  gboolean unused;
  int w, h, scale;
  GLenum status;

  if (priv->context == NULL)
    {
      gtk_gl_area_draw_error_screen (area,
                                     cr,
                                     gtk_widget_get_allocated_width (widget),
                                     gtk_widget_get_allocated_height (widget));
      return FALSE;
    }

  gtk_gl_area_make_current (area);

  gtk_gl_area_attach_buffers (area);

 if (priv->has_depth_buffer)
   glEnable (GL_DEPTH_TEST);
 else
   glDisable (GL_DEPTH_TEST);

  scale = gtk_widget_get_scale_factor (widget);
  w = gtk_widget_get_allocated_width (widget) * scale;
  h = gtk_widget_get_allocated_height (widget) * scale;
 
  status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
  if (status ==  GL_FRAMEBUFFER_COMPLETE_EXT)
    {
      if (priv->needs_render || priv->auto_render)
	{
	  glViewport(0, 0, w, h);
	  g_signal_emit (area, area_signals[RENDER], 0, priv->context, &unused);
	}
      priv->needs_render = FALSE;

      gdk_cairo_draw_from_gl (cr,
                              gtk_widget_get_window (widget),
                              priv->texture ? priv->texture : priv->render_buffer,
                              priv->texture ? GL_TEXTURE : GL_RENDERBUFFER,
                              scale, 0, 0, w, h);
      gtk_gl_area_make_current (area);
      gtk_gl_area_post_render (area);
    }
  else
    {
      g_print ("fb setup not supported\n");
    }

  return TRUE;
}

static void
gtk_gl_area_class_init (GtkGLAreaClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  widget_class->realize = gtk_gl_area_realize;
  widget_class->unrealize = gtk_gl_area_unrealize;
  widget_class->size_allocate = gtk_gl_area_size_allocate;
  widget_class->draw = gtk_gl_area_draw;

  gtk_widget_class_set_accessible_role (widget_class, ATK_ROLE_DRAWING_AREA);

  /**
   * GtkGLArea:context:
   *
   * The #GdkGLContext used by the #GtkGLArea widget.
   *
   * The #GtkGLArea widget is responsible for creating the #GdkGLContext
   * instance. If you need to render with other kinds of buffers (stencil,
   * depth, etc), use render buffers.
   *
   * Since: 3.16
   */
  obj_props[PROP_CONTEXT] =
    g_param_spec_object ("context",
                         P_("Context"),
                         P_("The GL context"),
                         GDK_TYPE_GL_CONTEXT,
                         G_PARAM_READABLE |
                         G_PARAM_STATIC_STRINGS);

  /**
   * GtkGLArea:auto-render:
   *
   * If set to %TRUE the #GtkGLArea::render signal will be emitted every time the widget
   * draws. This is the default and is useful if drawing the widget is fastr.
   *
   * If set to %FALSE the data from previous rendering is kept around and will be used
   * for drawing the widget the next time, unless the window is resized. In order to
   * force a rendering gtk_gl_area_queue_render() must be called. This mode is useful
   * when the scene changes seldom, but takes a long time to redraw.
   *
   * Since: 3.16
   */
  obj_props[PROP_AUTO_RENDER] =
    g_param_spec_boolean ("auto-render",
                          P_("Auto render"),
                          P_("Whether the gl area renders on each redraw"),
                          TRUE,
                          GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkGLArea:has-alpha:
   *
   * If set to %TRUE the buffer allocated by the widget will have an alpha channel component,
   * and when rendering to the window the result will be composited over whatever is below
   * the widget.
   *
   * If set to %FALSE there will be no alpha channel, and the buffer will fully replace anything
   * below the widget.
   *
   * Since: 3.16
   */
  obj_props[PROP_HAS_ALPHA] =
    g_param_spec_boolean ("has-alpha",
                          P_("Has alpha"),
                          P_("Whether the gl area color buffer has an alpha component"),
                          FALSE,
                          GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkGLArea:has-depth-buffer:
   *
   * If set to %TRUE the widget will allocate and enable a depth buffer for the target
   * framebuffer.
   *
   * Since: 3.16
   */
  obj_props[PROP_HAS_DEPTH_BUFFER] =
    g_param_spec_boolean ("has-depth-buffer",
                          P_("Has depth buffer"),
                          P_("Whether a depth buffer is allocated"),
                          FALSE,
                          GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkGLArea:has-stencil-buffer:
   *
   * If set to %TRUE the widget will allocate and enable a stencil buffer for the target
   * framebuffer.
   *
   * Since: 3.16
   */
  obj_props[PROP_HAS_STENCIL_BUFFER] =
    g_param_spec_boolean ("has-stencil-buffer",
                          P_("Has stencil buffer"),
                          P_("Whether a stencil buffer is allocated"),
                          FALSE,
                          GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  gobject_class->set_property = gtk_gl_area_set_property;
  gobject_class->get_property = gtk_gl_area_get_property;
  gobject_class->dispose = gtk_gl_area_dispose;

  g_object_class_install_properties (gobject_class, LAST_PROP, obj_props);

  /**
   * GtkGLArea::render:
   * @area: the #GtkGLArea that emitted the signal
   * @context: the #GdkGLContext used by @area
   *
   * The ::render signal is emitted every time the contents
   * of the #GtkGLArea should be redrawn.
   *
   * The @context is bound to the @area prior to emitting this function,
   * and the buffers are painted to the window once the emission terminates.
   *
   * Returns: %TRUE to stop other handlers from being invoked for the event.
   *   %FALSE to propagate the event further.
   *
   * Since: 3.16
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
gtk_gl_area_init (GtkGLArea *area)
{
  GtkGLAreaPrivate *priv = gtk_gl_area_get_instance_private (area);

  gtk_widget_set_has_window (GTK_WIDGET (area), FALSE);
  gtk_widget_set_app_paintable (GTK_WIDGET (area), TRUE);

  priv->auto_render = TRUE;
  priv->needs_render = TRUE;
}

/**
 * gtk_gl_area_new:
 *
 * Creates a new #GtkGLArea widget.
 *
 * Returns: (transfer full): the newly created #GtkGLArea
 *
 * Since: 3.16
 */
GtkWidget *
gtk_gl_area_new (void)
{
  return g_object_new (GTK_TYPE_GL_AREA, NULL);
}

/**
 * gtk_gl_area_get_has_alpha:
 * @area: a #GtkGLArea
 *
 * Returns whether the area has an alpha component.
 *
 * Returns: %TRUE if the @area has an alpha component, %FALSE otherwise
 *
 * Since: 3.16
 */
gboolean
gtk_gl_area_get_has_alpha (GtkGLArea *area)
{
  GtkGLAreaPrivate *priv = gtk_gl_area_get_instance_private (area);

  g_return_val_if_fail (GTK_IS_GL_AREA (area), FALSE);

  return priv->has_alpha;
}

/**
 * gtk_gl_area_set_has_alpha:
 * @area: a #GtkGLArea
 * @has_alpha: %TRUE to add an alpha component
 *
 * If @has_alpha is %TRUE the buffer allocated by the widget will have
 * an alpha channel component, and when rendering to the window the
 * result will be composited over whatever is below the widget.
 *
 * If @has_alpha is %FALSE there will be no alpha channel, and the
 * buffer will fully replace anything below the widget.
 *
 * Since: 3.16
 */
void
gtk_gl_area_set_has_alpha (GtkGLArea *area,
                           gboolean   has_alpha)
{
  GtkGLAreaPrivate *priv = gtk_gl_area_get_instance_private (area);

  g_return_if_fail (GTK_IS_GL_AREA (area));

  has_alpha = !!has_alpha;

  if (priv->has_alpha != has_alpha)
    {
      priv->has_alpha = has_alpha;

      g_object_notify (G_OBJECT (area), "has-alpha");

      gtk_gl_area_maybe_allocate_buffers (area);
    }
}

/**
 * gtk_gl_area_get_has_depth_buffer:
 * @area: a #GtkGLArea
 *
 * Returns whether the area has a depth buffer.
 *
 * Returns: %TRUE if the @area has a depth buffer, %FALSE otherwise
 *
 * Since: 3.16
 */
gboolean
gtk_gl_area_get_has_depth_buffer (GtkGLArea *area)
{
  GtkGLAreaPrivate *priv = gtk_gl_area_get_instance_private (area);

  g_return_val_if_fail (GTK_IS_GL_AREA (area), FALSE);

  return priv->has_depth_buffer;
}

/**
 * gtk_gl_area_set_has_depth_buffer:
 * @area: a #GtkGLArea
 * @has_depth_buffer: %TRUE to add a depth buffer
 *
 * If @has_depth_buffer is %TRUE the widget will allocate and
 * enable a depth buffer for the target framebuffer. Otherwise
 * there will be none.
 *
 * Since: 3.16
 */
void
gtk_gl_area_set_has_depth_buffer (GtkGLArea *area,
                                  gboolean   has_depth_buffer)
{
  GtkGLAreaPrivate *priv = gtk_gl_area_get_instance_private (area);

  g_return_if_fail (GTK_IS_GL_AREA (area));

  has_depth_buffer = !!has_depth_buffer;

  if (priv->has_depth_buffer != has_depth_buffer)
    {
      priv->has_depth_buffer = has_depth_buffer;

      g_object_notify (G_OBJECT (area), "has-depth-buffer");

      gtk_gl_area_maybe_allocate_buffers (area);
    }
}

/**
 * gtk_gl_area_get_has_stencil_buffer:
 * @area: a #GtkGLArea
 *
 * Returns whether the area has a stencil buffer.
 *
 * Returns: %TRUE if the @area has a stencil buffer, %FALSE otherwise
 *
 * Since: 3.16
 */
gboolean
gtk_gl_area_get_has_stencil_buffer (GtkGLArea *area)
{
  GtkGLAreaPrivate *priv = gtk_gl_area_get_instance_private (area);

  g_return_val_if_fail (GTK_IS_GL_AREA (area), FALSE);

  return priv->has_stencil_buffer;
}

/**
 * gtk_gl_area_set_has_stencil_buffer:
 * @area: a #GtkGLArea
 * @has_stencil_buffer: %TRUE to add a stencil buffer
 *
 * If @has_stencil_buffer is %TRUE the widget will allocate and
 * enable a stencil buffer for the target framebuffer. Otherwise
 * there will be none.
 *
 * Since: 3.16
 */
void
gtk_gl_area_set_has_stencil_buffer (GtkGLArea *area,
				    gboolean   has_stencil_buffer)
{
  GtkGLAreaPrivate *priv = gtk_gl_area_get_instance_private (area);

  g_return_if_fail (GTK_IS_GL_AREA (area));

  has_stencil_buffer = !!has_stencil_buffer;

  if (priv->has_stencil_buffer != has_stencil_buffer)
    {
      priv->has_stencil_buffer = has_stencil_buffer;

      g_object_notify (G_OBJECT (area), "has-stencil-buffer");

      gtk_gl_area_maybe_allocate_buffers (area);
    }
}

/**
 * gtk_gl_area_queue_render:
 * @area: a #GtkGLArea
 *
 * Marks the currently rendered data (if any) as invalid, and queues a
 * redraw of the widget, ensuring that the #GtkGLArea::render signal
 * is emitted during the draw.
 *
 * This is only needed when the gtk_gl_area_set_auto_render() has
 * been called with a %FALSE value. The default behaviour is to
 * emit #GtkGLArea::render on each draw.
 *
 * Since: 3.16
 */
void
gtk_gl_area_queue_render (GtkGLArea *area)
{
  GtkGLAreaPrivate *priv = gtk_gl_area_get_instance_private (area);

  g_return_if_fail (GTK_IS_GL_AREA (area));

  priv->needs_render = TRUE;

  gtk_widget_queue_draw (GTK_WIDGET (area));
}


/**
 * gtk_gl_area_get_auto_render:
 * @area: a #GtkGLArea
 *
 * Returns whether the area is in auto render mode or not.
 *
 * Returns: %TRUE if the @area is auto rendering, %FALSE otherwise
 *
 * Since: 3.16
 */
gboolean
gtk_gl_area_get_auto_render (GtkGLArea *area)
{
  GtkGLAreaPrivate *priv = gtk_gl_area_get_instance_private (area);

  g_return_val_if_fail (GTK_IS_GL_AREA (area), FALSE);

  return priv->auto_render;
}

/**
 * gtk_gl_area_set_auto_render:
 * @area: a #GtkGLArea
 * @auto_render: a boolean
 *
 * If @auto_render is %TRUE the #GtkGLArea::render signal will be
 * emitted every time the widget draws. This is the default and is
 * useful if drawing the widget is fastr.
 *
 * If @auto_render is %FALSE the data from previous rendering is kept
 * around and will be used for drawing the widget the next time,
 * unless the window is resized. In order to force a rendering
 * gtk_gl_area_queue_render() must be called. This mode is useful when
 * the scene changes seldom, but takes a long time to redraw.
 *
 * Since: 3.16
 */
void
gtk_gl_area_set_auto_render (GtkGLArea *area,
			     gboolean   auto_render)
{
  GtkGLAreaPrivate *priv = gtk_gl_area_get_instance_private (area);

  g_return_if_fail (GTK_IS_GL_AREA (area));

  auto_render = !!auto_render;

  if (priv->auto_render != auto_render)
    {
      priv->auto_render = auto_render;

      g_object_notify (G_OBJECT (area), "auto-render");

      if (auto_render)
	gtk_widget_queue_draw (GTK_WIDGET (area));
    }
}

/**
 * gtk_gl_area_get_context:
 * @area: a #GtkGLArea
 *
 * Retrieves the #GdkGLContext used by @area.
 *
 * Returns: (transfer none): the #GdkGLContext
 *
 * Since: 3.16
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
 * #GtkGLArea::render signal, and doesn't normally need to be called
 * by application code.
 *
 * Since: 3.16
 */
void
gtk_gl_area_make_current (GtkGLArea *area)
{
  GtkGLAreaPrivate *priv = gtk_gl_area_get_instance_private (area);
  GtkWidget *widget;

  g_return_if_fail (GTK_IS_GL_AREA (area));

  widget = GTK_WIDGET (area);

  g_return_if_fail (gtk_widget_get_realized (widget));

  if (priv->context)
    gdk_gl_context_make_current (priv->context);
}
