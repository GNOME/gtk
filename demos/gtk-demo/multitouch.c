/* Multitouch
 *
 * Demonstrates some general multitouch event handling,
 * using GdkTouchCluster in order to get grouped motion
 * events for the touches within a cluster.
 */

#include <math.h>
#include <gtk/gtk.h>
#include "demo-common.h"

static GtkWidget *window = NULL;
static GList *shapes = NULL;

typedef struct {
  GdkTouchCluster *cluster;
  GdkRGBA color;

  gdouble angle;
  gdouble zoom;

  gdouble center_x;
  gdouble center_y;

  gdouble x;
  gdouble y;
  gdouble width;
  gdouble height;

  gdouble base_zoom;
  gdouble base_angle;
  gdouble initial_distance;
  gdouble initial_angle;

  GdkPoint points[4];
} ShapeInfo;

static void
calculate_rotated_point (gdouble  angle,
                         gdouble  zoom,
                         gdouble  center_x,
                         gdouble  center_y,
                         gdouble  point_x,
                         gdouble  point_y,
                         gdouble *ret_x,
                         gdouble *ret_y)
{
  gdouble distance, xd, yd, ang;

  if (angle == 0)
    {
      *ret_x = point_x;
      *ret_y = point_y;
      return;
    }

  xd = center_x - point_x;
  yd = center_y - point_y;

  if (xd == 0 && yd == 0)
    {
      *ret_x = center_x;
      *ret_y = center_y;
      return;
    }

  distance = sqrt ((xd * xd) + (yd * yd));
  distance *= zoom;

  ang = atan2 (xd, yd);

  /* Invert angle */
  ang = (2 * G_PI) - ang;

  /* Shift it 270° */
  ang += 3 * (G_PI / 2);

  /* And constraint it to 0°-360° */
  ang = fmod (ang, 2 * G_PI);
  ang += angle;

  *ret_x = center_x + (distance * cos (ang));
  *ret_y = center_y + (distance * sin (ang));
}

static void
shape_info_allocate_input_rect (ShapeInfo *info)
{
  gint width, height, i;

  width = info->width;
  height = info->height;

  /* Top/left */
  info->points[0].x = info->x - info->center_x;
  info->points[0].y = info->y - info->center_y;

  /* Top/right */
  info->points[1].x = info->x - info->center_x + width;
  info->points[1].y = info->y - info->center_y;

  /* Bottom/right */
  info->points[2].x = info->x - info->center_x + width;
  info->points[2].y = info->y - info->center_y + height;

  /* Bottom/left */
  info->points[3].x = info->x - info->center_x;
  info->points[3].y = info->y - info->center_y + height;

  for (i = 0; i < 4; i++)
    {
      gdouble ret_x, ret_y;

      calculate_rotated_point (info->angle,
                               info->zoom,
                               info->x,
                               info->y,
                               (gdouble) info->points[i].x,
                               (gdouble) info->points[i].y,
                               &ret_x,
                               &ret_y);

      info->points[i].x = (gint) ret_x;
      info->points[i].y = (gint) ret_y;
    }
}

static void
shape_info_bounding_rect (ShapeInfo    *info,
                          GdkRectangle *rect)
{
  gint i, left, right, top, bottom;

  left = top = G_MAXINT;
  right = bottom = 0;

  for (i = 0; i < 4; i++)
    {
      if (info->points[i].x < left)
        left = info->points[i].x;

      if (info->points[i].x > right)
        right = info->points[i].x;

      if (info->points[i].y < top)
        top = info->points[i].y;

      if (info->points[i].y > bottom)
        bottom = info->points[i].y;
    }

  rect->x = left - 20;
  rect->y = top - 20;
  rect->width = right - left + 40;
  rect->height = bottom - top + 40;
}

