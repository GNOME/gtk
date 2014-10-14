/* The rendering code in here is taken from glxgears, which has the
 * following copyright notice:
 *
 * Copyright (C) 1999-2001  Brian Paul   All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * BRIAN PAUL BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
 * AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <math.h>
#include <epoxy/gl.h>

#include "gtkgears.h"

typedef struct {
  GLfloat view_rot[GTK_GEARS_N_AXIS];
  GLint gear1, gear2, gear3;
  GLfloat angle;
  gint64 first_frame_time;
  guint tick;
  GtkLabel *fps_label;
} GtkGearsPrivate;


G_DEFINE_TYPE_WITH_PRIVATE (GtkGears, gtk_gears, GTK_TYPE_GL_AREA);

static gboolean gtk_gears_render        (GtkGLArea     *area,
                                         GdkGLContext  *context);
static void     gtk_gears_size_allocate (GtkWidget     *widget,
                                         GtkAllocation *allocation);
static void     gtk_gears_realize       (GtkWidget     *widget);
static gboolean gtk_gears_tick          (GtkWidget     *widget,
                                         GdkFrameClock *frame_clock,
                                         gpointer       user_data);

GtkWidget *
gtk_gears_new (void)
{
  GtkWidget *gears;

  gears = g_object_new (gtk_gears_get_type (),
                        "has-depth-buffer", TRUE,
                        NULL);

  return gears;
}

static void
gtk_gears_init (GtkGears *gears)
{
  GtkGearsPrivate *priv = gtk_gears_get_instance_private (gears);

  priv->view_rot[GTK_GEARS_X_AXIS] = 20.0;
  priv->view_rot[GTK_GEARS_Y_AXIS] = 30.0;
  priv->view_rot[GTK_GEARS_Z_AXIS] = 20.0;

  priv->tick = gtk_widget_add_tick_callback (GTK_WIDGET (gears), gtk_gears_tick, gears, NULL);
}

static void
gtk_gears_finalize (GObject *obj)
{
  GtkGears *gears = GTK_GEARS (obj);
  GtkGearsPrivate *priv = gtk_gears_get_instance_private (gears);

  gtk_widget_remove_tick_callback (GTK_WIDGET (gears), priv->tick);

  g_clear_object (&priv->fps_label);

  G_OBJECT_CLASS (gtk_gears_parent_class)->finalize (obj);
}

static void
gtk_gears_class_init (GtkGearsClass *klass)
{
  GTK_GL_AREA_CLASS (klass)->render = gtk_gears_render;
  GTK_WIDGET_CLASS (klass)->realize = gtk_gears_realize;
  GTK_WIDGET_CLASS (klass)->size_allocate = gtk_gears_size_allocate;
  G_OBJECT_CLASS (klass)->finalize = gtk_gears_finalize;
}

/*
 *
 *  Draw a gear wheel.  You'll probably want to call this function when
 *  building a display list since we do a lot of trig here.
 *
 *  Input:  inner_radius - radius of hole at center
 *          outer_radius - radius at center of teeth
 *          width - width of gear
 *          teeth - number of teeth
 *          tooth_depth - depth of tooth
 */
