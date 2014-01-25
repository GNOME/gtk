/* Offscreen Windows/Rotated Button
 *
 * Offscreen windows can be used to transform parts of a widget
 * hierarchy. Note that the rotated button is fully functional.
 */
#include <math.h>
#include <gtk/gtk.h>

#define GTK_TYPE_ROTATED_BIN              (gtk_rotated_bin_get_type ())
#define GTK_ROTATED_BIN(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_ROTATED_BIN, GtkRotatedBin))
#define GTK_ROTATED_BIN_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_ROTATED_BIN, GtkRotatedBinClass))
#define GTK_IS_ROTATED_BIN(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_ROTATED_BIN))
#define GTK_IS_ROTATED_BIN_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_ROTATED_BIN))
#define GTK_ROTATED_BIN_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_ROTATED_BIN, GtkRotatedBinClass))

typedef struct _GtkRotatedBin   GtkRotatedBin;
typedef struct _GtkRotatedBinClass  GtkRotatedBinClass;

struct _GtkRotatedBin
{
  GtkContainer container;

  GtkWidget *child;
  GdkWindow *offscreen_window;
  gdouble angle;
};

struct _GtkRotatedBinClass
{
  GtkContainerClass parent_class;
};

GType      gtk_rotated_bin_get_type  (void) G_GNUC_CONST;
GtkWidget* gtk_rotated_bin_new       (void);
void       gtk_rotated_bin_set_angle (GtkRotatedBin *bin,
                                      gdouble        angle);

/*** implementation ***/

static void     gtk_rotated_bin_realize       (GtkWidget       *widget);
static void     gtk_rotated_bin_unrealize     (GtkWidget       *widget);
static void     gtk_rotated_bin_get_preferred_width  (GtkWidget *widget,
                                                      gint      *minimum,
                                                      gint      *natural);
static void     gtk_rotated_bin_get_preferred_height (GtkWidget *widget,
                                                      gint      *minimum,
                                                      gint      *natural);
static void     gtk_rotated_bin_size_allocate (GtkWidget       *widget,
                                               GtkAllocation   *allocation);
static gboolean gtk_rotated_bin_damage        (GtkWidget       *widget,
                                               GdkEventExpose  *event);
static gboolean gtk_rotated_bin_draw          (GtkWidget       *widget,
                                               cairo_t         *cr);

static void     gtk_rotated_bin_add           (GtkContainer    *container,
                                               GtkWidget       *child);
static void     gtk_rotated_bin_remove        (GtkContainer    *container,
                                               GtkWidget       *widget);
static void     gtk_rotated_bin_forall        (GtkContainer    *container,
                                               gboolean         include_internals,
                                               GtkCallback      callback,
                                               gpointer         callback_data);
static GType    gtk_rotated_bin_child_type    (GtkContainer    *container);

G_DEFINE_TYPE (GtkRotatedBin, gtk_rotated_bin, GTK_TYPE_CONTAINER);

static void
to_child (GtkRotatedBin *bin,
          double         widget_x,
          double         widget_y,
          double        *x_out,
          double        *y_out)
{
  GtkAllocation child_area;
  double x, y, xr, yr;
  double c, s;
  double w, h;

  s = sin (bin->angle);
  c = cos (bin->angle);
  gtk_widget_get_allocation (bin->child, &child_area);

  w = c * child_area.width + s * child_area.height;
  h = s * child_area.width + c * child_area.height;

  x = widget_x;
  y = widget_y;

  x -= (w - child_area.width) / 2;
  y -= (h - child_area.height) / 2;

  x -= child_area.width / 2;
  y -= child_area.height / 2;

  xr = x * c + y * s;
  yr = y * c - x * s;
  x = xr;
  y = yr;

  x += child_area.width / 2;
  y += child_area.height / 2;

  *x_out = x;
  *y_out = y;
}

