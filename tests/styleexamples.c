#include <string.h>
#include <gtk/gtk.h>

static gboolean
draw_cb_checks (GtkWidget *widget, cairo_t *cr)
{
  GtkStyleContext *context;

  context = gtk_widget_get_style_context (widget);

  gtk_style_context_save (context);

  gtk_style_context_add_class (context, "check");
  gtk_style_context_set_state (context, 0);
  gtk_render_check (context, cr, 12, 12, 12, 12);
  gtk_style_context_set_state (context, GTK_STATE_FLAG_ACTIVE);
  gtk_render_check (context, cr, 36, 12, 12, 12);
  gtk_style_context_set_state (context, GTK_STATE_FLAG_INCONSISTENT);
  gtk_render_check (context, cr, 60, 12, 12, 12);
  gtk_style_context_set_state (context, GTK_STATE_FLAG_INSENSITIVE);
  gtk_render_check (context, cr, 84, 12, 12, 12);

  gtk_style_context_restore (context);

  return TRUE;
}


static gboolean
draw_cb_options (GtkWidget *widget, cairo_t *cr)
{
  GtkStyleContext *context;

  context = gtk_widget_get_style_context (widget);

  gtk_style_context_save (context);

  gtk_style_context_add_class (context, "radio");
  gtk_style_context_set_state (context, 0);
  gtk_render_option (context, cr, 12, 12, 12, 12);
  gtk_style_context_set_state (context, GTK_STATE_FLAG_ACTIVE);
  gtk_render_option (context, cr, 36, 12, 12, 12);
  gtk_style_context_set_state (context, GTK_STATE_FLAG_INCONSISTENT);
  gtk_render_option (context, cr, 60, 12, 12, 12);
  gtk_style_context_set_state (context, GTK_STATE_FLAG_INSENSITIVE);
  gtk_render_option (context, cr, 84, 12, 12, 12);

  gtk_style_context_restore (context);

  return TRUE;
}

static gboolean
draw_cb_arrows (GtkWidget *widget, cairo_t *cr)
{
  GtkStyleContext *context;

  context = gtk_widget_get_style_context (widget);

  gtk_style_context_save (context);

  gtk_style_context_set_state (context, 0);
  gtk_render_arrow (context, cr, 0,        12, 12, 12);
  gtk_render_arrow (context, cr, G_PI/2,   36, 12, 12);
  gtk_render_arrow (context, cr, G_PI,     60, 12, 12);
  gtk_render_arrow (context, cr, G_PI*3/2, 84, 12, 12);

  gtk_style_context_restore (context);

  return TRUE;
}

static gboolean
draw_cb_expanders (GtkWidget *widget, cairo_t *cr)
{
  GtkStyleContext *context;

  context = gtk_widget_get_style_context (widget);

  gtk_style_context_save (context);

  gtk_style_context_add_class (context, "expander");
  gtk_style_context_set_state (context, 0);
  gtk_render_expander (context, cr, 12, 12, 12, 12);
  gtk_style_context_set_state (context, GTK_STATE_FLAG_PRELIGHT);
  gtk_render_expander (context, cr, 36, 12, 12, 12);
  gtk_style_context_set_state (context, GTK_STATE_FLAG_ACTIVE);
  gtk_render_expander (context, cr, 60, 12, 12, 12);
  gtk_style_context_set_state (context, GTK_STATE_FLAG_PRELIGHT | GTK_STATE_FLAG_ACTIVE);
  gtk_render_expander (context, cr, 84, 12, 12, 12);

  gtk_style_context_restore (context);

  return TRUE;
}

static gboolean
draw_cb_background (GtkWidget *widget, cairo_t *cr)
{
  GtkStyleProvider *provider;
  GtkStyleContext *context;

  context = gtk_widget_get_style_context (widget);

  gtk_style_context_save (context);

  provider = (GtkStyleProvider *)gtk_css_provider_new ();
  gtk_css_provider_load_from_data (GTK_CSS_PROVIDER (provider),
                                   "* {\n"
                                   "   border-radius: 10;\n"
                                   "   border-width: 0;\n"
                                   "   background-image: -gtk-gradient (linear, left top, right bottom, from(#ff00ff), to(#aabbcc));\n"
                                   "}\n", -1, NULL);
  gtk_style_context_add_provider (context, provider, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  gtk_style_context_set_junction_sides (context, 0);
  gtk_render_background (context, cr, 12, 12, 100, 100);
  gtk_style_context_remove_provider (context, provider);
  gtk_style_context_restore (context);

  return TRUE;
}

static gboolean
draw_cb_frame (GtkWidget *widget, cairo_t *cr)
{
  GtkStyleProvider *provider;
  GtkStyleContext *context;

  context = gtk_widget_get_style_context (widget);

  gtk_style_context_save (context);

  provider = (GtkStyleProvider *)gtk_css_provider_new ();
  gtk_css_provider_load_from_data (GTK_CSS_PROVIDER (provider),
                                   ".frame {\n"
                                   "   border-image: url('gradient1.png') 10 10 10 10 stretch;\n"
                                   "   border-style; solid\n"
                                   "}\n", -1, NULL);
  gtk_style_context_add_provider (context, provider, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  gtk_style_context_set_junction_sides (context, 0);
  gtk_render_frame (context, cr, 12, 12, 25, 25);
  gtk_style_context_remove_provider (context, provider);
  gtk_style_context_restore (context);

  return TRUE;
}

static char *what;

static gboolean
draw_cb (GtkWidget *widget, cairo_t *cr)
{
  if (strcmp (what, "check") == 0)
    return draw_cb_checks (widget, cr);
  else if (strcmp (what, "option") == 0)
    return draw_cb_options (widget, cr);
  else if (strcmp (what, "arrow") == 0)
    return draw_cb_arrows (widget, cr);
  else if (strcmp (what, "expander") == 0)
    return draw_cb_expanders (widget, cr);
  else if (strcmp (what, "background") == 0)
    return draw_cb_background (widget, cr);
  else if (strcmp (what, "frame") == 0)
    return draw_cb_frame (widget, cr);

  return FALSE;
}

int main (int argc, char *argv[])
{
  GtkWidget *window;
  GtkWidget *ebox;

  gtk_init (&argc, &argv);

  if (argc > 1)
    what = argv[1];
  else
    what = "check";

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_has_resize_grip (GTK_WINDOW (window), FALSE);
  ebox = gtk_event_box_new ();
  gtk_event_box_set_visible_window (GTK_EVENT_BOX (ebox), TRUE);
  gtk_container_add (GTK_CONTAINER (window), ebox);
  gtk_widget_set_name (ebox, "ebox");
  g_signal_connect_after (ebox, "draw", G_CALLBACK (draw_cb), NULL);

  gtk_widget_show_all (window);

  gtk_main ();

  return 0;
}

