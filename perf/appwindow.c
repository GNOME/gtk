/* This file contains utility functions to create what would be a typical "main
 * window" for an application.
 *
 * TODO:
 *
 * Measurements happen from the start of the destruction of the last window.  Use
 * GTimer rather than X timestamps to fix this (it uses gettimeofday() internally!)
 *
 * Make non-interactive as well by using the above.
 *
 */

#include <string.h>
#include <gtk/gtk.h>

static void
quit_cb (GtkWidget *widget, gpointer data)
{
  gtk_main_quit ();
}

static void
noop_cb (GtkWidget *widget, gpointer data)
{
  /* nothing */
}

static const GtkActionEntry menu_action_entries[] = {
  { "FileMenu", NULL, "_File" },
  { "EditMenu", NULL, "_Edit" },
  { "ViewMenu", NULL, "_View" },
  { "HelpMenu", NULL, "_Help" },

  { "New", GTK_STOCK_NEW, "_New", "<control>N", "Create a new document", G_CALLBACK (noop_cb) },
  { "Open", GTK_STOCK_OPEN, "_Open", "<control>O", "Open a file", G_CALLBACK (noop_cb) },
  { "Save", GTK_STOCK_SAVE, "_Save", "<control>S", "Save the document", G_CALLBACK (noop_cb) },
  { "SaveAs", GTK_STOCK_SAVE_AS, "Save _As...", NULL, "Save the document with a different name", NULL},
  { "PrintPreview", GTK_STOCK_PRINT_PREVIEW, "Print Previe_w", NULL, "See how the document will be printed", G_CALLBACK (noop_cb) },
  { "Print", GTK_STOCK_PRINT, "_Print", "<control>P", "Print the document", G_CALLBACK (noop_cb) },
  { "Close", GTK_STOCK_CLOSE, "_Close", "<control>W", "Close the document", G_CALLBACK (noop_cb) },
  { "Quit", GTK_STOCK_QUIT, "_Quit", "<control>Q", "Quit the program", G_CALLBACK (quit_cb) },

  { "Undo", GTK_STOCK_UNDO, "_Undo", "<control>Z", "Undo the last action", G_CALLBACK (noop_cb) },
  { "Redo", GTK_STOCK_REDO, "_Redo", "<control>Y", "Redo the last action", G_CALLBACK (noop_cb) },
  { "Cut", GTK_STOCK_CUT, "Cu_t", "<control>X", "Cut the selection to the clipboard", G_CALLBACK (noop_cb) },
  { "Copy", GTK_STOCK_COPY, "_Copy", "<control>C", "Copy the selection to the clipboard", G_CALLBACK (noop_cb) },
  { "Paste", GTK_STOCK_PASTE, "_Paste", "<control>V", "Paste the contents of the clipboard", G_CALLBACK (noop_cb) },
  { "Delete", GTK_STOCK_DELETE, "_Delete", "Delete", "Delete the selection", G_CALLBACK (noop_cb) },
  { "SelectAll", NULL, "Select _All", "<control>A", "Select the whole document", G_CALLBACK (noop_cb) },
  { "Preferences", GTK_STOCK_PREFERENCES, "Pr_eferences", NULL, "Configure the application", G_CALLBACK (noop_cb) },

  { "ZoomFit", GTK_STOCK_ZOOM_FIT, "Zoom to _Fit", NULL, "Zoom the document to fit the window", G_CALLBACK (noop_cb) },
  { "Zoom100", GTK_STOCK_ZOOM_100, "Zoom _1:1", NULL, "Zoom to 1:1 scale", G_CALLBACK (noop_cb) },
  { "ZoomIn", GTK_STOCK_ZOOM_IN, "Zoom _In", NULL, "Zoom into the document", G_CALLBACK (noop_cb) },
  { "ZoomOut", GTK_STOCK_ZOOM_OUT, "Zoom _Out", NULL, "Zoom away from the document", G_CALLBACK (noop_cb) },
  { "FullScreen", GTK_STOCK_FULLSCREEN, "Full _Screen", "F11", "Use the whole screen to view the document", G_CALLBACK (noop_cb) },

  { "HelpContents", GTK_STOCK_HELP, "_Contents", "F1", "Show the table of contents for the help system", G_CALLBACK (noop_cb) },
  { "About", GTK_STOCK_ABOUT, "_About", NULL, "About this application", G_CALLBACK (noop_cb) }
};

