#include <gtk/gtk.h>


#define RADIUS 5

typedef struct
{
  gboolean smooth;
} PointData;

typedef struct
{
  GtkWidget parent_instance;
  graphene_point_t *points;
  int n_points;
  PointData *point_data; /* length is n_points / 3 */
  int dragged;
  gboolean symmetric;
  gboolean edit;
} DemoWidget;

typedef struct
{
  GtkWidgetClass parent_class;
} DemoWidgetClass;

GType demo_widget_get_type (void) G_GNUC_CONST;

G_DEFINE_TYPE (DemoWidget, demo_widget, GTK_TYPE_WIDGET)

static float
dist (graphene_point_t *a, graphene_point_t *b)
{
  graphene_vec2_t v;

  graphene_vec2_init (&v, a->x - b->x, a->y - b->y);
  return graphene_vec2_length (&v);
}

static void
drag_begin (GtkGestureDrag *gesture,
            double          start_x,
            double          start_y,
            DemoWidget     *self)
{
  int i;
  graphene_point_t p = GRAPHENE_POINT_INIT (start_x, start_y);

  if (self->edit)
    for (i = 0; i < self->n_points; i++)
      {
        if (dist (&self->points[i], &p) < RADIUS)
          {
            gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);

            self->dragged = i;
            self->symmetric = (gtk_event_controller_get_current_event_state (GTK_EVENT_CONTROLLER (gesture)) & GDK_CONTROL_MASK) == 0;

            gtk_widget_queue_draw (GTK_WIDGET (self));
            return;
          }
      }

  gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_DENIED);
}