static void
gear(GLfloat inner_radius, GLfloat outer_radius, GLfloat width,
     GLint teeth, GLfloat tooth_depth)
{
   GLint i;
   GLfloat r0, r1, r2;
   GLfloat angle, da;
   GLfloat u, v, len;

   r0 = inner_radius;
   r1 = outer_radius - tooth_depth / 2.0;
   r2 = outer_radius + tooth_depth / 2.0;

   da = 2.0 * G_PI / teeth / 4.0;

   glShadeModel(GL_FLAT);

   glNormal3f(0.0, 0.0, 1.0);

   /* draw front face */
   glBegin(GL_QUAD_STRIP);
   for (i = 0; i <= teeth; i++) {
      angle = i * 2.0 * M_PI / teeth;
      glVertex3f(r0 * cos(angle), r0 * sin(angle), width * 0.5);
      glVertex3f(r1 * cos(angle), r1 * sin(angle), width * 0.5);
      if (i < teeth) {
	 glVertex3f(r0 * cos(angle), r0 * sin(angle), width * 0.5);
	 glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da),
		    width * 0.5);
      }
   }
   glEnd();

   /* draw front sides of teeth */
   glBegin(GL_QUADS);
   da = 2.0 * M_PI / teeth / 4.0;
   for (i = 0; i < teeth; i++) {
      angle = i * 2.0 * M_PI / teeth;

      glVertex3f(r1 * cos(angle), r1 * sin(angle), width * 0.5);
      glVertex3f(r2 * cos(angle + da), r2 * sin(angle + da), width * 0.5);
      glVertex3f(r2 * cos(angle + 2 * da), r2 * sin(angle + 2 * da),
		 width * 0.5);
      glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da),
		 width * 0.5);
   }
   glEnd();

   glNormal3f(0.0, 0.0, -1.0);

   /* draw back face */
   glBegin(GL_QUAD_STRIP);
   for (i = 0; i <= teeth; i++) {
      angle = i * 2.0 * M_PI / teeth;
      glVertex3f(r1 * cos(angle), r1 * sin(angle), -width * 0.5);
      glVertex3f(r0 * cos(angle), r0 * sin(angle), -width * 0.5);
      if (i < teeth) {
	 glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da),
		    -width * 0.5);
	 glVertex3f(r0 * cos(angle), r0 * sin(angle), -width * 0.5);
      }
   }
   glEnd();

   /* draw back sides of teeth */
   glBegin(GL_QUADS);
   da = 2.0 * M_PI / teeth / 4.0;
   for (i = 0; i < teeth; i++) {
      angle = i * 2.0 * M_PI / teeth;

      glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da),
		 -width * 0.5);
      glVertex3f(r2 * cos(angle + 2 * da), r2 * sin(angle + 2 * da),
		 -width * 0.5);
      glVertex3f(r2 * cos(angle + da), r2 * sin(angle + da), -width * 0.5);
      glVertex3f(r1 * cos(angle), r1 * sin(angle), -width * 0.5);
   }
   glEnd();

   /* draw outward faces of teeth */
   glBegin(GL_QUAD_STRIP);
   for (i = 0; i < teeth; i++) {
      angle = i * 2.0 * M_PI / teeth;

      glVertex3f(r1 * cos(angle), r1 * sin(angle), width * 0.5);
      glVertex3f(r1 * cos(angle), r1 * sin(angle), -width * 0.5);
      u = r2 * cos(angle + da) - r1 * cos(angle);
      v = r2 * sin(angle + da) - r1 * sin(angle);
      len = sqrt(u * u + v * v);
      u /= len;
      v /= len;
      glNormal3f(v, -u, 0.0);
      glVertex3f(r2 * cos(angle + da), r2 * sin(angle + da), width * 0.5);
      glVertex3f(r2 * cos(angle + da), r2 * sin(angle + da), -width * 0.5);
      glNormal3f(cos(angle), sin(angle), 0.0);
      glVertex3f(r2 * cos(angle + 2 * da), r2 * sin(angle + 2 * da),
		 width * 0.5);
      glVertex3f(r2 * cos(angle + 2 * da), r2 * sin(angle + 2 * da),
		 -width * 0.5);
      u = r1 * cos(angle + 3 * da) - r2 * cos(angle + 2 * da);
      v = r1 * sin(angle + 3 * da) - r2 * sin(angle + 2 * da);
      glNormal3f(v, -u, 0.0);
      glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da),
		 width * 0.5);
      glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da),
		 -width * 0.5);
      glNormal3f(cos(angle), sin(angle), 0.0);
   }

   glVertex3f(r1 * cos(0), r1 * sin(0), width * 0.5);
   glVertex3f(r1 * cos(0), r1 * sin(0), -width * 0.5);

   glEnd();

   glShadeModel(GL_SMOOTH);

   /* draw inside radius cylinder */
   glBegin(GL_QUAD_STRIP);
   for (i = 0; i <= teeth; i++) {
      angle = i * 2.0 * M_PI / teeth;
      glNormal3f(-cos(angle), -sin(angle), 0.0);
      glVertex3f(r0 * cos(angle), r0 * sin(angle), -width * 0.5);
      glVertex3f(r0 * cos(angle), r0 * sin(angle), width * 0.5);
   }
   glEnd();
}

