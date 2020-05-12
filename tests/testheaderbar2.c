#include <gtk/gtk.h>

static void
unset_title (GtkWidget *window)
{
  GtkWidget *box;

  g_assert (GTK_IS_WINDOW (window));

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_hide (box);

  gtk_window_set_titlebar (GTK_WINDOW (window), box);
}

static void
load_css (GtkWidget  *widget,
          const char *css)
{
  GtkCssProvider *provider;
  GtkStyleContext *context;

  context = gtk_widget_get_style_context (widget);

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (provider, css, -1);
  gtk_style_context_add_provider (context, GTK_STYLE_PROVIDER (provider), 800);
}

static void
create_regular (GtkApplication *app)
{
  GtkWidget *window, *label;

  window = gtk_application_window_new (app);
  gtk_window_set_title (GTK_WINDOW (window), "Regular window");

  label = gtk_label_new ("This window has no titlebar set");
  gtk_label_set_wrap (GTK_LABEL (label), TRUE);
  gtk_window_set_child (GTK_WINDOW (window), label);

  gtk_widget_show (window);
}

static void
create_headerbar_as_titlebar (GtkApplication *app)
{
  GtkWidget *window, *header, *label;

  window = gtk_application_window_new (app);
  gtk_window_set_title (GTK_WINDOW (window), "Headerbar as titlebar");

  header = gtk_header_bar_new ();
  gtk_header_bar_set_show_title_buttons (GTK_HEADER_BAR (header), TRUE);
  gtk_window_set_titlebar (GTK_WINDOW (window), header);

  label = gtk_label_new ("This window has a headerbar set as a titlebar");
  gtk_label_set_wrap (GTK_LABEL (label), TRUE);
  gtk_window_set_child (GTK_WINDOW (window), label);

  gtk_widget_show (window);
}

static void
create_headerbar_inside_window (GtkApplication *app)
{
  GtkWidget *window, *box, *header, *label;

  window = gtk_application_window_new (app);
  gtk_window_set_title (GTK_WINDOW (window), "Headerbar inside window");
  unset_title (window);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_window_set_child (GTK_WINDOW (window), box);

  header = gtk_header_bar_new ();
  gtk_header_bar_set_show_title_buttons (GTK_HEADER_BAR (header), TRUE);
  gtk_box_append (GTK_BOX (box), header);

  label = gtk_label_new ("This window has a headerbar inside the window and no titlebar");
  gtk_label_set_wrap (GTK_LABEL (label), TRUE);
  gtk_widget_set_vexpand (label, TRUE);
  gtk_box_append (GTK_BOX (box), label);

  gtk_widget_show (window);
}

static void
create_headerbar_overlay (GtkApplication *app)
{
  GtkWidget *window, *overlay, *sw, *box, *header, *label;

  window = gtk_application_window_new (app);
  gtk_window_set_title (GTK_WINDOW (window), "Headerbar overlaying content");
  unset_title (window);

  overlay = gtk_overlay_new ();
  gtk_window_set_child (GTK_WINDOW (window), overlay);

  header = gtk_header_bar_new ();
  gtk_header_bar_set_show_title_buttons (GTK_HEADER_BAR (header), TRUE);
  gtk_widget_set_valign (header, GTK_ALIGN_START);
  gtk_overlay_add_overlay (GTK_OVERLAY (overlay), header);
  load_css (header, "headerbar { background: alpha(shade(@theme_bg_color, .9), .8); }");

  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_widget_set_size_request (sw, 300, 250);
  gtk_overlay_set_child (GTK_OVERLAY (overlay), sw);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), box);
  gtk_widget_set_size_request (sw, 300, 250);

  label = gtk_label_new ("Lorem ipsum dolor sit amet, consectetur adipiscing elit. "
                         "Nulla innn urna ac dui malesuada ornare. Nullam dictum "
                         "tempor mi et tincidunt. Aliquam metus nulla, auctor "
                         "vitae pulvinar nec, egestas at mi. Class aptent taciti "
                         "sociosqu ad litora torquent per conubia nostra, per "
                         "inceptos himenaeos. Aliquam sagittis, tellus congue "
                         "cursus congue, diam massa mollis enim, sit amet gravida "
                         "magna turpis egestas sapien. Aenean vel molestie nunc. "
                         "In hac habitasse platea dictumst. Suspendisse lacinia"
                         "mi eu ipsum vestibulum in venenatis enim commodo. "
                         "Vivamus non malesuada ligula.");
  gtk_label_set_wrap (GTK_LABEL (label), TRUE);
  gtk_box_append (GTK_BOX (box), label);

  label = gtk_label_new ("This window has a headerbar inside an overlay, so the text is visible underneath it");
  gtk_label_set_wrap (GTK_LABEL (label), TRUE);
  gtk_widget_set_vexpand (label, TRUE);
  gtk_box_append (GTK_BOX (box), label);

  gtk_widget_show (window);
}

