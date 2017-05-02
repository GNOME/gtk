#include <string.h>
#include "testtextlib.h"

#define NUM_ROWS_TO_LOOP 30

typedef struct
{
  GtkWidget *tb_others;
  GtkWidget *tb_ref_selection;
  GtkWidget *tb_ref_at;
  GtkWidget *tb_ref_accessible_child;
  GtkWidget *child_entry;
  GtkWidget *row_entry;
  GtkWidget *col_entry;
  GtkWidget *index_entry;
}TestChoice;

static void _check_table (AtkObject *in_obj);
void table_runtest(AtkObject *);
static void other_runtest(AtkObject *obj);
static void ref_at_runtest(AtkObject *obj, gint row, gint col);
static void ref_accessible_child_runtest(AtkObject *obj, gint childno);
static void ref_selection_runtest (AtkObject *obj, gint index);
static void _selection_tests(AtkObject *obj);
static void _display_header_info(gchar *type,
  AtkObject *header_obj, gint header_num);
static void _process_child(AtkObject *child_obj);

static void _notify_table_row_inserted (GObject *obj,
  gint start_offset, gint length);
static void _notify_table_column_inserted (GObject *obj,
  gint start_offset, gint length);
static void _notify_table_row_deleted (GObject *obj,
  gint start_offset, gint length);
static void _notify_table_column_deleted (GObject *obj,
  gint start_offset, gint length);
static void _notify_table_row_reordered (GObject *obj);
static void _notify_table_column_reordered (GObject *obj);
static void _notify_table_child_added (GObject *obj,
  gint index, AtkObject *child);
static void _notify_table_child_removed (GObject *obj,
  gint index, AtkObject *child);
static void _property_signal_connect (AtkObject	*obj);
static void _property_change_handler (AtkObject	*obj,
  AtkPropertyValues *values);

static gboolean tested_set_headers = FALSE;
static void test_choice_gui (AtkObject **obj);
static void nogui_runtest (AtkObject *obj);
static void nogui_ref_at_runtest (AtkObject *obj);
static void nogui_process_child (AtkObject *obj);

static TestChoice *tc;
static gint g_visibleGUI = 0;
static AtkTable *g_table = NULL;
static AtkObject *current_obj = NULL;
gboolean g_done = FALSE;
gboolean g_properties = TRUE;

/* 
 * destroy
 *
 * Destroy Callback 
 *
 */
static void destroy (GtkWidget *widget, gpointer data)
{
  gtk_main_quit();
}

static void choicecb (GtkWidget *widget, gpointer data)
{
  AtkObject **ptr_to_obj = (AtkObject **)data;
  AtkObject *obj = *ptr_to_obj;

  if (GTK_TOGGLE_BUTTON(tc->tb_others)->active)
  {
    other_runtest(obj);
  }
  else if (GTK_TOGGLE_BUTTON(tc->tb_ref_selection)->active)
  {
    const gchar *indexstr;
    gint index;

    indexstr = gtk_entry_get_text(GTK_ENTRY(tc->index_entry));
    index = string_to_int((gchar *)indexstr);

    ref_selection_runtest(obj, index); 
  }
  else if (GTK_TOGGLE_BUTTON(tc->tb_ref_at)->active)
  {
    const gchar *rowstr, *colstr;
    gint row, col;
 
    rowstr = gtk_entry_get_text(GTK_ENTRY(tc->row_entry));
    colstr = gtk_entry_get_text(GTK_ENTRY(tc->col_entry));
    row = string_to_int((gchar *)rowstr);
    col = string_to_int((gchar *)colstr);
 
    ref_at_runtest(obj, row, col);
  }
  else if (GTK_TOGGLE_BUTTON(tc->tb_ref_accessible_child)->active)
  {
    const gchar *childstr;
    gint childno;
    childstr = gtk_entry_get_text(GTK_ENTRY(tc->child_entry));
    childno = string_to_int((gchar *)childstr);

    ref_accessible_child_runtest(obj, childno);
  }
}

