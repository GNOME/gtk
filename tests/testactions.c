/* testactions.c
 * Copyright (C) 2003  Matthias Clasen
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#define GDK_DISABLE_DEPRECATION_WARNINGS
#include <gtk/gtk.h>

static GtkActionGroup *action_group = NULL;
static GtkToolbar *toolbar = NULL;

static void
activate_action (GtkAction *action)
{
  const gchar *name = gtk_action_get_name (action);
  const gchar *typename = G_OBJECT_TYPE_NAME (action);

  g_message ("Action %s (type=%s) activated", name, typename);
}

static void
recent_action (GtkAction *action)
{
  const gchar *name = gtk_action_get_name (action);
  const gchar *typename = G_OBJECT_TYPE_NAME (action);
  gchar *uri = gtk_recent_chooser_get_current_uri (GTK_RECENT_CHOOSER (action));

  g_message ("Action %s (type=%s) activated (uri=%s)",
             name, typename,
             uri ? uri : "no item selected");
  g_free (uri);
}

static void
toggle_cnp_actions (GtkAction *action)
{
  gboolean sensitive = FALSE;

  action = gtk_action_group_get_action (action_group, "cut");
  g_object_set (action, "sensitive", sensitive, NULL);
  action = gtk_action_group_get_action (action_group, "copy");
  g_object_set (action, "sensitive", sensitive, NULL);
  action = gtk_action_group_get_action (action_group, "paste");
  g_object_set (action, "sensitive", sensitive, NULL);

  action = gtk_action_group_get_action (action_group, "toggle-cnp");
  if (sensitive)
    g_object_set (action, "label", "Disable Cut and paste ops", NULL);
  else
    g_object_set (action, "label", "Enable Cut and paste ops", NULL);
}

static void
show_accel_dialog (GtkAction *action)
{
  g_message ("Sorry, accel dialog not available");
}

static void
toolbar_size_small (GtkAction *action)
{
  g_return_if_fail (toolbar != NULL);

  gtk_toolbar_set_icon_size (toolbar, GTK_ICON_SIZE_SMALL_TOOLBAR);
}

static void
toolbar_size_large (GtkAction *action)
{
  g_return_if_fail (toolbar != NULL);

  gtk_toolbar_set_icon_size (toolbar, GTK_ICON_SIZE_LARGE_TOOLBAR);
}

/* convenience functions for declaring actions */
static GtkActionEntry entries[] = {
  { "Menu1Action", NULL, "Menu _1" },
  { "Menu2Action", NULL, "Menu _2" },
  { "Menu3Action", NULL, "_Dynamic Menu" },

  { "attach", "mail-attachment", "_Attachment...", "<Control>m",
    "Attach a file", G_CALLBACK (activate_action) },
  { "cut", NULL, "C_ut", "<control>X",
    "Cut the selected text to the clipboard", G_CALLBACK (activate_action) },
  { "copy", NULL, "_Copy", "<control>C",
    "Copy the selected text to the clipboard", G_CALLBACK (activate_action) },
  { "paste", NULL, "_Paste", "<control>V",
    "Paste the text from the clipboard", G_CALLBACK (activate_action) },
  { "quit", NULL,  NULL, "<control>Q",
    "Quit the application", G_CALLBACK (gtk_main_quit) },
  { "customise-accels", NULL, "Customise _Accels", NULL,
    "Customise keyboard shortcuts", G_CALLBACK (show_accel_dialog) },
  { "toolbar-small-icons", NULL, "Small Icons", NULL, 
    NULL, G_CALLBACK (toolbar_size_small) },
  { "toolbar-large-icons", NULL, "Large Icons", NULL,
    NULL, G_CALLBACK (toolbar_size_large) }
};
static guint n_entries = G_N_ELEMENTS (entries);

enum {
  JUSTIFY_LEFT,
  JUSTIFY_CENTER,
  JUSTIFY_RIGHT,
  JUSTIFY_FILL
};

/* XML description of the menus for the test app.  The parser understands
 * a subset of the Bonobo UI XML format, and uses GMarkup for parsing */
