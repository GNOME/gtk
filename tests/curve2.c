#include <gtk/gtk.h>

#define DEMO_TYPE_WIDGET (demo_widget_get_type ())
G_DECLARE_FINAL_TYPE (DemoWidget, demo_widget, DEMO, WIDGET, GtkWidget)

struct _DemoWidget
{
  GtkWidget parent_instance;
  GskPath *orig_path;
  GskPath *path;
  GskPathMeasure *measure;
  graphene_point_t point;
  graphene_point_t point2;
  graphene_vec2_t tangent;
  double start, end;

  gboolean track;
  gboolean show_bounding_box;
  GtkWidget *label;

  gboolean do_stroke;
  gboolean do_offset;
  GskPath *stroke_path;
  GskPathMeasure *stroke_measure;
  GskStroke *stroke;
  GskPath *outline_path;
  GskStroke *outline_stroke;
  gboolean inside;
  GskFillRule fill_rule;
  gboolean do_fill;

  graphene_rect_t bounds;
  gboolean show_cairo;

  double zoom;
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
  if (self->track)
    {
      float distance;
      char *text;
      float t;

      gsk_path_measure_get_closest_point_full (self->measure,
                                               &GRAPHENE_POINT_INIT (x, y),
                                               INFINITY,
                                               &distance,
                                               &self->point,
                                               &t,
                                               &self->tangent);

      gsk_path_measure_get_point (self->measure, t, &self->point2, NULL);

      text = g_strdup_printf ("%.1f", distance);
      gtk_label_set_label (GTK_LABEL (self->label), text);

      gtk_widget_queue_draw (GTK_WIDGET (self));
    }

  if (self->do_stroke && self->do_fill)
    {
      gboolean inside = TRUE;

      inside = gsk_path_measure_in_fill (self->stroke_measure, &GRAPHENE_POINT_INIT (x, y), self->fill_rule);

      if (self->inside != inside)
        {
          self->inside = inside;
          gtk_widget_queue_draw (GTK_WIDGET (self));
        }
    }
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
  graphene_rect_t bounds;

  if (!self->path)
    return;

  gtk_snapshot_save (snapshot);

  gtk_snapshot_scale (snapshot, self->zoom, self->zoom);

  width = gtk_widget_get_width (widget);
  height = gtk_widget_get_width (widget);

  if (self->do_stroke)
    {
      if (self->show_cairo)
        {
          cairo_t *cr;

          graphene_rect_init (&bounds, 0, 0, width, height);
          cr = gtk_snapshot_append_cairo (snapshot, &bounds);
          gsk_path_to_cairo (self->path, cr);
          gsk_stroke_to_cairo (self->stroke, cr);
          cairo_set_source_rgba (cr, 0, 0, 1, 0.2);
          cairo_stroke (cr);
          cairo_destroy (cr);
        }

      if (self->do_fill && self->inside)
        {
           gtk_snapshot_push_fill (snapshot, self->stroke_path, self->fill_rule);

           gtk_snapshot_append_color (snapshot,
                                      &(GdkRGBA){ 1, 0, 1, 0.3},
                                      &GRAPHENE_RECT_INIT (0, 0, width, height ));

           gtk_snapshot_pop (snapshot);
        }

      gtk_snapshot_push_stroke (snapshot, self->outline_path, self->outline_stroke);

      gtk_snapshot_append_color (snapshot,
                                 &(GdkRGBA){ 0, 0, 0, 0.2},
                                 &GRAPHENE_RECT_INIT (0, 0, width, height ));

      gtk_snapshot_pop (snapshot);

      stroke = gsk_stroke_new (1);
      gtk_snapshot_push_stroke (snapshot, self->path, stroke);
      gsk_stroke_free (stroke);

      gtk_snapshot_append_color (snapshot,
                                 &(GdkRGBA){ 0, 0, 0, 0.3},
                                 &GRAPHENE_RECT_INIT (0, 0, width, height ));

      gtk_snapshot_pop (snapshot);
    }
  else if (self->do_offset)
    {
      for (int i = -3; i < 4; i++)
        {
          path = gsk_path_offset (self->path,
                                  5 * i,
                                  gsk_stroke_get_line_join (self->stroke),
                                  gsk_stroke_get_miter_limit (self->stroke));

          gtk_snapshot_push_stroke (snapshot, path, self->outline_stroke);

          gtk_snapshot_append_color (snapshot,
                                     &(GdkRGBA){ 0, 0, 0, 1},
                                     &GRAPHENE_RECT_INIT (0, 0, width, height ));

          gtk_snapshot_pop (snapshot);
          gsk_path_unref (path);
        }
    }
  else
    {
      gtk_snapshot_push_stroke (snapshot, self->path, self->outline_stroke);

      gtk_snapshot_append_color (snapshot,
                                 &(GdkRGBA){ 0, 0, 0, 1},
                                 &GRAPHENE_RECT_INIT (0, 0, width, height ));

      gtk_snapshot_pop (snapshot);
    }