static void _check_table (AtkObject *in_obj)
{
  AtkObject *obj = NULL;
  const char *no_properties;
  const char *no_gui;

  no_properties = g_getenv ("TEST_ACCESSIBLE_NO_PROPERTIES");
  no_gui = g_getenv ("TEST_ACCESSIBLE_NO_GUI");

  if (no_properties != NULL)
    g_properties = FALSE;
  if (no_gui != NULL)
    g_visibleGUI = 1;
  
  obj = find_object_by_type(in_obj, "GailTreeView");
  if (obj != NULL)
    g_print("Found GailTreeView table in child!\n");
  else
  {
    obj = find_object_by_type(in_obj, "GailCList");
    if (obj != NULL)
      g_print("Found GailCList in child!\n");
    else
    {
      g_print("No object found %s\n", g_type_name (G_OBJECT_TYPE (in_obj)));
      return;
    }
  }

  g_print ("In _check_table\n");

  if (!already_accessed_atk_object(obj))
  {
    /* Set up signal handlers */

    g_print ("Adding signal handler\n");
    g_signal_connect_closure_by_id (obj,
		g_signal_lookup ("column_inserted", G_OBJECT_TYPE (obj)),
		0,
		g_cclosure_new (G_CALLBACK (_notify_table_column_inserted),
		NULL, NULL),
		FALSE);
    g_signal_connect_closure_by_id (obj,
		g_signal_lookup ("row_inserted", G_OBJECT_TYPE (obj)),
		0,
                g_cclosure_new (G_CALLBACK (_notify_table_row_inserted),
			NULL, NULL),
		FALSE);
    g_signal_connect_closure_by_id (obj,
		g_signal_lookup ("column_deleted", G_OBJECT_TYPE (obj)),
		0,
                g_cclosure_new (G_CALLBACK (_notify_table_column_deleted),
			NULL, NULL),
		FALSE);
    g_signal_connect_closure_by_id (obj,
		g_signal_lookup ("row_deleted", G_OBJECT_TYPE (obj)),
		0,
                g_cclosure_new (G_CALLBACK (_notify_table_row_deleted),
			NULL, NULL),
		FALSE);
    g_signal_connect_closure_by_id (obj,
		g_signal_lookup ("column_reordered", G_OBJECT_TYPE (obj)),
		0,
                g_cclosure_new (G_CALLBACK (_notify_table_column_reordered),
			NULL, NULL),
		FALSE);
    g_signal_connect_closure_by_id (obj,
		g_signal_lookup ("row_reordered", G_OBJECT_TYPE (obj)),
		0,
                g_cclosure_new (G_CALLBACK (_notify_table_row_reordered),
			NULL, NULL),
	        FALSE);
    g_signal_connect_closure (obj, "children_changed::add",
		g_cclosure_new (G_CALLBACK (_notify_table_child_added),
                        NULL, NULL),
		FALSE);

    g_signal_connect_closure (obj, "children_changed::remove",
		g_cclosure_new (G_CALLBACK (_notify_table_child_removed),
                        NULL, NULL),
		FALSE);

  }
  g_table = ATK_TABLE(obj);

  atk_object_connect_property_change_handler (obj,
                   (AtkPropertyChangeHandler*) _property_change_handler);

  current_obj = obj;
  /*
   * The use of &current_obj allows us to change the object being processed
   * in the GUI.
   */
  if (g_visibleGUI != 1)
    test_choice_gui(&current_obj);
  else if (no_gui != NULL)
    nogui_runtest(obj);
}

