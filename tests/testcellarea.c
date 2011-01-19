#include <gtk/gtk.h>
#include "cellareascaffold.h"

/*******************************************************
 *                      Simple Test                    *
 *******************************************************/
enum {
  SIMPLE_COLUMN_NAME,
  SIMPLE_COLUMN_ICON,
  SIMPLE_COLUMN_DESCRIPTION,
  N_SIMPLE_COLUMNS
};

static GtkCellRenderer *cell_1 = NULL, *cell_2 = NULL, *cell_3 = NULL;

static GtkTreeModel *
simple_list_model (void)
{
  GtkTreeIter   iter;
  GtkListStore *store = 
    gtk_list_store_new (N_SIMPLE_COLUMNS,
			G_TYPE_STRING,  /* name text */
			G_TYPE_STRING,  /* icon name */
			G_TYPE_STRING); /* description text */

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 
		      SIMPLE_COLUMN_NAME, "Alice in wonderland",
		      SIMPLE_COLUMN_ICON, "gtk-execute",
		      SIMPLE_COLUMN_DESCRIPTION, 
		      "Twas brillig, and the slithy toves "
		      "did gyre and gimble in the wabe; "
		      "all mimsy were the borogoves, "
		      "and the mome raths outgrabe",
		      -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 
		      SIMPLE_COLUMN_NAME, "Marry Poppins",
		      SIMPLE_COLUMN_ICON, "gtk-yes",
		      SIMPLE_COLUMN_DESCRIPTION, "Supercalifragilisticexpialidocious",
		      -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 
		      SIMPLE_COLUMN_NAME, "George Bush",
		      SIMPLE_COLUMN_ICON, "gtk-dialog-warning",
		      SIMPLE_COLUMN_DESCRIPTION, "It's a very good question, very direct, "
		      "and I'm not going to answer it",
		      -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 
		      SIMPLE_COLUMN_NAME, "Whinnie the pooh",
		      SIMPLE_COLUMN_ICON, "gtk-stop",
		      SIMPLE_COLUMN_DESCRIPTION, "The most wonderful thing about tiggers, "
		      "is tiggers are wonderful things",
		      -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 
		      SIMPLE_COLUMN_NAME, "Aleister Crowley",
		      SIMPLE_COLUMN_ICON, "gtk-about",
		      SIMPLE_COLUMN_DESCRIPTION, 
		      "Thou shalt do what thou wilt shall be the whole of the law",
		      -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 
		      SIMPLE_COLUMN_NAME, "Mark Twain",
		      SIMPLE_COLUMN_ICON, "gtk-quit",
		      SIMPLE_COLUMN_DESCRIPTION, 
		      "Giving up smoking is the easiest thing in the world. "
		      "I know because I've done it thousands of times.",
		      -1);


  return (GtkTreeModel *)store;
}

static GtkWidget *
simple_scaffold (void)
{
  GtkTreeModel *model;
  GtkWidget *scaffold;
  GtkCellArea *area;
  GtkCellRenderer *renderer;

  scaffold = cell_area_scaffold_new ();
  gtk_widget_show (scaffold);

  model = simple_list_model ();

  cell_area_scaffold_set_model (CELL_AREA_SCAFFOLD (scaffold), model);

  area = cell_area_scaffold_get_area (CELL_AREA_SCAFFOLD (scaffold));

  cell_1 = renderer = gtk_cell_renderer_text_new ();
  gtk_cell_area_box_pack_start (GTK_CELL_AREA_BOX (area), renderer, FALSE, FALSE, FALSE);
  gtk_cell_area_attribute_connect (area, renderer, "text", SIMPLE_COLUMN_NAME);

  cell_2 = renderer = gtk_cell_renderer_pixbuf_new ();
  g_object_set (G_OBJECT (renderer), "xalign", 0.0F, NULL);
  gtk_cell_area_box_pack_start (GTK_CELL_AREA_BOX (area), renderer, TRUE, FALSE, FALSE);
  gtk_cell_area_attribute_connect (area, renderer, "stock-id", SIMPLE_COLUMN_ICON);

  cell_3 = renderer = gtk_cell_renderer_text_new ();
  g_object_set (G_OBJECT (renderer), 
		"wrap-mode", PANGO_WRAP_WORD,
		"wrap-width", 215,
		NULL);
  gtk_cell_area_box_pack_start (GTK_CELL_AREA_BOX (area), renderer, FALSE, TRUE, FALSE);
  gtk_cell_area_attribute_connect (area, renderer, "text", SIMPLE_COLUMN_DESCRIPTION);

  return scaffold;
}

