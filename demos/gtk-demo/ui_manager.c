/* UI Manager
 *
 * The GtkUIManager object allows the easy creation of menus
 * from an array of actions and a description of the menu hierarchy.
 */

#include <gtk/gtk.h>

static void
activate_action (GtkAction *action)
{
  g_message ("Action \"%s\" activated", gtk_action_get_name (action));
}

static void
activate_radio_action (GtkAction *action, GtkRadioAction *current)
{
  g_message ("Radio action \"%s\" selected",
             gtk_action_get_name (GTK_ACTION (current)));
}

static GtkActionEntry entries[] = {
  { "FileMenu", NULL, "_File" },               /* name, stock id, label */
  { "PreferencesMenu", NULL, "_Preferences" }, /* name, stock id, label */
  { "ColorMenu", NULL, "_Color"  },            /* name, stock id, label */
  { "ShapeMenu", NULL, "_Shape" },             /* name, stock id, label */
  { "HelpMenu", NULL, "_Help" },               /* name, stock id, label */
  { "New", GTK_STOCK_NEW,                      /* name, stock id */
    "_New", "<control>N",                      /* label, accelerator */
    "Create a new file",                       /* tooltip */
    G_CALLBACK (activate_action) },
  { "Open", GTK_STOCK_OPEN,                    /* name, stock id */
    "_Open","<control>O",                      /* label, accelerator */
    "Open a file",                             /* tooltip */
    G_CALLBACK (activate_action) },
  { "Save", GTK_STOCK_SAVE,                    /* name, stock id */
    "_Save","<control>S",                      /* label, accelerator */
    "Save current file",                       /* tooltip */
    G_CALLBACK (activate_action) },
  { "SaveAs", GTK_STOCK_SAVE,                  /* name, stock id */
    "Save _As...", NULL,                       /* label, accelerator */
    "Save to a file",                          /* tooltip */
    G_CALLBACK (activate_action) },
  { "Quit", GTK_STOCK_QUIT,                    /* name, stock id */
    "_Quit", "<control>Q",                     /* label, accelerator */
    "Quit",                                    /* tooltip */
    G_CALLBACK (activate_action) },
  { "About", NULL,                             /* name, stock id */
    "_About", "<control>A",                    /* label, accelerator */
    "About",                                   /* tooltip */
    G_CALLBACK (activate_action) },
  { "Logo", "demo-gtk-logo",                   /* name, stock id */
     NULL, NULL,                               /* label, accelerator */
    "GTK+",                                    /* tooltip */
    G_CALLBACK (activate_action) },
};
static guint n_entries = G_N_ELEMENTS (entries);


static GtkToggleActionEntry toggle_entries[] = {
  { "Bold", GTK_STOCK_BOLD,                    /* name, stock id */
     "_Bold", "<control>B",                    /* label, accelerator */
    "Bold",                                    /* tooltip */
    G_CALLBACK (activate_action),
    TRUE },                                    /* is_active */
};
static guint n_toggle_entries = G_N_ELEMENTS (toggle_entries);

enum {
  COLOR_RED,
  COLOR_GREEN,
  COLOR_BLUE
};

static GtkRadioActionEntry color_entries[] = {
  { "Red", NULL,                               /* name, stock id */
    "_Red", "<control>R",                      /* label, accelerator */
    "Blood", COLOR_RED },                      /* tooltip, value */
  { "Green", NULL,                             /* name, stock id */
    "_Green", "<control>G",                    /* label, accelerator */
    "Grass", COLOR_GREEN },                    /* tooltip, value */
  { "Blue", NULL,                              /* name, stock id */
    "_Blue", "<control>B",                     /* label, accelerator */
    "Sky", COLOR_BLUE },                       /* tooltip, value */
};
static guint n_color_entries = G_N_ELEMENTS (color_entries);

enum {
  SHAPE_SQUARE,
  SHAPE_RECTANGLE,
  SHAPE_OVAL
};

static GtkRadioActionEntry shape_entries[] = {
  { "Square", NULL,                            /* name, stock id */
    "_Square", "<control>S",                   /* label, accelerator */
    "Square",  SHAPE_SQUARE },                 /* tooltip, value */
  { "Rectangle", NULL,                         /* name, stock id */
    "_Rectangle", "<control>R",                /* label, accelerator */
    "Rectangle", SHAPE_RECTANGLE },            /* tooltip, value */
  { "Oval", NULL,                              /* name, stock id */
    "_Oval", "<control>O",                     /* label, accelerator */
    "Egg", SHAPE_OVAL },                       /* tooltip, value */
};
static guint n_shape_entries = G_N_ELEMENTS (shape_entries);