static void
to_parent (GtkRotatedBin *bin,
           double         offscreen_x,
           double         offscreen_y,
           double        *x_out,
           double        *y_out)
{
  GtkAllocation child_area;
  double x, y, xr, yr;
  double c, s;
  double w, h;

  s = sin (bin->angle);
  c = cos (bin->angle);
  gtk_widget_get_allocation (bin->child, &child_area);

  w = c * child_area.width + s * child_area.height;
  h = s * child_area.width + c * child_area.height;

  x = offscreen_x;
  y = offscreen_y;

  x -= child_area.width / 2;
  y -= child_area.height / 2;

  xr = x * c - y * s;
  yr = x * s + y * c;
  x = xr;
  y = yr;

  x += child_area.width / 2;
  y += child_area.height / 2;

  x -= (w - child_area.width) / 2;
  y -= (h - child_area.height) / 2;

  *x_out = x;
  *y_out = y;
}

static void
gtk_rotated_bin_class_init (GtkRotatedBinClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);

  widget_class->realize = gtk_rotated_bin_realize;
  widget_class->unrealize = gtk_rotated_bin_unrealize;
  widget_class->get_preferred_width = gtk_rotated_bin_get_preferred_width;
  widget_class->get_preferred_height = gtk_rotated_bin_get_preferred_height;
  widget_class->size_allocate = gtk_rotated_bin_size_allocate;
  widget_class->draw = gtk_rotated_bin_draw;

  g_signal_override_class_closure (g_signal_lookup ("damage-event", GTK_TYPE_WIDGET),
                                   GTK_TYPE_ROTATED_BIN,
                                   g_cclosure_new (G_CALLBACK (gtk_rotated_bin_damage),
                                                   NULL, NULL));

  container_class->add = gtk_rotated_bin_add;
  container_class->remove = gtk_rotated_bin_remove;
  container_class->forall = gtk_rotated_bin_forall;
  container_class->child_type = gtk_rotated_bin_child_type;
}

static void
gtk_rotated_bin_init (GtkRotatedBin *bin)
{
  gtk_widget_set_has_window (GTK_WIDGET (bin), TRUE);
}

GtkWidget *
gtk_rotated_bin_new (void)
{
  return g_object_new (GTK_TYPE_ROTATED_BIN, NULL);
}

static GdkWindow *
pick_offscreen_child (GdkWindow     *offscreen_window,
                      double         widget_x,
                      double         widget_y,
                      GtkRotatedBin *bin)
{
 GtkAllocation child_area;
 double x, y;

 if (bin->child && gtk_widget_get_visible (bin->child))
    {
      to_child (bin, widget_x, widget_y, &x, &y);

      gtk_widget_get_allocation (bin->child, &child_area);

      if (x >= 0 && x < child_area.width &&
          y >= 0 && y < child_area.height)
        return bin->offscreen_window;
    }

  return NULL;
}

static void
offscreen_window_to_parent (GdkWindow     *offscreen_window,
                            double         offscreen_x,
                            double         offscreen_y,
                            double        *parent_x,
                            double        *parent_y,
                            GtkRotatedBin *bin)
{
  to_parent (bin, offscreen_x, offscreen_y, parent_x, parent_y);
}

static void
offscreen_window_from_parent (GdkWindow     *window,
                              double         parent_x,
                              double         parent_y,
                              double        *offscreen_x,
                              double        *offscreen_y,
                              GtkRotatedBin *bin)
{
  to_child (bin, parent_x, parent_y, offscreen_x, offscreen_y);
}

