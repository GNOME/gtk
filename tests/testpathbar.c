#include "config.h"
#include "glib.h"
#include <gtk/gtk.h>

static GActionGroup *action_group;
static GtkWidget *path_bar;
static GtkWidget *path_bar_inverted;
static GtkWidget *path_bar_slash;
static GtkWidget *path_bar_custom_root_label;
static GtkWidget *path_bar_custom_root_icon;
static GtkWidget *files_path_bar_random;
static GtkWidget *files_path_bar_recent;
static const gchar* REAL_LOCATION_RANDOM = "file:///boot/efi/EFI/BOOT";
static const gchar* REAL_LOCATION_RECENT = "recent:///";
static const gchar* ORIGINAL_PATH = "/test/test 2/test 3/asda lkasdl/pppppppppppppppp/alskd/t/t/test3/tttttt/tast";
static const gchar* ROOT_PATH = "/test/test 2/test 3";
static const gchar* DISPLAY_PATH = "/test/test 2/This Is A Root/asda lkasdl/pppppppppppppppp/ alskd";

static void
action_menu_1 (GSimpleAction *action,
               GVariant      *variant,
               gpointer       user_data)
{
  g_print ("Menu 1 action\n");
}

static void
action_menu_2 (GSimpleAction *action,
               GVariant      *variant,
               gpointer       user_data)
{
  g_print ("Menu 2 action\n");
}

static void
action_special (GSimpleAction *action,
                GVariant      *variant,
                gpointer       user_data)
{
  g_print ("Special action\n");
}

const GActionEntry entries[] = {
  { "menu_1",  action_menu_1 },
  { "menu_2",  action_menu_2 },
  { "special", action_special },
};

static void
on_populate_popup (GtkPathBar  *path_bar,
                   GtkWidget   *container,
                   const gchar *selected_path)
{
  GtkWidget *menu_item;

  menu_item = gtk_model_button_new ();
  gtk_actionable_set_action_name (GTK_ACTIONABLE (menu_item),
                                  "action_group.menu_1");
  g_object_set (menu_item, "text", "Menu 1", NULL);
  gtk_container_add (GTK_CONTAINER (container), menu_item);

  menu_item = gtk_model_button_new ();
  gtk_actionable_set_action_name (GTK_ACTIONABLE (menu_item),
                                  "action_group.menu_2");
  g_object_set (menu_item, "text", "Menu 2", NULL);
  gtk_container_add (GTK_CONTAINER (container), menu_item);

  if (g_strcmp0 (selected_path, "/test/test 2/test 3") == 0)
    {
      menu_item = gtk_model_button_new ();
      gtk_actionable_set_action_name (GTK_ACTIONABLE (menu_item),
                                      "action_group.special");
      g_object_set (menu_item, "text", "Special", NULL);
      gtk_container_add (GTK_CONTAINER (container), menu_item);
    }

  gtk_widget_show_all (container);

  g_print ("Populate popup\n");
}

static void
on_path_selected (GtkPathBar *path_bar,
                  GParamSpec *pspec,
                  gpointer   *user_data)
{
  g_print ("Path selected: %s\n", gtk_path_bar_get_selected_path (path_bar));
}

static gchar*
get_display_path_from_selected (const gchar *selected_path)
{
  gchar **splitted_path;
  gchar **display_splitted_path;
  gint i;
  GString *display_path;
  gchar *display_path_gchar;

  splitted_path = g_strsplit (selected_path, "/", -1);
  display_splitted_path = g_strsplit (DISPLAY_PATH, "/", -1);
  display_path = g_string_new ("");
  /* Skip the first empty split part */
  for (i = 1; i < g_strv_length (splitted_path); i++)
    {
      g_string_append (display_path, "/");
      g_string_append (display_path, display_splitted_path[i]);
    }

  display_path_gchar = display_path->str;

  g_string_free (display_path, FALSE);
  g_strfreev (splitted_path);
  g_strfreev (display_splitted_path);

  return display_path_gchar;
}

static void
on_path_selected_set_path (GtkPathBar *path_bar,
                           GParamSpec *pspec,
                           gpointer   *user_data)
{
  gchar *selected_path;
  gchar *new_display_path;

  selected_path = g_strdup (gtk_path_bar_get_selected_path (path_bar));
  new_display_path = get_display_path_from_selected (selected_path);
  g_print ("Path selected: %s, setting path to GtkPathBar and new display path %s\n", selected_path, new_display_path);
  if (path_bar == GTK_PATH_BAR (path_bar_custom_root_label))
    {
      gtk_path_bar_set_path_extended (GTK_PATH_BAR (path_bar_custom_root_label),
                                      selected_path, ROOT_PATH, "This Is A Root", NULL);
    }
  else if (path_bar == GTK_PATH_BAR (path_bar_custom_root_icon))
    {
      GIcon *icon;

      icon = g_themed_icon_new ("drive-harddisk");
      gtk_path_bar_set_path_extended (GTK_PATH_BAR (path_bar_custom_root_icon),
                                      selected_path, "/", NULL, icon);
      g_object_unref (icon);
    }
  else
    {
      gtk_path_bar_set_path (path_bar, selected_path);
    }

  g_free (selected_path);
  g_free (new_display_path);
}