static const gchar *ui_info =
"<ui>"
"  <menubar name='MenuBar'>"
"    <menu action='FileMenu'>"
"      <menuitem action='New'/>"
"      <menuitem action='Open'/>"
"      <menuitem action='Save'/>"
"      <menuitem action='SaveAs'/>"
"      <separator/>"
"      <menuitem action='Quit'/>"
"    </menu>"
"    <menu action='PreferencesMenu'>"
"      <menu action='ColorMenu'>"
"       <menuitem action='Red'/>"
"       <menuitem action='Green'/>"
"       <menuitem action='Blue'/>"
"      </menu>"
"      <menu action='ShapeMenu'>"
"        <menuitem action='Square'/>"
"        <menuitem action='Rectangle'/>"
"        <menuitem action='Oval'/>"
"      </menu>"
"      <menuitem action='Bold'/>"
"    </menu>"
"    <menu action='HelpMenu'>"
"      <menuitem action='About'/>"
"    </menu>"
"  </menubar>"
"  <toolbar  name='ToolBar'>"
"    <toolitem action='Open'/>"
"    <toolitem action='Quit'/>"
"    <separator action='Sep1'/>"
"    <toolitem action='Logo'/>"
"  </toolbar>"
"</ui>";

GtkWidget *
do_ui_manager (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;

  if (!window)
    {
      GtkWidget *box1;
      GtkWidget *box2;
      GtkWidget *separator;
      GtkWidget *label;
      GtkWidget *button;
      GtkUIManager *ui;
      GtkActionGroup *actions;
      GError *error = NULL;

      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_screen (GTK_WINDOW (window),
                             gtk_widget_get_screen (do_widget));

      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed), &window);
      g_signal_connect (window, "delete-event",
                        G_CALLBACK (gtk_true), NULL);

      actions = gtk_action_group_new ("Actions");
      gtk_action_group_add_actions (actions, entries, n_entries, NULL);
      gtk_action_group_add_toggle_actions (actions,
                                           toggle_entries, n_toggle_entries,
                                           NULL);
      gtk_action_group_add_radio_actions (actions,
                                          color_entries, n_color_entries,
                                          COLOR_RED,
                                          G_CALLBACK (activate_radio_action),
                                          NULL);
      gtk_action_group_add_radio_actions (actions,
                                          shape_entries, n_shape_entries,
                                          SHAPE_OVAL,
                                          G_CALLBACK (activate_radio_action),
                                          NULL);

      ui = gtk_ui_manager_new ();
      gtk_ui_manager_insert_action_group (ui, actions, 0);
      g_object_unref (actions);
      gtk_window_add_accel_group (GTK_WINDOW (window),
                                  gtk_ui_manager_get_accel_group (ui));
      gtk_window_set_title (GTK_WINDOW (window), "UI Manager");
      gtk_container_set_border_width (GTK_CONTAINER (window), 0);

      if (!gtk_ui_manager_add_ui_from_string (ui, ui_info, -1, &error))
        {
          g_message ("building menus failed: %s", error->message);
          g_error_free (error);
        }

      box1 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_container_add (GTK_CONTAINER (window), box1);

      gtk_box_pack_start (GTK_BOX (box1),
                          gtk_ui_manager_get_widget (ui, "/MenuBar"),
                          FALSE, FALSE, 0);

      label = gtk_label_new ("Type\n<alt>\nto start");
      gtk_widget_set_size_request (label, 200, 200);
      gtk_widget_set_halign (label, GTK_ALIGN_CENTER);
      gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
      gtk_box_pack_start (GTK_BOX (box1), label, TRUE, TRUE, 0);

      separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
      gtk_box_pack_start (GTK_BOX (box1), separator, FALSE, TRUE, 0);

      box2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
      gtk_container_set_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, FALSE, TRUE, 0);

      button = gtk_button_new_with_label ("close");
      g_signal_connect_swapped (button, "clicked",
                                G_CALLBACK (gtk_widget_destroy), window);
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
      gtk_widget_set_can_default (button, TRUE);
      gtk_widget_grab_default (button);

      gtk_widget_show_all (window);
      g_object_unref (ui);
    }
  else
    {
      gtk_widget_destroy (window);
      window = NULL;
    }

  return window;
}
