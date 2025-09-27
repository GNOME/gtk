/* Benchmark/Scrolling
 * #Keywords: GtkScrolledWindow
 *
 * This demo scrolls a view with various content.
 */

#include <gtk/gtk.h>
#include "symbolicpaintable.h"
#include "gtk/gtkrendernodepaintableprivate.h"

static guint tick_cb;
static GtkAdjustment *hadjustment;
static GtkAdjustment *vadjustment;
static GtkWidget *window = NULL;
static GtkWidget *scrolledwindow;
static int selected;

#define N_WIDGET_TYPES 11


static int hincrement = 5;
static int vincrement = 5;

static gboolean
scroll_cb (GtkWidget *widget,
           GdkFrameClock *frame_clock,
           gpointer data)
{
  double value;

  value = gtk_adjustment_get_value (vadjustment);
  if (value + vincrement <= gtk_adjustment_get_lower (vadjustment) ||
     (value + vincrement >= gtk_adjustment_get_upper (vadjustment) - gtk_adjustment_get_page_size (vadjustment)))
    vincrement = - vincrement;

  gtk_adjustment_set_value (vadjustment, value + vincrement);

  value = gtk_adjustment_get_value (hadjustment);
  if (value + hincrement <= gtk_adjustment_get_lower (hadjustment) ||
     (value + hincrement >= gtk_adjustment_get_upper (hadjustment) - gtk_adjustment_get_page_size (hadjustment)))
    hincrement = - hincrement;

  gtk_adjustment_set_value (hadjustment, value + hincrement);

  return G_SOURCE_CONTINUE;
}

extern GtkWidget *create_icon (void);

static void
populate_icons (void)
{
  GtkWidget *grid;
  int top, left;

  grid = gtk_grid_new ();
  gtk_widget_set_halign (grid, GTK_ALIGN_CENTER);
  gtk_widget_set_margin_start (grid, 10);
  gtk_widget_set_margin_end (grid, 10);
  gtk_widget_set_margin_top (grid, 10);
  gtk_widget_set_margin_bottom (grid, 10);
  gtk_grid_set_row_spacing (GTK_GRID (grid), 10);
  gtk_grid_set_column_spacing (GTK_GRID (grid), 10);

  for (top = 0; top < 100; top++)
    for (left = 0; left < 15; left++)
      gtk_grid_attach (GTK_GRID (grid), create_icon (), left, top, 1, 1);

  hincrement = 0;
  vincrement = 5;

  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow),
                                  GTK_POLICY_NEVER,
                                  GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scrolledwindow), grid);
}

extern void fontify (const char *format, GtkTextBuffer *buffer);

enum {
  PLAIN_TEXT,
  HIGHLIGHTED_TEXT,
  UNDERLINED_TEXT,
};

static void
underlinify (GtkTextBuffer *buffer)
{
  GtkTextTagTable *tags;
  GtkTextTag *tag[3];
  GtkTextIter start, end;

  tags = gtk_text_buffer_get_tag_table (buffer);
  tag[0] = gtk_text_tag_new ("error");
  tag[1] = gtk_text_tag_new ("strikeout");
  tag[2] = gtk_text_tag_new ("double");
  g_object_set (tag[0], "underline", PANGO_UNDERLINE_ERROR, NULL);
  g_object_set (tag[1], "strikethrough", TRUE, NULL);
  g_object_set (tag[2],
                "underline", PANGO_UNDERLINE_DOUBLE,
                "underline-rgba", &(GdkRGBA){0., 1., 1., 1. },
                NULL);
  gtk_text_tag_table_add (tags, tag[0]);
  gtk_text_tag_table_add (tags, tag[1]);
  gtk_text_tag_table_add (tags, tag[2]);

  gtk_text_buffer_get_start_iter (buffer, &end);

  while (TRUE)
    {
      gtk_text_iter_forward_word_end (&end);
      start = end;
      gtk_text_iter_backward_word_start (&start);
      gtk_text_buffer_apply_tag (buffer, tag[g_random_int_range (0, 3)], &start, &end);
      if (!gtk_text_iter_forward_word_ends (&end, 3))
        break;
    }
}