static 
void other_runtest(AtkObject *obj)
{
  AtkObject *header_obj;
  AtkObject *out_obj;
  const gchar *out_string;
  GString *out_desc;
  gint n_cols, n_rows;
  gint rows_to_loop = NUM_ROWS_TO_LOOP;
  gint i, j;
  out_desc = g_string_sized_new(256);

  n_cols = atk_table_get_n_columns(ATK_TABLE(obj));
  n_rows = atk_table_get_n_rows(ATK_TABLE(obj));

  g_print ("Number of columns is %d\n", n_cols);
  g_print ("Number of rows is %d\n", n_rows);

  /* Loop NUM_ROWS_TO_LOOP rows if possible */
  if (n_rows < NUM_ROWS_TO_LOOP)
     rows_to_loop = n_rows;

  g_print ("\n");

  /* Caption */

  out_obj = atk_table_get_caption(ATK_TABLE(obj));
  if (out_obj != NULL)
  {
    out_string = atk_object_get_name (out_obj);
    if (out_string)
      g_print("Caption Name is <%s>\n", out_string);
    else
      g_print("Caption has no name\n");
  }
  else
    g_print("No caption\n");

  /* Column descriptions and headers */

  g_print ("\n");
  for (i=0; i < n_cols; i++)
  {
    /* check default */
    out_string = atk_table_get_column_description(ATK_TABLE(obj), i);
    if (out_string != NULL)
      g_print("%d: Column description is <%s>\n", i, out_string);
    else
      g_print("%d: Column description is <NULL>\n", i);

    /* check setting a new value */
    
    g_string_printf(out_desc, "new column description %d", i);

    if (out_string == NULL || strcmp (out_string, out_desc->str) != 0)
    {
      g_print("%d, Setting the column description to <%s>\n",
        i, out_desc->str);
      atk_table_set_column_description(ATK_TABLE(obj), i, out_desc->str);
      out_string = atk_table_get_column_description(ATK_TABLE(obj), i);
      if (out_string != NULL)
        g_print("%d: Column description is <%s>\n", i, out_string);
      else
        g_print("%d: Column description is <NULL>\n", i);
    }

    /* Column header */
    header_obj = atk_table_get_column_header(ATK_TABLE(obj), i);
    _display_header_info("Column", header_obj, i);
  }

  /* Row description */

  g_print ("\n");

  for (i=0; i < rows_to_loop; i++)
  {
    out_string = atk_table_get_row_description(ATK_TABLE(obj), i);
    if (out_string != NULL)
      g_print("%d: Row description is <%s>\n", i, out_string);
    else
      g_print("%d: Row description is <NULL>\n", i);

    g_string_printf(out_desc, "new row description %d", i);

    if (out_string == NULL || strcmp (out_string, out_desc->str) != 0)
    {
      g_print("%d: Setting the row description to <%s>\n",
        i, out_desc->str);
      atk_table_set_row_description(ATK_TABLE(obj), i, out_desc->str);

      out_string = atk_table_get_row_description(ATK_TABLE(obj), i);
      if (out_string != NULL)
        g_print("%d: Row description is <%s>\n", i, out_string);
      else
        g_print("%d: Row description is <NULL>\n", i);
    }

    header_obj = atk_table_get_row_header(ATK_TABLE(obj), i);
    _display_header_info("Row", header_obj, i);
    
    for (j=0; j <n_cols; j++)
    {
      gint index = atk_table_get_index_at(ATK_TABLE(obj), i, j);
      gint row, column;

      column = atk_table_get_column_at_index (ATK_TABLE (obj), index);
      g_return_if_fail (column == j);

      row = atk_table_get_row_at_index (ATK_TABLE (obj), index);
      g_return_if_fail (row == i);

      if(atk_selection_is_child_selected(ATK_SELECTION(obj), index))
        g_print("atk_selection_is_child_selected,index = %d returns TRUE\n", index);
      /* Generic cell tests */
      /* Just test setting column headers once. */

      if (!tested_set_headers)
      {
        tested_set_headers = TRUE;
   
        /* Hardcode to 1,1 for now */
        g_print(
          "Testing set_column_header for column %d, to table\n",
          (n_cols - 1));
        atk_table_set_column_header(ATK_TABLE(obj), (n_cols - 1), obj);

        g_print("Testing set_row_header for row %d, to table\n", n_rows);
        atk_table_set_row_header(ATK_TABLE(obj), n_rows, obj);
      }
    }
  }

  /* row/column extents */

  g_print("\n");
  g_print("Row extents at 1,1 is %d\n",
    atk_table_get_row_extent_at(ATK_TABLE(obj), 1, 1));
  g_print("Column extents at 1,1 is %d\n",
    atk_table_get_column_extent_at(ATK_TABLE(obj), 1, 1));
}

