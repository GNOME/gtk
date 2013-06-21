/* Offscreen windows/Effects
 *
 * Offscreen windows can be used to render elements multiple times to achieve
 * various effects.
 */
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#define GTK_TYPE_MIRROR_BIN              (gtk_mirror_bin_get_type ())
#define GTK_MIRROR_BIN(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_MIRROR_BIN, GtkMirrorBin))
#define GTK_MIRROR_BIN_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_MIRROR_BIN, GtkMirrorBinClass))
#define GTK_IS_MIRROR_BIN(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_MIRROR_BIN))
#define GTK_IS_MIRROR_BIN_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_MIRROR_BIN))
#define GTK_MIRROR_BIN_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_MIRROR_BIN, GtkMirrorBinClass))

typedef struct _GtkMirrorBin   GtkMirrorBin;
typedef struct _GtkMirrorBinClass  GtkMirrorBinClass;

struct _GtkMirrorBin
{
  GtkContainer container;

  GtkWidget *child;
  GdkWindow *offscreen_window;
};

struct _GtkMirrorBinClass
{
  GtkContainerClass parent_class;
};

GType      gtk_mirror_bin_get_type  (void) G_GNUC_CONST;
GtkWidget* gtk_mirror_bin_new       (void);

/*** implementation ***/

static void     gtk_mirror_bin_realize       (GtkWidget       *widget);
static void     gtk_mirror_bin_unrealize     (GtkWidget       *widget);
static void     gtk_mirror_bin_get_preferred_width  (GtkWidget *widget,
                                                     gint      *minimum,
                                                     gint      *natural);
static void     gtk_mirror_bin_get_preferred_height (GtkWidget *widget,
                                                     gint      *minimum,
                                                     gint      *natural);
static void     gtk_mirror_bin_size_allocate (GtkWidget       *widget,
                                               GtkAllocation   *allocation);
static gboolean gtk_mirror_bin_damage        (GtkWidget       *widget,
                                               GdkEventExpose  *event);
static gboolean gtk_mirror_bin_draw          (GtkWidget       *widget,
                                              cairo_t         *cr);

static void     gtk_mirror_bin_add           (GtkContainer    *container,
                                               GtkWidget       *child);
static void     gtk_mirror_bin_remove        (GtkContainer    *container,
                                               GtkWidget       *widget);
static void     gtk_mirror_bin_forall        (GtkContainer    *container,
                                               gboolean         include_internals,
                                               GtkCallback      callback,
                                               gpointer         callback_data);
static GType    gtk_mirror_bin_child_type    (GtkContainer    *container);

G_DEFINE_TYPE (GtkMirrorBin, gtk_mirror_bin, GTK_TYPE_CONTAINER);

static void
to_child (GtkMirrorBin *bin,
          double         widget_x,
          double         widget_y,
          double        *x_out,
          double        *y_out)
{
  *x_out = widget_x;
  *y_out = widget_y;
}

static void
to_parent (GtkMirrorBin *bin,
           double         offscreen_x,
           double         offscreen_y,
           double        *x_out,
           double        *y_out)
{
  *x_out = offscreen_x;
  *y_out = offscreen_y;
}

static void
gtk_mirror_bin_class_init (GtkMirrorBinClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);

  widget_class->realize = gtk_mirror_bin_realize;
  widget_class->unrealize = gtk_mirror_bin_unrealize;
  widget_class->get_preferred_width = gtk_mirror_bin_get_preferred_width;
  widget_class->get_preferred_height = gtk_mirror_bin_get_preferred_height;
  widget_class->size_allocate = gtk_mirror_bin_size_allocate;
  widget_class->draw = gtk_mirror_bin_draw;

  g_signal_override_class_closure (g_signal_lookup ("damage-event", GTK_TYPE_WIDGET),
                                   GTK_TYPE_MIRROR_BIN,
                                   g_cclosure_new (G_CALLBACK (gtk_mirror_bin_damage),
                                                   NULL, NULL));

  container_class->add = gtk_mirror_bin_add;
  container_class->remove = gtk_mirror_bin_remove;
  container_class->forall = gtk_mirror_bin_forall;
  container_class->child_type = gtk_mirror_bin_child_type;
}

static void
gtk_mirror_bin_init (GtkMirrorBin *bin)
{
  gtk_widget_set_has_window (GTK_WIDGET (bin), TRUE);
}