static void
on_file_changed (GtkFilesPathBar *path_bar,
                 GParamSpec      *pspec,
                 gpointer        *user_data)
{
  GFile *file;
  gchar *uri;

  file = gtk_files_path_bar_get_file (path_bar);
  uri = g_file_get_uri (file);
  g_print ("File selected: %s in GtkFilesPathBar\n", uri);
  g_free (uri);
}


static void
connect_path_bar (GtkPathBar *path_bar)
{
  g_signal_connect (GTK_PATH_BAR (path_bar), "populate-popup",
                    G_CALLBACK (on_populate_popup), NULL);
  g_signal_connect (GTK_PATH_BAR (path_bar), "notify::selected-path",
                    G_CALLBACK (on_path_selected), NULL);
}

static void
connect_path_bar_set_path (GtkPathBar *path_bar)
{
  g_signal_connect (GTK_PATH_BAR (path_bar), "populate-popup",
                    G_CALLBACK (on_populate_popup), NULL);
  g_signal_connect (GTK_PATH_BAR (path_bar), "notify::selected-path",
                    G_CALLBACK (on_path_selected_set_path), NULL);
}

static void
connect_files_path_bar (GtkFilesPathBar *files_path_bar)
{
  g_signal_connect (GTK_FILES_PATH_BAR (files_path_bar), "populate-popup",
                    G_CALLBACK (on_populate_popup), NULL);
  g_signal_connect (GTK_FILES_PATH_BAR (files_path_bar), "notify::file",
                    G_CALLBACK (on_file_changed), NULL);
}

static void
on_reset_button_clicked (GtkButton *reset_button)
{
  GFile *file;
  GIcon *icon;

  gtk_path_bar_set_path (GTK_PATH_BAR (path_bar), ORIGINAL_PATH);
  gtk_path_bar_set_path (GTK_PATH_BAR (path_bar_inverted), ORIGINAL_PATH);
  gtk_path_bar_set_path (GTK_PATH_BAR (path_bar_slash), "/");
  gtk_path_bar_set_path_extended (GTK_PATH_BAR (path_bar_custom_root_label),
                                  ORIGINAL_PATH, ROOT_PATH, "This Is A Root", NULL);
  icon = g_themed_icon_new ("drive-harddisk");
  gtk_path_bar_set_path_extended (GTK_PATH_BAR (path_bar_custom_root_icon),
                                  ORIGINAL_PATH, "/", NULL, icon);
  g_object_unref (icon);

  file = g_file_new_for_uri (REAL_LOCATION_RANDOM);
  gtk_files_path_bar_set_file (GTK_FILES_PATH_BAR (files_path_bar_random), file);
  g_object_unref (file);
  file = g_file_new_for_uri (REAL_LOCATION_RECENT);
  gtk_files_path_bar_set_file (GTK_FILES_PATH_BAR (files_path_bar_recent), file);
  g_object_unref (file);

}

int
main (int argc, char *argv[])
{
  GtkWidget *window;
  GtkWidget *grid;
  GtkWidget *reset_button;
  GtkWidget *label;
  GFile *file = NULL;
  GIcon *icon;

  gtk_init (&argc, &argv);

  window = g_object_connect (g_object_new (gtk_window_get_type (),
                                           "type", GTK_WINDOW_TOPLEVEL,
                                           "title", "Test path bar",
                                           "resizable", TRUE,
                                           "default-height", 200,
                                           NULL),
                             "signal::destroy", gtk_main_quit, NULL,
                             NULL);

  action_group = G_ACTION_GROUP (g_simple_action_group_new ());
  g_action_map_add_action_entries (G_ACTION_MAP (action_group), entries,
                                   G_N_ELEMENTS (entries), window);

  gtk_widget_insert_action_group (window, "action_group", action_group);

  grid = gtk_grid_new ();
  g_type_ensure (GTK_TYPE_PATH_BAR);

  label = gtk_label_new ("Generic GtkPathBar tests");
  gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 2, 1);

  /* ----------------------------------------------------------------------- */
  path_bar = gtk_path_bar_new ();
  gtk_grid_attach (GTK_GRID (grid), path_bar, 0, 1, 1, 1);
  gtk_path_bar_set_path (GTK_PATH_BAR (path_bar), ORIGINAL_PATH);
  connect_path_bar (GTK_PATH_BAR (path_bar));