static gboolean
shape_info_point_in (ShapeInfo *info,
                     gint       x,
                     gint       y)
{
  GdkPoint *left, *right, *top, *bottom;
  gint i;

  left = right = top = bottom = NULL;

  for (i = 0; i < 4; i++)
    {
      GdkPoint *p = &info->points[i];

      if (!left ||
          p->x < left->x ||
          (p->x == left->x && p->y > left->y))
        left = p;

      if (!right ||
          p->x > right->x ||
          (p->x == right->x && p->y < right->y))
        right = p;
    }

  for (i = 0; i < 4; i++)
    {
      GdkPoint *p = &info->points[i];

      if (p == left || p == right)
        continue;

      if (!top ||
          p->y < top->y)
        top = p;

      if (!bottom ||
          p->y > bottom->y)
        bottom = p;
    }

  g_assert (left && right && top && bottom);

  if (x < left->x ||
      x > right->x ||
      y < top->y ||
      y > bottom->y)
    return FALSE;

  /* Check whether point is above the sides
   * between leftmost and topmost, and
   * topmost and rightmost corners.
   */
  if (x <= top->x)
    {
      if (left->y - ((left->y - top->y) * (((gdouble) x - left->x) / (top->x - left->x))) > y)
        return FALSE;
    }
  else
    {
      if (top->y + ((right->y - top->y) * (((gdouble) x - top->x) / (right->x - top->x))) > y)
        return FALSE;
    }

  /* Check whether point is below the sides
   * between leftmost and bottom, and
   * bottom and rightmost corners.
   */
  if (x <= bottom->x)
    {
      if (left->y + ((bottom->y - left->y) * (((gdouble) x - left->x) / (bottom->x - left->x))) < y)
        return FALSE;
    }
  else
    {
      if (bottom->y - ((bottom->y - right->y) * (((gdouble) x - bottom->x) / (right->x - bottom->x))) < y)
        return FALSE;
    }

  return TRUE;
}

static ShapeInfo *
shape_info_new (gdouble  x,
                gdouble  y,
                gdouble  width,
                gdouble  height,
                GdkRGBA *color)
{
  ShapeInfo *info;

  info = g_slice_new0 (ShapeInfo);
  info->cluster = NULL;
  info->color = *color;

  info->x = x;
  info->y = y;
  info->width = width;
  info->height = height;

  info->angle = 0;
  info->zoom = 1;

  info->base_zoom = 1;
  info->base_angle = 0;
  info->initial_distance = 0;
  info->initial_angle = 0;

  shape_info_allocate_input_rect (info);

  shapes = g_list_prepend (shapes, info);

  return info;
}

static void
shape_info_free (ShapeInfo *info)
{
  g_slice_free (ShapeInfo, info);
}

static void
shape_info_draw (cairo_t   *cr,
                 ShapeInfo *info)
{
  cairo_save (cr);

  cairo_translate (cr, info->points[0].x, info->points[0].y);
  cairo_scale (cr, info->zoom, info->zoom);
  cairo_rotate (cr, info->angle);

  cairo_rectangle (cr, 0, 0, info->width, info->height);
  gdk_cairo_set_source_rgba (cr, &info->color);
  cairo_fill_preserve (cr);

  cairo_set_line_width (cr, 6);
  cairo_set_source_rgb (cr, 0, 0, 0);
  cairo_stroke (cr);

  cairo_restore (cr);
}

static gboolean
draw_cb (GtkWidget *widget,
         cairo_t   *cr,
         gpointer   user_data)
{
  GList *l;

  for (l = shapes; l; l = l->next)
    shape_info_draw (cr, l->data);

  return FALSE;
}

static gboolean
button_press_cb (GtkWidget *widget,
                 GdkEvent  *event,
                 gpointer   user_data)
{
  ShapeInfo *shape = NULL;
  guint touch_id;

  if (gdk_event_get_touch_id (event, &touch_id))
    {
      GList *l;

      for (l = shapes; l; l = l->next)
        {
          ShapeInfo *info = l->data;

          if (shape_info_point_in (info,
                                   (gint) event->button.x,
                                   (gint) event->button.y))
            shape = info;
        }

      if (!shape)
        return FALSE;

      if (!shape->cluster)
        shape->cluster = gdk_window_create_touch_cluster (gtk_widget_get_window (widget));

      /* Only change cluster device if there were no touches or no device */
      if (!gdk_touch_cluster_get_device (shape->cluster) ||
          !gdk_touch_cluster_get_touches (shape->cluster))
        gdk_touch_cluster_set_device (shape->cluster,
                                      gdk_event_get_source_device (event));

      gdk_touch_cluster_add_touch (shape->cluster, touch_id);
      return TRUE;
    }

  return FALSE;
}