GtkWidget *
gtk_mirror_bin_new (void)
{
  return g_object_new (GTK_TYPE_MIRROR_BIN, NULL);
}

static GdkWindow *
pick_offscreen_child (GdkWindow     *offscreen_window,
                      double         widget_x,
                      double         widget_y,
                      GtkMirrorBin *bin)
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
                            GtkMirrorBin *bin)
{
  to_parent (bin, offscreen_x, offscreen_y, parent_x, parent_y);
}

static void
offscreen_window_from_parent (GdkWindow     *window,
                              double         parent_x,
                              double         parent_y,
                              double        *offscreen_x,
                              double        *offscreen_y,
                              GtkMirrorBin *bin)
{
  to_child (bin, parent_x, parent_y, offscreen_x, offscreen_y);
}

static void
gtk_mirror_bin_realize (GtkWidget *widget)
{
  GtkMirrorBin *bin = GTK_MIRROR_BIN (widget);
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
  bin->offscreen_window = gdk_window_new (gtk_widget_get_root_window (widget),
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
gtk_mirror_bin_unrealize (GtkWidget *widget)
{
  GtkMirrorBin *bin = GTK_MIRROR_BIN (widget);

  gdk_window_set_user_data (bin->offscreen_window, NULL);
  gdk_window_destroy (bin->offscreen_window);
  bin->offscreen_window = NULL;

  GTK_WIDGET_CLASS (gtk_mirror_bin_parent_class)->unrealize (widget);
}

static GType
gtk_mirror_bin_child_type (GtkContainer *container)
{
  GtkMirrorBin *bin = GTK_MIRROR_BIN (container);

  if (bin->child)
    return G_TYPE_NONE;

  return GTK_TYPE_WIDGET;
}

static void
gtk_mirror_bin_add (GtkContainer *container,
                     GtkWidget    *widget)
{
  GtkMirrorBin *bin = GTK_MIRROR_BIN (container);

  if (!bin->child)
    {
      gtk_widget_set_parent_window (widget, bin->offscreen_window);
      gtk_widget_set_parent (widget, GTK_WIDGET (bin));
      bin->child = widget;
    }
  else
    g_warning ("GtkMirrorBin cannot have more than one child\n");
}

static void
gtk_mirror_bin_remove (GtkContainer *container,
                        GtkWidget    *widget)
{
  GtkMirrorBin *bin = GTK_MIRROR_BIN (container);
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
gtk_mirror_bin_forall (GtkContainer *container,
                        gboolean      include_internals,
                        GtkCallback   callback,
                        gpointer      callback_data)
{
  GtkMirrorBin *bin = GTK_MIRROR_BIN (container);

  g_return_if_fail (callback != NULL);

  if (bin->child)
    (*callback) (bin->child, callback_data);
}

static void
gtk_mirror_bin_size_request (GtkWidget      *widget,
                              GtkRequisition *requisition)
{
  GtkMirrorBin *bin = GTK_MIRROR_BIN (widget);
  GtkRequisition child_requisition;
  guint border_width;

  child_requisition.width = 0;
  child_requisition.height = 0;

  if (bin->child && gtk_widget_get_visible (bin->child))
    gtk_widget_get_preferred_size ( (bin->child),
                               &child_requisition, NULL);

  border_width = gtk_container_get_border_width (GTK_CONTAINER (widget));
  requisition->width = border_width * 2 + child_requisition.width + 10;
  requisition->height = border_width * 2 + child_requisition.height * 2 + 10;
}

static void
gtk_mirror_bin_get_preferred_width (GtkWidget *widget,
                                    gint      *minimum,
                                    gint      *natural)
{
  GtkRequisition requisition;

  gtk_mirror_bin_size_request (widget, &requisition);

  *minimum = *natural = requisition.width;
}

static void
gtk_mirror_bin_get_preferred_height (GtkWidget *widget,
                                     gint      *minimum,
                                     gint      *natural)
{
  GtkRequisition requisition;

  gtk_mirror_bin_size_request (widget, &requisition);

  *minimum = *natural = requisition.width;
}

static void
gtk_mirror_bin_size_allocate (GtkWidget     *widget,
                               GtkAllocation *allocation)
{
  GtkMirrorBin *bin = GTK_MIRROR_BIN (widget);
  gint w, h;
  guint border_width;

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

      gtk_widget_get_preferred_size (bin->child,
                                     &child_requisition, NULL);
      child_allocation.x = 0;
      child_allocation.y = 0;
      child_allocation.height = child_requisition.height;
      child_allocation.width = child_requisition.width;

      if (gtk_widget_get_realized (widget))
        gdk_window_move_resize (bin->offscreen_window,
                                allocation->x + border_width,
                                allocation->y + border_width,
                                child_allocation.width, child_allocation.height);
      gtk_widget_size_allocate (bin->child, &child_allocation);
    }
}

static gboolean
gtk_mirror_bin_damage (GtkWidget      *widget,
                        GdkEventExpose *event)
{
  gdk_window_invalidate_rect (gtk_widget_get_window (widget),
                              NULL, FALSE);

  return TRUE;
}

static gboolean
gtk_mirror_bin_draw (GtkWidget *widget,
                     cairo_t   *cr)
{
  GtkMirrorBin *bin = GTK_MIRROR_BIN (widget);
  GdkWindow *window;

  window = gtk_widget_get_window (widget);
  if (gtk_cairo_should_draw_window (cr, window))
    {
      cairo_surface_t *surface;
      cairo_matrix_t matrix;
      cairo_pattern_t *mask;
      int height;

      if (bin->child && gtk_widget_get_visible (bin->child))
        {
          surface = gdk_offscreen_window_get_surface (bin->offscreen_window);
          height = gdk_window_get_height (bin->offscreen_window);

          /* paint the offscreen child */
          cairo_set_source_surface (cr, surface, 0, 0);
          cairo_paint (cr);

          cairo_matrix_init (&matrix, 1.0, 0.0, 0.3, 1.0, 0.0, 0.0);
          cairo_matrix_scale (&matrix, 1.0, -1.0);
          cairo_matrix_translate (&matrix, -10, - 3 * height - 10);
          cairo_transform (cr, &matrix);

          cairo_set_source_surface (cr, surface, 0, height);

          /* create linear gradient as mask-pattern to fade out the source */
          mask = cairo_pattern_create_linear (0.0, height, 0.0, 2*height);
          cairo_pattern_add_color_stop_rgba (mask, 0.0,  0.0, 0.0, 0.0, 0.0);
          cairo_pattern_add_color_stop_rgba (mask, 0.25, 0.0, 0.0, 0.0, 0.01);
          cairo_pattern_add_color_stop_rgba (mask, 0.5,  0.0, 0.0, 0.0, 0.25);
          cairo_pattern_add_color_stop_rgba (mask, 0.75, 0.0, 0.0, 0.0, 0.5);
          cairo_pattern_add_color_stop_rgba (mask, 1.0,  0.0, 0.0, 0.0, 1.0);

          /* paint the reflection */
          cairo_mask (cr, mask);

          cairo_pattern_destroy (mask);
        }
    }
  else if (gtk_cairo_should_draw_window (cr, bin->offscreen_window))
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

static GtkWidget *window = NULL;

GtkWidget *
do_offscreen_window2 (GtkWidget *do_widget)
{
  if (!window)
    {
      GtkWidget *bin, *vbox;
      GtkWidget *hbox, *entry, *applybutton, *backbutton;
      GtkSizeGroup *group;

      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_screen (GTK_WINDOW (window),
                             gtk_widget_get_screen (do_widget));
      gtk_window_set_title (GTK_WINDOW (window), "Effects");

      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed), &window);

      gtk_container_set_border_width (GTK_CONTAINER (window), 10);

      vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

      bin = gtk_mirror_bin_new ();

      group = gtk_size_group_new (GTK_SIZE_GROUP_VERTICAL);

      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
      backbutton = gtk_button_new ();
      gtk_container_add (GTK_CONTAINER (backbutton),
                         gtk_image_new_from_icon_name ("go-previous", 4));
      gtk_size_group_add_widget (group, backbutton);
      entry = gtk_entry_new ();
      gtk_size_group_add_widget (group, entry);
      applybutton = gtk_button_new_with_label (_("Apply"));
      gtk_size_group_add_widget (group, applybutton);

      gtk_container_add (GTK_CONTAINER (window), vbox);
      gtk_box_pack_start (GTK_BOX (vbox), bin, TRUE, TRUE, 0);
      gtk_container_add (GTK_CONTAINER (bin), hbox);
      gtk_box_pack_start (GTK_BOX (hbox), backbutton, FALSE, FALSE, 0);
      gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 0);
      gtk_box_pack_start (GTK_BOX (hbox), applybutton, FALSE, FALSE, 0);
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