static
void ref_accessible_child_runtest(AtkObject *obj, gint child)
{
  AtkObject *child_obj;
  /* ref_child */
  g_print ("Accessing child %d\n", child);
  child_obj = atk_object_ref_accessible_child (obj, child);
  _property_signal_connect(child_obj);
  if (child_obj != NULL)
     _process_child(child_obj);
}

static 
void ref_selection_runtest (AtkObject *obj, gint index)
{
  AtkObject *child_obj;

  /* use atk_selection_ref_selection just once to check it works */
  child_obj = atk_selection_ref_selection(ATK_SELECTION(obj), index);
  if (child_obj)
  {
    g_print("child_obj gotten from atk_selection_ref_selection\n");
    g_object_unref (child_obj);
  }
  else 
    g_print("NULL returned by atk_selection_ref_selection\n");

  _selection_tests(obj);
}

static
void ref_at_runtest(AtkObject *obj, gint row, gint col)
{
  AtkObject *child_obj;
  /* ref_at */

  g_print("Testing ref_at row %d column %d\n", row, col);

  child_obj = atk_table_ref_at(ATK_TABLE(obj), row, col);
  _property_signal_connect(child_obj);

  g_print("Row is %d, col is %d\n", row, col);

  _process_child(child_obj);
  if (child_obj)
    g_object_unref (child_obj);
}

/**
 * process_child
 **/
static void
_process_child(AtkObject *child_obj)
{
  if (child_obj != NULL)
  {
    if (ATK_IS_TEXT(child_obj))
    {
      add_handlers(child_obj);
      setup_gui(child_obj, runtest);
    }
    else
    {
      g_print("Interface is not text!\n");
    }
/*
    if (ATK_IS_ACTION(child_obj))
      {
	gint i, j;
	gchar *action_name;
	gchar *action_description;
	gchar *action_keybinding;
	AtkAction *action = ATK_ACTION(child_obj);

	i = atk_action_get_n_actions (action);
	g_print ("Supports AtkAction with %d actions.\n", i);
	for (j = 0; j < i; j++)
	  {
	    g_print ("Action %d:\n", j);
	    action_name = atk_action_get_name (action, j);
	    if (action_name)
	      g_print (" Name = %s\n", action_name);
	    action_description = atk_action_get_description (action, j);
	    if (action_description)
	      g_print (" Description = %s\n", action_description);
	    action_keybinding = atk_action_get_keybinding (action, j);
	    if (action_keybinding)
	      g_print (" Keybinding = %s\n", action_keybinding);
	    action_description = "new description";
	    g_print (" Setting description to %s\n", action_description);
	    atk_action_set_description (action, j, action_description);
	    action_description = atk_action_get_description (action, j);
	    if (action_description)
	      g_print (" New description is now %s\n", action_description);
	  }
      }
*/
  }
  else
  {
    g_print("Child is NULL!\n");
  }
}
       
/**
 * Combined tests on AtkTable and AtkSelection on individual rows rather than 
 * all of them 
 **/
