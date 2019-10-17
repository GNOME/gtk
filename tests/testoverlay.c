#include <string.h>
#include <gtk/gtk.h>

static const char *css =
".overlay-green {"
"  background-image: none;"
"  background-color: green;"
"}\n"
".overlay-white {"
"  background-image: none;"
"  background-color: white;"
"}\n"
".transparent-red {"
"  background-image: none;"
"  background-color: rgba(255, 0, 0, 0.8);"
"}\n"
".transparent-green {"
"  background-image: none;"
"  background-color: rgba(0, 255, 0, 0.8);"
"}\n"
".transparent-blue {"
"  background-image: none;"
"  background-color: rgba(0, 0, 255, 0.8);"
"}\n"
".transparent-purple {"
"  background-image: none;"
"  background-color: rgba(255, 0, 255, 0.8);"
"}\n"
;


/* test that margins and non-zero allocation x/y
 * of the main widget are handled correctly
 */
static GtkWidget *
test_nonzerox (void)
{
  GtkWidget *win;
  GtkWidget *grid;
  GtkWidget *overlay;
  GtkWidget *text;
  GtkWidget *child;

  win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (win), "Non-zero X");

  grid = gtk_grid_new ();
  g_object_set (grid, "margin", 5, NULL);
  gtk_container_add (GTK_CONTAINER (win), grid);
  gtk_grid_attach (GTK_GRID (grid), gtk_label_new ("Above"), 1, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), gtk_label_new ("Below"), 1, 2, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), gtk_label_new ("Left"), 0, 1, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), gtk_label_new ("Right"), 2, 1, 1, 1);

  overlay = gtk_overlay_new ();
  gtk_grid_attach (GTK_GRID (grid), overlay, 1, 1, 1, 1);

  text = gtk_text_view_new ();
  gtk_widget_set_size_request (text, 200, 200);
  gtk_widget_set_hexpand (text, TRUE);
  gtk_widget_set_vexpand (text, TRUE);
  gtk_container_add (GTK_CONTAINER (overlay), text);

  child = gtk_label_new ("I'm the overlay");
  gtk_widget_set_halign (child, GTK_ALIGN_START);
  gtk_widget_set_valign (child, GTK_ALIGN_START);
  g_object_set (child, "margin", 3, NULL);
  gtk_overlay_add_overlay (GTK_OVERLAY (overlay), child);

  child = gtk_label_new ("No, I'm the overlay");
  gtk_widget_set_halign (child, GTK_ALIGN_END);
  gtk_widget_set_valign (child, GTK_ALIGN_END);
  gtk_overlay_add_overlay (GTK_OVERLAY (overlay), child);
  g_object_set (child, "margin", 3, NULL);

  return win;
}

static gboolean
get_child_position (GtkOverlay    *overlay,
                    GtkWidget     *widget,
                    GtkAllocation *alloc,
                    GtkWidget     *relative)
{
  GtkRequisition req;
  GtkWidget *child;
  GtkAllocation main_alloc;
  gint x, y;

  child = gtk_bin_get_child (GTK_BIN (overlay));

  gtk_widget_translate_coordinates (relative, child, 0, 0, &x, &y);
  main_alloc.x = x;
  main_alloc.y = y;
  main_alloc.width = gtk_widget_get_allocated_width (relative);
  main_alloc.height = gtk_widget_get_allocated_height (relative);

  gtk_widget_get_preferred_size (widget, NULL, &req);

  alloc->x = main_alloc.x;
  alloc->width = MIN (main_alloc.width, req.width);
  if (gtk_widget_get_halign (widget) == GTK_ALIGN_END)
    alloc->x += main_alloc.width - req.width;

  alloc->y = main_alloc.y;
  alloc->height = MIN (main_alloc.height, req.height);
  if (gtk_widget_get_valign (widget) == GTK_ALIGN_END)
    alloc->y += main_alloc.height - req.height;

  return TRUE;
}