static const char ui_description[] =
"<ui>"
"  <menubar name=\"MainMenu\">"
"    <menu action=\"FileMenu\">"
"      <menuitem action=\"New\"/>"
"      <menuitem action=\"Open\"/>"
"      <menuitem action=\"Save\"/>"
"      <menuitem action=\"SaveAs\"/>"
"      <separator/>"
"      <menuitem action=\"PrintPreview\"/>"
"      <menuitem action=\"Print\"/>"
"      <separator/>"
"      <menuitem action=\"Close\"/>"
"      <menuitem action=\"Quit\"/>"
"    </menu>"
"    <menu action=\"EditMenu\">"
"      <menuitem action=\"Undo\"/>"
"      <menuitem action=\"Redo\"/>"
"      <separator/>"
"      <menuitem action=\"Cut\"/>"
"      <menuitem action=\"Copy\"/>"
"      <menuitem action=\"Paste\"/>"
"      <menuitem action=\"Delete\"/>"
"      <separator/>"
"      <menuitem action=\"SelectAll\"/>"
"      <separator/>"
"      <menuitem action=\"Preferences\"/>"
"    </menu>"
"    <menu action=\"ViewMenu\">"
"      <menuitem action=\"ZoomFit\"/>"
"      <menuitem action=\"Zoom100\"/>"
"      <menuitem action=\"ZoomIn\"/>"
"      <menuitem action=\"ZoomOut\"/>"
"      <separator/>"
"      <menuitem action=\"FullScreen\"/>"
"    </menu>"
"    <menu action=\"HelpMenu\">"
"      <menuitem action=\"HelpContents\"/>"
"      <menuitem action=\"About\"/>"
"    </menu>"
"  </menubar>"
"  <toolbar name=\"MainToolbar\">"
"    <toolitem action=\"New\"/>"
"    <toolitem action=\"Open\"/>"
"    <toolitem action=\"Save\"/>"
"    <separator/>"
"    <toolitem action=\"Print\"/>"
"    <separator/>"
"    <toolitem action=\"Undo\"/>"
"    <toolitem action=\"Redo\"/>"
"    <separator/>"
"    <toolitem action=\"Cut\"/>"
"    <toolitem action=\"Copy\"/>"
"    <toolitem action=\"Paste\"/>"
"  </toolbar>"
"</ui>";

static GtkUIManager *
uimanager_new (void)
{
  GtkUIManager *ui;
  GtkActionGroup *action_group;
  GError *error;

  ui = gtk_ui_manager_new ();

  action_group = gtk_action_group_new ("Actions");
  gtk_action_group_add_actions (action_group, menu_action_entries, G_N_ELEMENTS (menu_action_entries), NULL);

  gtk_ui_manager_insert_action_group (ui, action_group, 0);
  g_object_unref (action_group);

  error = NULL;
  if (!gtk_ui_manager_add_ui_from_string (ui, ui_description, -1, &error))
    g_error ("Could not parse the uimanager XML: %s", error->message);

  return ui;
}

static GtkWidget *
menubar_new (GtkUIManager *ui)
{
  return gtk_ui_manager_get_widget (ui, "/MainMenu");
}

static GtkWidget *
toolbar_new (GtkUIManager *ui)
{
  return gtk_ui_manager_get_widget (ui, "/MainToolbar");
}

struct row_data {
  char *stock_id;
  char *text1;
  char *text2;
};