static void
_selection_tests(AtkObject *obj)
{
  gint n_rows = 0;
  gint n_cols = 0;
  gint selection_count = 0;
  gint i = 0;
  gint *selected = NULL;
  AtkTable *table;

  table = ATK_TABLE (obj);

  n_rows = atk_table_get_selected_rows(table, &selected);
  for (i = 0; i < n_rows; i++)
  {
    g_print("atk_table_get_selected_row returns : %d\n", 
              selected[i]);
    if (!atk_table_is_row_selected (table, selected[i]))
      g_print("atk_table_is_row_selected returns false for selected row %d\n", 
              selected[i]);
  }
  g_free (selected);

  selected = NULL;
  n_cols = atk_table_get_selected_columns(table, &selected);
  for (i = 0; i < n_cols; i++)
    g_print("atk_table_get_selected_columns returns : %d\n", selected[i]);
  g_free (selected);
	
  selection_count = atk_selection_get_selection_count(ATK_SELECTION(obj));
  g_print("atk_selection_get_selection_count returns %d\n", selection_count);

  if (atk_table_is_row_selected(table, 2))
  {
    g_print("atk_table_is_row_selected (table, 2) returns TRUE\n");
    atk_selection_clear_selection (ATK_SELECTION (obj));
    if (atk_table_add_row_selection(table, 4))
      g_print("atk_table_add_row_selection: selected row 4\n");
    if (!atk_table_is_row_selected (table, 4))
      g_print("atk_table_is_row_selected returns false for row 2\n");
    if (atk_table_is_row_selected (table, 2))
      g_print("atk_table_is_row_selected gives false positive for row 2\n");
  }

  if (atk_table_is_row_selected(table, 3))
  {
    if (atk_table_remove_row_selection(table, 3))
      g_print("atk_table_remove_row_selection unselected row 3\n");
  }

  if (atk_table_is_selected(table, 5, 4))
  {
    atk_selection_clear_selection(ATK_SELECTION(obj));
    g_print("atk_selection_clear_selection: just cleared all selected\n");
  }

  if (atk_table_is_column_selected(table, 2))
  {
    g_print("atk_table_is_column_selected(obj, 2) returns TRUE\n");
    if (atk_table_add_column_selection(table, 4))
      g_print("atk_table_add_column_selection: selected column 4\n");
    g_print("atk_table_is_column_selected(obj, 2) returns TRUE\n");
  }

  if (atk_table_is_column_selected(table, 3))
  {
    if (atk_table_remove_column_selection(table, 3))
      g_print("atk_table_remove_column_selection: unselected column 3\n");
  }
}

static void
_create_event_watcher (void)
{
  atk_add_focus_tracker (_check_table);
}

int
gtk_module_init(gint argc, char* argv[])
{
    g_print("TestTable Module loaded\n");

    _create_event_watcher();

    return 0;
}

static void
_notify_table_row_inserted (GObject *obj, gint start_offset, gint length)
{
  g_print ("SIGNAL - Row inserted at position %d, num of rows inserted %d!\n",
    start_offset, length);
}

static void
_notify_table_column_inserted (GObject *obj, gint start_offset, gint length)
{
  g_print ("SIGNAL - Column inserted at position %d, num of columns inserted %d!\n",
    start_offset, length);
}

static void
_notify_table_row_deleted (GObject *obj, gint start_offset, gint length)
{
  g_print ("SIGNAL - Row deleted at position %d, num of rows deleted %d!\n",
    start_offset, length);
}

static void
_notify_table_column_deleted (GObject *obj, gint start_offset, gint length)
{
  g_print ("SIGNAL - Column deleted at position %d, num of columns deleted %d!\n",
    start_offset, length);
}

static void
_notify_table_row_reordered (GObject *obj)
{
  g_print ("SIGNAL - Row reordered!\n");
}

static void
_notify_table_column_reordered (GObject *obj)
{
  g_print ("SIGNAL - Column reordered!\n");
}

static void _notify_table_child_added (GObject *obj,
  gint index, AtkObject *child)
{
   g_print ("SIGNAL - Child added - index %d\n", index);
}

