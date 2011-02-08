#include <gtk/gtk.h>

#include <math.h>

#define SLOW_TYPE_INPUT_STREAM         (slow_input_stream_get_type ())
#define SLOW_INPUT_STREAM(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), SLOW_TYPE_INPUT_STREAM, SlowInputStream))
#define SLOW_INPUT_STREAM_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), SLOW_TYPE_INPUT_STREAM, SlowInputStreamClass))
#define SLOW_IS_INPUT_STREAM(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), SLOW_TYPE_INPUT_STREAM))
#define SLOW_IS_INPUT_STREAM_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), SLOW_TYPE_INPUT_STREAM))
#define SLOW_INPUT_STREAM_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), SLOW_TYPE_INPUT_STREAM, SlowInputStreamClass))

typedef GFilterInputStream         SlowInputStream;
typedef GFilterInputStreamClass    SlowInputStreamClass;
G_DEFINE_TYPE (SlowInputStream, slow_input_stream, G_TYPE_FILTER_INPUT_STREAM)

static gssize
slow_input_stream_read (GInputStream  *stream,
                        void          *buffer,
                        gsize          count,
                        GCancellable  *cancellable,
                        GError       **error)
{
  gssize result;

  count = MIN (count, 100);

  result = g_input_stream_read (g_filter_input_stream_get_base_stream (G_FILTER_INPUT_STREAM (stream)),
                                buffer,
                                count,
                                cancellable,
                                error);

  if (result > 0)
    g_usleep (result * G_USEC_PER_SEC / 500);

  return result;
}

static gboolean
slow_input_stream_close (GInputStream  *stream,
                         GCancellable  *cancellable,
                         GError       **error)
{
  g_usleep (5 * G_USEC_PER_SEC);

  return g_input_stream_close (g_filter_input_stream_get_base_stream (G_FILTER_INPUT_STREAM (stream)),
                               cancellable,
                               error);
}


static void
slow_input_stream_class_init (SlowInputStreamClass *klass)
{
  GInputStreamClass *input_stream_class = G_INPUT_STREAM_CLASS (klass);

  input_stream_class->read_fn = slow_input_stream_read;
  input_stream_class->close_fn = slow_input_stream_close;
}

static void
slow_input_stream_init (SlowInputStream *klass)
{
}

static GInputStream *
slow_input_stream_new (GInputStream *base_stream)
{
  return g_object_new (slow_input_stream_get_type (), "base-stream", base_stream, NULL);
}

typedef struct _Demo Demo;
struct _Demo
{
  const char *name;
  void (* create) (Demo *demo);
  GtkWidget *widget;
  GdkPicture *picture;
  GdkPicture *attached_picture;
};

static void
file_is_loaded_callback (GObject      *object,
                         GAsyncResult *res,
                         gpointer      user_data)
{
}

static void
slowly_load_file (Demo *demo, GFile *file)
{
  GInputStream *file_stream, *load_stream;
  
  file_stream = G_INPUT_STREAM (g_file_read (file, NULL, NULL));
  if (file_stream == NULL)
    return;
  load_stream = slow_input_stream_new (file_stream);
  gdk_picture_loader_load_from_stream_async (GDK_PICTURE_LOADER (demo->picture),
                                             load_stream,
                                             G_PRIORITY_DEFAULT,
                                             NULL,
                                             file_is_loaded_callback,
                                             demo);
  g_object_unref (file_stream);
  g_object_unref (load_stream);
}

static void
file_set_callback (GtkFileChooser *button,
                   Demo *demo)
{
  GFile *file = gtk_file_chooser_get_file (button);

  if (file)
    slowly_load_file (demo, file);
}

static void
create_slowly_loading_image (Demo *demo)
{
  GtkWidget *button, *image;
  GtkFileFilter *filter;
  GFile *default_file;

  demo->widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  demo->picture = gdk_picture_loader_new ();

  button = gtk_file_chooser_button_new ("Select file to slowly load",
                                        GTK_FILE_CHOOSER_ACTION_OPEN);
  gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (button), FALSE);
  filter = gtk_file_filter_new ();
  gtk_file_filter_add_pixbuf_formats (filter);
  gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (button), filter);
  default_file = g_file_new_for_path ("apple-red.png");
  gtk_file_chooser_set_file (GTK_FILE_CHOOSER (button), 
                             default_file, NULL);
  slowly_load_file (demo, default_file);
  g_object_unref (default_file);
  g_signal_connect (button, 
                    "file-set",
                    G_CALLBACK (file_set_callback),
                    demo);
                             
  gtk_box_pack_start (GTK_BOX (demo->widget), button, TRUE, TRUE, 0);

  image = gtk_image_new_from_picture (demo->picture);

  gtk_box_pack_start (GTK_BOX (demo->widget), image, TRUE, TRUE, 0);
}