static struct row_data row_data[] = {
  { GTK_STOCK_NEW,		"First",		"Here bygynneth the Book of the tales of Caunterbury." },
  { GTK_STOCK_OPEN,		"Second",		"Whan that Aprille, with hise shoures soote," },
  { GTK_STOCK_ABOUT, 		"Third",		"The droghte of March hath perced to the roote" },
  { GTK_STOCK_ADD, 		"Fourth",		"And bathed every veyne in swich licour," },
  { GTK_STOCK_APPLY, 		"Fifth",		"Of which vertu engendred is the flour;" },
  { GTK_STOCK_BOLD, 		"Sixth",		"Whan Zephirus eek with his swete breeth" },
  { GTK_STOCK_CANCEL,		"Seventh",		"Inspired hath in every holt and heeth" },
  { GTK_STOCK_CDROM,		"Eighth",		"The tendre croppes, and the yonge sonne" },
  { GTK_STOCK_CLEAR,		"Ninth",		"Hath in the Ram his halfe cours yronne," },
  { GTK_STOCK_CLOSE,		"Tenth",		"And smale foweles maken melodye," },
  { GTK_STOCK_COLOR_PICKER,	"Eleventh",		"That slepen al the nyght with open eye-" },
  { GTK_STOCK_CONVERT,		"Twelfth",		"So priketh hem Nature in hir corages-" },
  { GTK_STOCK_CONNECT,		"Thirteenth",		"Thanne longen folk to goon on pilgrimages" },
  { GTK_STOCK_COPY,		"Fourteenth",		"And palmeres for to seken straunge strondes" },
  { GTK_STOCK_CUT,		"Fifteenth",		"To ferne halwes, kowthe in sondry londes;" },
  { GTK_STOCK_DELETE,		"Sixteenth",		"And specially, from every shires ende" },
  { GTK_STOCK_DIRECTORY,	"Seventeenth",		"Of Engelond, to Caunturbury they wende," },
  { GTK_STOCK_DISCONNECT,	"Eighteenth",		"The hooly blisful martir for the seke" },
  { GTK_STOCK_EDIT,		"Nineteenth",		"That hem hath holpen, whan that they were seeke." },
  { GTK_STOCK_EXECUTE,		"Twentieth",		"Bifil that in that seson, on a day," },
  { GTK_STOCK_FILE,		"Twenty-first",		"In Southwerk at the Tabard as I lay," },
  { GTK_STOCK_FIND,		"Twenty-second",	"Redy to wenden on my pilgrymage" },
  { GTK_STOCK_FIND_AND_REPLACE,	"Twenty-third",		"To Caunterbury, with ful devout corage," },
  { GTK_STOCK_FLOPPY,		"Twenty-fourth",	"At nyght were come into that hostelrye" },
  { GTK_STOCK_FULLSCREEN,	"Twenty-fifth",		"Wel nyne and twenty in a compaignye" },
  { GTK_STOCK_GOTO_BOTTOM,	"Twenty-sixth",		"Of sondry folk, by aventure yfalle" },
};

static GtkTreeModel *
tree_model_new (void)
{
  GtkListStore *list;
  int i;

  list = gtk_list_store_new (3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);

  for (i = 0; i < G_N_ELEMENTS (row_data); i++)
    {
      GtkTreeIter iter;

      gtk_list_store_append (list, &iter);
      gtk_list_store_set (list,
			  &iter,
			  0, row_data[i].stock_id,
			  1, row_data[i].text1,
			  2, row_data[i].text2,
			  -1);
    }

  return GTK_TREE_MODEL (list);
}

static GtkWidget *
tree_view_new (void)
{
  GtkWidget *sw;
  GtkWidget *tree;
  GtkTreeModel *model;
  GtkTreeViewColumn *column;

  sw = gtk_scrolled_window_new (NULL, NULL);

  model = tree_model_new ();
  tree = gtk_tree_view_new_with_model (model);
  g_object_unref (model);

  gtk_widget_set_size_request (tree, 300, 100);

  column = gtk_tree_view_column_new_with_attributes ("Icon",
						     gtk_cell_renderer_pixbuf_new (),
						     "stock-id", 0,
						     NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column);

  column = gtk_tree_view_column_new_with_attributes ("Index",
						     gtk_cell_renderer_text_new (),
						     "text", 1,
						     NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column);

  column = gtk_tree_view_column_new_with_attributes ("Canterbury Tales",
						     gtk_cell_renderer_text_new (),
						     "text", 2,
						     NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column);

  gtk_container_add (GTK_CONTAINER (sw), tree);

  return sw;
}

static GtkWidget *
left_pane_new (void)
{
  return tree_view_new ();
}