static void _notify_table_child_removed (GObject *obj,
  gint index, AtkObject *child)
{
   g_print ("SIGNAL - Child removed - index %d\n", index);
}

static void
_display_header_info(gchar *type, AtkObject *header_obj, gint header_num)
{
  if (header_obj != NULL)
  {
    AtkRole role;
    role = atk_object_get_role(header_obj);

    if (role == ATK_ROLE_PUSH_BUTTON)
    {
      g_print ("%d: %s header is a push button!\n", header_num, type);
    }
    else if (role == ATK_ROLE_LABEL)
    {
      g_print ("%d: %s header is a label!\n", header_num, type);
    }
    else if (ATK_IS_TEXT(header_obj))
    {
      gchar *header_text;

      header_text = atk_text_get_text (ATK_TEXT (header_obj), 0, 3);
      if (header_text != NULL)
      {
        g_print("%d: %s header is a text value <%s>\n", header_num,
          type, header_text);
      }
      else
      {
        g_print("%d: %s header is a text value <NULL>\n", header_num,
          type);
      }
    }
    else 
    {
      g_print ("%d: %s header is of type %s!\n", header_num,
        type, atk_role_get_name (role));
    }
  }
  else
  {
    g_print ("%d: %s header object is NULL!\n", header_num, type);
  }
}

static void _property_signal_connect (AtkObject *obj)
{
  if (g_properties && obj != NULL)
  {
    g_signal_connect_closure_by_id (obj,
    g_signal_lookup ("property_change", G_OBJECT_TYPE (obj)),
      0,
      g_cclosure_new (G_CALLBACK (_property_change_handler),
      NULL, NULL),
      FALSE);
  }
}

static void 
_property_change_handler (AtkObject         *obj,
                          AtkPropertyValues *values)
{
  gchar *obj_text;
  const gchar *name;

  if (g_table != NULL)
  {
    gint index = atk_object_get_index_in_parent(obj);
    
    if (index >= 0)
      g_print("Index is %d, row is %d, col is %d\n", index,
              atk_table_get_row_at_index(g_table, index),
              atk_table_get_column_at_index(g_table, index));
    else
      g_print ("index: %d for %s\n", index, g_type_name (G_OBJECT_TYPE (obj)));
  }

  if (ATK_IS_TEXT(obj))
  {
     obj_text = atk_text_get_text (ATK_TEXT (obj), 0, 15);
     if (obj_text == NULL)
       g_print("  Cell text is <NULL>\n");
     else
       g_print("  Cell text is <%s>\n", obj_text);
  }

  g_print("  PropertyName <%s>\n",
	   values->property_name ? values->property_name: "NULL");
  g_print("    - ");

  if (&values->old_value != NULL && G_IS_VALUE (&values->old_value))
  {
    GType old_type = G_VALUE_TYPE (&values->old_value);

    switch (old_type)
    {
    case G_TYPE_INT:
      g_print("value was <%d>\n", g_value_get_int (&values->old_value));
      break;
    case G_TYPE_STRING:
      name = g_value_get_string (&values->old_value);
      if (name != NULL)
        g_print ("value was <%s>\n", name);
      else
        g_print ("value was <NULL>\n");
      break;
    default: 
      g_print("value was <unknown type>\n");
      break;
    }
  }
  else
  {
    g_print("value was <not a value>\n");
  }
  g_print("    - ");
  if (&values->new_value != NULL && G_IS_VALUE (&values->new_value))
  {
    GType new_type = G_VALUE_TYPE (&values->new_value);

    switch (new_type)
    {
    case G_TYPE_INT:
      g_print("value is <%d>\n", g_value_get_int (&values->new_value));
      break;
    case G_TYPE_STRING:
      name = g_value_get_string (&values->new_value);
      if (name != NULL)
        g_print ("value is <%s>\n", name);
      else
        g_print ("value is <NULL>\n");
      break;
    default: 
      g_print("value is <unknown type>\n");
      break;
    }
  }
  else
  {
    g_print("value is <not a value>\n");
  }
}

