#include <gtk/gtk.h>

static void
check_finalized (gpointer data,
                 GObject *where_the_object_was)
{
  gboolean *did_finalize = (gboolean *)data;

  *did_finalize = TRUE;
}

static void
popover (void)
{
  GtkWidget *button = gtk_button_new_with_label ("Label");
  GtkWidget *p = gtk_popover_new (button);
  gboolean finalized = FALSE;

  /* GtkButton is a normal widget and thus floating */
  g_assert (g_object_is_floating (button));
  /* GtkPopver sinks itself */
  g_assert (!g_object_is_floating (p));

  g_object_weak_ref (G_OBJECT (p), check_finalized, &finalized);

  g_object_ref_sink (button);
  g_object_unref (button);
  /* We do NOT unref p since the only reference held to it gets
   * removed when the button gets disposed. */
  g_assert (finalized);
}

static void
popover2 (void)
{
  GtkWidget *button = gtk_button_new_with_label ("Label");
  GtkWidget *p = gtk_popover_new (button);
  gboolean finalized = FALSE;

  g_assert (g_object_is_floating (button));
  g_assert (!g_object_is_floating (p));

  g_object_weak_ref (G_OBJECT (p), check_finalized, &finalized);

  g_object_ref_sink (button);

  /* Explicitly set relative-to to NULL, causing the popover to release its internal
   * reference to itself. */
  gtk_popover_set_relative_to (GTK_POPOVER (p), NULL);
  g_assert (finalized);

  g_object_unref (button);
}

static void
filechooserwidget (void)
{
  /* We use GtkFileChooserWidget simply because it's a complex widget, that's it. */
  GtkWidget *w = gtk_file_chooser_widget_new (GTK_FILE_CHOOSER_ACTION_OPEN);
  gboolean finalized = FALSE;

  g_assert (g_object_is_floating (w));
  g_object_ref_sink (w);
  g_object_weak_ref (G_OBJECT (w), check_finalized, &finalized);

  g_object_unref (w);

  g_assert (finalized);
}

static void
window (void)
{
  GtkWidget *w = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gboolean finalized = FALSE;

  /* GTK holds a ref */
  g_assert (!g_object_is_floating (w));
  g_object_weak_ref (G_OBJECT (w), check_finalized, &finalized);

  gtk_window_destroy (GTK_WINDOW (w));

  g_assert (finalized);
}

static void
menu (void)
{
  GtkWidget *m = gtk_menu_new ();
  gboolean finalized = FALSE;

  /* GtkMenu is not actually a toplevel, but it has one. */
  g_assert (g_object_is_floating (m));
  g_object_weak_ref (G_OBJECT (m), check_finalized, &finalized);

  g_object_ref_sink (m);

  g_object_unref (m);
  g_assert (finalized);
}

static void
detached_cb (GtkWidget *attach_widget,
             GtkMenu   *menu)
{
  gboolean *detached = g_object_get_data (G_OBJECT (menu), "detached-address");

  g_assert_nonnull (detached);

  *detached = TRUE;
}

static void
menu2 (void)
{
  GtkWidget *m = gtk_menu_new ();
  GtkWidget *button = gtk_button_new_with_label ("Label");
  gboolean finalized = FALSE;
  gboolean detached = FALSE;

  /* GtkMenu is not actually a toplevel, but it has one. */
  g_assert (g_object_is_floating (m));
  g_object_weak_ref (G_OBJECT (m), check_finalized, &finalized);

  g_object_ref_sink (m);

  g_object_set_data (G_OBJECT (m), "detached-address", &detached);
  gtk_menu_attach_to_widget (GTK_MENU (m), button, detached_cb);

  /* Finalize by unref after detach! */
  gtk_menu_detach (GTK_MENU (m));
  g_assert (detached);

  g_object_unref (m);
  g_assert (finalized);
}

static void
menu3 (void)
{
  GtkWidget *m = gtk_menu_new ();
  GtkWidget *button = gtk_button_new_with_label ("Label");
  gboolean finalized = FALSE;
  gboolean detached = FALSE;

  /* GtkMenu is not actually a toplevel, but it has one. */
  g_assert (g_object_is_floating (m));
  g_object_weak_ref (G_OBJECT (m), check_finalized, &finalized);
  /* NO ref_sink of the menu! */

  g_object_set_data (G_OBJECT (m), "detached-address", &detached);
  gtk_menu_attach_to_widget (GTK_MENU (m), button, detached_cb);

  /* Finalize by detach! */
  gtk_menu_detach (GTK_MENU (m));
  g_assert (finalized);
  g_assert (detached);
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);
  gtk_init ();

  g_test_add_func ("/gtk/widget-refcount/popover", popover);
  g_test_add_func ("/gtk/widget-refcount/popover2", popover2);
  g_test_add_func ("/gtk/widget-refcount/filechoosewidget", filechooserwidget);
  g_test_add_func ("/gtk/widget-refcount/window", window);
  g_test_add_func ("/gtk/widget-refcount/menu", menu);
  g_test_add_func ("/gtk/widget-refcount/menu2", menu2);
  g_test_add_func ("/gtk/widget-refcount/menu3", menu3);

  return g_test_run ();
}