static void
orientation_changed (GtkComboBox      *combo,
		     CellAreaScaffold *scaffold)
{
  GtkCellArea   *area = cell_area_scaffold_get_area (scaffold);
  GtkOrientation orientation = gtk_combo_box_get_active (combo);

  gtk_orientable_set_orientation (GTK_ORIENTABLE (area), orientation);
}

static void
align_cell_2_toggled (GtkToggleButton  *toggle,
		      CellAreaScaffold *scaffold)
{
  GtkCellArea *area = cell_area_scaffold_get_area (scaffold);
  gboolean     align = gtk_toggle_button_get_active (toggle);

  gtk_cell_area_cell_set (area, cell_2, "align", align, NULL);
}

static void
align_cell_3_toggled (GtkToggleButton  *toggle,
		      CellAreaScaffold *scaffold)
{
  GtkCellArea *area = cell_area_scaffold_get_area (scaffold);
  gboolean     align = gtk_toggle_button_get_active (toggle);

  gtk_cell_area_cell_set (area, cell_3, "align", align, NULL);
}

static void
expand_cell_1_toggled (GtkToggleButton  *toggle,
		       CellAreaScaffold *scaffold)
{
  GtkCellArea *area = cell_area_scaffold_get_area (scaffold);
  gboolean     expand = gtk_toggle_button_get_active (toggle);

  gtk_cell_area_cell_set (area, cell_1, "expand", expand, NULL);
}

static void
expand_cell_2_toggled (GtkToggleButton  *toggle,
		       CellAreaScaffold *scaffold)
{
  GtkCellArea *area = cell_area_scaffold_get_area (scaffold);
  gboolean     expand = gtk_toggle_button_get_active (toggle);

  gtk_cell_area_cell_set (area, cell_2, "expand", expand, NULL);
}

static void
expand_cell_3_toggled (GtkToggleButton  *toggle,
		       CellAreaScaffold *scaffold)
{
  GtkCellArea *area = cell_area_scaffold_get_area (scaffold);
  gboolean     expand = gtk_toggle_button_get_active (toggle);

  gtk_cell_area_cell_set (area, cell_3, "expand", expand, NULL);
}

static void
simple_cell_area (void)
{
  GtkWidget *window, *widget;
  GtkWidget *scaffold, *frame, *vbox, *hbox;

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

  gtk_window_set_title (GTK_WINDOW (window), "CellArea expand and alignments");

  scaffold = simple_scaffold ();

  hbox  = gtk_hbox_new (FALSE, 4);
  frame = gtk_frame_new (NULL);
  gtk_widget_show (hbox);
  gtk_widget_show (frame);

  gtk_widget_set_valign (frame, GTK_ALIGN_CENTER);
  gtk_widget_set_halign (frame, GTK_ALIGN_FILL);

  gtk_container_add (GTK_CONTAINER (frame), scaffold);

  gtk_box_pack_end (GTK_BOX (hbox), frame, TRUE, TRUE, 0);

  /* Now add some controls */
  vbox  = gtk_vbox_new (FALSE, 4);
  gtk_widget_show (vbox);
  gtk_box_pack_end (GTK_BOX (hbox), vbox, FALSE, FALSE, 0);

  widget = gtk_combo_box_text_new ();
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (widget), "Horizontal");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (widget), "Vertical");
  gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);
  gtk_widget_show (widget);
  gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 0);

  g_signal_connect (G_OBJECT (widget), "changed",
                    G_CALLBACK (orientation_changed), scaffold);

  widget = gtk_check_button_new_with_label ("Align 2nd Cell");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), FALSE);
  gtk_widget_show (widget);
  gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 0);
  
  g_signal_connect (G_OBJECT (widget), "toggled",
                    G_CALLBACK (align_cell_2_toggled), scaffold);

  widget = gtk_check_button_new_with_label ("Align 3rd Cell");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);
  gtk_widget_show (widget);
  gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 0);
  
  g_signal_connect (G_OBJECT (widget), "toggled",
                    G_CALLBACK (align_cell_3_toggled), scaffold);


  widget = gtk_check_button_new_with_label ("Expand 1st Cell");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), FALSE);
  gtk_widget_show (widget);
  gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 0);
  
  g_signal_connect (G_OBJECT (widget), "toggled",
                    G_CALLBACK (expand_cell_1_toggled), scaffold);

  widget = gtk_check_button_new_with_label ("Expand 2nd Cell");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);
  gtk_widget_show (widget);
  gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 0);
  
  g_signal_connect (G_OBJECT (widget), "toggled",
                    G_CALLBACK (expand_cell_2_toggled), scaffold);

  widget = gtk_check_button_new_with_label ("Expand 3rd Cell");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), FALSE);
  gtk_widget_show (widget);
  gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 0);
  
  g_signal_connect (G_OBJECT (widget), "toggled",
                    G_CALLBACK (expand_cell_3_toggled), scaffold);

  gtk_container_add (GTK_CONTAINER (window), hbox);

  gtk_widget_show (window);
}