static
void test_choice_gui(AtkObject **obj)
{
  GtkWidget *window;
  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *child_label;
  GtkWidget *row_label;
  GtkWidget *col_label;
  GtkWidget *index_label;
  GtkWidget *hseparator;
  GtkWidget *hbuttonbox;
  GtkWidget *button;


  tc = (TestChoice *) g_malloc (sizeof(TestChoice));
  
  window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(window), "Test to run");

  g_signal_connect(GTK_OBJECT (window), "destroy",
                   G_CALLBACK (destroy), &window);

  vbox = gtk_vbox_new(TRUE, 0);
  gtk_box_set_spacing(GTK_BOX(vbox), 10);


  hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_set_spacing(GTK_BOX(hbox), 10);
  tc->tb_ref_selection = gtk_toggle_button_new_with_label("ref_selection");
  gtk_box_pack_start (GTK_BOX (hbox), tc->tb_ref_selection, TRUE, TRUE, 0);
  index_label = gtk_label_new("index: ");
  gtk_box_pack_start (GTK_BOX (hbox), index_label, TRUE, TRUE, 0);
  tc->index_entry = gtk_entry_new();
  gtk_entry_set_text(GTK_ENTRY(tc->index_entry), "1");
  gtk_box_pack_start (GTK_BOX (hbox), tc->index_entry, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0); 

  hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_set_spacing(GTK_BOX(hbox), 10);
  tc->tb_ref_at = gtk_toggle_button_new_with_label("ref_at");
  gtk_box_pack_start (GTK_BOX (hbox), tc->tb_ref_at, TRUE, TRUE, 0);
  row_label = gtk_label_new("row:");
  gtk_box_pack_start (GTK_BOX (hbox), row_label, TRUE, TRUE, 0);
  tc->row_entry = gtk_entry_new();
  gtk_entry_set_text(GTK_ENTRY(tc->row_entry), "1");
  gtk_box_pack_start (GTK_BOX (hbox), tc->row_entry, TRUE, TRUE, 0);
  col_label = gtk_label_new("column:");
  gtk_box_pack_start (GTK_BOX (hbox), col_label, TRUE, TRUE, 0);
  tc->col_entry = gtk_entry_new();
  gtk_entry_set_text(GTK_ENTRY(tc->col_entry), "1");
  gtk_box_pack_start (GTK_BOX (hbox), tc->col_entry, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0); 

  hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_set_spacing(GTK_BOX(hbox), 10);
  tc->tb_ref_accessible_child = gtk_toggle_button_new_with_label("ref_accessible_child");
  gtk_box_pack_start (GTK_BOX (hbox), tc->tb_ref_accessible_child, TRUE, TRUE, 0);
  child_label = gtk_label_new("Child no:");
  gtk_box_pack_start (GTK_BOX (hbox), child_label, TRUE, TRUE, 0); 
  tc->child_entry = gtk_entry_new();
  gtk_entry_set_text(GTK_ENTRY(tc->child_entry), "1");
  gtk_box_pack_start (GTK_BOX (hbox), tc->child_entry, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);

  tc->tb_others = gtk_toggle_button_new_with_label("others");
  gtk_box_pack_start (GTK_BOX (vbox), tc->tb_others, TRUE, TRUE, 0);
  
  hseparator = gtk_hseparator_new();
  gtk_box_pack_start (GTK_BOX (vbox), hseparator, TRUE, TRUE, 0);

  button = gtk_button_new_with_mnemonic("_Run Test");

  hbuttonbox = gtk_hbutton_box_new();
  gtk_button_box_set_layout(GTK_BUTTON_BOX(hbuttonbox),
    GTK_BUTTONBOX_SPREAD);
  gtk_box_pack_end (GTK_BOX (hbuttonbox), GTK_WIDGET (button), TRUE, TRUE, 0);
  gtk_box_pack_end (GTK_BOX (vbox), hbuttonbox, TRUE, TRUE, 0);
  g_signal_connect(GTK_OBJECT(button), "clicked", G_CALLBACK (choicecb), obj);

  gtk_container_add(GTK_CONTAINER(window), vbox);
  gtk_widget_show(vbox);
  gtk_widget_show(window);
  gtk_widget_show_all(GTK_WIDGET(window));

  g_visibleGUI = 1;
}