  if (self->show_bounding_box)
    {
      if (gsk_path_get_bounds (self->do_stroke ? self->outline_path : self->path, &bounds))
        {
          builder = gsk_path_builder_new ();

          gsk_path_builder_add_rect (builder, &bounds);

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
      graphene_point_t p;

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
      gsk_path_builder_add_circle (builder, &self->point2, 5);

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

  gtk_snapshot_restore (snapshot);
}

static void
demo_widget_measure (GtkWidget      *widget,
                     GtkOrientation  orientation,
                     int             for_size,
                     int            *minimum,
                     int            *natural,
                     int            *minimum_baseline,
                     int            *natural_baseline)
{
  DemoWidget *self = DEMO_WIDGET (widget);
  float size;

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    size = self->zoom * (self->bounds.origin.x + self->bounds.size.width);
  else
    size = self->zoom * (self->bounds.origin.y + self->bounds.size.height);

  *minimum = *natural = (int)size;
}

static void
demo_widget_size_allocate (GtkWidget           *widget,
                           int                  width,
                           int                  height,
                           int                  baseline)
{
  DemoWidget *self = DEMO_WIDGET (widget);
  GtkRequisition min, nat;

  gtk_widget_get_preferred_size (self->label, &min, &nat);

  gtk_widget_size_allocate (self->label,
                            &(GtkAllocation) { width - nat.width, 0, nat.width, nat.height},
                            -1);
}

static void
demo_widget_dispose (GObject *object)
{
  DemoWidget *self = DEMO_WIDGET (object);

  g_clear_pointer (&self->path, gsk_path_unref);
  g_clear_pointer (&self->measure, gsk_path_measure_unref);
  g_clear_pointer (&self->stroke_path, gsk_path_unref);
  g_clear_pointer (&self->stroke_measure, gsk_path_measure_unref);
  g_clear_pointer (&self->stroke, gsk_stroke_free);
  g_clear_pointer (&self->outline_path, gsk_path_unref);
  g_clear_pointer (&self->outline_stroke, gsk_stroke_free);
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
  widget_class->measure = demo_widget_measure;
  widget_class->size_allocate = demo_widget_size_allocate;
}

static GtkWidget *
demo_widget_new (void)
{
  return g_object_new (DEMO_TYPE_WIDGET, NULL);
}

static void
update_outline_path (DemoWidget *self)
{
  if (self->stroke_path)
    {
      g_clear_pointer (&self->outline_path, gsk_path_unref);
      self->outline_path = gsk_path_ref (self->stroke_path);
      gtk_widget_queue_draw (GTK_WIDGET (self));
    }
}

static void
update_stroke_path (DemoWidget *self)
{
  g_clear_pointer (&self->stroke_path, gsk_path_unref);
  g_clear_pointer (&self->stroke_measure, gsk_path_measure_unref);

  if (self->do_stroke)
    {
      self->stroke_path = gsk_path_stroke (self->path, self->stroke);
      self->stroke_measure = gsk_path_measure_new (self->stroke_path);
      update_outline_path (self);
    }

  gsk_path_get_bounds (self->orig_path, &self->bounds);

  if (self->stroke_path)
    {
      graphene_rect_t b;
      gsk_path_get_bounds (self->stroke_path, &b);
      graphene_rect_union (&b, &self->bounds, &self->bounds);
    }

  gtk_widget_queue_resize (GTK_WIDGET (self));
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
      gsk_path_builder_add_segment (builder, measure, self->start * length, self->end * length);
      self->path = gsk_path_builder_free_to_path (builder);
      gsk_path_measure_unref (measure);
    }
  else
    self->path = gsk_path_ref (self->orig_path);

  self->measure = gsk_path_measure_new (self->path);

  update_stroke_path (self);
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
  gsk_path_builder_add_rect (builder, &GRAPHENE_RECT_INIT (100, 100, 100, 100));
  gsk_path_builder_move_to (builder, 300, 150);
  gsk_path_builder_curve_to (builder, 300, 50, 400, 50, 400, 150);
  gsk_path_builder_curve_to (builder, 400, 250, 500, 250, 500, 150);
  gsk_path_builder_line_to (builder, 600, 150);
  gsk_path_builder_line_to (builder, 530, 190);
  path = gsk_path_builder_free_to_path (builder);

  demo_widget_set_path (demo, path);

  string = gsk_path_to_string (path);
  gtk_editable_set_text (editable, string);
  g_free (string);
  gsk_path_unref (path);

  demo->stroke = gsk_stroke_new (20);
  demo->outline_stroke = gsk_stroke_new (1);

  demo->zoom = 1;
}

static void
zoom_changed (GtkRange   *range,
              DemoWidget *self)
{
  self->zoom = gtk_range_get_value (range);
  gtk_widget_queue_resize (GTK_WIDGET (self));
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

static void
cairo_toggled (GtkCheckButton *button,
               DemoWidget      *self)
{
  self->show_cairo = gtk_check_button_get_active (button);
  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
stroke_toggled (GtkCheckButton *button,
                DemoWidget      *self)
{
  self->do_stroke = gtk_check_button_get_active (button);
  update_stroke_path (self);
  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
offset_toggled (GtkCheckButton *button,
                DemoWidget      *self)
{
  self->do_offset = gtk_check_button_get_active (button);
  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
fill_toggled (GtkCheckButton *button,
              DemoWidget      *self)
{
  self->do_fill = gtk_check_button_get_active (button);
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

static void
fill_rule_changed (GtkDropDown *combo,
                   GParamSpec  *pspec,
                   DemoWidget  *self)
{
  self->fill_rule = (GskFillRule)gtk_drop_down_get_selected (combo);
  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
cap_changed (GtkDropDown *combo,
             GParamSpec  *pspec,
             DemoWidget  *self)
{
  gsk_stroke_set_line_cap (self->stroke, (GskLineCap)gtk_drop_down_get_selected (combo));
  update_stroke_path (self);
}

static void
join_changed (GtkDropDown *combo,
              GParamSpec  *pspec,
              DemoWidget  *self)
{
  gsk_stroke_set_line_join (self->stroke, (GskLineJoin)gtk_drop_down_get_selected (combo));
  update_stroke_path (self);
}

static void
limit_changed (GtkSpinButton *spin,
               DemoWidget    *self)
{
  gsk_stroke_set_miter_limit (self->stroke, gtk_spin_button_get_value (spin));
  update_stroke_path (self);
}

static void
stroke_width_changed (GtkSpinButton *spin,
                      DemoWidget    *self)
{
  gsk_stroke_set_line_width (self->stroke, gtk_spin_button_get_value (spin));
  update_stroke_path (self);
}

static void
line_width_changed (GtkSpinButton *spin,
                    DemoWidget    *self)
{
  gsk_stroke_set_line_width (self->outline_stroke, gtk_spin_button_get_value (spin));
  update_outline_path (self);
}

static void
dash_changed (GtkEntry   *entry,
              DemoWidget *self)
{
  const char *text;
  char **s;
  float dash[20];
  int i;

  text = gtk_editable_get_text (GTK_EDITABLE (entry));
  s = g_strsplit (text, " ", -1);
  for (i = 0; s[i]; i++)
    {
      char *end;
      float f;

      f = g_strtod (s[i], &end);
      if (*end != '\0')
        return;

      dash[i] = f;
    }

  g_strfreev (s);

  gsk_stroke_set_dash (self->stroke, dash, i);

  update_stroke_path (self);
}

static void
dash_offset_changed (GtkSpinButton *spin,
                     DemoWidget    *self)
{
  gsk_stroke_set_dash_offset (self->stroke, gtk_spin_button_get_value (spin));
  update_stroke_path (self);
}

int
main (int argc, char *argv[])
{
  GtkWidget *window, *box, *demo, *entry;
  GtkWidget *popover, *button, *grid;
  GtkWidget *header, *toggle, *toggle2, *toggle3;
  GtkWidget *combo, *spin, *sw, *hbox;
  GtkWidget *zoom_scale;
  int row;

  gtk_init ();

  window = gtk_window_new ();
  gtk_window_set_default_size (GTK_WINDOW (window), 700, 500);
  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_window_set_child (GTK_WINDOW (window), box);

  demo = demo_widget_new ();
  gtk_widget_set_hexpand (demo, TRUE);
  gtk_widget_set_vexpand (demo, TRUE);
  sw = gtk_scrolled_window_new ();
  gtk_box_append (GTK_BOX (box), sw);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), demo);

  header = gtk_header_bar_new ();
  button = gtk_menu_button_new ();
  gtk_menu_button_set_icon_name (GTK_MENU_BUTTON (button), "emblem-system-symbolic");
  gtk_header_bar_pack_start (GTK_HEADER_BAR (header), button);

  zoom_scale = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 1, 20, 0.25);
  gtk_range_set_value (GTK_RANGE (zoom_scale), 1);
  g_signal_connect (zoom_scale, "value-changed", G_CALLBACK (zoom_changed), demo);
  gtk_widget_set_size_request (zoom_scale, 150, -1);
  gtk_header_bar_pack_end (GTK_HEADER_BAR (header), zoom_scale);

  gtk_window_set_titlebar (GTK_WINDOW (window), header);

  popover = gtk_popover_new ();
  gtk_menu_button_set_popover (GTK_MENU_BUTTON (button), popover);

  grid = gtk_grid_new ();
  gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
  gtk_grid_set_column_spacing (GTK_GRID (grid), 6);
  gtk_popover_set_child (GTK_POPOVER (popover), grid);

  row = 0;
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  toggle = gtk_check_button_new_with_label ("Plain");
  gtk_check_button_set_active (GTK_CHECK_BUTTON (toggle), TRUE);
  gtk_box_append (GTK_BOX (hbox), toggle);
  gtk_widget_set_hexpand (toggle, TRUE);
  toggle2 = gtk_check_button_new_with_label ("Stroke");
  gtk_widget_set_hexpand (toggle2, TRUE);
  gtk_box_append (GTK_BOX (hbox), toggle2);
  gtk_check_button_set_group (GTK_CHECK_BUTTON (toggle2), GTK_CHECK_BUTTON (toggle));
  g_signal_connect (toggle2, "toggled", G_CALLBACK (stroke_toggled), demo);
  toggle3 = gtk_check_button_new_with_label ("Offset");
  gtk_widget_set_hexpand (toggle3, TRUE);
  gtk_box_append (GTK_BOX (hbox), toggle3);
  gtk_check_button_set_group (GTK_CHECK_BUTTON (toggle3), GTK_CHECK_BUTTON (toggle));
  g_signal_connect (toggle3, "toggled", G_CALLBACK (offset_toggled), demo);
  gtk_grid_attach (GTK_GRID (grid), hbox, 0, row++, 2, 1);

  gtk_grid_attach (GTK_GRID (grid), gtk_label_new ("Fill rule"), 0, row, 1, 1);

  combo = gtk_drop_down_new_from_strings ((const char *[]){"Winding", "Even-Odd", NULL });
  g_signal_connect (combo, "notify::selected", G_CALLBACK (fill_rule_changed), demo);
  gtk_grid_attach (GTK_GRID (grid), combo, 1, row++, 1, 1);

  gtk_grid_attach (GTK_GRID (grid), gtk_label_new ("Line cap:"), 0, row, 1, 1);
  combo = gtk_drop_down_new_from_strings ((const char *[]){"Butt", "Round", "Square", NULL});
  g_signal_connect (combo, "notify::selected", G_CALLBACK (cap_changed), demo);
  gtk_grid_attach (GTK_GRID (grid), combo, 1, row++, 1, 1);

  gtk_grid_attach (GTK_GRID (grid), gtk_label_new ("Line join:"), 0, row, 1, 1);
  combo = gtk_drop_down_new_from_strings ((const char *[]){"Miter", "Miter-clip", "Round", "Bevel", "Arcs", NULL});
  g_signal_connect (combo, "notify::selected", G_CALLBACK (join_changed), demo);
  gtk_grid_attach (GTK_GRID (grid), combo, 1, row++, 1, 1);

  gtk_grid_attach (GTK_GRID (grid), gtk_label_new ("Miter limit:"), 0, row, 1, 1);
  spin = gtk_spin_button_new_with_range (0, 10, 1);
  gtk_spin_button_set_digits (GTK_SPIN_BUTTON (spin), 1);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (spin), 4);
  g_signal_connect (spin, "value-changed", G_CALLBACK (limit_changed), demo);
  gtk_grid_attach (GTK_GRID (grid), spin, 1, row++, 1, 1);

  gtk_grid_attach (GTK_GRID (grid), gtk_label_new ("Stroke width:"), 0, row, 1, 1);
  spin = gtk_spin_button_new_with_range (1, 40, 1);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (spin), 20);
  g_signal_connect (spin, "value-changed", G_CALLBACK (stroke_width_changed), demo);
  gtk_grid_attach (GTK_GRID (grid), spin, 1, row++, 1, 1);

  gtk_grid_attach (GTK_GRID (grid), gtk_label_new ("Line width:"), 0, row, 1, 1);
  spin = gtk_spin_button_new_with_range (1, 20, 1);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (spin), 1);
  g_signal_connect (spin, "value-changed", G_CALLBACK (line_width_changed), demo);
  gtk_grid_attach (GTK_GRID (grid), spin, 1, row++, 1, 1);

  gtk_grid_attach (GTK_GRID (grid), gtk_label_new ("Dash pattern:"), 0, row, 1, 1);
  entry = gtk_entry_new ();
  g_signal_connect (entry, "changed", G_CALLBACK (dash_changed), demo);
  gtk_grid_attach (GTK_GRID (grid), entry, 1, row++, 1, 1);

  gtk_grid_attach (GTK_GRID (grid), gtk_label_new ("Dash offset:"), 0, row, 1, 1);
  spin = gtk_spin_button_new_with_range (-1000, 1000, 10);
  gtk_spin_button_set_digits (GTK_SPIN_BUTTON (spin), 1);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (spin), 0);
  g_signal_connect (spin, "value-changed", G_CALLBACK (dash_offset_changed), demo);
  gtk_grid_attach (GTK_GRID (grid), spin, 1, row++, 1, 1);