static void
gtk_rotated_bin_realize (GtkWidget *widget)
{
  GtkRotatedBin *bin = GTK_ROTATED_BIN (widget);
  GtkAllocation allocation;
  GtkStyleContext *context;
  GdkWindow *window;
  GdkWindowAttr attributes;
  gint attributes_mask;
  guint border_width;
  GtkRequisition child_requisition;

  gtk_widget_set_realized (widget, TRUE);

  gtk_widget_get_allocation (widget, &allocation);
  border_width = gtk_container_get_border_width (GTK_CONTAINER (widget));

  attributes.x = allocation.x + border_width;
  attributes.y = allocation.y + border_width;
  attributes.width = allocation.width - 2 * border_width;
  attributes.height = allocation.height - 2 * border_width;
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.event_mask = gtk_widget_get_events (widget)
                        | GDK_EXPOSURE_MASK
                        | GDK_POINTER_MOTION_MASK
                        | GDK_BUTTON_PRESS_MASK
                        | GDK_BUTTON_RELEASE_MASK
                        | GDK_SCROLL_MASK
                        | GDK_ENTER_NOTIFY_MASK
                        | GDK_LEAVE_NOTIFY_MASK;

  attributes.visual = gtk_widget_get_visual (widget);
  attributes.wclass = GDK_INPUT_OUTPUT;

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL;

  window = gdk_window_new (gtk_widget_get_parent_window (widget),
                           &attributes, attributes_mask);
  gtk_widget_set_window (widget, window);
  gdk_window_set_user_data (window, widget);
  g_signal_connect (window, "pick-embedded-child",
                    G_CALLBACK (pick_offscreen_child), bin);

  attributes.window_type = GDK_WINDOW_OFFSCREEN;

  child_requisition.width = child_requisition.height = 0;
  if (bin->child && gtk_widget_get_visible (bin->child))
    {
      GtkAllocation child_allocation;

      gtk_widget_get_allocation (bin->child, &child_allocation);
      attributes.width = child_allocation.width;
      attributes.height = child_allocation.height;
    }
  bin->offscreen_window = gdk_window_new (gdk_screen_get_root_window (gtk_widget_get_screen (widget)),
                                          &attributes, attributes_mask);
  gdk_window_set_user_data (bin->offscreen_window, widget);
  if (bin->child)
    gtk_widget_set_parent_window (bin->child, bin->offscreen_window);
  gdk_offscreen_window_set_embedder (bin->offscreen_window, window);
  g_signal_connect (bin->offscreen_window, "to-embedder",
                    G_CALLBACK (offscreen_window_to_parent), bin);
  g_signal_connect (bin->offscreen_window, "from-embedder",
                    G_CALLBACK (offscreen_window_from_parent), bin);

  context = gtk_widget_get_style_context (widget);
  gtk_style_context_set_background (context, window);
  gtk_style_context_set_background (context, bin->offscreen_window);
  gdk_window_show (bin->offscreen_window);
}

static void
gtk_rotated_bin_unrealize (GtkWidget *widget)
{
  GtkRotatedBin *bin = GTK_ROTATED_BIN (widget);

  gdk_window_set_user_data (bin->offscreen_window, NULL);
  gdk_window_destroy (bin->offscreen_window);
  bin->offscreen_window = NULL;

  GTK_WIDGET_CLASS (gtk_rotated_bin_parent_class)->unrealize (widget);
}

static GType
gtk_rotated_bin_child_type (GtkContainer *container)
{
  GtkRotatedBin *bin = GTK_ROTATED_BIN (container);

  if (bin->child)
    return G_TYPE_NONE;

  return GTK_TYPE_WIDGET;
}

static void
gtk_rotated_bin_add (GtkContainer *container,
                     GtkWidget    *widget)
{
  GtkRotatedBin *bin = GTK_ROTATED_BIN (container);

  if (!bin->child)
    {
      gtk_widget_set_parent_window (widget, bin->offscreen_window);
      gtk_widget_set_parent (widget, GTK_WIDGET (bin));
      bin->child = widget;
    }
  else
    g_warning ("GtkRotatedBin cannot have more than one child\n");
}

static void
gtk_rotated_bin_remove (GtkContainer *container,
                        GtkWidget    *widget)
{
  GtkRotatedBin *bin = GTK_ROTATED_BIN (container);
  gboolean was_visible;

  was_visible = gtk_widget_get_visible (widget);

  if (bin->child == widget)
    {
      gtk_widget_unparent (widget);

      bin->child = NULL;

      if (was_visible && gtk_widget_get_visible (GTK_WIDGET (container)))
        gtk_widget_queue_resize (GTK_WIDGET (container));
    }
}

