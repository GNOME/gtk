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
  GLuint framebuffer;
  gboolean has_alpha;
  gboolean has_depth_buffer;
} GtkGLAreaPrivate;

enum {
  PROP_0,

  PROP_CONTEXT,
  PROP_HAS_ALPHA,
  PROP_HAS_DEPTH_BUFFER,

  LAST_PROP
};

static GParamSpec *obj_props[LAST_PROP] = { NULL, };

enum {
  RENDER,

  LAST_SIGNAL
};

static guint area_signals[LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE_WITH_PRIVATE (GtkGLArea, gtk_gl_area, GTK_TYPE_WIDGET)

static void
gtk_gl_area_dispose (GObject *gobject)
{
  GtkGLArea *self = GTK_GL_AREA (gobject);
  GtkGLAreaPrivate *priv = gtk_gl_area_get_instance_private (self);

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
    case PROP_HAS_ALPHA:
      gtk_gl_area_set_has_alpha (GTK_GL_AREA(gobject),
                                 g_value_get_boolean (value));
      break;

    case PROP_HAS_DEPTH_BUFFER:
      gtk_gl_area_set_has_depth_buffer (GTK_GL_AREA(gobject),
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
    case PROP_HAS_ALPHA:
      g_value_set_boolean (value, priv->has_alpha);
      break;

    case PROP_HAS_DEPTH_BUFFER:
      g_value_set_boolean (value, priv->has_depth_buffer);
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
                                                NULL);
  if (priv->context != NULL)
    {
      gdk_gl_context_make_current (priv->context);
      glGenFramebuffersEXT (1, &priv->framebuffer);
      glBindFramebufferEXT (GL_FRAMEBUFFER_EXT, priv->framebuffer);
    }
}

static void
gtk_gl_area_unrealize (GtkWidget *widget)
{
  GtkGLArea *self = GTK_GL_AREA (widget);
  GtkGLAreaPrivate *priv = gtk_gl_area_get_instance_private (self);

  if (priv->context != NULL)
    {
      if (priv->framebuffer != 0)
	{
	  gtk_gl_area_make_current (self);
	  /* Bind 0, which means render to back buffer, as a result, fb is unbound */
	  glBindFramebufferEXT (GL_FRAMEBUFFER_EXT, 0);
	  glDeleteFramebuffersEXT (1, &priv->framebuffer);
	  priv->framebuffer = 0;
	}
      else
	g_warning ("can't free framebuffer");

      gdk_gl_context_clear_current ();
    }

  GTK_WIDGET_CLASS (gtk_gl_area_parent_class)->unrealize (widget);
}

static void
gtk_gl_area_size_allocate (GtkWidget     *widget,
                           GtkAllocation *allocation)
{
  GTK_WIDGET_CLASS (gtk_gl_area_parent_class)->size_allocate (widget, allocation);
}

static gboolean
gtk_gl_area_draw (GtkWidget *widget,
                  cairo_t   *cr)
{
  GtkGLArea *self = GTK_GL_AREA (widget);
  GtkGLAreaPrivate *priv = gtk_gl_area_get_instance_private (self);
  gboolean unused;
  int w, h, scale;
  GLuint color_rb = 0, depth_rb = 0, color_tex = 0;
  GLenum status;

  if (priv->context == NULL)
    return FALSE;

  gtk_gl_area_make_current (self);

  scale = gtk_widget_get_scale_factor (widget);
  w = gtk_widget_get_allocated_width (widget) * scale;
  h = gtk_widget_get_allocated_height (widget) * scale;

  if (priv->has_alpha)
    {
      /* For alpha we use textures as that is required for blending to work */
      glGenTextures (1, &color_tex);
      glBindTexture (GL_TEXTURE_2D, color_tex);
      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_BGRA, GL_UNSIGNED_BYTE, NULL);
      glFramebufferTexture2DEXT (GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
                                 GL_TEXTURE_2D, color_tex, 0);
    }
  else
    {
      /* For non-alpha we use render buffers so we can blit instead of texture the result */
      glGenRenderbuffersEXT (1, &color_rb);
      glBindRenderbufferEXT (GL_RENDERBUFFER_EXT, color_rb);
      glRenderbufferStorageEXT (GL_RENDERBUFFER_EXT, GL_RGB8, w, h);
      glFramebufferRenderbufferEXT (GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
                                    GL_RENDERBUFFER_EXT, color_rb);
    }

  if (priv->has_depth_buffer)
    {
      glGenRenderbuffersEXT (1, &depth_rb);
      glBindRenderbufferEXT (GL_RENDERBUFFER_EXT, depth_rb);
      /* TODO: Pick actual requested depth */
      glRenderbufferStorageEXT (GL_RENDERBUFFER_EXT, GL_DEPTH_COMPONENT24, w, h);
      glFramebufferRenderbufferEXT (GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT,
				    GL_RENDERBUFFER_EXT, depth_rb);
      glEnable (GL_DEPTH_TEST);
    }
  else
    glDisable (GL_DEPTH_TEST);

  status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
  if (status ==  GL_FRAMEBUFFER_COMPLETE_EXT)
    {
      glViewport(0, 0, w, h);

      g_signal_emit (self, area_signals[RENDER], 0, priv->context, &unused);

      gdk_cairo_draw_from_gl (cr,
                              gtk_widget_get_window (widget),
                              color_tex ? color_tex : color_rb,
                              color_tex ? GL_TEXTURE : GL_RENDERBUFFER,
                              scale, 0, 0, w, h);

      gtk_gl_area_make_current (self);
    }
  else
    {
      g_print ("fb setup not supported\n");
    }

  if (color_tex != 0)
    glDeleteTextures(1, &color_tex);

  if (color_rb != 0)
    glDeleteRenderbuffersEXT(1, &color_rb);

  if (depth_rb != 0)
    glDeleteRenderbuffersEXT(1, &depth_rb);

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

static void
gtk_gl_area_class_init (GtkGLAreaClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  widget_class->screen_changed = gtk_gl_area_screen_changed;
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
   * instance. See the #GtkGLArea::create-context signal on how to
   * override the default behavior.
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
   * GtkGLArea:has-alpha:
   *
   * If set to #TRUE the buffer allocated by the widget will have an alpha channel component,
   * and when rendering to the window the result will be composited over whatever is below
   * the widget.
   *
   * If set to #FALSE there will be no alpha channel, and the buffer will fully replace anything
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
   * If set to #TRUE the widget will allocate and enable a depth buffer for the target
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
gtk_gl_area_init (GtkGLArea *self)
{
  gtk_widget_set_has_window (GTK_WIDGET (self), FALSE);
  gtk_widget_set_app_paintable (GTK_WIDGET (self), TRUE);
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
  return g_object_new (GTK_TYPE_GL_AREA,
                       NULL);
}

/**
 * gtk_gl_area_get_has_alpha:
 * @area: a #GtkGLArea
 *
 * Returns: @true if the @area has an alpha component, @false otherwise
 *
 * Since: 3.16
 */
gboolean
gtk_gl_area_get_has_alpha (GtkGLArea        *area)
{
  GtkGLAreaPrivate *priv = gtk_gl_area_get_instance_private (area);

  g_return_val_if_fail (GTK_IS_GL_AREA (area), FALSE);

  return priv->has_alpha;
}

/**
 * gtk_gl_area_set_has_alpha:
 * @area: a #GtkGLArea
 * @has_alpha: a boolean
 *
 * If @has_alpha is #TRUE the buffer allocated by the widget will have an alpha channel component,
 * and when rendering to the window the result will be composited over whatever is below
 * the widget.
 *
 * If @has_alpha is #FALSE there will be no alpha channel, and the buffer will fully replace anything
 * below the widget.
 *
 * Since: 3.16
 */
void
gtk_gl_area_set_has_alpha (GtkGLArea        *area,
                           gboolean          has_alpha)
{
  GtkGLAreaPrivate *priv = gtk_gl_area_get_instance_private (area);

  g_return_if_fail (GTK_IS_GL_AREA (area));

  has_alpha = !!has_alpha;

  if (priv->has_alpha != has_alpha)
    {
      priv->has_alpha = has_alpha;

      g_object_notify (G_OBJECT (area), "has-alpha");
    }
}

/**
 * gtk_gl_area_get_has_depth_buffer:
 * @area: a #GtkGLArea
 *
 * Returns: @true if the @area has an depth buffer, @false otherwise
 *
 * Since: 3.16
 */
gboolean
gtk_gl_area_get_has_depth_buffer (GtkGLArea        *area)
{
  GtkGLAreaPrivate *priv = gtk_gl_area_get_instance_private (area);

  g_return_val_if_fail (GTK_IS_GL_AREA (area), FALSE);

  return priv->has_depth_buffer;
}

/**
 * gtk_gl_area_set_has_depth_buffer:
 * @area: a #GtkGLArea
 * @has_depth_buffer: a boolean
 *
 * If @has_depth_buffer is #TRUE the widget will allocate and enable a depth buffer for the target
 * framebuffer. Otherwise there will be none.
 *
 * Since: 3.16
 */
void
gtk_gl_area_set_has_depth_buffer (GtkGLArea        *area,
                                  gboolean          has_depth_buffer)
{
  GtkGLAreaPrivate *priv = gtk_gl_area_get_instance_private (area);

  g_return_if_fail (GTK_IS_GL_AREA (area));

  has_depth_buffer = !!has_depth_buffer;

  if (priv->has_depth_buffer != has_depth_buffer)
    {
      priv->has_depth_buffer = has_depth_buffer;

      g_object_notify (G_OBJECT (area), "has-depth-buffer");
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
 * #GtkGLArea::render signal, and should not be called by
 * application code.
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