static void
populate_text (const char *resource, int kind)
{
  GtkWidget *textview;
  GtkTextBuffer *buffer;
  char *content;
  gsize content_len;
  GBytes *bytes;

  bytes = g_resources_lookup_data (resource, 0, NULL);
  content = g_bytes_unref_to_data (bytes, &content_len);

  buffer = gtk_text_buffer_new (NULL);
  gtk_text_buffer_set_text (buffer, content, (int)content_len);

  switch (kind)
    {
    case HIGHLIGHTED_TEXT:
      fontify ("c", buffer);
      break;

    case UNDERLINED_TEXT:
      underlinify (buffer);
      break;

    case PLAIN_TEXT:
    default:
      break;
    }

  textview = gtk_text_view_new ();
  gtk_text_view_set_buffer (GTK_TEXT_VIEW (textview), buffer);

  hincrement = 0;
  vincrement = 5;

  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow),
                                  GTK_POLICY_NEVER,
                                  GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scrolledwindow), textview);
}

static void
populate_emoji_text (void)
{
  GtkWidget *textview;
  GtkTextBuffer *buffer;
  GString *s;
  GtkTextIter iter;

  s = g_string_sized_new (500 * 30 * 4);

  for (int i = 0; i < 500; i++)
    {
      if (i % 2)
        g_string_append (s, "<span underline=\"single\" underline_color=\"red\">x</span>");
      for (int j = 0; j < 30; j++)
        {
          g_string_append (s, "💓");
          g_string_append (s, "<span underline=\"single\" underline_color=\"red\">x</span>");
        }
      g_string_append (s, "\n");
    }

  buffer = gtk_text_buffer_new (NULL);
  gtk_text_buffer_get_start_iter (buffer, &iter);
  gtk_text_buffer_insert_markup (buffer, &iter, s->str, s->len);

  g_string_free (s, TRUE);

  textview = gtk_text_view_new ();
  gtk_text_view_set_buffer (GTK_TEXT_VIEW (textview), buffer);

  hincrement = 0;
  vincrement = 5;

  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow),
                                  GTK_POLICY_NEVER,
                                  GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scrolledwindow), textview);
}

static void
populate_image (void)
{
  GtkWidget *image;

  image = gtk_picture_new_for_resource ("/sliding_puzzle/portland-rose.jpg");
  gtk_picture_set_can_shrink (GTK_PICTURE (image), FALSE);

  hincrement = 5;
  vincrement = 5;

  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow),
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scrolledwindow), image);
}

extern GtkWidget *create_weather_view (void);

static void
populate_list (void)
{
  GtkWidget *list;

  list = create_weather_view ();

  hincrement = 5;
  vincrement = 0;

  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow),
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scrolledwindow), list);
}

extern GtkWidget *create_color_grid (void);
extern GListModel *gtk_color_list_new (guint size);

static void
populate_grid (void)
{
  GtkWidget *list;
  GtkNoSelection *selection;

  list = create_color_grid ();

  selection = gtk_no_selection_new (gtk_color_list_new (2097152));
  gtk_grid_view_set_model (GTK_GRID_VIEW (list), GTK_SELECTION_MODEL (selection));
  g_object_unref (selection);

  hincrement = 0;
  vincrement = 5;

  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow),
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scrolledwindow), list);
}

extern GtkWidget *create_ucd_view (GtkWidget *label);

static void
populate_list2 (void)
{
  GtkWidget *list;

  list = create_ucd_view (NULL);

  hincrement = 0;
  vincrement = 5;

  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow),
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scrolledwindow), list);
}

