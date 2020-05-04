/* Peg Solitaire
 *
 * This demo demonstrates how to use drag'n'drop to implement peg solitaire.
 *
 */

#include <gtk/gtk.h>

static GtkWidget *window = NULL;

/* Create an object for the pegs that get moved around in the game.
 *
 * We implement the GdkPaintable interface for them, so we can use GtkPicture
 * objects for the wholes we put the pegs into.
 */
#define SOLITAIRE_TYPE_PEG (solitaire_peg_get_type ())
G_DECLARE_FINAL_TYPE (SolitairePeg, solitaire_peg, SOLITAIRE, PEG, GObject)

/* Declare the struct. */
struct _SolitairePeg
{
  GObject parent_instance;

  int x;
  int y;
};

struct _SolitairePegClass
{
  GObjectClass parent_class;
};

/* Here, we implement the functionality required by the GdkPaintable interface */
static void
solitaire_peg_snapshot (GdkPaintable *paintable,
                        GdkSnapshot  *snapshot,
                        double        width,
                        double        height)
{
  /* The snapshot function is the only function we need to implement.
   * It does the actual drawing of the paintable.
   */
  gtk_snapshot_append_color (snapshot,
                             &(GdkRGBA) { 0.6, 0.3, 0.0, 1.0 },
                             &GRAPHENE_RECT_INIT (0, 0, width, height));
}

static GdkPaintableFlags
solitaire_peg_get_flags (GdkPaintable *paintable)
{
  /* The flags are very useful to let GTK know that this image
   * is never going to change.
   * This allows many optimizations and should therefore always
   * be set.
   */
  return GDK_PAINTABLE_STATIC_CONTENTS | GDK_PAINTABLE_STATIC_SIZE;
}

static void
solitaire_peg_paintable_init (GdkPaintableInterface *iface)
{
  iface->snapshot = solitaire_peg_snapshot;
  iface->get_flags = solitaire_peg_get_flags;
}

/* When defining the GType, we need to implement the GdkPaintable interface */
G_DEFINE_TYPE_WITH_CODE (SolitairePeg, solitaire_peg, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GDK_TYPE_PAINTABLE,
                                                solitaire_peg_paintable_init))

/* Here's the boilerplate for the GObject declaration.
 * We don't need to do anything special here, because we keep no
 * data of our own.
 */
static void
solitaire_peg_class_init (SolitairePegClass *klass)
{
}

static void
solitaire_peg_init (SolitairePeg *peg)
{
}

/* Add a little setter for the peg's position.
 * We want to track those so that we can check for legal moves
 * during drag'n'drop operations.
 */
static void
solitaire_peg_set_position (SolitairePeg *peg,
                            guint         x,
                            guint         y)
{
  peg->x = x;
  peg->y = y;
}

/* And finally, we add a simple constructor.
 */
static SolitairePeg *
solitaire_peg_new (void)
{
  return g_object_new (SOLITAIRE_TYPE_PEG, NULL);
}

/*** DRAG AND DROP ***/

/* The user tries to start a drag operation.
 * We check if the image contains a peg, and if so, we return the
 * peg as the content to be dragged.
 */
static GdkContentProvider *
drag_prepare (GtkDragSource *source,
              double         x,
              double         y,
              GtkWidget     *image)
{
  GdkPaintable *paintable = gtk_image_get_paintable (GTK_IMAGE (image));

  if (!SOLITAIRE_IS_PEG (paintable))
    return NULL;

  return gdk_content_provider_new_typed (SOLITAIRE_TYPE_PEG, paintable);
}

/* This notifies us that the drag has begun.
 * We can now set up the icon and the widget for the ongoing drag.
 */
static void
drag_begin (GtkDragSource *source,
            GdkDrag       *drag,
            GtkWidget     *image)
{
  GdkPaintable *paintable = gtk_image_get_paintable (GTK_IMAGE (image));

  /* We guaranteed in the drag_prepare function above that we
   * only start a drag if a peg is available.
   * So let's make sure we did not screw that up.
   */
  g_assert (SOLITAIRE_IS_PEG (paintable));

  /* We use the peg as the drag icon.
   */
  gtk_drag_source_set_icon (source, paintable, -2, -2);

  /* We also attach it to the drag operation as custom user data,
   * so that we can get it back later if the drag fails.
   */
  g_object_set_data (G_OBJECT (drag), "the peg", paintable);

  /* Because we are busy dragging the peg, we want to unset it
   * on the image.
   */
  gtk_image_clear (GTK_IMAGE (image));
}

/* Thie is called once a drag operation has ended (successfully or not).
 * We want to undo what we did in drag_begin() above and react
 * to a potential move of the peg.
 */
static void
drag_end (GtkDragSource *source,
          GdkDrag       *drag,
          gboolean       delete_data,
          GtkWidget     *image)
{
  SolitairePeg *peg;

  /* If the drag was successful, we should now delete the peg.
   * We did this in drag_begin() above to prepare for the drag, so
   * there's no need to do anything anymore.
   */
  if (delete_data)
    return;

  /* However, if the drag did not succeed, we need to undo what
   * we did in drag_begin() and reinsert the peg here.
   * Because we used it as the drag data
   */
  peg = g_object_get_data (G_OBJECT (drag), "the peg");
  gtk_image_set_from_paintable (GTK_IMAGE (image), GDK_PAINTABLE (peg));
}

/* Whenever a new drop operation starts, we need to check if we can
 * accept it.
 * The default check unfortunately is not good enough, because it only
 * checks the data type. But we also need to check if our image can
 * even accept data.
 */