/*******************************************************
 *                      Focus Test                     *
 *******************************************************/
static GtkCellRenderer *focus_renderer, *sibling_renderer;

enum {
  FOCUS_COLUMN_NAME,
  FOCUS_COLUMN_CHECK,
  FOCUS_COLUMN_STATIC_TEXT,
  N_FOCUS_COLUMNS
};

static GtkTreeModel *
focus_list_model (void)
{
  GtkTreeIter   iter;
  GtkListStore *store = 
    gtk_list_store_new (N_FOCUS_COLUMNS,
			G_TYPE_STRING,  /* name text */
			G_TYPE_BOOLEAN, /* check */
			G_TYPE_STRING); /* static text */

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 
		      FOCUS_COLUMN_NAME, "Enter a string",
		      FOCUS_COLUMN_CHECK, TRUE,
		      FOCUS_COLUMN_STATIC_TEXT, "Does it fly ?",
		      -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 
		      FOCUS_COLUMN_NAME, "Enter a string",
		      FOCUS_COLUMN_CHECK, FALSE,
		      FOCUS_COLUMN_STATIC_TEXT, "Would you put it in a toaster ?",
		      -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 
		      FOCUS_COLUMN_NAME, "Type something",
		      FOCUS_COLUMN_CHECK, FALSE,
		      FOCUS_COLUMN_STATIC_TEXT, "Does it feed on cute kittens ?",
		      -1);

  return (GtkTreeModel *)store;
}

static void
cell_toggled (GtkCellRendererToggle *cell_renderer,
	      const gchar           *path,
	      CellAreaScaffold      *scaffold)
{
  GtkTreeModel *model = cell_area_scaffold_get_model (scaffold);
  GtkTreeIter   iter;
  gboolean      active;

  g_print ("Cell toggled !\n");

  if (!gtk_tree_model_get_iter_from_string (model, &iter, path))
    return;

  gtk_tree_model_get (model, &iter, FOCUS_COLUMN_CHECK, &active, -1);
  gtk_list_store_set (GTK_LIST_STORE (model), &iter, FOCUS_COLUMN_CHECK, !active, -1);
}

static void
cell_edited (GtkCellRendererToggle *cell_renderer,
	     const gchar           *path,
	     const gchar           *new_text,
	     CellAreaScaffold      *scaffold)
{
  GtkTreeModel *model = cell_area_scaffold_get_model (scaffold);
  GtkTreeIter   iter;

  g_print ("Cell edited with new text '%s' !\n", new_text);

  if (!gtk_tree_model_get_iter_from_string (model, &iter, path))
    return;

  gtk_list_store_set (GTK_LIST_STORE (model), &iter, FOCUS_COLUMN_NAME, new_text, -1);
}

