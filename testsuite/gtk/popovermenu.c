#include <gtk/gtk.h>

static const char menu[] =
"<interface>"
  "<menu id=\"menu1\">"
    "<item>"
      "<attribute name=\"label\">Record events</attribute>"
      "<attribute name=\"action\">record.record-events</attribute>"
    "</item>"
  "</menu>"
  "<menu id=\"menu2\">"
    "<item>"
      "<attribute name=\"label\">Do not record events</attribute>"
      "<attribute name=\"action\">record.no-record-events</attribute>"
    "</item>"
  "</menu>"
"</interface>";

static void
test_set_model (void)
{
  GMenuModel *menu1;
  GMenuModel *menu2;
  GtkWidget *popover;
  GtkBuilder *builder;

  builder = gtk_builder_new_from_string (menu, -1);
  menu1 = G_MENU_MODEL (gtk_builder_get_object (builder, "menu1"));
  menu2 = G_MENU_MODEL (gtk_builder_get_object (builder, "menu2"));

  popover = gtk_popover_menu_new_from_model (menu1);

  gtk_popover_menu_set_menu_model (GTK_POPOVER_MENU (popover), NULL);

  gtk_popover_set_position (GTK_POPOVER (popover), GTK_POS_LEFT);

  gtk_popover_menu_set_menu_model (GTK_POPOVER_MENU (popover), menu2);

  gtk_popover_set_position (GTK_POPOVER (popover), GTK_POS_BOTTOM);

  g_object_ref_sink (popover);
  g_object_unref (popover);

  g_object_unref (builder);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv);

  g_test_add_func ("/popover/menu/set-model", test_set_model);

  return g_test_run ();
}