static void
create_stock_picture (Demo *demo)
{
  demo->widget = gtk_image_new_from_stock (GTK_STOCK_GOTO_FIRST, GTK_ICON_SIZE_BUTTON);
  gtk_widget_set_direction (demo->widget, GTK_TEXT_DIR_RTL);
  demo->picture = gtk_stock_picture_new (GTK_STOCK_GOTO_FIRST, GTK_ICON_SIZE_BUTTON);
}

static void
create_icon_set_picture (Demo *demo)
{
  struct {
    const char *icon_name;
    GtkStateType state;
  } states[] = {
    { GTK_STOCK_HOME, GTK_STATE_NORMAL },
    { GTK_STOCK_APPLY, GTK_STATE_ACTIVE },
    { GTK_STOCK_YES, GTK_STATE_PRELIGHT },
    { GTK_STOCK_SELECT_ALL, GTK_STATE_SELECTED },
    { GTK_STOCK_HELP, GTK_STATE_INSENSITIVE },
    { GTK_STOCK_ABOUT, GTK_STATE_INCONSISTENT },
    { GTK_STOCK_OK, GTK_STATE_FOCUSED }
  };
  GtkIconSet *set;
  GtkIconSource *source;
  guint i;

  set = gtk_icon_set_new ();
  for (i = 0; i < G_N_ELEMENTS (states); i++)
    {
      source = gtk_icon_source_new ();
      gtk_icon_source_set_state (source, states[i].state);
      gtk_icon_source_set_state_wildcarded (source, FALSE);
      gtk_icon_source_set_icon_name (source, states[i].icon_name);
      gtk_icon_set_add_source (set, source);
      gtk_icon_source_free (source);
    }

  demo->widget = gtk_label_new ("Shows a manually constructed icon set.\n"
                                "It displays random stock icons fordifferent states.");

  demo->picture = gtk_icon_set_picture_new (set, GTK_ICON_SIZE_BUTTON);

  gtk_icon_set_unref (set);
}

Demo demos[] = {
  { "Slowly loading image", create_slowly_loading_image, NULL, NULL },
  { "Another slowly loading image", create_slowly_loading_image, NULL, NULL },
  { "Named theme icons", create_stock_picture, NULL, NULL },
  { "Icon Set", create_icon_set_picture, NULL, NULL }
};

static guint rotation = 0;
static guint rotation_source = 0;

static void
get_picture_offset (GtkWidget *widget,
                    guint      i,
                    double    *offset_x,
                    double    *offset_y)
{
  double rx, ry;
  
  rx = (gtk_widget_get_allocated_width (widget) - gdk_picture_get_width (demos[i].attached_picture)) / 2.0;
  ry = (gtk_widget_get_allocated_height (widget) - gdk_picture_get_height (demos[i].attached_picture)) / 2.0;

  *offset_x = gtk_widget_get_allocated_width (widget) / 2.0
              + rx * sin (2 * G_PI * i / G_N_ELEMENTS (demos) + (2 * G_PI * rotation / 360))
              - gdk_picture_get_width (demos[i].attached_picture) / 2.0;
  *offset_y = gtk_widget_get_allocated_height (widget) / 2.0
              + ry * cos (2 * G_PI * i / G_N_ELEMENTS (demos) + (2 * G_PI * rotation / 360))
              - gdk_picture_get_height (demos[i].attached_picture) / 2.0;
}

static gboolean
draw_callback (GtkWidget *area,
               cairo_t   *cr,
               gpointer   unused)
{
  guint i;

  for (i = 0; i < G_N_ELEMENTS (demos); i++)
  {
    double x, y;

    get_picture_offset (area, i, &x, &y);

    cairo_save (cr);

    cairo_translate (cr, x, y);
    gdk_picture_draw (demos[i].attached_picture, cr);

    cairo_restore (cr);
  }

  return FALSE;
}

#define ROTATE_FPS 40
#define ROTATE_SECONDS 3

static gboolean
rotate_area (gpointer area)
{
  rotation += 360 / ROTATE_SECONDS / ROTATE_FPS;
  rotation %= 360;

  gtk_widget_queue_draw (area);

  return TRUE;
}

static void
rotation_toggled (GtkButton *button, GtkWidget *area)
{
  if (rotation_source)
    {
      g_source_remove (rotation_source);
      rotation_source = 0;
      gtk_button_set_label (button, GTK_STOCK_MEDIA_PLAY);
    }
  else
    {
      rotation_source = gdk_threads_add_timeout (1000 / ROTATE_FPS, rotate_area, area);
      gtk_button_set_label (button, GTK_STOCK_MEDIA_PAUSE);
    }
}