void static
nogui_runtest (AtkObject *obj)
{
  g_print ("Running non-GUI tests...\n");
  other_runtest (obj);
  nogui_ref_at_runtest (obj);
}

static void
nogui_ref_at_runtest (AtkObject *obj)
{
  AtkObject *child_obj;
  gint i, j;
  gint n_cols;
  gint rows_to_loop = 5;

  n_cols = atk_table_get_n_columns (ATK_TABLE(obj));
  
  if (atk_table_get_n_rows(ATK_TABLE(obj)) < rows_to_loop)
    rows_to_loop = atk_table_get_n_rows (ATK_TABLE(obj));

  for (i=0; i < rows_to_loop; i++)
  {
    /* Just the first rows_to_loop rows */
    for (j=0; j < n_cols; j++)
    {
      gint index = atk_table_get_index_at(ATK_TABLE(obj), i, j);
	  if(atk_selection_is_child_selected(ATK_SELECTION(obj), index))
		 g_print("atk_selection_is_child_selected,index = %d returns TRUE\n", index);

      g_print("Testing ref_at row %d column %d\n", i, j);

      if (i == 3 && j == 0)
      {
        g_print("child_obj gotten from atk_selection_ref_selection\n");

        /* use atk_selection_ref_selection just once to check it works */
        child_obj = atk_selection_ref_selection(ATK_SELECTION(obj), index );
      }
      else	
      {
        child_obj = atk_table_ref_at(ATK_TABLE(obj), i, j);
      }

      _property_signal_connect(child_obj);

      g_print("Index is %d, row is %d, col is %d\n", index,
        atk_table_get_row_at_index(ATK_TABLE(obj), index),
        atk_table_get_column_at_index(ATK_TABLE(obj), index));

      nogui_process_child (child_obj);

      /* Generic cell tests */
      /* Just test setting column headers once. */

      if (!tested_set_headers)
      {
        tested_set_headers = TRUE;

        g_print("Testing set_column_header for column %d, to cell value %d,%d\n",
          j, i, j);
        atk_table_set_column_header(ATK_TABLE(obj), j, child_obj);

        g_print("Testing set_row_header for row %d, to cell value %d,%d\n",
          i, i, j);
        atk_table_set_row_header(ATK_TABLE(obj), i, child_obj);
      }
      if (child_obj)
        g_object_unref (child_obj);
    }
  }
}

static void
nogui_process_child (AtkObject *obj)
{
  gchar default_val[5] = "NULL";

  if (ATK_IS_TEXT(obj))
    {
      gchar *current_text;
      current_text = atk_text_get_text (ATK_TEXT(obj), 0, -1);
      g_print ("Child supports text interface.\nCurrent text is %s\n", current_text);
    }

  if (ATK_IS_ACTION(obj))
    {
      AtkAction *action = ATK_ACTION(obj);
      gint n_actions, i;
      const gchar *name, *description;
      
      n_actions = atk_action_get_n_actions (action);
      g_print ("Child supports %d actions.\n", n_actions);
      for (i = 0; i < n_actions; i++)
	{
	  name = atk_action_get_name (action, i);
	  description = atk_action_get_description (action, i);

          if (name == NULL)
             name = default_val;
          if (description == NULL)
             description = default_val;
          
	  g_print (" %d: name = <%s>\n", i, name);
          g_print ("    description = <%s>\n", description);
	}
    }
}