static const gchar *ui_info =
"  <menubar>\n"
"    <menu name=\"Menu _1\" action=\"Menu1Action\">\n"
"      <menuitem name=\"cut\" action=\"cut\" />\n"
"      <menuitem name=\"copy\" action=\"copy\" />\n"
"      <menuitem name=\"paste\" action=\"paste\" />\n"
"      <separator name=\"sep1\" />\n"
"      <menuitem name=\"bold1\" action=\"bold\" />\n"
"      <menuitem name=\"bold2\" action=\"bold\" />\n"
"      <separator name=\"sep2\" />\n"
"      <menuitem name=\"recent\" action=\"recent\" />\n"
"      <separator name=\"sep3\" />\n"
"      <menuitem name=\"toggle-cnp\" action=\"toggle-cnp\" />\n"
"      <separator name=\"sep4\" />\n"
"      <menuitem name=\"quit\" action=\"quit\" />\n"
"    </menu>\n"
"    <menu name=\"Menu _2\" action=\"Menu2Action\">\n"
"      <menuitem name=\"cut\" action=\"cut\" />\n"
"      <menuitem name=\"copy\" action=\"copy\" />\n"
"      <menuitem name=\"paste\" action=\"paste\" />\n"
"      <separator name=\"sep5\"/>\n"
"      <menuitem name=\"bold\" action=\"bold\" />\n"
"      <separator name=\"sep6\"/>\n"
"      <menuitem name=\"justify-left\" action=\"justify-left\" />\n"
"      <menuitem name=\"justify-center\" action=\"justify-center\" />\n"
"      <menuitem name=\"justify-right\" action=\"justify-right\" />\n"
"      <menuitem name=\"justify-fill\" action=\"justify-fill\" />\n"
"      <separator name=\"sep7\"/>\n"
"      <menuitem  name=\"customise-accels\" action=\"customise-accels\" />\n"
"      <separator name=\"sep8\"/>\n"
"      <menuitem action=\"toolbar-icons\" />\n"
"      <menuitem action=\"toolbar-text\" />\n"
"      <menuitem action=\"toolbar-both\" />\n"
"      <menuitem action=\"toolbar-both-horiz\" />\n"
"      <separator name=\"sep9\"/>\n"
"      <menuitem action=\"toolbar-small-icons\" />\n"
"      <menuitem action=\"toolbar-large-icons\" />\n"
"    </menu>\n"
"    <menu name=\"DynamicMenu\" action=\"Menu3Action\" />\n"
"  </menubar>\n"
"  <toolbar name=\"toolbar\">\n"
"    <toolitem name=\"attach\" action=\"attach\" />\n"
"    <toolitem name=\"cut\" action=\"cut\" />\n"
"    <toolitem name=\"copy\" action=\"copy\" />\n"
"    <toolitem name=\"paste\" action=\"paste\" />\n"
"    <toolitem name=\"recent\" action=\"recent\" />\n"
"    <separator name=\"sep10\" />\n"
"    <toolitem name=\"bold\" action=\"bold\" />\n"
"    <separator name=\"sep11\" />\n"
"    <toolitem name=\"justify-left\" action=\"justify-left\" />\n"
"    <toolitem name=\"justify-center\" action=\"justify-center\" />\n"
"    <toolitem name=\"justify-right\" action=\"justify-right\" />\n"
"    <toolitem name=\"justify-fill\" action=\"justify-fill\" />\n"
"    <separator name=\"sep12\"/>\n"
"    <toolitem name=\"quit\" action=\"quit\" />\n"
"  </toolbar>\n"
"  <popup name=\"popup\">\n"
"    <menuitem name=\"popcut\" action=\"cut\" />\n"
"    <menuitem name=\"popcopy\" action=\"copy\" />\n"
"    <menuitem name=\"poppaste\" action=\"paste\" />\n"
"  </popup>\n";

static guint ui_id = 0;
static GtkActionGroup *dag = NULL;

static void
create_window (GtkActionGroup *action_group)
{
 }

int
main (int argc, char **argv)
{
  GtkAction *action;

  gtk_init (&argc, &argv);

  if (g_file_test ("accels", G_FILE_TEST_IS_REGULAR))
    gtk_accel_map_load ("accels");

  action = gtk_recent_action_new ("recent",
                                  "Open Recent", "Open recent files",
                                  NULL);
  g_signal_connect (action, "item-activated",
                    G_CALLBACK (recent_action),
                    NULL);
  g_signal_connect (action, "activate",
                    G_CALLBACK (recent_action),
                    NULL);

  action_group = gtk_action_group_new ("TestActions");
  gtk_action_group_add_actions (action_group, 
				entries, n_entries, 
				NULL);
  gtk_action_group_add_action_with_accel (action_group, action, NULL);

  create_window (action_group);

  gtk_main ();

  g_object_unref (action);
  g_object_unref (action_group);

  gtk_accel_map_save ("accels");

  return 0;
}
