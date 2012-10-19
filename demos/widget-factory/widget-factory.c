/* widget-factory: a collection of widgets in a single page, for easy
 *                 theming
 *
 * Copyright (C) 2011 Canonical Ltd
 *
 * This  library is free  software; you can  redistribute it and/or
 * modify it  under  the terms  of the  GNU Lesser  General  Public
 * License  as published  by the Free  Software  Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed  in the hope that it will be useful,
 * but  WITHOUT ANY WARRANTY; without even  the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by Andrea Cimitan <andrea.cimitan@canonical.com>
 *
 */

#include "config.h"
#include <gtk/gtk.h>

static void
dark_toggled (GtkCheckMenuItem *item, gpointer data)
{
  gboolean dark;

  dark = gtk_check_menu_item_get_active (item);
  g_object_set (gtk_settings_get_default (),
                "gtk-application-prefer-dark-theme", dark,
                NULL);
}

static void
show_about (GtkMenuItem *item, GtkWidget *window)
{
  GdkPixbuf *pixbuf;
  const gchar *authors[] = {
    "Andrea Cimitan",
    "Cosimo Cecchi",
    NULL
  };

  pixbuf = gdk_pixbuf_new_from_resource ("/logos/gtk-logo-256.png", NULL);

  gtk_show_about_dialog (GTK_WINDOW (window),
                         "program-name", "GTK+ Widget Factory",
                         "version", g_strdup_printf ("%s,\nRunning against GTK+ %d.%d.%d",
                                                     PACKAGE_VERSION,
                                                     gtk_get_major_version (),
                                                     gtk_get_minor_version (),
                                                     gtk_get_micro_version ()),
                         "copyright", "(C) 1997-2009 The GTK+ Team",
                         "license-type", GTK_LICENSE_LGPL_2_1,
                         "website", "http://www.gtk.org",
                         "comments", "Program to demonstrate GTK+ themes and widgets",
                         "authors", authors,
                         "logo", pixbuf,
                         "title", "About GTK+ Widget Factory",
                         NULL);

  g_object_unref (pixbuf);
}

static void
on_page_toggled (GtkToggleButton *button,
                 GtkNotebook     *pages)
{
  gint page;

  if (!gtk_toggle_button_get_active (button))
    return;

  page = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (button), "page"));
  gtk_notebook_set_current_page (pages, page);
}

int
main (int argc, char *argv[])
{
  GtkBuilder *builder;
  GtkWidget  *window;
  GtkWidget  *widget;
  GtkWidget  *notebook;
  gboolean    dark = FALSE;

  gtk_init (&argc, &argv);

  if (argc > 1 && (g_strcmp0 (argv[1], "--dark") == 0))
    dark = TRUE;

  builder = gtk_builder_new ();
  gtk_builder_add_from_resource (builder, "/ui/widget-factory.ui", NULL);

  window = GTK_WIDGET (gtk_builder_get_object (builder, "window"));
  gtk_builder_connect_signals (builder, NULL);

  widget = (GtkWidget*) gtk_builder_get_object (builder, "darkmenuitem");
  g_signal_connect (widget, "toggled", G_CALLBACK (dark_toggled), NULL);
  gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (widget), dark);

  notebook = (GtkWidget*) gtk_builder_get_object (builder, "toplevel_notebook");
  widget = (GtkWidget*) gtk_builder_get_object (builder, "togglepage1");
  g_object_set_data (G_OBJECT (widget), "page", GINT_TO_POINTER (0));
  g_signal_connect (widget, "toggled", G_CALLBACK (on_page_toggled), notebook);

  widget = (GtkWidget*) gtk_builder_get_object (builder, "togglepage2");
  g_object_set_data (G_OBJECT (widget), "page", GINT_TO_POINTER (1));
  g_signal_connect (widget, "toggled", G_CALLBACK (on_page_toggled), notebook);

  widget = (GtkWidget*) gtk_builder_get_object (builder, "aboutmenuitem");
  g_signal_connect (widget, "activate", G_CALLBACK (show_about), window);

  g_object_unref (G_OBJECT (builder));

  gtk_widget_show (window);
  gtk_main ();

  return 0;
}