struct {
  const char *path;
  GdkPaintable *paintable;
} symbolics[] = {
  { "bookmark-new-symbolic.svg", NULL },
  { "color-select-symbolic.svg", NULL },
  { "document-open-recent-symbolic.svg", NULL },
  { "document-open-symbolic.svg", NULL },
  { "document-save-as-symbolic.svg", NULL },
  { "document-save-symbolic.svg", NULL },
  { "edit-clear-all-symbolic.svg", NULL },
  { "edit-clear-symbolic-rtl.svg", NULL },
  { "edit-clear-symbolic.svg", NULL },
  { "edit-copy-symbolic.svg", NULL },
  { "edit-cut-symbolic.svg", NULL },
  { "edit-delete-symbolic.svg", NULL },
  { "edit-find-symbolic.svg", NULL },
  { "edit-paste-symbolic.svg", NULL },
  { "edit-select-all-symbolic.svg", NULL },
  { "find-location-symbolic.svg", NULL },
  { "folder-new-symbolic.svg", NULL },
  { "function-linear-symbolic.svg", NULL },
  { "gesture-pinch-symbolic.svg", NULL },
  { "gesture-rotate-anticlockwise-symbolic.svg", NULL },
  { "gesture-rotate-clockwise-symbolic.svg", NULL },
  { "gesture-stretch-symbolic.svg", NULL },
  { "gesture-swipe-left-symbolic.svg", NULL },
  { "gesture-swipe-right-symbolic.svg", NULL },
  { "gesture-two-finger-swipe-left-symbolic.svg", NULL },
  { "gesture-two-finger-swipe-right-symbolic.svg", NULL },
  { "go-next-symbolic-rtl.svg", NULL },
  { "go-next-symbolic.svg", NULL },
  { "go-previous-symbolic-rtl.svg", NULL },
  { "go-previous-symbolic.svg", NULL },
  { "insert-image-symbolic.svg", NULL },
  { "insert-object-symbolic.svg", NULL },
  { "list-add-symbolic.svg", NULL },
  { "list-remove-all-symbolic.svg", NULL },
  { "list-remove-symbolic.svg", NULL },
  { "media-eject-symbolic.svg", NULL },
  { "media-playback-pause-symbolic.svg", NULL },
  { "media-playback-start-symbolic.svg", NULL },
  { "media-playback-stop-symbolic.svg", NULL },
  { "media-record-symbolic.svg", NULL },
  { "object-select-symbolic.svg", NULL },
  { "open-menu-symbolic.svg", NULL },
  { "pan-down-symbolic.svg", NULL },
  { "pan-end-symbolic-rtl.svg", NULL },
  { "pan-end-symbolic.svg", NULL },
  { "pan-start-symbolic-rtl.svg", NULL },
  { "pan-start-symbolic.svg", NULL },
  { "pan-up-symbolic.svg", NULL },
  { "system-run-symbolic.svg", NULL },
  { "system-search-symbolic.svg", NULL },
  { "value-decrease-symbolic.svg", NULL },
  { "value-increase-symbolic.svg", NULL },
  { "view-conceal-symbolic.svg", NULL },
  { "view-grid-symbolic.svg", NULL },
  { "view-list-symbolic.svg", NULL },
  { "view-more-symbolic.svg", NULL },
  { "view-refresh-symbolic.svg", NULL },
  { "view-reveal-symbolic.svg", NULL },
  { "window-close-symbolic.svg", NULL },
  { "window-maximize-symbolic.svg", NULL },
  { "window-minimize-symbolic.svg", NULL },
  { "window-restore-symbolic.svg", NULL },
  { "emoji-activities-symbolic.svg", NULL },
  { "emoji-body-symbolic.svg", NULL },
  { "emoji-flags-symbolic.svg", NULL },
  { "emoji-food-symbolic.svg", NULL },
  { "emoji-nature-symbolic.svg", NULL },
  { "emoji-objects-symbolic.svg", NULL },
  { "emoji-people-symbolic.svg", NULL },
  { "emoji-recent-symbolic.svg", NULL },
  { "emoji-symbols-symbolic.svg", NULL },
  { "emoji-travel-symbolic.svg", NULL },
  { "drive-harddisk-symbolic.svg", NULL },
  { "printer-symbolic.svg", NULL },
  { "emblem-important-symbolic.svg", NULL },
  { "emblem-system-symbolic.svg", NULL },
  { "face-smile-big-symbolic.svg", NULL },
  { "face-smile-symbolic.svg", NULL },
  { "application-x-executable-symbolic.svg", NULL },
  { "text-x-generic-symbolic.svg", NULL },
  { "folder-documents-symbolic.svg", NULL },
  { "folder-download-symbolic.svg", NULL },
  { "folder-music-symbolic.svg", NULL },
  { "folder-pictures-symbolic.svg", NULL },
  { "folder-publicshare-symbolic.svg", NULL },
  { "folder-remote-symbolic.svg", NULL },
  { "folder-saved-search-symbolic.svg", NULL },
  { "folder-symbolic.svg", NULL },
  { "folder-templates-symbolic.svg", NULL },
  { "folder-videos-symbolic.svg", NULL },
  { "network-server-symbolic.svg", NULL },
  { "network-workgroup-symbolic.svg", NULL },
  { "user-desktop-symbolic.svg", NULL },
  { "user-home-symbolic.svg", NULL },
  { "user-trash-symbolic.svg", NULL },
  { "audio-volume-high-symbolic.svg", NULL },
  { "audio-volume-low-symbolic.svg", NULL },
  { "audio-volume-medium-symbolic.svg", NULL },
  { "audio-volume-muted-symbolic.svg", NULL },
  { "caps-lock-symbolic.svg", NULL },
  { "changes-allow-symbolic.svg", NULL },
  { "changes-prevent-symbolic.svg", NULL },
  { "dialog-error-symbolic.svg", NULL },
  { "dialog-information-symbolic.svg", NULL },
  { "dialog-password-symbolic.svg", NULL },
  { "dialog-question-symbolic.svg", NULL },
  { "dialog-warning-symbolic.svg", NULL },
  { "display-brightness-symbolic.svg", NULL },
  { "media-playlist-repeat-symbolic.svg", NULL },
  { "orientation-landscape-inverse-symbolic.svg", NULL },
  { "orientation-landscape-symbolic.svg", NULL },
  { "orientation-portrait-inverse-symbolic.svg", NULL },
  { "orientation-portrait-symbolic.svg", NULL },
  { "process-working-symbolic.svg", NULL },
  { "switch-off-symbolic.svg", NULL },
  { "switch-on-symbolic.svg", NULL }, 
};