  toggle = gtk_check_button_new_with_label ("Show closest point");
  g_signal_connect (toggle, "toggled", G_CALLBACK (track_toggled), demo);
  gtk_grid_attach (GTK_GRID (grid), toggle, 1, row++, 1, 1);

  toggle = gtk_check_button_new_with_label ("Show bounding box");
  g_signal_connect (toggle, "toggled", G_CALLBACK (bb_toggled), demo);
  gtk_grid_attach (GTK_GRID (grid), toggle, 1, row++, 1, 1);

  toggle = gtk_check_button_new_with_label ("Highlight on hover");
  g_signal_connect (toggle, "toggled", G_CALLBACK (fill_toggled), demo);
  gtk_grid_attach (GTK_GRID (grid), toggle, 1, row++, 1, 1);

  toggle = gtk_check_button_new_with_label ("Compare cairo stroke");
  g_signal_connect (toggle, "toggled", G_CALLBACK (cairo_toggled), demo);
  gtk_grid_attach (GTK_GRID (grid), toggle, 1, row++, 1, 1);

  entry = gtk_entry_new ();
  g_signal_connect (entry, "activate", G_CALLBACK (activate), demo);
  gtk_box_append (GTK_BOX (box), entry);

  start_scale = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 0, 1, 0.01);
  g_signal_connect (start_scale, "value-changed", G_CALLBACK (range_changed), demo);
  gtk_box_append (GTK_BOX (box), start_scale);

  end_scale = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 0, 1, 0.01);
  gtk_range_set_value (GTK_RANGE (end_scale), 1);
  g_signal_connect (end_scale, "value-changed", G_CALLBACK (range_changed), demo);
  gtk_box_append (GTK_BOX (box), end_scale);

  init_demo (DEMO_WIDGET (demo), GTK_EDITABLE (entry));

  gtk_window_present (GTK_WINDOW (window));

  while (g_list_model_get_n_items (gtk_window_get_toplevels ()) > 0)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
