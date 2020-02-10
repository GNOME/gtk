#include <gtk/gtk.h>
#include <glib/gstdio.h>

static GtkWidget *header_stack;
static GtkWidget *page_stack;

void
back_to_main (GtkButton *button)
{
  gtk_stack_set_visible_child_name (GTK_STACK (header_stack), "main");
  gtk_stack_set_visible_child_name (GTK_STACK (page_stack), "page1");
}

void
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

#ifdef GTK_SRCDIR
  g_chdir (GTK_SRCDIR);
#endif

  gtk_init ();

  builder = gtk_builder_new ();
  gtk_builder_add_from_file (builder, "teststackedheaders.ui", NULL);

  win = (GtkWidget *)gtk_builder_get_object (builder, "window");
  header_stack = (GtkWidget *)gtk_builder_get_object (builder, "header_stack");
  page_stack = (GtkWidget *)gtk_builder_get_object (builder, "page_stack");

  gtk_window_present (GTK_WINDOW (win));

  while (TRUE)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