/* test custom positioning */
static GtkWidget *
test_relative (void)
{
  GtkWidget *win;
  GtkWidget *grid;
  GtkWidget *overlay;
  GtkWidget *text;
  GtkWidget *child;

  win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (win), "Custom positioning");

  overlay = gtk_overlay_new ();
  gtk_container_add (GTK_CONTAINER (win), overlay);

  grid = gtk_grid_new ();
  gtk_container_add (GTK_CONTAINER (overlay), grid);
  gtk_grid_attach (GTK_GRID (grid), gtk_label_new ("Above"), 1, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), gtk_label_new ("Below"), 1, 2, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), gtk_label_new ("Left"), 0, 1, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), gtk_label_new ("Right"), 2, 1, 1, 1);

  text = gtk_text_view_new ();
  gtk_widget_set_size_request (text, 200, 200);
  g_object_set (text, "margin", 5, NULL);
  gtk_widget_set_hexpand (text, TRUE);
  gtk_widget_set_vexpand (text, TRUE);
  gtk_grid_attach (GTK_GRID (grid), text, 1, 1, 1, 1);
  g_signal_connect (overlay, "get-child-position",
                    G_CALLBACK (get_child_position), text);

  child = gtk_label_new ("Top left overlay");
  gtk_widget_set_halign (child, GTK_ALIGN_START);
  gtk_widget_set_valign (child, GTK_ALIGN_START);
  gtk_overlay_add_overlay (GTK_OVERLAY (overlay), child);
  g_object_set (child, "margin", 1, NULL);

  child = gtk_label_new ("Bottom right overlay");
  gtk_widget_set_halign (child, GTK_ALIGN_END);
  gtk_widget_set_valign (child, GTK_ALIGN_END);
  gtk_overlay_add_overlay (GTK_OVERLAY (overlay), child);
  g_object_set (child, "margin", 1, NULL);

  return win;
}

/* test GTK_ALIGN_FILL handling */
static GtkWidget *
test_fullwidth (void)
{
  GtkWidget *win;
  GtkWidget *overlay;
  GtkWidget *text;
  GtkWidget *child;

  win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (win), "Full-width");

  overlay = gtk_overlay_new ();
  gtk_container_add (GTK_CONTAINER (win), overlay);

  text = gtk_text_view_new ();
  gtk_widget_set_size_request (text, 200, 200);
  gtk_widget_set_hexpand (text, TRUE);
  gtk_widget_set_vexpand (text, TRUE);
  gtk_container_add (GTK_CONTAINER (overlay), text);

  child = gtk_label_new ("Fullwidth top overlay");
  gtk_widget_set_halign (child, GTK_ALIGN_FILL);
  gtk_widget_set_valign (child, GTK_ALIGN_START);
  gtk_overlay_add_overlay (GTK_OVERLAY (overlay), child);
  g_object_set (child, "margin", 4, NULL);

  return win;
}

/* test that scrolling works as expected */
static GtkWidget *
test_scrolling (void)
{
  GtkWidget *win;
  GtkWidget *overlay;
  GtkWidget *sw;
  GtkWidget *text;
  GtkWidget *child;
  GtkTextBuffer *buffer;
  gchar *contents;
  gsize len;

  win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (win), "Scrolling");

  overlay = gtk_overlay_new ();
  gtk_container_add (GTK_CONTAINER (win), overlay);

  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_min_content_width (GTK_SCROLLED_WINDOW (sw), 200);
  gtk_scrolled_window_set_min_content_height (GTK_SCROLLED_WINDOW (sw), 200);
  gtk_container_add (GTK_CONTAINER (overlay), sw);

  text = gtk_text_view_new ();
  buffer = gtk_text_buffer_new (NULL);
  if (!g_file_get_contents ("testoverlay.c", &contents, &len, NULL))
    {
      contents = g_strdup ("Text should go here...");
      len = strlen (contents);
    }
  gtk_text_buffer_set_text (buffer, contents, len);
  g_free (contents);
  gtk_text_view_set_buffer (GTK_TEXT_VIEW (text), buffer);

  gtk_widget_set_hexpand (text, TRUE);
  gtk_widget_set_vexpand (text, TRUE);
  gtk_container_add (GTK_CONTAINER (sw), text);

  child = gtk_label_new ("This should be visible");
  gtk_widget_set_halign (child, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (child, GTK_ALIGN_END);
  gtk_overlay_add_overlay (GTK_OVERLAY (overlay), child);
  g_object_set (child, "margin", 4, NULL);

  return win;
}

