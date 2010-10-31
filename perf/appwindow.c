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

#include "widgets.h"

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

static GtkWidget *
drawing_area_new (void)
{
  GtkWidget *darea;

  darea = gtk_drawing_area_new ();
  gtk_widget_set_size_request (darea, 640, 480);
  return darea;
}

static GtkWidget *
content_area_new (void)
{
  GtkWidget *notebook;

  notebook = gtk_notebook_new ();

  gtk_notebook_append_page (GTK_NOTEBOOK (notebook),
			    drawing_area_new (),
			    gtk_label_new ("First"));

  gtk_notebook_append_page (GTK_NOTEBOOK (notebook),
			    drawing_area_new (),
			    gtk_label_new ("Second"));

  gtk_notebook_append_page (GTK_NOTEBOOK (notebook),
			    drawing_area_new (),
			    gtk_label_new ("Third"));

  return notebook;
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

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add (GTK_CONTAINER (window), vbox);

  widget = menubar_new (ui);
  gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 0);

  widget = toolbar_new (ui);
  gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 0);

  widget = content_area_new ();
  gtk_box_pack_start (GTK_BOX (vbox), widget, TRUE, TRUE, 0);

  widget = status_bar_new ();
  gtk_box_pack_end (GTK_BOX (vbox), widget, FALSE, FALSE, 0);

  return window;
}