static void
drag_update (GtkGestureDrag *gesture,
             double          offset_x,
             double          offset_y,
             DemoWidget     *self)
{
  double x, y;
  double dx, dy;
  graphene_point_t *c, *p, *d;

  if (self->dragged == -1)
    return;

  gtk_gesture_drag_get_start_point (gesture, &x, &y);

  x += offset_x;
  y += offset_y;

  d = &self->points[self->dragged];

  dx = x - d->x;
  dy = y - d->y;

  d->x += dx;
  d->y += dy;

  if (self->dragged % 3 == 0)
    {
      /* point is on curve */

      self->points[(self->dragged - 1 + self->n_points) % self->n_points].x += dx;
      self->points[(self->dragged - 1 + self->n_points) % self->n_points].y += dy;

      self->points[(self->dragged + 1) % self->n_points].x += dx;
      self->points[(self->dragged + 1) % self->n_points].y += dy;
    }
  else
    {
      int point;

      if (self->dragged % 3 == 1)
        {
          point = (self->dragged - 1 + self->n_points) % self->n_points;
          c = &self->points[(self->dragged - 2 + self->n_points) % self->n_points];
          p = &self->points[point];
        }
      else if (self->dragged % 3 == 2)
        {
          point = (self->dragged + 1) % self->n_points;
          c = &self->points[(self->dragged + 2) % self->n_points];
          p = &self->points[point];
        }
      else
        g_assert_not_reached ();

      if (self->point_data[point / 3].smooth)
        {
          double a, l;

          a = atan2 (self->points[self->dragged].y - p->y, self->points[self->dragged].x - p->x) + M_PI;
          l = dist (c, p);
          c->x = p->x + l * cos (a);
          c->y = p->y + l * sin (a);
        }

      if (self->symmetric)
        {
          double l, l2;

          l = dist (d, p);
          l2 = dist (c, p);

          c->x = p->x + (l / l2) * (c->x - p->x);
          c->y = p->y + (l / l2) * (c->y - p->y);
        }
    }

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
drag_end (GtkGestureDrag *gesture,
          double          offset_x,
          double          offset_y,
          DemoWidget     *self)
{
  drag_update (gesture, offset_x, offset_y, self);
  self->dragged = -1;
  self->symmetric = FALSE;
}

static void
released (GtkGestureClick *gesture,
          int              n_press,
          double           x,
          double           y,
          DemoWidget      *self)
{
  graphene_point_t m = GRAPHENE_POINT_INIT (x, y);
  int i;

  if (!self->edit)
    return;

  for (i = 0; i < self->n_points; i++)
    {
      if (dist (&self->points[i], &m) < RADIUS)
        {
          if (i % 3 == 0)
            {
              self->point_data[i / 3].smooth = !self->point_data[i / 3].smooth;
              if (self->point_data[i / 3].smooth)
                {
                  graphene_point_t *p, *c, *c2;
                  float a, d;

                  p = &self->points[i];
                  c = &self->points[(i - 1 + self->n_points) % self->n_points];
                  c2 = &self->points[(i + 1 + self->n_points) % self->n_points];

                  a = atan2 (c2->y - p->y, c2->x - p->x) + M_PI;
                  d = dist (c, p);
                  c->x = p->x + d * cos (a);
                  c->y = p->y + d * sin (a);
                }

              gtk_widget_queue_draw (GTK_WIDGET (self));
            }
          break;
        }
    }
}

static void
init_points (DemoWidget *self)
{
  float w = 200;
  float h = 200;
  float cx = w / 2;
  float cy = h / 2;
  float pad = 20;
  float r = (w - 2 * pad) / 2;
  float k = 0.55228;
  float kr = k  * r;
  int i;

  g_free (self->points);
  g_free (self->point_data);

  self->n_points = 12;
  self->points = g_new (graphene_point_t, self->n_points);
  self->point_data = g_new (PointData, self->n_points / 3);


  self->points[0] = GRAPHENE_POINT_INIT (cx, pad);
  self->points[1] = GRAPHENE_POINT_INIT (cx + kr, pad);
  self->points[2] = GRAPHENE_POINT_INIT (w - pad, cy - kr);

  self->points[3] = GRAPHENE_POINT_INIT (w - pad, cy);
  self->points[4] = GRAPHENE_POINT_INIT (w - pad, cy + kr);
  self->points[5] = GRAPHENE_POINT_INIT (cx + kr, h - pad);

  self->points[6] = GRAPHENE_POINT_INIT (cx, h - pad);
  self->points[7] = GRAPHENE_POINT_INIT (cx - kr, h - pad);
  self->points[8] = GRAPHENE_POINT_INIT (pad, cy + kr);
 
  self->points[9] = GRAPHENE_POINT_INIT (pad, cy);
  self->points[10] = GRAPHENE_POINT_INIT (pad, cy - kr);
  self->points[11] = GRAPHENE_POINT_INIT (cx - kr, pad);

  for (i = 0; i < self->n_points / 3; i++)
    self->point_data[i].smooth = TRUE;
}

static void
demo_widget_init (DemoWidget *self)
{
  GtkGesture *gesture;

  self->dragged = -1;
  self->edit = FALSE;

  gesture = gtk_gesture_drag_new ();
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture), GDK_BUTTON_PRIMARY);
  g_signal_connect (gesture, "drag-begin", G_CALLBACK (drag_begin), self);
  g_signal_connect (gesture, "drag-update", G_CALLBACK (drag_update), self);
  g_signal_connect (gesture, "drag-end", G_CALLBACK (drag_end), self);
  gtk_widget_add_controller (GTK_WIDGET (self), GTK_EVENT_CONTROLLER (gesture));

  gesture = gtk_gesture_click_new ();
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture), GDK_BUTTON_SECONDARY);
  g_signal_connect (gesture, "released", G_CALLBACK (released), self);
  gtk_widget_add_controller (GTK_WIDGET (self), GTK_EVENT_CONTROLLER (gesture));

  init_points (self);
}

