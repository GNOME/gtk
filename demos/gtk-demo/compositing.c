/* Rendering/Compositing
 *
 * Shows the effect of the various Porter/Duff compositing
 * operators that are available for composite render nodes.
 */

#include <gtk/gtk.h>

static const struct
{
  const char *name;
  GskPorterDuff op;
} operators[] = {
  { "Source",          GSK_PORTER_DUFF_SOURCE },
  { "Dest",            GSK_PORTER_DUFF_DEST },
  { "Source over",     GSK_PORTER_DUFF_SOURCE_OVER_DEST },
  { "Dest over",       GSK_PORTER_DUFF_DEST_OVER_SOURCE },
  { "Source in",       GSK_PORTER_DUFF_SOURCE_IN_DEST },
  { "Dest in",         GSK_PORTER_DUFF_DEST_IN_SOURCE },
  { "Source out",      GSK_PORTER_DUFF_SOURCE_OUT_DEST },
  { "Dest out",        GSK_PORTER_DUFF_DEST_OUT_SOURCE },
  { "Source atop",     GSK_PORTER_DUFF_SOURCE_ATOP_DEST },
  { "Dest atop",       GSK_PORTER_DUFF_DEST_ATOP_SOURCE },
  { "Xor",             GSK_PORTER_DUFF_XOR },
  { "Clear",           GSK_PORTER_DUFF_CLEAR },
};

typedef struct
{
  GtkWidget parent;

  GskPorterDuff op;
} CompositingWidget;

typedef GtkWidgetClass CompositingWidgetClass;

G_DEFINE_TYPE (CompositingWidget, compositing_widget, GTK_TYPE_WIDGET)

static void
compositing_widget_snapshot (GtkWidget   *widget,
                             GtkSnapshot *snapshot)
{
  CompositingWidget *self = (CompositingWidget *) widget;
  int width = gtk_widget_get_width (widget);
  int height = gtk_widget_get_height (widget);
  float size = MIN (width, height);
  float cx = width / 2.0f;
  float cy = height / 2.0f;

  gtk_snapshot_push_isolation (snapshot, GSK_ISOLATION_ALL);

  gtk_snapshot_append_color (snapshot,
                             &(GdkRGBA) { 0.94f, 0.75f, 0.0f, 0.7f },
                             &GRAPHENE_RECT_INIT (cx - size * 0.35f,
                                                  cy - size * 0.35f,
                                                  size * 0.6f,
                                                  size * 0.6f));

  gtk_snapshot_push_composite (snapshot, self->op);

  gtk_snapshot_append_color (snapshot,
                             &(GdkRGBA) { 1.0f, 1.0f, 1.0f, 1.0f },
                             &GRAPHENE_RECT_INIT (cx - size * 0.45f,
                                                  cy - size * 0.45f,
                                                  size * 0.9f,
                                                  size * 0.9f));

  gtk_snapshot_pop (snapshot);

  gtk_snapshot_append_color (snapshot,
                             &(GdkRGBA) { 0.0f, 0.75f, 0.94f, 0.7f },
                             &GRAPHENE_RECT_INIT (cx - size * 0.25f,
                                                  cy - size * 0.25f,
                                                  size * 0.6f,
                                                  size * 0.6f));

  gtk_snapshot_pop (snapshot);

  gtk_snapshot_pop (snapshot);
}

static void
compositing_widget_measure (GtkWidget      *widget,
                            GtkOrientation  orientation,
                            int             for_size,
                            int            *minimum,
                            int            *natural,
                            int            *minimum_baseline,
                            int            *natural_baseline)
{
  *minimum = *natural = 80;
}

static void
compositing_widget_init (CompositingWidget *self)
{
  self->op = GSK_PORTER_DUFF_SOURCE_OVER_DEST;
}

static void
compositing_widget_class_init (CompositingWidgetClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  widget_class->snapshot = compositing_widget_snapshot;
  widget_class->measure = compositing_widget_measure;
}

GtkWidget *
do_compositing (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;

  if (!window)
    {
      GtkWidget *grid;

      window = gtk_window_new ();
      gtk_window_set_title (GTK_WINDOW (window), "Compositing Operators");
      gtk_window_set_transient_for (GTK_WINDOW (window), GTK_WINDOW (do_widget));
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *) &window);
      gtk_window_set_default_size (GTK_WINDOW (window), 600, 400);
      gtk_window_set_resizable (GTK_WINDOW (window), FALSE);

      grid = gtk_grid_new ();
      gtk_grid_set_column_spacing (GTK_GRID (grid), 6);
      gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
      gtk_widget_set_margin_start (grid, 12);
      gtk_widget_set_margin_end (grid, 12);
      gtk_widget_set_margin_top (grid, 12);
      gtk_widget_set_margin_bottom (grid, 12);
      gtk_window_set_child (GTK_WINDOW (window), grid);

      for (guint i = 0; i < G_N_ELEMENTS (operators); i++)
        {
          GtkWidget *box, *label, *comp;
          int col = i % 5;
          int row = (i / 5) * 2;

          box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
          gtk_widget_set_hexpand (box, TRUE);
          gtk_widget_set_vexpand (box, TRUE);

          comp = g_object_new (compositing_widget_get_type (), NULL);
          ((CompositingWidget *) comp)->op = operators[i].op;
          gtk_widget_set_hexpand (comp, TRUE);
          gtk_widget_set_vexpand (comp, TRUE);

          label = gtk_label_new (operators[i].name);
          gtk_widget_add_css_class (label, "dim-label");

          gtk_box_append (GTK_BOX (box), comp);
          gtk_box_append (GTK_BOX (box), label);

          gtk_grid_attach (GTK_GRID (grid), box, col, row, 1, 2);
        }
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_set_visible (window, TRUE);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
