
#include<gtk/gtk.h>

static const char *css =
".one {"
"  all: unset;"
"  min-width: 100px;"
"  min-height:100px;"
"   border-left:   50px solid #0f0;"
"   border-top:    10px solid red;"
"   border-bottom: 50px solid teal;"
"   border-right:  100px solid pink;"
"   border-radius: 100px;"
"}"
".two {"
"  all: unset;"
"  min-width: 100px;"
"  min-height:100px;"
"   border-left:   50px solid #0f0;"
"   border-top:    10px solid red;"
"   border-bottom: 50px solid teal;"
"   border-right:  100px solid pink;"
"   border-radius: 50%;"
"}"
".three {"
"  all: unset;"
"  min-width: 100px;"
"  min-height:100px;"
"   border-left:   50px solid #0f0;"
"   border-top:    10px solid red;"
"   border-bottom: 50px solid teal;"
"   border-right:  100px solid pink;"
"   border-radius: 0px;"
"}"
".four {"
"  all: unset;"
"  min-width: 100px;"
"  min-height:100px;"
"  border: 10px solid black;"
"  border-radius: 999px;"
"}"
".five {"
"  all: unset;"
"  min-width: 100px;"
"  min-height:100px;"
"  border: 30px solid black;"
"  border-radius: 0px;"
"}"
".b1 {"
"  all: unset;"
"  min-width: 100px;"
"  min-height:100px;"
"  border-top: 30px solid black;"
"  border-radius: 0px;"
"}"
".b2 {"
"  all: unset;"
"  min-width: 100px;"
"  min-height:100px;"
"  border-bottom: 30px solid black;"
"  border-radius: 0px;"
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
;


int
main (int argc, char **argv)
{
  GtkWidget *window;
  GtkWidget *box;
  GtkWidget *top;
  GtkWidget *bottom;
  GtkWidget *w;
  GtkCssProvider *provider;

  gtk_init ();

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (provider, css, -1);
  gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                              GTK_STYLE_PROVIDER (provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 40);
  top = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 40);
  bottom = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 40);
  g_object_set (box, "margin", 40, NULL);

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

  w = gtk_button_new ();
  gtk_widget_set_valign (w, GTK_ALIGN_CENTER);
  gtk_style_context_add_class (gtk_widget_get_style_context (w), "b2");
  gtk_container_add (GTK_CONTAINER (bottom), w);

  w = gtk_button_new ();
  gtk_widget_set_valign (w, GTK_ALIGN_CENTER);
  gtk_style_context_add_class (gtk_widget_get_style_context (w), "b3");
  gtk_container_add (GTK_CONTAINER (bottom), w);

  w = gtk_button_new ();
  gtk_widget_set_valign (w, GTK_ALIGN_CENTER);
  gtk_style_context_add_class (gtk_widget_get_style_context (w), "b4");
  gtk_container_add (GTK_CONTAINER (bottom), w);

  gtk_container_add (GTK_CONTAINER (box), top);
  gtk_container_add (GTK_CONTAINER (box), bottom);
  gtk_container_add (GTK_CONTAINER (window), box);
  g_signal_connect (window, "destroy", gtk_main_quit, NULL);
  gtk_widget_show (window);

  gtk_main ();

  gtk_widget_destroy (window);
}