static GtkWidget *
focus_scaffold (gboolean color_bg, GtkCellRenderer **focus, GtkCellRenderer **sibling)
{
  GtkTreeModel *model;
  GtkWidget *scaffold;
  GtkCellArea *area;
  GtkCellRenderer *renderer, *toggle;

  scaffold = cell_area_scaffold_new ();
  gtk_widget_show (scaffold);

  model = focus_list_model ();

  cell_area_scaffold_set_model (CELL_AREA_SCAFFOLD (scaffold), model);

  area = cell_area_scaffold_get_area (CELL_AREA_SCAFFOLD (scaffold));

  renderer = gtk_cell_renderer_text_new ();
  g_object_set (G_OBJECT (renderer), "editable", TRUE, NULL);
  gtk_cell_area_box_pack_start (GTK_CELL_AREA_BOX (area), renderer, TRUE, FALSE, FALSE);
  gtk_cell_area_attribute_connect (area, renderer, "text", FOCUS_COLUMN_NAME);

  if (color_bg)
    g_object_set (G_OBJECT (renderer), "cell-background", "red", NULL);

  g_signal_connect (G_OBJECT (renderer), "edited",
		    G_CALLBACK (cell_edited), scaffold);

  toggle = renderer = gtk_cell_renderer_toggle_new ();
  g_object_set (G_OBJECT (renderer), "xalign", 0.0F, NULL);
  gtk_cell_area_box_pack_start (GTK_CELL_AREA_BOX (area), renderer, FALSE, TRUE, FALSE);
  gtk_cell_area_attribute_connect (area, renderer, "active", FOCUS_COLUMN_CHECK);

  if (color_bg)
    g_object_set (G_OBJECT (renderer), "cell-background", "green", NULL);

  if (focus)
    *focus = renderer;

  g_signal_connect (G_OBJECT (renderer), "toggled",
		    G_CALLBACK (cell_toggled), scaffold);

  renderer = gtk_cell_renderer_text_new ();
  g_object_set (G_OBJECT (renderer), 
		"wrap-mode", PANGO_WRAP_WORD,
		"wrap-width", 150,
		NULL);

  if (color_bg)
    g_object_set (G_OBJECT (renderer), "cell-background", "blue", NULL);

  if (sibling)
    *sibling = renderer;

  gtk_cell_area_box_pack_start (GTK_CELL_AREA_BOX (area), renderer, FALSE, TRUE, FALSE);
  gtk_cell_area_attribute_connect (area, renderer, "text", FOCUS_COLUMN_STATIC_TEXT);

  gtk_cell_area_add_focus_sibling (area, toggle, renderer);

  return scaffold;
}

static void
focus_sibling_toggled (GtkToggleButton  *toggle,
		       CellAreaScaffold *scaffold)
{
  GtkCellArea *area = cell_area_scaffold_get_area (scaffold);
  gboolean     active = gtk_toggle_button_get_active (toggle);

  if (active)
    gtk_cell_area_add_focus_sibling (area, focus_renderer, sibling_renderer);
  else
    gtk_cell_area_remove_focus_sibling (area, focus_renderer, sibling_renderer);

  gtk_widget_queue_draw (GTK_WIDGET (scaffold));
}


static void
focus_cell_area (void)
{
  GtkWidget *window, *widget;
  GtkWidget *scaffold, *frame, *vbox, *hbox;

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  hbox  = gtk_hbox_new (FALSE, 4);
  gtk_widget_show (hbox);

  gtk_window_set_title (GTK_WINDOW (window), "Focus and editable cells");

  scaffold = focus_scaffold (FALSE, &focus_renderer, &sibling_renderer);

  frame = gtk_frame_new (NULL);
  gtk_widget_show (frame);

  gtk_widget_set_valign (frame, GTK_ALIGN_CENTER);
  gtk_widget_set_halign (frame, GTK_ALIGN_FILL);

  gtk_container_add (GTK_CONTAINER (frame), scaffold);

  gtk_box_pack_end (GTK_BOX (hbox), frame, TRUE, TRUE, 0);

  /* Now add some controls */
  vbox  = gtk_vbox_new (FALSE, 4);
  gtk_widget_show (vbox);
  gtk_box_pack_end (GTK_BOX (hbox), vbox, FALSE, FALSE, 0);

  widget = gtk_combo_box_text_new ();
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (widget), "Horizontal");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (widget), "Vertical");
  gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);
  gtk_widget_show (widget);
  gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 0);

  g_signal_connect (G_OBJECT (widget), "changed",
                    G_CALLBACK (orientation_changed), scaffold);

  widget = gtk_check_button_new_with_label ("Focus Sibling");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);
  gtk_widget_show (widget);
  gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 0);

  g_signal_connect (G_OBJECT (widget), "toggled",
                    G_CALLBACK (focus_sibling_toggled), scaffold);

  gtk_container_add (GTK_CONTAINER (window), hbox);

  gtk_widget_show (window);
}



/*******************************************************
 *                  Background Area                    *
 *******************************************************/