static const gchar *buffer =
"<interface>"
"  <object class='GtkWindow' id='window'>"
"    <property name='title'>GtkBuilder support</property>"
"    <child>"
"      <object class='GtkOverlay' id='overlay'>"
"        <child type='overlay'>"
"          <object class='GtkLabel' id='overlay-child'>"
"            <property name='label'>Witty remark goes here</property>"
"            <property name='halign'>end</property>"
"            <property name='valign'>end</property>"
"            <property name='margin'>4</property>"
"          </object>"
"        </child>"
"        <child>"
"          <object class='GtkGrid' id='grid'>"
"            <child>"
"              <object class='GtkLabel' id='left'>"
"                <property name='label'>Left</property>"
"              </object>"
"              <packing>"
"                <property name='left_attach'>0</property>"
"                <property name='top_attach'>0</property>"
"              </packing>"
"            </child>"
"            <child>"
"              <object class='GtkLabel' id='right'>"
"                <property name='label'>Right</property>"
"              </object>"
"              <packing>"
"                <property name='left_attach'>2</property>"
"                <property name='top_attach'>0</property>"
"              </packing>"
"            </child>"
"            <child>"
"              <object class='GtkTextView' id='text'>"
"                 <property name='width-request'>200</property>"
"                 <property name='height-request'>200</property>"
"                 <property name='hexpand'>True</property>"
"                 <property name='vexpand'>True</property>"
"              </object>"
"              <packing>"
"                <property name='left_attach'>1</property>"
"                <property name='top_attach'>0</property>"
"              </packing>"
"            </child>"
"          </object>"
"        </child>"
"      </object>"
"    </child>"
"  </object>"
"</interface>";

/* test that overlays can be constructed with GtkBuilder */
static GtkWidget *
test_builder (void)
{
  GtkBuilder *builder;
  GtkWidget *win;
  GError *error;

  builder = gtk_builder_new ();

  error = NULL;
  if (!gtk_builder_add_from_string (builder, buffer, -1, &error))
    {
      g_warning ("%s", error->message);
      g_error_free (error);
      return NULL;
    }

  win = (GtkWidget *)gtk_builder_get_object (builder, "window");
  g_object_ref (win);

  g_object_unref (builder);

  return win;
}

static void
on_enter (GtkEventController *controller,
          double              x,
          double              y,
          GtkWidget          *overlay)
{
  GtkWidget *child = gtk_event_controller_get_widget (controller);

  if (gtk_widget_get_halign (child) == GTK_ALIGN_START)
    gtk_widget_set_halign (child, GTK_ALIGN_END);
  else
    gtk_widget_set_halign (child, GTK_ALIGN_START);

  gtk_widget_queue_resize (overlay);
}

static GtkWidget *
test_chase (void)
{
  GtkWidget *win;
  GtkWidget *overlay;
  GtkWidget *sw;
  GtkWidget *text;
  GtkWidget *child;
  GtkTextBuffer *buffer;
  gchar *contents;
  gsize len;
  GtkEventController *controller;

  win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (win), "Chase");

  overlay = gtk_overlay_new ();
  gtk_container_add (GTK_CONTAINER (win), overlay);

  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_min_content_width (GTK_SCROLLED_WINDOW (sw), 200);
  gtk_scrolled_window_set_min_content_height (GTK_SCROLLED_WINDOW (sw), 200);
  gtk_container_add (GTK_CONTAINER (overlay), sw);

  text = gtk_text_view_new ();
  buffer = gtk_text_buffer_new (NULL);
  if (!g_file_get_contents ("testoverlay.c", &contents, &len, NULL))
    {
      contents = g_strdup ("Text should go here...");
      len = strlen (contents);
    }
  gtk_text_buffer_set_text (buffer, contents, len);
  g_free (contents);
  gtk_text_view_set_buffer (GTK_TEXT_VIEW (text), buffer);

  gtk_widget_set_hexpand (text, TRUE);
  gtk_widget_set_vexpand (text, TRUE);
  gtk_container_add (GTK_CONTAINER (sw), text);

  child = gtk_label_new ("Try to enter");
  gtk_widget_set_halign (child, GTK_ALIGN_START);
  gtk_widget_set_valign (child, GTK_ALIGN_END);
  gtk_overlay_add_overlay (GTK_OVERLAY (overlay), child);
  g_object_set (child, "margin", 4, NULL);

  controller = gtk_event_controller_motion_new ();
  g_signal_connect (controller, "enter", G_CALLBACK (on_enter), overlay);
  gtk_widget_add_controller (child, controller);

  return win;
}