static GtkWidget *
text_view_new (void)
{
  GtkWidget *sw;
  GtkWidget *text_view;
  GtkTextBuffer *buffer;

  sw = gtk_scrolled_window_new (NULL, NULL);

  text_view = gtk_text_view_new ();
  gtk_widget_set_size_request (text_view, 400, 300);
  gtk_container_add (GTK_CONTAINER (sw), text_view);

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (text_view));

  gtk_text_buffer_set_text (buffer,
			    "In felaweshipe, and pilgrimes were they alle,\n"
			    "That toward Caunterbury wolden ryde.\n"
			    "The chambres and the stables weren wyde,\n"
			    "And wel we weren esed atte beste;\n"
			    "And shortly, whan the sonne was to reste,\n"
			    "\n"
			    "So hadde I spoken with hem everychon \n"
			    "That I was of hir felaweshipe anon, \n"
			    "And made forward erly for to ryse \n"
			    "To take our wey, ther as I yow devyse. \n"
			    "   But nathelees, whil I have tyme and space, \n"
			    " \n"
			    "Er that I ferther in this tale pace, \n"
			    "Me thynketh it acordaunt to resoun \n"
			    "To telle yow al the condicioun \n"
			    "Of ech of hem, so as it semed me, \n"
			    "And whiche they weren, and of what degree, \n"
			    " \n"
			    "And eek in what array that they were inne; \n"
			    "And at a knyght than wol I first bigynne. \n"
			    "   A knyght ther was, and that a worthy man, \n"
			    "That fro the tyme that he first bigan \n"
			    "To riden out, he loved chivalrie, \n"
			    " \n"
			    "Trouthe and honour, fredom and curteisie. \n"
			    "Ful worthy was he in his lordes werre, \n"
			    " \n"
			    "And therto hadde he riden, no man ferre, \n"
			    "As wel in Cristendom as in Hethenesse, \n"
			    "And evere honoured for his worthynesse. \n"
			    " \n"
			    "   At Alisaundre he was, whan it was wonne; \n"
			    "Ful ofte tyme he hadde the bord bigonne \n"
			    "Aboven alle nacions in Pruce; \n"
			    "In Lettow hadde he reysed, and in Ruce, \n"
			    "No cristen man so ofte of his degree. \n",
			    -1);

  return sw;
}

static GtkWidget *
upper_right_new (void)
{
  GtkWidget *notebook;

  notebook = gtk_notebook_new ();

  gtk_notebook_append_page (GTK_NOTEBOOK (notebook),
			    text_view_new (),
			    gtk_label_new ("First"));

  gtk_notebook_append_page (GTK_NOTEBOOK (notebook),
			    text_view_new (),
			    gtk_label_new ("Second"));

  gtk_notebook_append_page (GTK_NOTEBOOK (notebook),
			    text_view_new (),
			    gtk_label_new ("Third"));

  return notebook;
}

static GtkWidget *
lower_right_new (void)
{
  return tree_view_new ();
}

static GtkWidget *
right_pane_new (void)
{
  GtkWidget *paned;
  GtkWidget *upper_right;
  GtkWidget *lower_right;

  paned = gtk_vpaned_new ();

  upper_right = upper_right_new ();
  gtk_paned_pack1 (GTK_PANED (paned), upper_right, TRUE, TRUE);

  lower_right = lower_right_new ();
  gtk_paned_pack2 (GTK_PANED (paned), lower_right, TRUE, TRUE);

  return paned;
}

static GtkWidget *
content_area_new (void)
{
  GtkWidget *hpaned;
  GtkWidget *left, *right;

  hpaned = gtk_hpaned_new ();

  left = left_pane_new ();
  gtk_paned_pack1 (GTK_PANED (hpaned), left, TRUE, TRUE);

  right = right_pane_new ();
  gtk_paned_pack2 (GTK_PANED (hpaned), right, TRUE, TRUE);

  return hpaned;
}

static GtkWidget *
status_bar_new (void)
{
  return gtk_statusbar_new ();
}

static gboolean
delete_event_cb (GtkWidget *widget, GdkEvent *event, gpointer data)
{
  gtk_main_quit ();
  return FALSE;
}

GtkWidget *
appwindow_new (void)
{
  GtkWidget *window;
  GtkUIManager *ui;
  GtkWidget *vbox;
  GtkWidget *widget;

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window), "Main window");
  g_signal_connect (window, "delete-event",
		    G_CALLBACK (delete_event_cb), NULL);

  ui = uimanager_new ();
  g_signal_connect_swapped (window, "destroy",
			    G_CALLBACK (g_object_unref), ui);

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (window), vbox);

  widget = menubar_new (ui);
  gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 0);

  widget = toolbar_new (ui);
  gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 0);

  widget = content_area_new ();
  gtk_box_pack_start (GTK_BOX (vbox), widget, TRUE, TRUE, 0);

  widget = status_bar_new ();
  gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 0);

  return window;
}
