#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkinternals.h>
#include <stdio.h>

void
hello (GtkWidget *button, char *label)
{
  g_print ("Click from %s\n", label);
}

int
main (int argc, char *argv[])
{
  GtkWidget **window;
  GtkWidget **button;
  gint num_screen = 0;
  gchar *displayname = NULL;
  gint i;
  GdkScreen **screen_list;
  GdkDisplay *dpy;
  gtk_init (&argc, &argv);

  dpy = gdk_display_manager_get_default(gdk_get_display_manager ());
  num_screen = gdk_display_get_n_screens(dpy);
  displayname = gdk_display_get_name(dpy);
  if(num_screen <= 1)
  {
    printf("This Xserver (%s) manages only one screen. exiting...\n", 
	   displayname);
    exit(1);
  }else{
    printf("This Xserver (%s) manages %d screens.\n", displayname, num_screen);
  }
  screen_list = g_new(GdkScreen *,num_screen);
  window = g_new(GtkWidget *,num_screen);
  button = g_new(GtkWidget *,num_screen);
  
  for(i=0 ;i < num_screen ; i++)
  {
    char *label = g_new(char,10);
    screen_list[i] = gdk_display_get_screen(dpy,i);
    sprintf(label,"Screen %d",i);
    
    window[i] = g_object_connect (gtk_widget_new (gtk_window_get_type (),
						  "screen", screen_list[i],
						  "user_data", NULL,
						  "type", GTK_WINDOW_TOPLEVEL,
						  "title", label, 
						  "allow_grow", FALSE,
						  "allow_shrink", FALSE,
						  "border_width", 10,
						  NULL),
				   "signal::destroy", gtk_main_quit, NULL,
				   NULL);
    button[i] = g_object_connect (gtk_widget_new (gtk_button_get_type (),
                                             "GtkButton::label", label,
                                             "GtkWidget::parent", window[i],
                                             "GtkWidget::visible", TRUE,
                                             NULL),
                             "signal::clicked", hello, label,
                             NULL);
  }
  for(i=0;i<num_screen;i++)
    gtk_widget_show (window[i]);

  gtk_main ();

  return 0;
}
