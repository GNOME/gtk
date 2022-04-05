/* Path/Glyphs
 *
 * This demo shows boolean operation on paths with the example
 * of glyphs from a font.
 */

#include <gtk/gtk.h>
#include <gsk/gskcurveprivate.h>

#include <hb-ot.h>

#define DEMO_TYPE_WIDGET (glyph_demo_get_type ())
G_DECLARE_FINAL_TYPE (GlyphDemo, glyph_demo, DEMO, WIDGET, GtkWidget)

struct _GlyphDemo
{
  GtkWidget parent_instance;
  GskPath *orig_path1;
  GskPath *orig_path2;
  GskPath *path;
  GskPath *control_path;
  graphene_point_t point;
  graphene_point_t point2;
  graphene_vec2_t tangent;

  GtkWidget *label;

  GskFillRule fill_rule;
  gboolean do_fill;
  gboolean do_outline;
  int operation;
  gboolean show_points;
  gboolean show_controls;
  gboolean show_bounding_box;

  graphene_rect_t bounds;

  char *font_file;
  char *text;

  GskPath *short_path;
  int short_count;

  GtkTextBuffer *buffer;
  GtkTextBuffer *out_buffer;
  GtkAdjustment *zoom_adj;
  GtkAdjustment *weight_adj;
};

struct _GlyphDemoClass
{
  GtkWidgetClass parent_class;
};

G_DEFINE_TYPE (GlyphDemo, glyph_demo, GTK_TYPE_WIDGET)

static int short_count;

static gboolean
short_cb (GskPathOperation        op,
          const graphene_point_t *pts,
          gsize                   n_pts,
          float                   weight,
          gpointer                user_data)
{
  GskPathBuilder *builder = user_data;

  if (short_count <= 0)
    return FALSE;

  short_count--;

  switch (op)
    {
    case GSK_PATH_MOVE:
      gsk_path_builder_move_to (builder, pts[0].x, pts[0].y);
      break;
    case GSK_PATH_CLOSE:
      gsk_path_builder_close (builder);
      break;
    case GSK_PATH_LINE:
      gsk_path_builder_line_to (builder, pts[1].x, pts[1].y);
      break;
    case GSK_PATH_CURVE:
      gsk_path_builder_curve_to (builder,
                                 pts[1].x, pts[1].y,
                                 pts[2].x, pts[2].y,
                                 pts[3].x, pts[3].y);
      break;
    case GSK_PATH_CONIC:
    default:
      g_assert_not_reached ();
    }

  return TRUE;
}