static void
demo_widget_snapshot (GtkWidget   *widget,
                      GtkSnapshot *snapshot)
{
  DemoWidget *self = (DemoWidget *)widget;
  GskPathBuilder *builder;
  GskPath *path;
  GskStroke *stroke;
  int i, j;
  float width;
  float height;

  width = gtk_widget_get_width (widget);
  height = gtk_widget_get_width (widget);

  builder = gsk_path_builder_new ();

  if (self->edit)
    {
      gsk_path_builder_move_to (builder, self->points[0].x, self->points[0].y);
      for (i = 1; i < self->n_points; i++)
        {
          if (i % 3 == 2)
            gsk_path_builder_move_to (builder, self->points[i].x, self->points[i].y);
          else
            gsk_path_builder_line_to (builder, self->points[i].x, self->points[i].y);
        }
      gsk_path_builder_line_to (builder, self->points[0].x, self->points[0].y);
    }

  gsk_path_builder_move_to (builder, self->points[0].x, self->points[0].y);
  for (i = 1; i < self->n_points; i += 3)
    {
      gsk_path_builder_curve_to (builder,
                                 self->points[i].x, self->points[i].y,
                                 self->points[(i + 1) % self->n_points].x, self->points[(i + 1) % self->n_points].y,
                                 self->points[(i + 2) % self->n_points].x, self->points[(i + 2) % self->n_points].y);
    }

  path = gsk_path_builder_free_to_path (builder);
  stroke = gsk_stroke_new (1);
  gtk_snapshot_push_stroke (snapshot, path, stroke);
  gsk_stroke_free (stroke);
  gsk_path_unref (path);

  gtk_snapshot_append_color (snapshot,
                             &(GdkRGBA){ 0, 0, 0, 1 },
                             &GRAPHENE_RECT_INIT (0, 0, width, height ));

  gtk_snapshot_pop (snapshot);

  if (self->edit)
    {
      const char *colors[] = {
        "white",
        "red",
        "green",
        "blue"
      };
      GdkRGBA color;

      for (j = 0; j < 4; j++)
        {
          builder = gsk_path_builder_new ();

          for (i = 0; i < self->n_points; i++)
            {
              switch (j)
                {
                case 0:
                  if (i == self->dragged)
                    break;
                  else
                    continue;

                case 1:
                  if (i != self->dragged &&
                      i % 3 == 0 &&
                      self->point_data[i / 3].smooth)
                    break;
                  else
                    continue;

                case 2:
                  if (i != self->dragged &&
                      i % 3 == 0 &&
                      !self->point_data[i / 3].smooth)
                    break;
                  else
                    continue;

                case 3:
                  if (i != self->dragged &&
                      i % 3 != 0)
                    break;
                  else
                    continue;

                default:
                  g_assert_not_reached ();
                }

              gsk_path_builder_add_circle (builder, &self->points[i], RADIUS);
            }

          path = gsk_path_builder_free_to_path (builder);

          gtk_snapshot_push_fill (snapshot, path, GSK_FILL_RULE_WINDING);
          gdk_rgba_parse (&color, colors[j]);
          gtk_snapshot_append_color (snapshot, &color, &GRAPHENE_RECT_INIT (0, 0, width, height));
          gtk_snapshot_pop (snapshot);

          stroke = gsk_stroke_new (1.0);
          gtk_snapshot_push_stroke (snapshot, path, stroke);
          gsk_stroke_free (stroke);

          gdk_rgba_parse (&color, "black");
          gtk_snapshot_append_color (snapshot, &color, &GRAPHENE_RECT_INIT (0, 0, width, height));
          gtk_snapshot_pop (snapshot);

          gsk_path_unref (path);
        }
    }
}

static void
demo_widget_measure (GtkWidget      *widget,
                     GtkOrientation  orientation,
                     int             for_size,
                     int            *minimum_size,
                     int            *natural_size,
                     int            *minimum_baseline,
                     int            *natural_baseline)
{
  *minimum_size = 100;
  *natural_size = 200;
}

static void
demo_widget_class_init (DemoWidgetClass *class)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  widget_class->snapshot = demo_widget_snapshot;
  widget_class->measure = demo_widget_measure;
}

static GtkWidget *
demo_widget_new (void)
{
  return g_object_new (demo_widget_get_type (), NULL);
}

static void
edit_changed (GtkToggleButton *button,
              GParamSpec      *pspec,
              DemoWidget      *self)
{
  self->edit = gtk_toggle_button_get_active (button);
  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
reset (GtkButton  *button,
       DemoWidget *self)
{
  init_points (self);
  gtk_widget_queue_draw (GTK_WIDGET (self));
}

int
main (int argc, char *argv[])
{
  GtkWindow *window;
  GtkWidget *demo;
  GtkWidget *edit_toggle;
  GtkWidget *reset_button;
  GtkWidget *titlebar;

  gtk_init ();

  window = GTK_WINDOW (gtk_window_new ());
  gtk_window_set_default_size (GTK_WINDOW (window), 250, 250);

  edit_toggle = gtk_toggle_button_new ();
  gtk_button_set_icon_name (GTK_BUTTON (edit_toggle), "document-edit-symbolic");

  reset_button = gtk_button_new_from_icon_name ("edit-undo-symbolic");

  titlebar = gtk_header_bar_new ();
  gtk_header_bar_pack_start (GTK_HEADER_BAR (titlebar), edit_toggle);
  gtk_header_bar_pack_start (GTK_HEADER_BAR (titlebar), reset_button);

  gtk_window_set_titlebar (GTK_WINDOW (window), titlebar);

  demo = demo_widget_new ();

  g_signal_connect (edit_toggle, "notify::active", G_CALLBACK (edit_changed), demo);
  g_signal_connect (reset_button, "clicked", G_CALLBACK (reset), demo);

  gtk_window_set_child (window, demo);

  gtk_window_present (window);

  while (g_list_model_get_n_items (gtk_window_get_toplevels ()) > 0)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
