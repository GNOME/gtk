#include <gtk/gtk.h>

#define DEMO_TYPE_WIDGET (demo_widget_get_type ())
G_DECLARE_FINAL_TYPE (DemoWidget, demo_widget, DEMO, WIDGET, GtkWidget)

struct _DemoWidget
{
  GtkWidget parent_instance;
  GskPath *orig_path;
  GskPath *path;
  GskPathMeasure *measure;
  double x, y;
  graphene_point_t point;
  graphene_vec2_t tangent;
  double start, end;

  gboolean track;
  gboolean show_bounding_box;
  GtkWidget *label;
};

struct _DemoWidgetClass
{
  GtkWidgetClass parent_class;
};

G_DEFINE_TYPE (DemoWidget, demo_widget, GTK_TYPE_WIDGET)

static void
motion (GtkEventControllerMotion *controller,
        double                    x,
        double                    y,
        DemoWidget               *self)
{
  float distance;
  char *text;

  if (!self->track)
    return;

  self->x = x;
  self->y = y;
  gsk_path_measure_get_closest_point_full (self->measure,
                                           &GRAPHENE_POINT_INIT (x, y),
                                           FLT_MAX,
                                           &distance,
                                           &self->point,
                                           NULL,
                                           &self->tangent);

  text = g_strdup_printf ("%.1f", distance);
  gtk_label_set_label (GTK_LABEL (self->label), text);

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
demo_widget_init (DemoWidget *self)
{
  GtkEventController *controller;

  self->start = 0;
  self->end = 1;

  self->label = gtk_label_new ("");
  gtk_widget_set_parent (self->label, GTK_WIDGET (self));
  gtk_widget_set_halign (self->label, GTK_ALIGN_END);
  gtk_widget_set_valign (self->label, GTK_ALIGN_START);

  controller = gtk_event_controller_motion_new ();
  g_signal_connect (controller, "motion", G_CALLBACK (motion), self);
  gtk_widget_add_controller (GTK_WIDGET (self), controller);
}

static void
demo_widget_snapshot (GtkWidget   *widget,
                      GtkSnapshot *snapshot)
{
  DemoWidget *self = DEMO_WIDGET (widget);
  int width, height;
  GskStroke *stroke;
  GskPathBuilder *builder;
  GskPath *path;
  graphene_point_t p;

  if (!self->path)
    return;

  width = gtk_widget_get_width (widget);
  height = gtk_widget_get_width (widget);

  stroke = gsk_stroke_new (1.0);
  gtk_snapshot_push_stroke (snapshot, self->path, stroke);
  gsk_stroke_free (stroke);

  gtk_snapshot_append_color (snapshot,
                             &(GdkRGBA){ 0, 0, 0, 1},
                             &GRAPHENE_RECT_INIT (0, 0, width, height ));

  gtk_snapshot_pop (snapshot);

  if (self->show_bounding_box)
    {
      graphene_rect_t bounds;

      if (gsk_path_get_bounds (self->path, &bounds))
        {
          builder = gsk_path_builder_new ();

          gsk_path_builder_add_rect (builder,
                                     bounds.origin.x, bounds.origin.y,
                                     bounds.size.width, bounds.size.height);

          path = gsk_path_builder_free_to_path (builder);

          stroke = gsk_stroke_new (1.0);
          gtk_snapshot_push_stroke (snapshot, path, stroke);
          gsk_stroke_free (stroke);

          gtk_snapshot_append_color (snapshot,
                                     &(GdkRGBA){ 0, 0, 0, 0.5},
                                     &GRAPHENE_RECT_INIT (0, 0, width, height ));

          gtk_snapshot_pop (snapshot);

          gsk_path_unref (path);
        }
    }

  if (self->track)
    {
      p.x = self->point.x + graphene_vec2_get_x (&self->tangent) * 40;
      p.y = self->point.y + graphene_vec2_get_y (&self->tangent) * 40;

      builder = gsk_path_builder_new ();

      gsk_path_builder_move_to (builder, self->point.x, self->point.y);
      gsk_path_builder_line_to (builder, p.x, p.y);

      path = gsk_path_builder_free_to_path (builder);

      stroke = gsk_stroke_new (1.0);
      gtk_snapshot_push_stroke (snapshot, path, stroke);
      gsk_stroke_free (stroke);

      gtk_snapshot_append_color (snapshot,
                                 &(GdkRGBA){ 0, 0, 0, 1},
                                 &GRAPHENE_RECT_INIT (0, 0, width, height ));

      gtk_snapshot_pop (snapshot);

      gsk_path_unref (path);

      builder = gsk_path_builder_new ();

      gsk_path_builder_add_circle (builder, &self->point, 5);
      gsk_path_builder_add_circle (builder, &p, 2.5);

      path = gsk_path_builder_free_to_path (builder);

      gtk_snapshot_push_fill (snapshot, path, 0);
      gtk_snapshot_append_color (snapshot,
                                 &(GdkRGBA){ 1, 0, 0, 1},
                                 &GRAPHENE_RECT_INIT (0, 0, width, height ));
      gtk_snapshot_pop (snapshot);

      stroke = gsk_stroke_new (1.0);
      gtk_snapshot_push_stroke (snapshot, path, stroke);
      gsk_stroke_free (stroke);

      gtk_snapshot_append_color (snapshot,
                                 &(GdkRGBA){ 0, 0, 0, 1},
                                 &GRAPHENE_RECT_INIT (0, 0, width, height ));

      gtk_snapshot_pop (snapshot);

      gsk_path_unref (path);

      gtk_widget_snapshot_child (widget, self->label, snapshot);
    }
}

static void
demo_widget_dispose (GObject *object)
{
  DemoWidget *self = DEMO_WIDGET (object);

  g_clear_pointer (&self->path, gsk_path_unref);
  g_clear_pointer (&self->measure, gsk_path_measure_unref);
  g_clear_pointer (&self->label, gtk_widget_unparent);

  G_OBJECT_CLASS (demo_widget_parent_class)->dispose (object);
}

static void
demo_widget_class_init (DemoWidgetClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->dispose = demo_widget_dispose;

  widget_class->snapshot = demo_widget_snapshot;

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
}

static GtkWidget *
demo_widget_new (void)
{
  return g_object_new (DEMO_TYPE_WIDGET, NULL);
}

static void
update_path (DemoWidget *self)
{
  g_clear_pointer (&self->path, gsk_path_unref);
  g_clear_pointer (&self->measure, gsk_path_measure_unref);

  if (self->start > 0 || self->end < 1)
    {
      GskPathMeasure *measure;
      GskPathBuilder *builder;
      float length;

      measure = gsk_path_measure_new (self->orig_path);
      length = gsk_path_measure_get_length (measure);
      builder = gsk_path_builder_new ();
      gsk_path_measure_add_segment (measure, builder, self->start * length, self->end * length);
      self->path = gsk_path_builder_free_to_path (builder);
      gsk_path_measure_unref (measure);
    }
  else
    self->path = gsk_path_ref (self->orig_path);

  self->measure = gsk_path_measure_new (self->path);

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
demo_widget_set_path (DemoWidget *self,
                      GskPath    *path)
{
  g_clear_pointer (&self->orig_path, gsk_path_unref);
  self->orig_path = gsk_path_ref (path);

  update_path (self);
}

static void
activate (GtkEntry *entry,
          DemoWidget *demo)
{
  GskPath *path;

  path = gsk_path_parse (gtk_editable_get_text (GTK_EDITABLE (entry)));
  if (path)
    {
      demo_widget_set_path (demo, path);
      gsk_path_unref (path);
    }
}

static void
init_demo (DemoWidget  *demo,
           GtkEditable *editable)
{
  GskPathBuilder *builder;
  GskPath *path;
  char *string;

  builder = gsk_path_builder_new ();
  gsk_path_builder_add_circle (builder, &GRAPHENE_POINT_INIT (150, 150), 100);
  gsk_path_builder_add_rect (builder, 100, 100, 100, 100);
  path = gsk_path_builder_free_to_path (builder);

  demo_widget_set_path (demo, path);

  string = gsk_path_to_string (path);
  gtk_editable_set_text (editable, string);
  g_free (string);
  gsk_path_unref (path);
}

static void
track_toggled (GtkCheckButton *button,
               DemoWidget      *self)
{
  self->track = gtk_check_button_get_active (button);
  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
bb_toggled (GtkCheckButton *button,
            DemoWidget      *self)
{
  self->show_bounding_box = gtk_check_button_get_active (button);
  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static GtkWidget *start_scale;
static GtkWidget *end_scale;

static void
range_changed (GtkRange   *range,
               DemoWidget *self)
{
  double start, end;

  if (range == GTK_RANGE (start_scale))
    {
      start = gtk_range_get_value (range);
      end = MAX (start, gtk_range_get_value (GTK_RANGE (end_scale)));
      gtk_range_set_value (GTK_RANGE (end_scale), end);
    }
  else
    {
      end = gtk_range_get_value (range);
      start = MIN (end, gtk_range_get_value (GTK_RANGE (start_scale)));
      gtk_range_set_value (GTK_RANGE (start_scale), start);
    }

  self->start = start;
  self->end = end;

  update_path (self);
}

int
main (int argc, char *argv[])
{
  GtkWidget *window, *box, *demo, *entry;
  GtkWidget *popover, *button, *grid;
  GtkWidget *header, *toggle;

  gtk_init ();

  window = gtk_window_new ();
  gtk_window_set_default_size (GTK_WINDOW (window), 600, 400);
  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_window_set_child (GTK_WINDOW (window), box);

  demo = demo_widget_new ();
  gtk_widget_set_hexpand (demo, TRUE);
  gtk_widget_set_vexpand (demo, TRUE);
  gtk_box_append (GTK_BOX (box), demo);

  header = gtk_header_bar_new ();
  button = gtk_menu_button_new ();
  gtk_menu_button_set_icon_name (GTK_MENU_BUTTON (button), "emblem-system-symbolic");
  gtk_header_bar_pack_start (GTK_HEADER_BAR (header), button);
  gtk_window_set_titlebar (GTK_WINDOW (window), header);

  popover = gtk_popover_new ();
  gtk_menu_button_set_popover (GTK_MENU_BUTTON (button), popover);

  grid = gtk_grid_new ();
  gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
  gtk_grid_set_column_spacing (GTK_GRID (grid), 6);
  gtk_popover_set_child (GTK_POPOVER (popover), grid);

  toggle = gtk_check_button_new_with_label ("Show closest point");
  g_signal_connect (toggle, "toggled", G_CALLBACK (track_toggled), demo);
  gtk_grid_attach (GTK_GRID (grid), toggle, 0, 0, 1, 1);

  toggle = gtk_check_button_new_with_label ("Show bounding box");
  g_signal_connect (toggle, "toggled", G_CALLBACK (bb_toggled), demo);
  gtk_grid_attach (GTK_GRID (grid), toggle, 0, 1, 1, 1);

  entry = gtk_entry_new ();
  g_signal_connect (entry, "activate", G_CALLBACK (activate), demo);
  gtk_box_append (GTK_BOX (box), entry);

  start_scale = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 0, 1, 0.1);
  g_signal_connect (start_scale, "value-changed", G_CALLBACK (range_changed), demo);
  gtk_box_append (GTK_BOX (box), start_scale);

  end_scale = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 0, 1, 0.1);
  gtk_range_set_value (GTK_RANGE (end_scale), 1);
  g_signal_connect (end_scale, "value-changed", G_CALLBACK (range_changed), demo);
  gtk_box_append (GTK_BOX (box), end_scale);

  init_demo (DEMO_WIDGET (demo), GTK_EDITABLE (entry));

  gtk_window_present (GTK_WINDOW (window));

  while (g_list_model_get_n_items (gtk_window_get_toplevels ()) > 0)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