static void
gtk_rotated_bin_forall (GtkContainer *container,
                        gboolean      include_internals,
                        GtkCallback   callback,
                        gpointer      callback_data)
{
  GtkRotatedBin *bin = GTK_ROTATED_BIN (container);

  g_return_if_fail (callback != NULL);

  if (bin->child)
    (*callback) (bin->child, callback_data);
}

void
gtk_rotated_bin_set_angle (GtkRotatedBin *bin,
                           gdouble        angle)
{
  g_return_if_fail (GTK_IS_ROTATED_BIN (bin));

  bin->angle = angle;
  gtk_widget_queue_resize (GTK_WIDGET (bin));

  gdk_window_geometry_changed (bin->offscreen_window);
}

static void
gtk_rotated_bin_size_request (GtkWidget      *widget,
                              GtkRequisition *requisition)
{
  GtkRotatedBin *bin = GTK_ROTATED_BIN (widget);
  GtkRequisition child_requisition;
  double s, c;
  double w, h;
  guint border_width;

  child_requisition.width = 0;
  child_requisition.height = 0;

  if (bin->child && gtk_widget_get_visible (bin->child))
    gtk_widget_get_preferred_size ( (bin->child),
                               &child_requisition, NULL);

  s = sin (bin->angle);
  c = cos (bin->angle);
  w = c * child_requisition.width + s * child_requisition.height;
  h = s * child_requisition.width + c * child_requisition.height;

  border_width = gtk_container_get_border_width (GTK_CONTAINER (widget));
  requisition->width = border_width * 2 + w;
  requisition->height = border_width * 2 + h;
}

static void
gtk_rotated_bin_get_preferred_width (GtkWidget *widget,
                                     gint      *minimum,
                                     gint      *natural)
{
  GtkRequisition requisition;

  gtk_rotated_bin_size_request (widget, &requisition);

  *minimum = *natural = requisition.width;
}

static void
gtk_rotated_bin_get_preferred_height (GtkWidget *widget,
                                      gint      *minimum,
                                      gint      *natural)
{
  GtkRequisition requisition;

  gtk_rotated_bin_size_request (widget, &requisition);

  *minimum = *natural = requisition.height;
}

static void
gtk_rotated_bin_size_allocate (GtkWidget     *widget,
                               GtkAllocation *allocation)
{
  GtkRotatedBin *bin = GTK_ROTATED_BIN (widget);
  guint border_width;
  gint w, h;
  gdouble s, c;

  gtk_widget_set_allocation (widget, allocation);

  border_width = gtk_container_get_border_width (GTK_CONTAINER (widget));

  w = allocation->width - border_width * 2;
  h = allocation->height - border_width * 2;

  if (gtk_widget_get_realized (widget))
    gdk_window_move_resize (gtk_widget_get_window (widget),
                            allocation->x + border_width,
                            allocation->y + border_width,
                            w, h);

  if (bin->child && gtk_widget_get_visible (bin->child))
    {
      GtkRequisition child_requisition;
      GtkAllocation child_allocation;

      s = sin (bin->angle);
      c = cos (bin->angle);

      gtk_widget_get_preferred_size (bin->child,
                                     &child_requisition, NULL);
      child_allocation.x = 0;
      child_allocation.y = 0;
      child_allocation.height = child_requisition.height;
      if (c == 0.0)
        child_allocation.width = h / s;
      else if (s == 0.0)
        child_allocation.width = w / c;
      else
        child_allocation.width = MIN ((w - s * child_allocation.height) / c,
                                      (h - c * child_allocation.height) / s);

      if (gtk_widget_get_realized (widget))
        gdk_window_move_resize (bin->offscreen_window,
                                child_allocation.x,
                                child_allocation.y,
                                child_allocation.width,
                                child_allocation.height);

      child_allocation.x = child_allocation.y = 0;
      gtk_widget_size_allocate (bin->child, &child_allocation);
    }
}

