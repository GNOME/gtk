#include <gtk/gtk.h>

static GtkWidget *header_stack;
static GtkWidget *page_stack;

static void
back_to_main (GtkButton *button)
{
  gtk_stack_set_visible_child_name (GTK_STACK (header_stack), "main");
  gtk_stack_set_visible_child_name (GTK_STACK (page_stack), "page1");
}

static void
go_to_secondary (GtkButton *button)
{
  gtk_stack_set_visible_child_name (GTK_STACK (header_stack), "secondary");
  gtk_stack_set_visible_child_name (GTK_STACK (page_stack), "secondary");
}

int
main (int argc, char *argv[])
{
  GtkBuilder *builder;
  GtkWidget *win;

  gtk_init (NULL, NULL);

  builder = gtk_builder_new ();
  gtk_builder_add_callback_symbol (builder, "back_to_main", G_CALLBACK (back_to_main));
  gtk_builder_add_callback_symbol (builder, "go_to_secondary", G_CALLBACK (go_to_secondary));
  gtk_builder_add_from_file (builder, "teststackedheaders.ui", NULL);
  gtk_builder_connect_signals (builder, NULL);

  win = (GtkWidget *)gtk_builder_get_object (builder, "window");
  header_stack = (GtkWidget *)gtk_builder_get_object (builder, "header_stack");
  page_stack = (GtkWidget *)gtk_builder_get_object (builder, "page_stack");

  gtk_window_present (GTK_WINDOW (win));

  gtk_main ();

  return 0;
}