static GtkWidget *
create_path (void)
{
  GtkWidget *image;
  static int idx = 0;

  idx = (idx + 1) % G_N_ELEMENTS (symbolics);
  if (symbolics[idx].paintable == NULL)
    {
      char *uri;
      GFile *file;

      uri = g_strconcat ("resource://", "/org/gtk/libgtk/icons/", symbolics[idx].path, NULL);
      file = g_file_new_for_uri (uri);
      symbolics[idx].paintable = GDK_PAINTABLE (symbolic_paintable_new (file));
      g_object_unref (file);
      g_free (uri);
    }

  image = gtk_image_new ();
  gtk_image_set_icon_size (GTK_IMAGE (image), GTK_ICON_SIZE_LARGE);
  gtk_image_set_from_paintable (GTK_IMAGE (image), symbolics[idx].paintable);

  return image;
}

static void
populate_paths (void)
{
  GtkWidget *grid;
  int top, left;

  grid = gtk_grid_new ();
  gtk_widget_set_halign (grid, GTK_ALIGN_CENTER);
  gtk_widget_set_margin_start (grid, 10);
  gtk_widget_set_margin_end (grid, 10);
  gtk_widget_set_margin_top (grid, 10);
  gtk_widget_set_margin_bottom (grid, 10);
  gtk_grid_set_row_spacing (GTK_GRID (grid), 10);
  gtk_grid_set_column_spacing (GTK_GRID (grid), 10);

  for (top = 0; top < 100; top++)
    for (left = 0; left < 15; left++)
       {
         gtk_grid_attach (GTK_GRID (grid), create_path (), left, top, 1, 1);
       }

  hincrement = 0;
  vincrement = 5;

  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow),
                                  GTK_POLICY_NEVER,
                                  GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scrolledwindow), grid);
}