static gboolean
gtk_rotated_bin_damage (GtkWidget      *widget,
                        GdkEventExpose *event)
{
  gdk_window_invalidate_rect (gtk_widget_get_window (widget),
                              NULL, FALSE);

  return TRUE;
}

static gboolean
gtk_rotated_bin_draw (GtkWidget *widget,
                      cairo_t   *cr)
{
  GtkRotatedBin *bin = GTK_ROTATED_BIN (widget);
  GdkWindow *window;
  gdouble s, c;
  gdouble w, h;

  window = gtk_widget_get_window (widget);
  if (gtk_cairo_should_draw_window (cr, window))
    {
      cairo_surface_t *surface;
      GtkAllocation child_area;

      if (bin->child && gtk_widget_get_visible (bin->child))
        {
          surface = gdk_offscreen_window_get_surface (bin->offscreen_window);
          gtk_widget_get_allocation (bin->child, &child_area);

          /* transform */
          s = sin (bin->angle);
          c = cos (bin->angle);
          w = c * child_area.width + s * child_area.height;
          h = s * child_area.width + c * child_area.height;

          cairo_translate (cr, (w - child_area.width) / 2, (h - child_area.height) / 2);
          cairo_translate (cr, child_area.width / 2, child_area.height / 2);
          cairo_rotate (cr, bin->angle);
          cairo_translate (cr, -child_area.width / 2, -child_area.height / 2);

          /* clip */
          cairo_rectangle (cr,
                           0, 0,
                           gdk_window_get_width (bin->offscreen_window),
                           gdk_window_get_height (bin->offscreen_window));
          cairo_clip (cr);
          /* paint */
          cairo_set_source_surface (cr, surface, 0, 0);
          cairo_paint (cr);
        }
    }
  if (gtk_cairo_should_draw_window (cr, bin->offscreen_window))
    {
      gtk_render_background (gtk_widget_get_style_context (widget),
                             cr,
                             0, 0,
                             gdk_window_get_width (bin->offscreen_window),
                             gdk_window_get_height (bin->offscreen_window));

      if (bin->child)
        gtk_container_propagate_draw (GTK_CONTAINER (widget),
                                      bin->child,
                                      cr);
    }

  return FALSE;
}

/*** ***/

static void
scale_changed (GtkRange      *range,
               GtkRotatedBin *bin)
{
  gtk_rotated_bin_set_angle (bin, gtk_range_get_value (range));
}

static GtkWidget *window = NULL;

GtkWidget *
do_offscreen_window (GtkWidget *do_widget)
{
  if (!window)
    {
      GtkWidget *bin, *vbox, *scale, *button;
      GdkRGBA black;

      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_screen (GTK_WINDOW (window),
                             gtk_widget_get_screen (do_widget));
      gtk_window_set_title (GTK_WINDOW (window), "Rotated widget");

      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed), &window);

      gdk_rgba_parse (&black, "black");
      gtk_widget_override_background_color (window, 0, &black);
      gtk_container_set_border_width (GTK_CONTAINER (window), 10);

      vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      scale = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL,
                                        0, G_PI/2, 0.01);
      gtk_scale_set_draw_value (GTK_SCALE (scale), FALSE);

      button = gtk_button_new_with_label ("A Button");
      bin = gtk_rotated_bin_new ();

      g_signal_connect (scale, "value-changed", G_CALLBACK (scale_changed), bin);

      gtk_container_add (GTK_CONTAINER (window), vbox);
      gtk_box_pack_start (GTK_BOX (vbox), scale, FALSE, FALSE, 0);
      gtk_box_pack_start (GTK_BOX (vbox), bin, TRUE, TRUE, 0);
      gtk_container_add (GTK_CONTAINER (bin), button);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show_all (window);
  else
    {
      gtk_widget_destroy (window);
      window = NULL;
    }

  return window;
}