#if 0
  /* ----------------------------------------------------------------------- */
  path_bar_inverted = gtk_path_bar_new ();
  gtk_path_bar_set_inverted (GTK_PATH_BAR (path_bar_inverted), TRUE);
  gtk_path_bar_set_path (GTK_PATH_BAR (path_bar_inverted), ORIGINAL_PATH);
  connect_path_bar (GTK_PATH_BAR (path_bar_inverted));
  gtk_grid_attach (GTK_GRID (grid), path_bar_inverted, 0, 2, 1, 1);

  label = gtk_label_new ("“/” a.k.a root, special case");
  gtk_grid_attach (GTK_GRID (grid), label, 0, 3, 2, 1);

  /* ----------------------------------------------------------------------- */
  path_bar_slash = gtk_path_bar_new ();
  gtk_path_bar_set_inverted (GTK_PATH_BAR (path_bar_slash), TRUE);
  gtk_path_bar_set_path (GTK_PATH_BAR (path_bar_slash), "/");
  connect_path_bar_set_path (GTK_PATH_BAR (path_bar_slash));
  gtk_grid_attach (GTK_GRID (grid), path_bar_slash, 0, 4, 1, 1);

  label = gtk_label_new ("GtkPathBar with special roots");
  gtk_grid_attach (GTK_GRID (grid), label, 0, 5, 2, 1);

  /* ----------------------------------------------------------------------- */
  path_bar_custom_root_label = gtk_path_bar_new ();
  gtk_path_bar_set_inverted (GTK_PATH_BAR (path_bar_custom_root_label), TRUE);
  gtk_path_bar_set_path_extended (GTK_PATH_BAR (path_bar_custom_root_label),
                                  ORIGINAL_PATH, ROOT_PATH, "This Is A Root", NULL);
  connect_path_bar_set_path (GTK_PATH_BAR (path_bar_custom_root_label));
  gtk_grid_attach (GTK_GRID (grid), path_bar_custom_root_label, 0, 6, 1, 1);

  /* ----------------------------------------------------------------------- */
  path_bar_custom_root_icon = gtk_path_bar_new ();
  gtk_path_bar_set_inverted (GTK_PATH_BAR (path_bar_custom_root_icon), TRUE);
  icon = g_themed_icon_new ("drive-harddisk");
  gtk_path_bar_set_path_extended (GTK_PATH_BAR (path_bar_custom_root_icon),
                                  ORIGINAL_PATH, "/", NULL, icon);
  g_object_unref (icon);
  connect_path_bar_set_path (GTK_PATH_BAR (path_bar_custom_root_icon));
  gtk_grid_attach (GTK_GRID (grid), path_bar_custom_root_icon, 0, 7, 1, 1);

  /* GtkFilesPathBar tests */
  label = gtk_label_new ("GtkFilesPathBar tests");
  gtk_grid_attach (GTK_GRID (grid), label, 0, 8, 2, 1);

  /* ----------------------------------------------------------------------- */
  files_path_bar_random = gtk_files_path_bar_new ();
  file = g_file_new_for_uri (REAL_LOCATION_RANDOM);
  gtk_files_path_bar_set_file (GTK_FILES_PATH_BAR (files_path_bar_random), file);
  connect_files_path_bar (GTK_FILES_PATH_BAR (files_path_bar_random));
  gtk_grid_attach (GTK_GRID (grid), files_path_bar_random, 0, 9, 1, 1);

  g_clear_object (&file);

  files_path_bar_recent = gtk_files_path_bar_new ();
  file = g_file_new_for_uri (REAL_LOCATION_RECENT);
  gtk_files_path_bar_set_file (GTK_FILES_PATH_BAR (files_path_bar_recent), file);
  connect_files_path_bar (GTK_FILES_PATH_BAR (files_path_bar_recent));
  gtk_grid_attach (GTK_GRID (grid), files_path_bar_recent, 0, 10, 1, 1);

  g_clear_object (&file);

  /* Reset button */
  reset_button = gtk_button_new_with_label ("Reset State");
  gtk_widget_set_hexpand (reset_button, TRUE);
  g_signal_connect (GTK_BUTTON (reset_button), "clicked",
                    G_CALLBACK (on_reset_button_clicked), window);
  gtk_grid_attach (GTK_GRID (grid), reset_button, 0, 11, 2, 1);

#endif
  gtk_container_add (GTK_CONTAINER (window), grid);
  gtk_widget_show_all (window);

  gtk_main ();

  return 0;
}
