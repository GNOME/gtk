
#include<gtk/gtk.h>

/*#define COLOR " #0f0;"*/
#define COLOR " red;"

static const char *css =
".one {"
"  all: unset;"
"  min-width: 100px;"
"  min-height:100px;"
"  box-shadow: -10px -20px 5px 40px" COLOR
"}"
".two {"
"  all: unset;"
"  min-width: 100px;"
"  min-height:100px;"
"  box-shadow: -10px -20px 0px 40px" COLOR
"}"
".three {"
"  all: unset;"
"  min-width: 100px;"
"  min-height:100px;"
"  border-radius: 0px;"
"  box-shadow: 0px 0px 10px 20px" COLOR
"}"
".four {"
"  all: unset;"
"  min-width: 100px;"
"  min-height: 100px;"
"  box-shadow: 10px 20px 5px 40px" COLOR
"  border-radius: 30px; "
"  margin-right: 50px;"
"}"
".five {"
"  all: unset;"
"  min-width: 100px;"
"  min-height:100px;"
"  border-radius: 30px; "
"  box-shadow: 10px 20px 0px 40px" COLOR
"}"
/* This is the default CSD drop shadow from (current) Adwaita */
".b1 {"
"  all: unset;"
"  min-width: 100px;"
"  min-height: 100px;"
"  border-radius: 7px 7px 0px 0px;"
"  box-shadow: 0px 0px 9px 0px rgba(0, 0, 0, 0.5);"
"}"
#if 0
".b2 {"
"  all: unset;"
"  min-width: 100px;"
"  min-height:100px;"
"  border-radius: 7px 7px 0 0;"
"  box-shadow: 0 0 0 30px green;"
"}"
".b3 {"
"  all: unset;"
"  min-width: 100px;"
"  min-height:100px;"
"  border-right: 30px solid blue;"
"  border-radius: 40px;"
"}"
".b4 {"
"  all: unset;"
"  min-width: 100px;"
"  min-height:100px;"
"  border-bottom: 30px solid blue;"
"  border-radius: 40px;"
"}"
#endif
""
;

static void
quit_cb (GtkWidget *widget,
         gpointer   data)
{
  gboolean *done = data;

  *done = TRUE;

  g_main_context_wakeup (NULL);
}

int
main (int argc, char **argv)
{
  GtkWidget *window;
  GtkWidget *box;
  GtkWidget *top;
  GtkWidget *bottom;
  GtkWidget *w;
  GtkCssProvider *provider;
  gboolean done = FALSE;

  gtk_init ();

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (provider, css, -1);
  gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                              GTK_STYLE_PROVIDER (provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  window = gtk_window_new ();
  gtk_window_set_decorated (GTK_WINDOW (window), FALSE);
  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 120);
  top = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 120);
  bottom = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 120);
  g_object_set (box, "margin", 120, NULL);

  w = gtk_button_new ();
  gtk_widget_set_valign (w, GTK_ALIGN_CENTER);
  gtk_style_context_add_class (gtk_widget_get_style_context (w), "one");
  gtk_container_add (GTK_CONTAINER (top), w);

  w = gtk_button_new ();
  gtk_widget_set_valign (w, GTK_ALIGN_CENTER);
  gtk_style_context_add_class (gtk_widget_get_style_context (w), "two");
  gtk_container_add (GTK_CONTAINER (top), w);

  w = gtk_button_new ();
  gtk_widget_set_valign (w, GTK_ALIGN_CENTER);
  gtk_style_context_add_class (gtk_widget_get_style_context (w), "three");
  gtk_widget_set_opacity (w, 0.7);
  gtk_container_add (GTK_CONTAINER (top), w);

  w = gtk_button_new ();
  gtk_widget_set_valign (w, GTK_ALIGN_CENTER);
  gtk_style_context_add_class (gtk_widget_get_style_context (w), "four");
  gtk_container_add (GTK_CONTAINER (top), w);

  w = gtk_button_new ();
  gtk_widget_set_valign (w, GTK_ALIGN_CENTER);
  gtk_style_context_add_class (gtk_widget_get_style_context (w), "five");
  gtk_container_add (GTK_CONTAINER (top), w);

  /* Bottom */
  w = gtk_button_new ();
  gtk_widget_set_valign (w, GTK_ALIGN_CENTER);
  gtk_style_context_add_class (gtk_widget_get_style_context (w), "b1");
  gtk_container_add (GTK_CONTAINER (bottom), w);

  /*w = gtk_button_new ();*/
  /*gtk_widget_set_valign (w, GTK_ALIGN_CENTER);*/
  /*gtk_style_context_add_class (gtk_widget_get_style_context (w), "b2");*/
  /*gtk_container_add (GTK_CONTAINER (bottom), w);*/

  /*w = gtk_button_new ();*/
  /*gtk_widget_set_valign (w, GTK_ALIGN_CENTER);*/
  /*gtk_style_context_add_class (gtk_widget_get_style_context (w), "b3");*/
  /*gtk_container_add (GTK_CONTAINER (bottom), w);*/

  /*w = gtk_button_new ();*/
  /*gtk_widget_set_valign (w, GTK_ALIGN_CENTER);*/
  /*gtk_style_context_add_class (gtk_widget_get_style_context (w), "b4");*/
  /*gtk_container_add (GTK_CONTAINER (bottom), w);*/

  gtk_container_add (GTK_CONTAINER (box), top);
  gtk_container_add (GTK_CONTAINER (box), bottom);
  gtk_container_add (GTK_CONTAINER (window), box);
  g_signal_connect (window, "destroy", G_CALLBACK (quit_cb), &done);
  gtk_widget_show (window);

  while (!done)
    g_main_context_iteration (NULL, TRUE);
}