static void
create_hiding_headerbar (GtkApplication *app)
{
  GtkWidget *window, *box, *revealer, *header, *label, *hbox, *toggle;

  window = gtk_application_window_new (app);
  gtk_window_set_title (GTK_WINDOW (window), "Hiding headerbar");
  unset_title (window);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_window_set_child (GTK_WINDOW (window), box);

  revealer = gtk_revealer_new ();
  gtk_box_append (GTK_BOX (box), revealer);

  header = gtk_header_bar_new ();
  gtk_header_bar_set_show_title_buttons (GTK_HEADER_BAR (header), TRUE);
  gtk_revealer_set_child (GTK_REVEALER (revealer), header);

  label = gtk_label_new ("This window's headerbar can be shown and hidden with animation");
  gtk_label_set_wrap (GTK_LABEL (label), TRUE);
  gtk_widget_set_vexpand (label, TRUE);
  gtk_box_append (GTK_BOX (box), label);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_widget_set_halign (hbox, GTK_ALIGN_CENTER);
  gtk_widget_set_margin_top (hbox, 12);
  gtk_widget_set_margin_bottom (hbox, 12);
  gtk_widget_set_margin_start (hbox, 12);
  gtk_widget_set_margin_end (hbox, 12);
  gtk_box_append (GTK_BOX (box), hbox);

  toggle = gtk_switch_new ();
  gtk_switch_set_active (GTK_SWITCH (toggle), TRUE);
  gtk_box_append (GTK_BOX (hbox), toggle);
  g_object_bind_property (toggle, "active",
                          revealer, "reveal-child",
                          G_BINDING_SYNC_CREATE);

  label = gtk_label_new ("Show headerbar");
  gtk_box_append (GTK_BOX (hbox), label);

  gtk_widget_show (window);
}

static void
create_fake_headerbar (GtkApplication *app)
{
  GtkWidget *window, *handle, *box, *center_box, *controls, *label;

  window = gtk_application_window_new (app);
  gtk_window_set_title (GTK_WINDOW (window), "Fake headerbar");
  unset_title (window);

  handle = gtk_window_handle_new ();
  gtk_window_set_child (GTK_WINDOW (window), handle);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_window_handle_set_child (GTK_WINDOW_HANDLE (handle), box);

  center_box = gtk_center_box_new ();
  gtk_box_append (GTK_BOX (box), center_box);

  label = gtk_label_new ("Fake headerbar");
  gtk_center_box_set_center_widget (GTK_CENTER_BOX (center_box), label);

  controls = gtk_window_controls_new (GTK_PACK_START);
  gtk_center_box_set_start_widget (GTK_CENTER_BOX (center_box), controls);

  controls = gtk_window_controls_new (GTK_PACK_END);
  gtk_center_box_set_end_widget (GTK_CENTER_BOX (center_box), controls);

  label = gtk_label_new ("This window's titlebar is just a centerbox with a label and window controls.\nThe whole window is draggable.");
  gtk_label_set_wrap (GTK_LABEL (label), TRUE);
  gtk_widget_set_vexpand (label, TRUE);
  gtk_box_append (GTK_BOX (box), label);

  gtk_widget_show (window);
}

struct {
  const gchar *name;
  void (*cb) (GtkApplication *app);
} buttons[] =
{
    { "Regular window", create_regular },
    { "Headerbar as titlebar", create_headerbar_as_titlebar },
    { "Headerbar inside window", create_headerbar_inside_window },
    { "Headerbar overlaying content", create_headerbar_overlay },
    { "Hiding headerbar", create_hiding_headerbar },
    { "Fake headerbar", create_fake_headerbar },
};
int n_buttons = sizeof (buttons) / sizeof (buttons[0]);

static void
app_activate_cb (GtkApplication *app)
{
  GtkWidget *window, *box;
  int i;

  window = gtk_application_window_new (app);
  gtk_window_set_title (GTK_WINDOW (window), "Headerbar test");

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_set_halign (box, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (box, GTK_ALIGN_CENTER);
  gtk_widget_add_css_class (box, "linked");
  gtk_window_set_child (GTK_WINDOW (window), box);

  for (i = 0; i < n_buttons; i++)
    {
      GtkWidget *btn;

      btn = gtk_button_new_with_label (buttons[i].name);
      g_signal_connect_object (btn,
                               "clicked",
                               G_CALLBACK (buttons[i].cb),
                               app,
                               G_CONNECT_SWAPPED);
      gtk_box_append (GTK_BOX (box), btn);
    }

  gtk_widget_show (window);
}

int
main (int    argc,
      char **argv)
{
  GtkApplication *app;

  app = gtk_application_new ("org.gtk.Test.headerbar2", 0);

  g_signal_connect (app,
                    "activate",
                    G_CALLBACK (app_activate_cb),
                    NULL);

  g_application_run (G_APPLICATION (app), argc, argv);

  return 0;
}
