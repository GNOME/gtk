/*
 * Copyright (C) 2009 Carlos Garnacho  <carlosg@gnome.org>
 *
 * This work is provided "as is"; redistribution and modification
 * in whole or in part, in any medium, physical or electronic is
 * permitted without restriction.
 *
 * This work is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * In no event shall the authors or contributors be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability, whether
 * in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even
 * if advised of the possibility of such damage.
 */

#include "testcoordinatewidget.h"

#define TEST_COORDINATE_WIDGET_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), TEST_TYPE_COORDINATE_WIDGET, TestCoordinateWidgetPrivate))

typedef struct TestCoordinateWidgetPrivate TestCoordinateWidgetPrivate;

struct TestCoordinateWidgetPrivate
{
  GtkDeviceGroup *group;
  GHashTable *coordinates;
};

static void test_coordinate_widget_class_init        (TestCoordinateWidgetClass *klass);
static void test_coordinate_widget_init              (TestCoordinateWidget      *coordinate);

static void test_coordinate_widget_destroy           (GtkObject          *object);

static gboolean test_coordinate_widget_button_press   (GtkWidget          *widget,
                                                       GdkEventButton     *event);
static gboolean test_coordinate_widget_button_release (GtkWidget          *widget,
                                                       GdkEventButton     *event);
static gboolean test_coordinate_widget_expose         (GtkWidget          *widget,
                                                       GdkEventExpose     *event);

static void     test_coordinate_widget_multidevice_event (GtkWidget           *widget,
                                                          GtkDeviceGroup      *group,
                                                          GtkMultiDeviceEvent *event);

G_DEFINE_TYPE (TestCoordinateWidget, test_coordinate_widget, GTK_TYPE_DRAWING_AREA)


static void
test_coordinate_widget_class_init (TestCoordinateWidgetClass *klass)
{
  GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->destroy = test_coordinate_widget_destroy;

  widget_class->button_press_event = test_coordinate_widget_button_press;
  widget_class->button_release_event = test_coordinate_widget_button_release;
  widget_class->expose_event = test_coordinate_widget_expose;

  g_type_class_add_private (klass, sizeof (TestCoordinateWidgetPrivate));
}

static void
test_coordinate_widget_init (TestCoordinateWidget *coord)
{
  TestCoordinateWidgetPrivate *priv;
  GtkWidget *widget;

  priv = TEST_COORDINATE_WIDGET_GET_PRIVATE (coord);
  widget = GTK_WIDGET (coord);

  priv->coordinates = g_hash_table_new_full (g_direct_hash,
                                             g_direct_equal,
                                             (GDestroyNotify) g_object_unref,
                                             (GDestroyNotify) g_free);

  gtk_widget_add_events (widget,
                         (GDK_BUTTON_PRESS_MASK |
                          GDK_BUTTON_RELEASE_MASK |
                          GDK_POINTER_MOTION_MASK |
                          GDK_ENTER_NOTIFY_MASK |
                          GDK_LEAVE_NOTIFY_MASK));

  priv->group = gtk_widget_create_device_group (GTK_WIDGET (coord));
  gtk_widget_set_support_multidevice (widget, TRUE);

  /* Multidevice events are not exposed through GtkWidgetClass */
  g_signal_connect (coord, "multidevice-event",
                    G_CALLBACK (test_coordinate_widget_multidevice_event), NULL);
}

static void
test_coordinate_widget_destroy (GtkObject *object)
{
  TestCoordinateWidgetPrivate *priv;

  priv = TEST_COORDINATE_WIDGET_GET_PRIVATE (object);

  if (priv->coordinates)
    {
      g_hash_table_destroy (priv->coordinates);
      priv->coordinates = NULL;
    }

  if (priv->group)
    {
      gtk_widget_remove_device_group (GTK_WIDGET (object), priv->group);
      priv->group = NULL;
    }

  GTK_OBJECT_CLASS (test_coordinate_widget_parent_class)->destroy (object);
}

static gboolean
test_coordinate_widget_button_press (GtkWidget      *widget,
                                     GdkEventButton *event)
{
  TestCoordinateWidgetPrivate *priv;
  GdkDevice *device;

  priv = TEST_COORDINATE_WIDGET_GET_PRIVATE (widget);
  device = gdk_event_get_device ((GdkEvent *) event);

  gtk_device_group_add_device (priv->group, device);

  return FALSE;
}

static gboolean
test_coordinate_widget_button_release (GtkWidget      *widget,
                                       GdkEventButton *event)
{
  TestCoordinateWidgetPrivate *priv;
  GdkDevice *device;

  priv = TEST_COORDINATE_WIDGET_GET_PRIVATE (widget);
  device = gdk_event_get_device ((GdkEvent *) event);

  gtk_device_group_remove_device (priv->group, device);

  return FALSE;
}

static void
test_coordinate_widget_multidevice_event (GtkWidget           *widget,
                                          GtkDeviceGroup      *group,
                                          GtkMultiDeviceEvent *event)
{
  TestCoordinateWidgetPrivate *priv;
  GdkEventMotion *ev;
  GdkPoint *point;

  priv = TEST_COORDINATE_WIDGET_GET_PRIVATE (widget);

  if (event->type == GTK_EVENT_DEVICE_REMOVED)
    g_hash_table_remove (priv->coordinates, event->updated_device);
  else
    {
      point = g_hash_table_lookup (priv->coordinates, event->updated_device);
      ev = event->updated_event;

      if (G_UNLIKELY (!point))
        {
          point = g_new (GdkPoint, 1);
          g_hash_table_insert (priv->coordinates,
                               g_object_ref (event->updated_device),
                               point);
        }

      point->x = ev->x;
      point->y = ev->y;
    }

  gtk_widget_queue_draw (widget);
}

static gboolean
test_coordinate_widget_expose (GtkWidget      *widget,
                               GdkEventExpose *event)
{
  TestCoordinateWidgetPrivate *priv;
  GList *coords, *c;
  cairo_t *cr;

  priv = TEST_COORDINATE_WIDGET_GET_PRIVATE (widget);
  cr = gdk_cairo_create (widget->window);

  coords = g_hash_table_get_values (priv->coordinates);

  cairo_set_source_rgb (cr, 1., 1., 1.);
  cairo_rectangle (cr,
                   widget->allocation.x,
                   widget->allocation.y,
                   widget->allocation.width,
                   widget->allocation.height);
  cairo_fill (cr);

  cairo_set_source_rgb (cr, 0.8, 0.8, 0.8);

  for (c = coords; c; c = c->next)
    {
      GdkPoint *point;

      point = c->data;

      cairo_move_to (cr, point->x, 0);
      cairo_rel_line_to (cr, 0, widget->allocation.height);

      cairo_move_to (cr, 0, point->y);
      cairo_rel_line_to (cr, widget->allocation.width, 0);
    }

  cairo_stroke (cr);

  cairo_set_source_rgba (cr, 0.5, 0., 0., 0.5);

  if (g_list_length (coords) > 1)
    {
      for (c = coords; c; c = c->next)
        {
          GdkPoint *point;

          point = c->data;

          cairo_line_to (cr, point->x, point->y);
        }

      cairo_close_path (cr);
      cairo_fill_preserve (cr);

      cairo_set_source_rgba (cr, 0., 0., 0.5, 0.5);
      cairo_stroke (cr);
    }

  cairo_destroy (cr);

  g_list_free (coords);

  return TRUE;
}

GtkWidget *
test_coordinate_widget_new (void)
{
  return g_object_new (TEST_TYPE_COORDINATE_WIDGET, NULL);
}