static GtkWidget *
create_squiggle (void)
{
  GtkWidget *image;
  graphene_rect_t bounds = GRAPHENE_RECT_INIT (0, 0, 18, 18);
  GskRenderNode *child, *node;
  GdkPaintable *paintable;
  GskStroke *stroke;
  GskPathBuilder *builder;
  GskPath *path;
  float x, y;

  builder = gsk_path_builder_new ();

  x = g_random_double_range (1, 16);
  y = g_random_double_range (1, 16);
  gsk_path_builder_move_to (builder, x, y);
  for (int i = 0; i < 5; i++)
    {
      x = g_random_double_range (1, 16);
      y = g_random_double_range (1, 16);
      gsk_path_builder_line_to (builder, x, y);
    }
  gsk_path_builder_close (builder);

  path = gsk_path_builder_free_to_path (builder);
  stroke = gsk_stroke_new (1);

  child = gsk_color_node_new (&(GdkRGBA) { 0, 0, 0, 1}, &bounds);
  node = gsk_stroke_node_new (child, path, stroke);
  paintable = gtk_render_node_paintable_new (node, &bounds);

  image = gtk_image_new ();
  gtk_image_set_icon_size (GTK_IMAGE (image), GTK_ICON_SIZE_LARGE);
  gtk_image_set_from_paintable (GTK_IMAGE (image), paintable);

  g_object_unref (paintable);
  gsk_stroke_free (stroke);
  gsk_path_unref (path);
  gsk_render_node_unref (node);
  gsk_render_node_unref (child);

  return image;
}

static void
populate_squiggles (void)
{
  GtkWidget *grid;
  int top, left;

  grid = gtk_grid_new ();
  gtk_widget_set_halign (grid, GTK_ALIGN_CENTER);
  gtk_widget_set_margin_start (grid, 10);
  gtk_widget_set_margin_end (grid, 10);
  gtk_widget_set_margin_top (grid, 10);
  gtk_widget_set_margin_bottom (grid, 10);
  gtk_grid_set_row_spacing (GTK_GRID (grid), 10);
  gtk_grid_set_column_spacing (GTK_GRID (grid), 10);

  for (top = 0; top < 100; top++)
    for (left = 0; left < 15; left++)
       {
         gtk_grid_attach (GTK_GRID (grid), create_squiggle (), left, top, 1, 1);
       }

  hincrement = 0;
  vincrement = 5;

  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow),
                                  GTK_POLICY_NEVER,
                                  GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scrolledwindow), grid);
}

static void
set_widget_type (int type)
{
  if (tick_cb)
    gtk_widget_remove_tick_callback (window, tick_cb);

  if (gtk_scrolled_window_get_child (GTK_SCROLLED_WINDOW (scrolledwindow)))
    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scrolledwindow), NULL);

  selected = type;

  switch (selected)
    {
    case 0:
      gtk_window_set_title (GTK_WINDOW (window), "Scrolling icons");
      populate_icons ();
      break;

    case 1:
      gtk_window_set_title (GTK_WINDOW (window), "Scrolling plain text");
      populate_text ("/sources/font_features.c", PLAIN_TEXT);
      break;

    case 2:
      gtk_window_set_title (GTK_WINDOW (window), "Scrolling colored text");
      populate_text ("/sources/font_features.c", HIGHLIGHTED_TEXT);
      break;

    case 3:
      gtk_window_set_title (GTK_WINDOW (window), "Scrolling text with underlines");
      populate_text ("/org/gtk/Demo4/Moby-Dick.txt", UNDERLINED_TEXT);
      break;

    case 4:
      gtk_window_set_title (GTK_WINDOW (window), "Scrolling text with Emoji");
      populate_emoji_text ();
      break;

    case 5:
      gtk_window_set_title (GTK_WINDOW (window), "Scrolling a big image");
      populate_image ();
      break;

    case 6:
      gtk_window_set_title (GTK_WINDOW (window), "Scrolling a list");
      populate_list ();
      break;

    case 7:
      gtk_window_set_title (GTK_WINDOW (window), "Scrolling a columned list");
      populate_list2 ();
      break;

    case 8:
      gtk_window_set_title (GTK_WINDOW (window), "Scrolling a grid");
      populate_grid ();
      break;

    case 9:
      gtk_window_set_title (GTK_WINDOW (window), "Scrolling paths");
      populate_paths ();
      break;

    case 10:
      gtk_window_set_title (GTK_WINDOW (window), "Scrolling squiggles");
      populate_squiggles ();
      break;

    default:
      g_assert_not_reached ();
    }

  tick_cb = gtk_widget_add_tick_callback (window, scroll_cb, NULL, NULL);
}