static void
cell_spacing_changed (GtkSpinButton    *spin_button,
		      CellAreaScaffold *scaffold)
{
  GtkCellArea *area = cell_area_scaffold_get_area (scaffold);
  gint        value;

  value = (gint)gtk_spin_button_get_value (spin_button);

  gtk_cell_area_box_set_spacing (GTK_CELL_AREA_BOX (area), value);
}

static void
row_spacing_changed (GtkSpinButton    *spin_button,
		     CellAreaScaffold *scaffold)
{
  gint value;

  value = (gint)gtk_spin_button_get_value (spin_button);

  cell_area_scaffold_set_row_spacing (scaffold, value);
}

static void
indentation_changed (GtkSpinButton    *spin_button,
		     CellAreaScaffold *scaffold)
{
  gint value;

  value = (gint)gtk_spin_button_get_value (spin_button);

  cell_area_scaffold_set_indentation (scaffold, value);
}

static void
background_area (void)
{
  GtkWidget *window, *widget, *label, *main_vbox;
  GtkWidget *scaffold, *frame, *vbox, *hbox;

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  hbox  = gtk_hbox_new (FALSE, 4);
  main_vbox  = gtk_vbox_new (FALSE, 4);
  gtk_widget_show (hbox);
  gtk_widget_show (main_vbox);
  gtk_container_add (GTK_CONTAINER (window), main_vbox);

  gtk_window_set_title (GTK_WINDOW (window), "Background Area");

  label = gtk_label_new ("In this example, row spacing gets devided into the background area, "
			 "column spacing is added between each background area, indentation is "
			 "prepended space distributed to the background area.");
  gtk_label_set_line_wrap  (GTK_LABEL (label), TRUE);
  gtk_label_set_width_chars  (GTK_LABEL (label), 40);
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (main_vbox), label, FALSE, FALSE, 0);

  scaffold = focus_scaffold (TRUE, NULL, NULL);

  frame = gtk_frame_new (NULL);
  gtk_widget_show (frame);

  gtk_widget_set_valign (frame, GTK_ALIGN_CENTER);
  gtk_widget_set_halign (frame, GTK_ALIGN_FILL);

  gtk_container_add (GTK_CONTAINER (frame), scaffold);

  gtk_box_pack_end (GTK_BOX (hbox), frame, TRUE, TRUE, 0);

  /* Now add some controls */
  vbox  = gtk_vbox_new (FALSE, 4);
  gtk_widget_show (vbox);
  gtk_box_pack_end (GTK_BOX (hbox), vbox, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (main_vbox), hbox, FALSE, FALSE, 0);

  widget = gtk_combo_box_text_new ();
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (widget), "Horizontal");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (widget), "Vertical");
  gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);
  gtk_widget_show (widget);
  gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 0);

  g_signal_connect (G_OBJECT (widget), "changed",
                    G_CALLBACK (orientation_changed), scaffold);

  widget = gtk_spin_button_new_with_range (0, 10, 1);
  label = gtk_label_new ("Cell spacing");
  hbox = gtk_hbox_new (FALSE, 4);
  gtk_widget_show (hbox);
  gtk_widget_show (label);
  gtk_widget_show (widget);
  gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

  g_signal_connect (G_OBJECT (widget), "value-changed",
                    G_CALLBACK (cell_spacing_changed), scaffold);


  widget = gtk_spin_button_new_with_range (0, 10, 1);
  label = gtk_label_new ("Row spacing");
  hbox = gtk_hbox_new (FALSE, 4);
  gtk_widget_show (hbox);
  gtk_widget_show (label);
  gtk_widget_show (widget);
  gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

  g_signal_connect (G_OBJECT (widget), "value-changed",
                    G_CALLBACK (row_spacing_changed), scaffold);

  widget = gtk_spin_button_new_with_range (0, 30, 1);
  label = gtk_label_new ("Intentation");
  hbox = gtk_hbox_new (FALSE, 4);
  gtk_widget_show (hbox);
  gtk_widget_show (label);
  gtk_widget_show (widget);
  gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

  g_signal_connect (G_OBJECT (widget), "value-changed",
                    G_CALLBACK (indentation_changed), scaffold);

  gtk_widget_show (window);
}






int
main (int argc, char *argv[])
{
  gtk_init (NULL, NULL);

  if (g_getenv ("RTL"))
    gtk_widget_set_default_direction (GTK_TEXT_DIR_RTL);

  simple_cell_area ();
  focus_cell_area ();
  background_area ();

  gtk_main ();

  return 0;
}