/* new window size or exposure */
static void
reshape(int width, int height)
{
  GLfloat h = (GLfloat) height / (GLfloat) width;

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glFrustum(-1.0, 1.0, -h, h, 5.0, 60.0);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glTranslatef(0.0, 0.0, -40.0);
}

static gboolean
gtk_gears_render (GtkGLArea    *area,
                  GdkGLContext *context)
{
  GtkGearsPrivate *priv = gtk_gears_get_instance_private (GTK_GEARS (area));

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glPushMatrix();
  glRotatef(priv->view_rot[GTK_GEARS_X_AXIS], 1.0, 0.0, 0.0);
  glRotatef(priv->view_rot[GTK_GEARS_Y_AXIS], 0.0, 1.0, 0.0);
  glRotatef(priv->view_rot[GTK_GEARS_Z_AXIS], 0.0, 0.0, 1.0);

  glPushMatrix();
  glTranslatef(-3.0, -2.0, 0.0);
  glRotatef(priv->angle, 0.0, 0.0, 1.0);
  glCallList(priv->gear1);
  glPopMatrix();

  glPushMatrix();
  glTranslatef(3.1, -2.0, 0.0);
  glRotatef(-2.0 * priv->angle - 9.0, 0.0, 0.0, 1.0);
  glCallList(priv->gear2);
  glPopMatrix();

  glPushMatrix();
  glTranslatef(-3.1, 4.2, 0.0);
  glRotatef(-2.0 * priv->angle - 25.0, 0.0, 0.0, 1.0);
  glCallList(priv->gear3);
  glPopMatrix();

  glPopMatrix();

  return TRUE;
}

static void
gtk_gears_size_allocate (GtkWidget     *widget,
                         GtkAllocation *allocation)
{
  GtkGLArea *glarea = GTK_GL_AREA (widget);

  GTK_WIDGET_CLASS (gtk_gears_parent_class)->size_allocate (widget, allocation);

  if (gtk_widget_get_realized (widget))
    {
      gtk_gl_area_make_current (glarea);
      reshape (allocation->width, allocation->height);
    }
}

static void
gtk_gears_realize (GtkWidget *widget)
{
  GtkGLArea *glarea = GTK_GL_AREA (widget);
  GtkGears *gears = GTK_GEARS(widget);
  GtkGearsPrivate *priv = gtk_gears_get_instance_private (gears);
  GtkAllocation allocation;
  static GLfloat pos[4] = { 5.0, 5.0, 10.0, 0.0 };
  static GLfloat red[4] = { 0.8, 0.1, 0.0, 1.0 };
  static GLfloat green[4] = { 0.0, 0.8, 0.2, 1.0 };
  static GLfloat blue[4] = { 0.2, 0.2, 1.0, 1.0 };

  GTK_WIDGET_CLASS (gtk_gears_parent_class)->realize (widget);

  gtk_gl_area_make_current (glarea);

  glLightfv(GL_LIGHT0, GL_POSITION, pos);
  glEnable(GL_CULL_FACE);
  glEnable(GL_LIGHTING);
  glEnable(GL_LIGHT0);
  glEnable(GL_DEPTH_TEST);

  /* make the gears */
  priv->gear1 = glGenLists(1);
  glNewList(priv->gear1, GL_COMPILE);
  glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, red);
  gear(1.0, 4.0, 1.0, 20, 0.7);
  glEndList();

  priv->gear2 = glGenLists(1);
  glNewList(priv->gear2, GL_COMPILE);
  glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, green);
  gear(0.5, 2.0, 2.0, 10, 0.7);
  glEndList();

  priv->gear3 = glGenLists(1);
  glNewList(priv->gear3, GL_COMPILE);
  glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, blue);
  gear(1.3, 2.0, 0.5, 10, 0.7);
  glEndList();

  glEnable(GL_NORMALIZE);

  gtk_widget_get_allocation (widget, &allocation);
  reshape (allocation.width, allocation.height);
}