static gboolean
multitouch_cb (GtkWidget *widget,
               GdkEvent  *event,
               gpointer   user_data)
{
  ShapeInfo *info = NULL;
  gboolean new_center = FALSE;
  gboolean new_position = FALSE;
  gdouble event_x, event_y;
  cairo_region_t *region;
  GdkRectangle rect;
  GList *l;

  for (l = shapes; l; l = l->next)
    {
      ShapeInfo *shape = l->data;

      if (event->multitouch.group == shape->cluster)
        {
          info = shape;
          break;
        }
    }

  if (!info)
    return FALSE;

  shape_info_bounding_rect (info, &rect);
  region = cairo_region_create_rectangle ((cairo_rectangle_int_t *) &rect);

  if (event->multitouch.n_events == 1)
    {
      /* Update center if we just got to
       * this situation from either way */
      if (event->type == GDK_MULTITOUCH_ADDED ||
          event->type == GDK_MULTITOUCH_REMOVED)
        new_center = TRUE;

      event_x = event->multitouch.events[0]->x;
      event_y = event->multitouch.events[0]->y;
      new_position = TRUE;
    }
  else if (event->multitouch.n_events == 2)
    {
      gdouble distance, angle;

      gdk_events_get_center ((GdkEvent *) event->multitouch.events[0],
                             (GdkEvent *) event->multitouch.events[1],
                             &event_x, &event_y);

      gdk_events_get_distance ((GdkEvent *) event->multitouch.events[0],
                               (GdkEvent *) event->multitouch.events[1],
                               &distance);

      gdk_events_get_angle ((GdkEvent *) event->multitouch.events[0],
                            (GdkEvent *) event->multitouch.events[1],
                            &angle);

      if (event->type == GDK_MULTITOUCH_ADDED)
        {
          /* Second touch was just added, update base zoom/angle */
          info->base_zoom = info->zoom;
          info->base_angle = info->angle;
          info->initial_angle = angle;
          info->initial_distance = distance;
          new_center = TRUE;
        }

      info->zoom = MAX (info->base_zoom * (distance / info->initial_distance), 1.0);
      info->angle = info->base_angle + (angle - info->initial_angle);
      new_position = TRUE;
    }

  if (new_center)
    {
      gdouble origin_x, origin_y;

      origin_x = info->x - info->center_x;
      origin_y = info->y - info->center_y;

      calculate_rotated_point (- info->angle,
                               1 / info->zoom,
                               info->x - origin_x,
                               info->y - origin_y,
                               event_x - origin_x,
                               event_y - origin_y,
                               &info->center_x,
                               &info->center_y);
    }

  if (new_position)
    {
      info->x = event_x;
      info->y = event_y;
    }

  shape_info_allocate_input_rect (info);

  shape_info_bounding_rect (info, &rect);
  cairo_region_union_rectangle (region, (cairo_rectangle_int_t *) &rect);
  gdk_window_invalidate_region (gtk_widget_get_window (widget), region, FALSE);

  return TRUE;
}

GtkWidget *
do_multitouch (GtkWidget *do_widget)
{
  if (!window)
    {
      GdkRGBA color;

      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_title (GTK_WINDOW (window), "Multitouch demo");
      gtk_window_set_default_size (GTK_WINDOW (window), 600, 600);

      gtk_window_set_screen (GTK_WINDOW (window),
			     gtk_widget_get_screen (do_widget));

      gtk_widget_add_events (window,
                             GDK_TOUCH_MASK |
                             GDK_POINTER_MOTION_MASK |
                             GDK_BUTTON_PRESS_MASK |
                             GDK_BUTTON_RELEASE_MASK);

      gtk_widget_set_app_paintable (window, TRUE);

      g_signal_connect (window, "draw",
                        G_CALLBACK (draw_cb), NULL);
      g_signal_connect (window, "button-press-event",
                        G_CALLBACK (button_press_cb), NULL);
      g_signal_connect (window, "multitouch-event",
                        G_CALLBACK (multitouch_cb), NULL);

      gdk_rgba_parse (&color, "red");
      color.alpha = 0.5;
      shape_info_new (100, 50, 100, 140, &color);

      gdk_rgba_parse (&color, "green");
      color.alpha = 0.5;
      shape_info_new (200, 100, 120, 90, &color);

      gdk_rgba_parse (&color, "blue");
      color.alpha = 0.5;
      shape_info_new (150, 190, 140, 90, &color);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    {
      gtk_widget_destroy (window);
      window = NULL;

      g_list_foreach (shapes, (GFunc) shape_info_free, NULL);
      g_list_free (shapes);
      shapes = NULL;
    }

  return window;
}