G_MODULE_EXPORT void
iconscroll_next_clicked_cb (GtkButton *source,
                            gpointer   user_data)
{
  int new_index;

  if (selected + 1 >= N_WIDGET_TYPES)
    new_index = 0;
  else
    new_index = selected + 1;
 

  set_widget_type (new_index);
}

G_MODULE_EXPORT void
iconscroll_prev_clicked_cb (GtkButton *source,
                            gpointer   user_data)
{
  int new_index;

  if (selected - 1 < 0)
    new_index = N_WIDGET_TYPES - 1;
  else
    new_index = selected - 1;

  set_widget_type (new_index);
}

static gboolean
update_fps (gpointer data)
{
  GtkWidget *label = data;
  GdkFrameClock *frame_clock;
  double fps;
  char *str;

  frame_clock = gtk_widget_get_frame_clock (label);

  fps = gdk_frame_clock_get_fps (frame_clock);
  str = g_strdup_printf ("%.2f fps", fps);
  gtk_label_set_label (GTK_LABEL (label), str);
  g_free (str);

  return G_SOURCE_CONTINUE;
}

static void
remove_timeout (gpointer data)
{
  g_source_remove (GPOINTER_TO_UINT (data));
}

G_MODULE_EXPORT GtkWidget *
do_iconscroll (GtkWidget *do_widget)
{
  if (!window)
    {
      GtkBuilder *builder;
      GtkBuilderScope *scope;
      GtkWidget *label;
      guint id;

      scope = gtk_builder_cscope_new ();
      gtk_builder_cscope_add_callback (GTK_BUILDER_CSCOPE (scope), iconscroll_prev_clicked_cb);
      gtk_builder_cscope_add_callback (GTK_BUILDER_CSCOPE (scope), iconscroll_next_clicked_cb);

      builder = gtk_builder_new ();
      gtk_builder_set_scope (builder, scope);

      gtk_builder_add_from_resource (builder, "/iconscroll/iconscroll.ui", NULL);
      window = GTK_WIDGET (gtk_builder_get_object (builder, "window"));
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));

      scrolledwindow = GTK_WIDGET (gtk_builder_get_object (builder, "scrolledwindow"));
      gtk_widget_realize (window);
      hadjustment = GTK_ADJUSTMENT (gtk_builder_get_object (builder, "hadjustment"));
      vadjustment = GTK_ADJUSTMENT (gtk_builder_get_object (builder, "vadjustment"));
      if (g_getenv ("ICONSCROLL"))
        set_widget_type (CLAMP (atoi (g_getenv ("ICONSCROLL")), 0, N_WIDGET_TYPES));
      else
        set_widget_type (0);

      label = GTK_WIDGET (gtk_builder_get_object (builder, "fps_label"));
      id = g_timeout_add_full (G_PRIORITY_HIGH, 500, update_fps, label, NULL);
      g_object_set_data_full (G_OBJECT (label), "timeout",
                              GUINT_TO_POINTER (id), remove_timeout);

      g_object_unref (builder);
      g_object_unref (scope);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_set_visible (window, TRUE);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