static gboolean
drop_accept (GtkDropTarget *target,
             GdkDrop       *drop,
             GtkWidget     *image)
{
  /* First, check the drop is actually trying to drop a peg */
  if (!gdk_content_formats_contain_gtype (gdk_drop_get_formats (drop), SOLITAIRE_TYPE_PEG))
    return FALSE;

  /* If the image already contains a peg, we cannot accept another one */
  if (SOLITAIRE_IS_PEG (gtk_image_get_paintable (GTK_IMAGE (image))))
    return FALSE;

  return TRUE;
}

static gboolean
drop_drop (GtkDropTarget *target,
           const GValue  *value,
           double         x,
           double         y,
           GtkWidget     *image)
{
  GtkGrid *grid;
  SolitairePeg *peg;
  int image_x, image_y;
  GtkWidget *jumped;

  grid = GTK_GRID (gtk_widget_get_parent (image));
  /* The value contains the data in the type we demanded.
   * We demanded a SolitairePeg, so that's what we get.
   */
  peg = g_value_get_object (value);

  /* Make sure this was a legal move. */
  /* First, figure out the image's position in the grid. */
  gtk_grid_query_child (grid,
                        image,
                        &image_x, &image_y,
                        NULL, NULL);

  /* If the peg was not moved 2 spaces horizontally or vertically,
   * this was not a valid jump. Reject it.
   */
  if (!((ABS (image_x - peg->x) == 2 && image_y == peg->y) ||
        (ABS (image_y - peg->y) == 2 && image_x == peg->x)))
    return FALSE;

  /* Get the widget that was jumped over
   */
  jumped = gtk_grid_get_child_at (grid,
                                  (image_x + peg->x) / 2,
                                  (image_y + peg->y) / 2);
  /* If the jumped widget does not have a peg in it, this move
   * isn't valid.
   */
  if (!SOLITAIRE_IS_PEG (gtk_image_get_paintable (GTK_IMAGE (jumped))))
    return FALSE;

  /* Finally, we know it's a legal move. */

  /* Clear the peg of the jumped-over image */
  gtk_image_clear (GTK_IMAGE (jumped));

  /* Add the peg to this image */
  solitaire_peg_set_position (peg, image_x, image_y);
  gtk_image_set_from_paintable (GTK_IMAGE (image), GDK_PAINTABLE (peg));

  /* Success! */
  return TRUE;
}

GtkWidget *
do_peg_solitaire (GtkWidget *do_widget)
{
  if (!window)
    {
      GtkWidget *header;
      GtkWidget *restart;
      GtkWidget *grid;
      GtkWidget *image;
      int x, y;
      GtkDragSource *source;
      GtkDropTarget *target;

      restart = gtk_button_new_from_icon_name ("view-refresh-symbolic");
      g_signal_connect (restart, "clicked", G_CALLBACK (restart), NULL);

      header = gtk_header_bar_new ();
      gtk_header_bar_set_title (GTK_HEADER_BAR (header), "Peg Solitaire");
      gtk_header_bar_set_show_title_buttons (GTK_HEADER_BAR (header), TRUE);
      gtk_header_bar_pack_start (GTK_HEADER_BAR (header), restart);
      window = gtk_window_new ();
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      gtk_window_set_title (GTK_WINDOW (window), "Sliding Puzzle");
      gtk_window_set_titlebar (GTK_WINDOW (window), header);
      gtk_window_set_default_size (GTK_WINDOW (window), 400, 300);
      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed), &window);

      grid = gtk_grid_new ();
      gtk_widget_set_halign (grid, GTK_ALIGN_CENTER);
      gtk_widget_set_valign (grid, GTK_ALIGN_CENTER);
      gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
      gtk_grid_set_column_spacing (GTK_GRID (grid), 6);
      gtk_grid_set_row_homogeneous (GTK_GRID (grid), TRUE);
      gtk_grid_set_column_homogeneous (GTK_GRID (grid), TRUE);
      gtk_window_set_child (GTK_WINDOW (window), grid);

      for (x = 0; x < 7; x++)
        {
          for (y = 0; y < 7; y++)
            {
              if ((x < 2 || x >= 5) && (y < 2 || y >= 5))
                continue;

              image = gtk_image_new ();
              if (x != 3 || y != 3)
                {
                  SolitairePeg *peg = solitaire_peg_new ();
                  solitaire_peg_set_position (peg, x, y);
                  gtk_image_set_from_paintable (GTK_IMAGE (image), GDK_PAINTABLE (peg));
                }

              gtk_grid_attach (GTK_GRID (grid), image, x, y, 1, 1);

              /* Set up the drag source.
               * This is rather straightforward: Set the supported actions
               * (in our case, pegs can only be moved) and connect all the
               * relevant signals.
               * And because all drag'n'drop handling is done via event controllers,
               * we need to add the controller to the widget.
               */
              source = gtk_drag_source_new ();
              gtk_drag_source_set_actions (source, GDK_ACTION_MOVE);
              g_signal_connect (source, "prepare", G_CALLBACK (drag_prepare), image);
              g_signal_connect (source, "drag-begin", G_CALLBACK (drag_begin), image);
              g_signal_connect (source, "drag-end", G_CALLBACK (drag_end), image);
              gtk_widget_add_controller (image, GTK_EVENT_CONTROLLER (source));

              /* Set up the drop target.
               * This is more involved, because the game logic goes here.
               */

              /* First we specify the data we accept: pegs.
               * And we only want moves.
               */
              target = gtk_drop_target_new (SOLITAIRE_TYPE_PEG, GDK_ACTION_MOVE);
              /* Then we connect our signals.
               */
              g_signal_connect (target, "accept", G_CALLBACK (drop_accept), image);
              g_signal_connect (target, "drop", G_CALLBACK (drop_drop), image);
              /* Finally, like above, we add it to the widget.
               */
              gtk_widget_add_controller (image, GTK_EVENT_CONTROLLER (target));
            }
        }
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);

  return window;
}