static void
picture_changed (GdkPicture *picture, const cairo_region_t *region, GtkWidget *area)
{
  cairo_region_t *copy;
  double x, y;
  int dx, dy;
  guint i;

  for (i = 0; i < G_N_ELEMENTS (demos); i++)
    {
      if (demos[i].attached_picture == picture)
        break;
    }
  g_assert (i < G_N_ELEMENTS (demos));

  get_picture_offset (area, i, &x, &y);
  copy = cairo_region_copy (region);
  cairo_region_translate (copy, floor (x), floor (y));
  gtk_widget_queue_draw_region (area, copy);
  dx = (floor (x) != ceil (x)) ? 1 : 0;
  dy = (floor (y) != ceil (y)) ? 1 : 0;
  if (dx || dy)
    {
      cairo_region_translate (copy, dx, dy);
      gtk_widget_queue_draw_region (area, copy);
    }
  cairo_region_destroy (copy);
}

static void
toggled_flag (GtkWidget *check, gpointer area)
{
  int flag = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (check), "flags"));

  if (flag)
    {
      if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (check)))
        gtk_widget_set_state_flags (area, flag, FALSE);
      else
        gtk_widget_unset_state_flags (area, flag);
    }
  else
    gtk_widget_set_direction (area,
                              gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (check)) ? GTK_TEXT_DIR_RTL :
                                                                                         GTK_TEXT_DIR_LTR);
}

GtkWidget *
create_optionsview (GtkWidget *area)
{
  struct {
    GtkStateFlags flags;
    const char *  text;
  } options[] = {
    { 0,                           "right-to-left" },
    { GTK_STATE_FLAG_ACTIVE,       "active" },
    { GTK_STATE_FLAG_PRELIGHT,     "prelight" },
    { GTK_STATE_FLAG_SELECTED,     "selected" },
    { GTK_STATE_FLAG_INSENSITIVE,  "insensitive" },
    { GTK_STATE_FLAG_INCONSISTENT, "inconsistent" },
    { GTK_STATE_FLAG_FOCUSED,      "focused" }
  };
  GtkWidget *box, *check;
  guint i;

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  for (i = 0; i < G_N_ELEMENTS (options); i++)
    {
      check = gtk_check_button_new_with_label (options[i].text);
      if (options[i].flags)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check),
                                      gtk_widget_get_state_flags (area) & options[i].flags);
      else
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check),
                                      gtk_widget_get_direction (area) == GTK_TEXT_DIR_RTL);
      g_object_set_data (G_OBJECT (check), "flags", GINT_TO_POINTER (options[i].flags));
      g_signal_connect (check, "toggled", G_CALLBACK (toggled_flag), area);
      gtk_box_pack_start (GTK_BOX (box), check, TRUE, TRUE, 0);
    }

  return box;
}

int
main (int argc, char **argv)
{
  GtkWidget *window, *vbox, *hbox, *area, *widget;
  guint i;

  gtk_init (&argc, &argv);

  //gdk_window_set_debug_updates (TRUE);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window), "Pictures");

  g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_container_add (GTK_CONTAINER (window), hbox);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);

  area = gtk_drawing_area_new ();
  gtk_widget_set_size_request (area, 400, 400);
  g_signal_connect (area, "draw", G_CALLBACK (draw_callback), NULL);
  gtk_box_pack_start (GTK_BOX (vbox), area, TRUE, TRUE, 0);

  widget = gtk_button_new_from_stock (GTK_STOCK_MEDIA_PAUSE);
  g_signal_connect (widget, "clicked", G_CALLBACK (rotation_toggled), area);
  rotation_toggled (GTK_BUTTON (widget), area);
  gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, TRUE, 0);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_box_pack_start (GTK_BOX (hbox), vbox, FALSE, TRUE, 0);

  widget = create_optionsview (area);
  gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, TRUE, 0);

  for (i = 0; i < G_N_ELEMENTS (demos); i++)
    {
      GtkWidget *expander;
      
      demos[i].create (&demos[i]);

      demos[i].attached_picture = gtk_widget_style_picture (area, demos[i].picture);
      g_signal_connect (demos[i].attached_picture, "changed", G_CALLBACK (picture_changed), area);

      expander = gtk_expander_new (demos[i].name);
      gtk_container_add (GTK_CONTAINER (expander), demos[i].widget);

      gtk_box_pack_start (GTK_BOX (vbox), expander, FALSE, TRUE, 0);
    }

  gtk_widget_show_all (window);

  gtk_main ();

  return 0;
}