static GtkWidget *
test_stacking (void)
{
  GtkWidget *win;
  GtkWidget *overlay;
  GtkWidget *main_child;
  GtkWidget *label;
  GtkWidget *child;
  GtkWidget *grid;
  GtkWidget *check1;
  GtkWidget *check2;

  win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (win), "Stacking");

  grid = gtk_grid_new ();
  overlay = gtk_overlay_new ();
  main_child = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_style_context_add_class (gtk_widget_get_style_context (main_child), "overlay-green");
  gtk_widget_set_hexpand (main_child, TRUE);
  gtk_widget_set_vexpand (main_child, TRUE);
  label = gtk_label_new ("Main child");
  child = gtk_label_new ("Overlay");
  gtk_widget_set_halign (child, GTK_ALIGN_END);
  gtk_widget_set_valign (child, GTK_ALIGN_END);

  check1 = gtk_check_button_new_with_label ("Show main");
  g_object_bind_property (main_child, "visible", check1, "active", G_BINDING_BIDIRECTIONAL);

  check2 = gtk_check_button_new_with_label ("Show overlay");
  g_object_bind_property (child, "visible", check2, "active", G_BINDING_BIDIRECTIONAL);
  gtk_container_add (GTK_CONTAINER (main_child), label);
  gtk_container_add (GTK_CONTAINER (overlay), main_child);
  gtk_overlay_add_overlay (GTK_OVERLAY (overlay), child);
  gtk_grid_attach (GTK_GRID (grid), overlay, 1, 0, 1, 3);
  gtk_container_add (GTK_CONTAINER (win), grid);

  gtk_grid_attach (GTK_GRID (grid), check1, 0, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), check2, 0, 1, 1, 1);
  child = gtk_label_new ("");
  gtk_widget_set_vexpand (child, TRUE);
  gtk_grid_attach (GTK_GRID (grid), child, 0, 2, 1, 1);

  return win;
}

static GtkWidget *
test_input_stacking (void)
{
  GtkWidget *win;
  GtkWidget *overlay;
  GtkWidget *label, *entry;
  GtkWidget *grid;
  GtkWidget *button;
  GtkWidget *vbox;
  int i,j;

  win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (win), "Input Stacking");

  overlay = gtk_overlay_new ();
  grid = gtk_grid_new ();
  gtk_container_add (GTK_CONTAINER (overlay), grid);

  for (j = 0; j < 5; j++)
    {
      for (i = 0; i < 5; i++)
	{
	  button = gtk_button_new_with_label ("     ");
	  gtk_widget_set_hexpand (button, TRUE);
	  gtk_widget_set_vexpand (button, TRUE);
	  gtk_grid_attach (GTK_GRID (grid), button, i, j, 1, 1);
	}
    }

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
  gtk_overlay_add_overlay (GTK_OVERLAY (overlay), vbox);
  gtk_widget_set_can_target (vbox, FALSE);
  gtk_widget_set_halign (vbox, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (vbox, GTK_ALIGN_CENTER);

  label = gtk_label_new ("This is some overlaid text\n"
			 "It does not get input\n"
			 "But the entry does");
  gtk_widget_set_margin_top (label, 8);
  gtk_widget_set_margin_bottom (label, 8);
  gtk_container_add (GTK_CONTAINER (vbox), label);

  entry = gtk_entry_new ();
  gtk_widget_set_margin_top (entry, 8);
  gtk_widget_set_margin_bottom (entry, 8);
  gtk_container_add (GTK_CONTAINER (vbox), entry);


  gtk_container_add (GTK_CONTAINER (win), overlay);

  return win;
}

int
main (int argc, char *argv[])
{
  GtkWidget *win1;
  GtkWidget *win2;
  GtkWidget *win3;
  GtkWidget *win4;
  GtkWidget *win5;
  GtkWidget *win6;
  GtkWidget *win7;
  GtkWidget *win8;
  GtkCssProvider *css_provider;

  gtk_init ();

  if (g_getenv ("RTL"))
    gtk_widget_set_default_direction (GTK_TEXT_DIR_RTL);

  css_provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (css_provider, css, -1);
  gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                              GTK_STYLE_PROVIDER (css_provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  win1 = test_nonzerox ();
  gtk_widget_show (win1);

  win2 = test_relative ();
  gtk_widget_show (win2);

  win3 = test_fullwidth ();
  gtk_widget_show (win3);

  win4 = test_scrolling ();
  gtk_widget_show (win4);

  win5 = test_builder ();
  gtk_widget_show (win5);

  win6 = test_chase ();
  gtk_widget_show (win6);

  win7 = test_stacking ();
  gtk_widget_show (win7);

  win8 = test_input_stacking ();
  gtk_widget_show (win8);

  gtk_main ();

  return 0;
}