static void
update_short_path (GlyphDemo *self)
{
  GskPathBuilder *builder;

  g_clear_pointer (&self->short_path, gsk_path_unref);

  builder = gsk_path_builder_new ();

  short_count = self->short_count;
  gsk_path_foreach (self->path, GSK_PATH_FOREACH_ALLOW_CURVE, short_cb, builder);
  self->short_path = gsk_path_builder_free_to_path (builder);

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
reset_short_path (GlyphDemo *self)
{
  self->short_count = 0;
  update_short_path (self);
}

static void
apply_short_path (GlyphDemo *self)
{
  self->short_count = 10000;
  update_short_path (self);
}

static void
short_path_step (GlyphDemo *self)
{
  self->short_count++;
  update_short_path (self);
}

static void
pressed_cb (GtkEventController *controller,
            guint               keyval,
            guint               keycode,
            GdkModifierType     state,
            gpointer            data)
{
  GlyphDemo *self = (GlyphDemo *)gtk_event_controller_get_widget (controller);

  if (keyval == GDK_KEY_BackSpace)
    reset_short_path (self);
  else
   short_path_step (self);
}

static gboolean
zoom_in (GtkWidget *widget,
         GVariant  *args,
         gpointer   user_data)
{
  GlyphDemo *self = (GlyphDemo *)widget;
  gtk_adjustment_set_value (self->zoom_adj,
                            gtk_adjustment_get_value (self->zoom_adj) + gtk_adjustment_get_step_increment (self->zoom_adj));
  return TRUE;
}

static gboolean
zoom_out (GtkWidget *widget,
          GVariant  *args,
          gpointer   user_data)
{
  GlyphDemo *self = (GlyphDemo *)widget;
  gtk_adjustment_set_value (self->zoom_adj,
                            gtk_adjustment_get_value (self->zoom_adj) - gtk_adjustment_get_step_increment (self->zoom_adj));
  return TRUE;
}

static void
glyph_demo_init (GlyphDemo *self)
{
  GtkEventController *controller;
  GtkShortcutTrigger *trigger;
  GtkShortcutAction *action;
  GtkShortcut *shortcut;

  gtk_widget_set_focusable (GTK_WIDGET (self), TRUE);

  self->label = gtk_label_new ("");
  gtk_widget_set_parent (self->label, GTK_WIDGET (self));
  gtk_widget_set_halign (self->label, GTK_ALIGN_END);
  gtk_widget_set_valign (self->label, GTK_ALIGN_START);
  self->do_outline = TRUE;
  self->font_file = g_strdup ("/usr/share/fonts/cantarell/Cantarell-VF.otf");
  self->text = g_strdup ("KP");

  controller = gtk_event_controller_key_new ();
  g_signal_connect (controller, "key-pressed", G_CALLBACK (pressed_cb), NULL);
  gtk_widget_add_controller (GTK_WIDGET (self), controller);

  controller = gtk_shortcut_controller_new ();
  gtk_shortcut_controller_set_scope (GTK_SHORTCUT_CONTROLLER (controller), GTK_SHORTCUT_SCOPE_GLOBAL);

  trigger = gtk_keyval_trigger_new (GDK_KEY_plus, GDK_CONTROL_MASK);
  action = gtk_callback_action_new (zoom_in, NULL, NULL);
  shortcut = gtk_shortcut_new (trigger, action);
  gtk_shortcut_controller_add_shortcut (GTK_SHORTCUT_CONTROLLER (controller), shortcut);

  trigger = gtk_keyval_trigger_new (GDK_KEY_minus, GDK_CONTROL_MASK);
  action = gtk_callback_action_new (zoom_out, NULL, NULL);
  shortcut = gtk_shortcut_new (trigger, action);
  gtk_shortcut_controller_add_shortcut (GTK_SHORTCUT_CONTROLLER (controller), shortcut);

  gtk_widget_add_controller (GTK_WIDGET (self), controller);
}

static void
draw_point2 (GtkSnapshot            *snapshot,
             const graphene_point_t *pt,
             GdkRGBA                *color,
             float                   size)
{
  graphene_rect_t bounds;

  bounds.origin.x = pt->x - size / 2;
  bounds.origin.y = pt->y - size / 2;
  bounds.size.width = size;
  bounds.size.height = size;

  gtk_snapshot_append_color (snapshot, color, &bounds);
}

static void
draw_point (GtkSnapshot            *snapshot,
            const graphene_point_t *pt)
{
  draw_point2 (snapshot, pt, &(GdkRGBA) { 1, 0, 0, 1 }, 4);
}

static gboolean
curve_cb (GskPathOperation        op,
          const graphene_point_t *pts,
          gsize                   n_ts,
          float                   weight,
          gpointer                user_data)
{
  GtkSnapshot *snapshot = user_data;

  switch (op)
    {
    case GSK_PATH_MOVE:
      break;

    case GSK_PATH_CLOSE:
      draw_point (snapshot, &pts[0]);
      break;

    case GSK_PATH_LINE:
      draw_point (snapshot, &pts[1]);
      break;

    case GSK_PATH_CURVE:
      draw_point (snapshot, &pts[1]);
      draw_point (snapshot, &pts[2]);
      draw_point (snapshot, &pts[3]);
      break;

    case GSK_PATH_CONIC:
    default:
      g_assert_not_reached ();
    }

  return TRUE;
}

static gboolean
point_cb (GskPathOperation        op,
          const graphene_point_t *pts,
          gsize                   n_ts,
          float                   weight,
          gpointer                user_data)
{
  GtkSnapshot *snapshot = user_data;

  switch (op)
    {
    case GSK_PATH_MOVE:
      draw_point2 (snapshot, &pts[0], &(GdkRGBA) { 0, 0, 1, 1 }, 8);
      break;

    case GSK_PATH_CLOSE:
      draw_point (snapshot, &pts[1]);
      break;

    case GSK_PATH_LINE:
      draw_point (snapshot, &pts[1]);
      break;

    case GSK_PATH_CURVE:
      draw_point (snapshot, &pts[3]);
      break;

    case GSK_PATH_CONIC:
    default:
      g_assert_not_reached ();
    }

  return TRUE;
}

static void
glyph_demo_snapshot (GtkWidget   *widget,
                      GtkSnapshot *snapshot)
{
  GlyphDemo *self = DEMO_WIDGET (widget);
  int width, height;
  double zoom;

  if (!self->path)
    return;

  zoom = gtk_adjustment_get_value (self->zoom_adj);

  gtk_snapshot_save (snapshot);

  gtk_snapshot_scale (snapshot, zoom, zoom);

  width = self->bounds.origin.x + self->bounds.size.width + 10;
  height =self->bounds.origin.y + self->bounds.size.height + 10;

  if (self->do_fill)
    {
      gtk_snapshot_push_fill (snapshot, self->path, self->fill_rule);

      gtk_snapshot_append_color (snapshot,
                                 &(GdkRGBA){ 1, 0, 1, 0.2},
                                 &GRAPHENE_RECT_INIT (0, 0, width, height ));

      gtk_snapshot_pop (snapshot);
    }

  if (self->do_outline)
    {
      GskStroke *stroke;

      stroke = gsk_stroke_new (1.0);
      gtk_snapshot_push_stroke (snapshot, self->orig_path1, stroke);
      gtk_snapshot_append_color (snapshot,
                                 &(GdkRGBA){ 0, 0, 0, 0.3 },
                                 &GRAPHENE_RECT_INIT (0, 0, width, height ));
      gtk_snapshot_pop (snapshot);
      gtk_snapshot_push_stroke (snapshot, self->orig_path2, stroke);
      gtk_snapshot_append_color (snapshot,
                                 &(GdkRGBA){ 0, 0, 0, 0.3 },
                                 &GRAPHENE_RECT_INIT (0, 0, width, height ));
      gtk_snapshot_pop (snapshot);
      gsk_stroke_free (stroke);
   }

  if (1)
    {
      GskStroke *stroke;

      stroke = gsk_stroke_new (2.0);
      gtk_snapshot_push_stroke (snapshot, self->short_path, stroke);
      gtk_snapshot_append_color (snapshot,
                                 &(GdkRGBA){ 0, 0, 0, 1.0 },
                                 &GRAPHENE_RECT_INIT (0, 0, width, height ));
      gtk_snapshot_pop (snapshot);
      gsk_stroke_free (stroke);
    }

  if (self->show_controls)
    {
      GskStroke *stroke;

      stroke = gsk_stroke_new (1.0);
      gtk_snapshot_push_stroke (snapshot, self->control_path, stroke);
      gsk_stroke_free (stroke);

      gtk_snapshot_append_color (snapshot,
                                 &(GdkRGBA){ 1, 0, 0, 1},
                                 &GRAPHENE_RECT_INIT (0, 0, width, height ));

      gtk_snapshot_pop (snapshot);

      gsk_path_foreach (self->path, GSK_PATH_FOREACH_ALLOW_CURVE, curve_cb, snapshot);
    }
  else if (self->show_points)
    {
      gsk_path_foreach (self->path, GSK_PATH_FOREACH_ALLOW_CURVE, point_cb, snapshot);
    }

  if (self->show_bounding_box)
    {
      graphene_rect_t bounds;

      if (gsk_path_get_bounds (self->path, &bounds))
        {
          GskPathBuilder *builder;
          GskPath *path;
          GskStroke *stroke;

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

  gtk_snapshot_restore (snapshot);
}

static void
glyph_demo_measure (GtkWidget      *widget,
                     GtkOrientation  orientation,
                     int             for_size,
                     int            *minimum,
                     int            *natural,
                     int            *minimum_baseline,
                     int            *natural_baseline)
{
  GlyphDemo *self = DEMO_WIDGET (widget);
  double zoom;
  float size;

  zoom = gtk_adjustment_get_value (self->zoom_adj);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    size = zoom * (self->bounds.origin.x + self->bounds.size.width + 10);
  else
    size = zoom * (self->bounds.origin.y + self->bounds.size.height + 10);

  *minimum = *natural = (int)size;
}

static void
glyph_demo_size_allocate (GtkWidget *widget,
                           int        width,
                           int        height,
                           int        baseline)
{
  GlyphDemo *self = DEMO_WIDGET (widget);
  GtkRequisition min, nat;

  gtk_widget_get_preferred_size (self->label, &min, &nat);

  gtk_widget_size_allocate (self->label,
                            &(GtkAllocation) { width - nat.width, 0, nat.width, nat.height},
                            -1);
}

static void
glyph_demo_dispose (GObject *object)
{
  GlyphDemo *self = DEMO_WIDGET (object);

  g_clear_pointer (&self->orig_path1, gsk_path_unref);
  g_clear_pointer (&self->orig_path2, gsk_path_unref);
  g_clear_pointer (&self->path, gsk_path_unref);
  g_clear_pointer (&self->control_path, gsk_path_unref);
  g_clear_pointer (&self->short_path, gsk_path_unref);
  g_clear_pointer (&self->label, gtk_widget_unparent);
  g_free (self->font_file);
  g_free (self->text);

  G_OBJECT_CLASS (glyph_demo_parent_class)->dispose (object);
}

static void
glyph_demo_class_init (GlyphDemoClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->dispose = glyph_demo_dispose;

  widget_class->snapshot = glyph_demo_snapshot;
  widget_class->measure = glyph_demo_measure;
  widget_class->size_allocate = glyph_demo_size_allocate;
}

static GtkWidget *
glyph_demo_new (void)
{
  return g_object_new (DEMO_TYPE_WIDGET, NULL);
}

static gboolean
control_cb (GskPathOperation        op,
            const graphene_point_t *pts,
            gsize                   n_pts,
            float                   weight,
            gpointer                user_data)
{
  GskPathBuilder *builder = user_data;

  switch (op)
    {
    case GSK_PATH_MOVE:
      gsk_path_builder_move_to (builder, pts[0].x, pts[0].y);
      break;
    case GSK_PATH_CLOSE:
      gsk_path_builder_close (builder);
      break;
    case GSK_PATH_LINE:
      gsk_path_builder_line_to (builder, pts[1].x, pts[1].y);
      break;
    case GSK_PATH_CURVE:
      gsk_path_builder_line_to (builder, pts[1].x, pts[1].y);
      gsk_path_builder_line_to (builder, pts[2].x, pts[2].y);
      gsk_path_builder_line_to (builder, pts[3].x, pts[3].y);
      break;
    case GSK_PATH_CONIC:
    default:
      g_assert_not_reached ();
    }

  return TRUE;
}

static char *
newlineify (char *s)
{
  if (*s == '\0')
    return s;

  for (char *p = s + 1; *p; p++)
    {
      if (strchr ("XZzMmLlHhVvCcSsQqTtOoAaa", *p))
        p[-1] = '\n';
    }

  return s;
}

static void
update_text_output (GlyphDemo *self)
{
  char *text;

  text = newlineify (gsk_path_to_string (self->path));
  gtk_text_buffer_set_text (self->out_buffer, text, -1);
  g_free (text);
}

static void
glyph_demo_set_path (GlyphDemo *self,
                      GskPath    *path1,
                      GskPath    *path2)
{
  GskPathBuilder *builder;
  GskPath *path;
  graphene_rect_t bounds;

  g_clear_pointer (&self->orig_path1, gsk_path_unref);
  g_clear_pointer (&self->orig_path2, gsk_path_unref);
  g_clear_pointer (&self->path, gsk_path_unref);
  g_clear_pointer (&self->control_path, gsk_path_unref);
  g_clear_pointer (&self->short_path, gsk_path_unref);

  self->orig_path1 = gsk_path_ref (path1);
  self->orig_path2 = gsk_path_ref (path2);
  switch (self->operation)
    {
    case 0:
      builder = gsk_path_builder_new ();
      gsk_path_builder_add_path (builder, self->orig_path1);
      gsk_path_builder_add_path (builder, self->orig_path2);
      self->path = gsk_path_builder_free_to_path (builder);
      break;
    case 1:
      builder = gsk_path_builder_new ();
      path = gsk_path_reverse (self->orig_path1);
      gsk_path_builder_add_path (builder, path);
      gsk_path_unref (path);
      path = gsk_path_reverse (self->orig_path2);
      gsk_path_builder_add_path (builder, path);
      gsk_path_unref (path);
      self->path = gsk_path_builder_free_to_path (builder);
      break;
    case 2:
      builder = gsk_path_builder_new ();
      path = gsk_path_simplify (self->orig_path1, self->fill_rule);
      gsk_path_builder_add_path (builder, path);
      gsk_path_unref (path);
      path = gsk_path_simplify (self->orig_path2, self->fill_rule);
      gsk_path_builder_add_path (builder, path);
      gsk_path_unref (path);
      self->path = gsk_path_builder_free_to_path (builder);
      break;
    case 3:
      self->path = gsk_path_union (self->orig_path1, self->orig_path2, self->fill_rule);
      break;
    case 4:
      self->path = gsk_path_intersection (self->orig_path1, self->orig_path2, self->fill_rule);
      break;
    case 5:
      self->path = gsk_path_difference (self->orig_path1, self->orig_path2, self->fill_rule);
      break;
    case 6:
      self->path = gsk_path_symmetric_difference (self->orig_path1, self->orig_path2, self->fill_rule);
      break;
    default:
      g_assert_not_reached ();
    }

  builder = gsk_path_builder_new ();
  gsk_path_foreach (self->path, GSK_PATH_FOREACH_ALLOW_CURVE, control_cb, builder);
  self->control_path = gsk_path_builder_free_to_path (builder);

  gsk_path_get_bounds (self->orig_path1, &bounds);
  self->bounds = bounds;
  gsk_path_get_bounds (self->orig_path2, &bounds);
  graphene_rect_union (&bounds, &self->bounds, &self->bounds);
  gsk_path_get_bounds (self->control_path, &bounds);
  graphene_rect_union (&bounds, &self->bounds, &self->bounds);

  self->short_count = 10000;
  update_short_path (self);
  update_text_output (self);

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static float glyph_x0, glyph_y0;

static void
move_to (hb_draw_funcs_t *dfuncs,
         GskPathBuilder  *builder,
         hb_draw_state_t *st,
         float            x,
         float            y,
         void            *data)
{
  gsk_path_builder_move_to (builder, glyph_x0 + x, glyph_y0 - y);
}

static void
line_to (hb_draw_funcs_t *dfuncs,
         GskPathBuilder  *builder,
         hb_draw_state_t *st,
         float            x,
         float            y,
         void            *data)
{
  gsk_path_builder_line_to (builder, glyph_x0 + x, glyph_y0 - y);
}

static void
cubic_to (hb_draw_funcs_t *dfuncs,
          GskPathBuilder  *builder,
          hb_draw_state_t *st,
          float            x1,
          float            y1,
          float            x2,
          float            y2,
          float            x3,
          float            y3,
          void            *data)
{
  gsk_path_builder_curve_to (builder,
                             glyph_x0 + x1, glyph_y0 - y1,
                             glyph_x0 + x2, glyph_y0 - y2,
                             glyph_x0 + x3, glyph_y0 - y3);
}


static void
close_path (hb_draw_funcs_t *dfuncs,
            GskPathBuilder  *builder,
            hb_draw_state_t *st,
            void            *data)
{
  gsk_path_builder_close (builder);
}

static GskPath *
char_to_path (hb_font_t *font,
              gunichar   ch)
{
  hb_codepoint_t glyph;
  hb_glyph_extents_t extents;
  GskPathBuilder *builder;
  hb_draw_funcs_t *funcs;

  builder = gsk_path_builder_new ();

  hb_font_get_nominal_glyph (font, ch, &glyph);
  hb_font_get_glyph_extents (font, glyph, &extents);

  glyph_x0 = 10 + extents.x_bearing;
  glyph_y0 = 10 + extents.y_bearing;

  gsk_path_builder_move_to (builder, extents.x_bearing, - extents.height);

  funcs = hb_draw_funcs_create ();

  hb_draw_funcs_set_move_to_func (funcs, (hb_draw_move_to_func_t) move_to, NULL, NULL);
  hb_draw_funcs_set_line_to_func (funcs, (hb_draw_line_to_func_t) line_to, NULL, NULL);
  hb_draw_funcs_set_cubic_to_func (funcs, (hb_draw_cubic_to_func_t) cubic_to, NULL, NULL);
  hb_draw_funcs_set_close_path_func (funcs, (hb_draw_close_path_func_t) close_path, NULL, NULL);

  hb_font_get_glyph_shape (font, glyph, funcs, builder);

  hb_draw_funcs_destroy (funcs);

  return gsk_path_builder_free_to_path (builder);
}

static void
init_demo_from_font (GlyphDemo *demo)
{
  hb_blob_t *blob;
  hb_face_t *face;
  hb_font_t *font;
  const char *p;
  GskPath *path1, *path2;
  char *s1, *s2, *text;
  hb_variation_t wght;

  blob = hb_blob_create_from_file (demo->font_file);
  face = hb_face_create (blob, 0);
  font = hb_font_create (face);
  wght.tag = HB_OT_TAG_VAR_AXIS_WEIGHT;
  wght.value = gtk_adjustment_get_value (demo->weight_adj);
  hb_font_set_variations (font, &wght, 1);

  path1 = char_to_path (font, g_utf8_get_char (demo->text));

  p = g_utf8_next_char (demo->text);
  if (*p)
    path2 = char_to_path (font, g_utf8_get_char (p));
  else
    path2 = gsk_path_ref (path1);

  s1 = newlineify (gsk_path_to_string (path1));
  s2 = newlineify (gsk_path_to_string (path2));
  text = g_strconcat (s1, "\n\n", s2, NULL);

  gtk_text_buffer_set_text (demo->buffer, text, -1);

  g_free (s1);
  g_free (s2);
  g_free (text);

  gsk_path_unref (path1);
  gsk_path_unref (path2);
}

static void
glyph_demo_set_font_file (GlyphDemo *demo,
                           const char *file)
{
  g_free (demo->font_file);
  demo->font_file = g_strdup (file);
  init_demo_from_font (demo);
}

static void
glyph_demo_set_text (GlyphDemo *demo,
                      const char *text)
{
  g_free (demo->text);
  demo->text = g_strdup (text);
  init_demo_from_font (demo);
}

static void
zoom_changed (GtkAdjustment *adj,
              GlyphDemo    *self)
{
  gtk_widget_queue_resize (GTK_WIDGET (self));
}

static void
weight_changed (GtkAdjustment *adj,
              GlyphDemo    *self)
{
  init_demo_from_font (self);
}

static void
bb_toggled (GtkCheckButton *button,
            GlyphDemo      *self)
{
  self->show_bounding_box = gtk_check_button_get_active (button);
  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
fill_toggled (GtkCheckButton *button,
              GlyphDemo      *self)
{
  self->do_fill = gtk_check_button_get_active (button);
  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
points_toggled (GtkCheckButton *button,
                GlyphDemo      *self)
{
  self->show_points = gtk_check_button_get_active (button);
  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
controls_toggled (GtkCheckButton *button,
                  GlyphDemo      *self)
{
  self->show_controls = gtk_check_button_get_active (button);
  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
operation_changed (GtkDropDown *combo,
                   GParamSpec  *pspec,
                   GlyphDemo  *self)
{
  GskPath *path1, *path2;

  self->operation = (int) gtk_drop_down_get_selected (combo);
  path1 = gsk_path_ref (self->orig_path1);
  path2 = gsk_path_ref (self->orig_path2);
  glyph_demo_set_path (self, path1, path2);
  gsk_path_unref (path1);
  gsk_path_unref (path2);
}

static void
outline_toggled (GtkCheckButton *button,
                 GlyphDemo      *self)
{
  self->do_outline = gtk_check_button_get_active (button);
  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
fill_rule_changed (GtkDropDown *combo,
                   GParamSpec  *pspec,
                   GlyphDemo  *self)
{
  self->fill_rule = (GskFillRule)gtk_drop_down_get_selected (combo);
  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
switch_stack (GtkToggleButton *button,
              GtkStack        *stack)
{
  if (gtk_toggle_button_get_active (button))
    gtk_stack_set_visible_child_name (stack, "visual");
  else
    gtk_stack_set_visible_child_name (stack, "text");
}

static void
response_cb (GtkNativeDialog *dialog,
             int response,
             gpointer user_data)
{
  GtkButton *button = user_data;
  GlyphDemo *demo;

  demo = (GlyphDemo *)g_object_get_data (G_OBJECT (dialog), "demo");

  if (response == GTK_RESPONSE_ACCEPT)
    {
      GFile *file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (dialog));
      const char *path = g_file_peek_path (file);
      char *basename = g_path_get_basename (path);

      gtk_button_set_label (button, basename);
      g_free (basename);

      glyph_demo_set_font_file (demo, path);
    }

  gtk_native_dialog_destroy (dialog);
  g_object_unref (dialog);
}

static void
filechooser_cb (GtkButton  *button,
                GlyphDemo *demo)
{
  GtkFileChooserNative *dialog;

  dialog = gtk_file_chooser_native_new ("Font",
                                        GTK_WINDOW (gtk_widget_get_ancestor (GTK_WIDGET (button), GTK_TYPE_WINDOW)),
                                        GTK_FILE_CHOOSER_ACTION_OPEN,
                                        "Open", "Cancel");

  g_object_set_data (G_OBJECT (dialog), "demo", demo);
  g_signal_connect (dialog, "response", G_CALLBACK (response_cb), button);
  gtk_native_dialog_show (GTK_NATIVE_DIALOG (dialog));
}

static void
activate_entry (GtkEntry   *entry,
                GlyphDemo *demo)
{
  glyph_demo_set_text (demo, gtk_editable_get_text (GTK_EDITABLE (entry)));
}

static void
text_changed (GtkTextBuffer *buffer,
              GlyphDemo    *demo)
{
  GtkTextIter start, end;
  char *text;
  char *p;
  GskPath *path1, *path2;

  gtk_text_buffer_get_bounds (buffer, &start, &end);
  text = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);

  text = g_strstrip (text);

  p = strstr (text, "\n\n");
  if (p)
    {
      char *text1, *text2;

      text1 = g_strstrip (g_strndup (text, p - text));
      text2 = g_strstrip (g_strdup (p + 2));

      path1 = gsk_path_parse (text1);
      path2 = gsk_path_parse (text2);

      g_free (text1);
      g_free (text2);
    }
  else
    {
      path1 = gsk_path_parse (text);
      path2 = NULL;
    }

  g_free (text);

  if (!path1)
    path1 = gsk_path_parse ("");
  if (!path2)
    path2 = gsk_path_ref (path1);

  glyph_demo_set_path (demo, path1, path2);

  gsk_path_unref (path1);
  gsk_path_unref (path2);
}

static const char css_data[] =
  ".bottombar {"
  "  border-top: 1px solid gray;"
  "  padding: 10px;"
  "  background: none;"
  "  border-spacing: 10px;"
  "}"
  ""
  ".sidebar {"
  "  border-left: 1px solid gray;"
  "  background: none;"
  "}"
  ""
  ".font {"
  "  padding: 10px;"
  "}";

GtkWidget *
do_glyphs (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;

  if (!window)
    {
      GtkWidget *box, *demo;
      GtkWidget *button;
      GtkWidget *header, *toggle;
      GtkWidget *combo, *sw;
      GtkWidget *zoom_scale;
      GtkWidget *weight_scale;
      GtkWidget *hbox, *entry;
      GtkWidget *text;
      GtkWidget *box3;
      GtkWidget *box2, *hbox2;
      GtkWidget *toggle2, *hbox3;
      GtkWidget *stack;
      GtkCssProvider *style;

      style = gtk_css_provider_new ();
      gtk_css_provider_load_from_data (style, css_data, -1);
      gtk_style_context_add_provider_for_display (gdk_display_get_default (), GTK_STYLE_PROVIDER (style), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
      g_object_unref (style);

      window = gtk_window_new ();

      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *) &window);

      box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_window_set_child (GTK_WINDOW (window), box);

      demo = glyph_demo_new ();
      gtk_widget_set_hexpand (demo, TRUE);
      gtk_widget_set_vexpand (demo, TRUE);
      sw = gtk_scrolled_window_new ();
      gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), demo);
      gtk_scrolled_window_set_propagate_natural_width (GTK_SCROLLED_WINDOW (sw), TRUE);
      gtk_scrolled_window_set_propagate_natural_height (GTK_SCROLLED_WINDOW (sw), TRUE);

      stack = gtk_stack_new ();
      gtk_stack_add_named (GTK_STACK (stack), sw, "visual");

      text = gtk_text_view_new ();
      g_object_set (text, "top-margin", 10,
                          "bottom-margin", 10,
                          "left-margin", 10,
                          "right-margin", 10,
                          NULL);
      ((GlyphDemo *)demo)->out_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (text));
      sw = gtk_scrolled_window_new ();
      gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), text);
      gtk_stack_add_named (GTK_STACK (stack), sw, "text");

      box3 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
      gtk_box_append (GTK_BOX (box), box3);
      gtk_box_append (GTK_BOX (box3), stack);

      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
      gtk_widget_add_css_class (hbox, "bottombar");
      gtk_box_append (GTK_BOX (box), hbox);

      combo = gtk_drop_down_new_from_strings ((const char *[]){ "Original", "Reverse", "Simplified", "Union", "Intersection", "Difference", "Symmetric Difference", NULL });
      g_signal_connect (combo, "notify::selected", G_CALLBACK (operation_changed), demo);
      gtk_box_append (GTK_BOX (hbox), combo);

      toggle = gtk_check_button_new_with_label ("Paths");
      gtk_check_button_set_active (GTK_CHECK_BUTTON (toggle), TRUE);
      g_signal_connect (toggle, "toggled", G_CALLBACK (outline_toggled), demo);
      gtk_box_append (GTK_BOX (hbox), toggle);

      toggle = gtk_check_button_new_with_label ("Fill");
      g_signal_connect (toggle, "toggled", G_CALLBACK (fill_toggled), demo);
      gtk_box_append (GTK_BOX (hbox), toggle);

      toggle = gtk_check_button_new_with_label ("Points");
      g_signal_connect (toggle, "toggled", G_CALLBACK (points_toggled), demo);
      gtk_box_append (GTK_BOX (hbox), toggle);

      toggle = gtk_check_button_new_with_label ("Controls");
      g_signal_connect (toggle, "toggled", G_CALLBACK (controls_toggled), demo);
      gtk_box_append (GTK_BOX (hbox), toggle);

      toggle = gtk_check_button_new_with_label ("Bounds");
      g_signal_connect (toggle, "toggled", G_CALLBACK (bb_toggled), demo);
      gtk_box_append (GTK_BOX (hbox), toggle);

      combo = gtk_drop_down_new_from_strings ((const char *[]){"Winding", "Even-Odd", NULL });
      g_signal_connect (combo, "notify::selected", G_CALLBACK (fill_rule_changed), demo);
      gtk_box_append (GTK_BOX (hbox), combo);

      button = gtk_button_new_from_icon_name ("edit-redo-symbolic");
      g_signal_connect_swapped (button, "clicked", G_CALLBACK (apply_short_path), demo);
      gtk_widget_set_hexpand (button, TRUE);
      gtk_widget_set_halign (button, GTK_ALIGN_END);
      gtk_box_append (GTK_BOX (hbox), button);

      button = gtk_button_new_from_icon_name ("list-add-symbolic");
      g_signal_connect_swapped (button, "clicked", G_CALLBACK (short_path_step), demo);
      gtk_box_append (GTK_BOX (hbox), button);

      button = gtk_button_new_from_icon_name ("edit-undo-symbolic");
      g_signal_connect_swapped (button, "clicked", G_CALLBACK (reset_short_path), demo);
      gtk_box_append (GTK_BOX (hbox), button);

      box2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_widget_add_css_class (box2, "sidebar");
      gtk_widget_set_hexpand (box2, FALSE);
      gtk_box_append (GTK_BOX (box3), box2);

      hbox2 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
      gtk_widget_add_css_class (hbox2, "font");
      gtk_widget_add_css_class (hbox2, "linked");
      gtk_widget_set_halign (hbox2, GTK_ALIGN_CENTER);
      gtk_box_append (GTK_BOX (box2), hbox2);
      button = gtk_button_new_with_label ("Cantarell-VF.otf");
      gtk_box_append (GTK_BOX (hbox2), button);
      g_signal_connect (button, "clicked", G_CALLBACK (filechooser_cb), demo);

      entry = gtk_entry_new ();
      gtk_editable_set_width_chars (GTK_EDITABLE (entry), 2);
      gtk_box_append (GTK_BOX (hbox2), entry);
      g_signal_connect (entry, "activate", G_CALLBACK (activate_entry), demo);

      sw = gtk_scrolled_window_new ();
      text = gtk_text_view_new ();
      g_object_set (text, "top-margin", 10,
                          "bottom-margin", 10,
                          "left-margin", 10,
                          "right-margin", 10,
                          NULL);
      gtk_widget_set_hexpand (text, TRUE);
      gtk_widget_set_vexpand (text, TRUE);
      ((GlyphDemo *)demo)->buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (text));
      g_signal_connect (((GlyphDemo *)demo)->buffer, "changed", G_CALLBACK (text_changed), demo);
      gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), text);
      gtk_box_append (GTK_BOX (box2), sw);

      ((GlyphDemo *)demo)->weight_adj = gtk_adjustment_new (400, 100, 1000, 10, 100, 0);
      g_signal_connect (((GlyphDemo *)demo)->weight_adj, "value-changed", G_CALLBACK (weight_changed), demo);

      gtk_editable_set_text (GTK_EDITABLE (entry), "KP");
      glyph_demo_set_text ((GlyphDemo *)demo, gtk_editable_get_text (GTK_EDITABLE (entry)));

      header = gtk_header_bar_new ();

      toggle = gtk_toggle_button_new ();
      gtk_button_set_icon_name (GTK_BUTTON (toggle), "image-x-generic-symbolic");
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle), TRUE);
      g_signal_connect (toggle, "toggled", G_CALLBACK (switch_stack), stack);
      toggle2 = gtk_toggle_button_new ();
      gtk_button_set_icon_name (GTK_BUTTON (toggle2), "view-list-symbolic");
      gtk_toggle_button_set_group (GTK_TOGGLE_BUTTON (toggle2), GTK_TOGGLE_BUTTON (toggle));

      hbox3 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
      gtk_widget_add_css_class (hbox3, "linked");
      gtk_box_append (GTK_BOX (hbox3), toggle);
      gtk_box_append (GTK_BOX (hbox3), toggle2);
      gtk_header_bar_pack_start (GTK_HEADER_BAR (header), hbox3);

      weight_scale = gtk_scale_new (GTK_ORIENTATION_HORIZONTAL, ((GlyphDemo *)demo)->weight_adj);
      gtk_widget_set_tooltip_text (weight_scale, "Weight");

      gtk_scale_add_mark (GTK_SCALE (weight_scale), 400, GTK_POS_BOTTOM, NULL);
      gtk_widget_set_size_request (weight_scale, 300, -1);
      gtk_header_bar_pack_start (GTK_HEADER_BAR (header), weight_scale);

      zoom_scale = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 0.3, 5, 0.1);
      gtk_widget_set_tooltip_text (zoom_scale, "Zoom");

      gtk_scale_add_mark (GTK_SCALE (zoom_scale), 1.0, GTK_POS_BOTTOM, NULL);
      gtk_scale_add_mark (GTK_SCALE (zoom_scale), 2.0, GTK_POS_BOTTOM, NULL);
      gtk_scale_add_mark (GTK_SCALE (zoom_scale), 3.0, GTK_POS_BOTTOM, NULL);
      gtk_scale_add_mark (GTK_SCALE (zoom_scale), 4.0, GTK_POS_BOTTOM, NULL);
      ((GlyphDemo *)demo)->zoom_adj = gtk_range_get_adjustment (GTK_RANGE (zoom_scale));
      g_signal_connect (((GlyphDemo *)demo)->zoom_adj, "value-changed", G_CALLBACK (zoom_changed), demo);
      gtk_widget_set_size_request (zoom_scale, 220, -1);
      gtk_header_bar_pack_end (GTK_HEADER_BAR (header), zoom_scale);
      gtk_range_set_value (GTK_RANGE (zoom_scale), 1);

      gtk_window_set_titlebar (GTK_WINDOW (window), header);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