static gboolean
gtk_gears_tick (GtkWidget     *widget,
                GdkFrameClock *frame_clock,
                gpointer       user_data)
{
  GtkGears *gears = GTK_GEARS (widget);
  GtkGearsPrivate *priv = gtk_gears_get_instance_private (gears);
  GdkFrameTimings *timings, *previous_timings;
  gint64 previous_frame_time = 0;
  gint64 frame_time;
  gint64 history_start, history_len;
  gint64 frame;
  char *s;

  frame = gdk_frame_clock_get_frame_counter (frame_clock);
  frame_time = gdk_frame_clock_get_frame_time (frame_clock);

  if (priv->first_frame_time == 0)
    {
      /* No need for changes on first frame */
      priv->first_frame_time = frame_time;
      if (priv->fps_label)
        gtk_label_set_label (priv->fps_label, "FPS: ---");
      return G_SOURCE_CONTINUE;
    }

  /* glxgears advances 70 degrees per second, so do the same */

  priv->angle = fmod ((frame_time - priv->first_frame_time) / (double)G_USEC_PER_SEC * 70.0, 360.0);

  gtk_widget_queue_draw (widget);

  history_start = gdk_frame_clock_get_history_start (frame_clock);

  if (priv->fps_label && frame % 60 == 0)
    {
      history_len = frame - history_start;
      if (history_len > 0)
        {
          previous_timings = gdk_frame_clock_get_timings (frame_clock, frame - history_len);
          previous_frame_time = gdk_frame_timings_get_frame_time (previous_timings);

          s = g_strdup_printf ("FPS: %-4.1f", (G_USEC_PER_SEC * history_len) / (double)(frame_time - previous_frame_time));
          gtk_label_set_label (priv->fps_label, s);
          g_free (s);
        }
    }

  timings = gdk_frame_clock_get_current_timings (frame_clock);
  previous_timings = gdk_frame_clock_get_timings (frame_clock,
                                                  gdk_frame_timings_get_frame_counter (timings) - 1);
  if (previous_timings != NULL)
    previous_frame_time = gdk_frame_timings_get_frame_time (previous_timings);

  return G_SOURCE_CONTINUE;
}

void
gtk_gears_set_axis (GtkGears *gears, int axis, double value)
{
  GtkGearsPrivate *priv = gtk_gears_get_instance_private (gears);

  if (axis < 0 || axis >= GTK_GEARS_N_AXIS)
    return;

  priv->view_rot[axis] = value;

  gtk_widget_queue_draw (GTK_WIDGET (gears));
}

double
gtk_gears_get_axis (GtkGears *gears, int axis)
{
  GtkGearsPrivate *priv = gtk_gears_get_instance_private (gears);

  if (axis < 0 || axis >= GTK_GEARS_N_AXIS)
    return 0.0;

  return priv->view_rot[axis];
}

void
gtk_gears_set_fps_label (GtkGears *gears, GtkLabel *label)
{
  GtkGearsPrivate *priv = gtk_gears_get_instance_private (gears);

  if (label)
    g_object_ref (label);

  g_clear_object (&priv->fps_label);

  priv->fps_label = label;
}
