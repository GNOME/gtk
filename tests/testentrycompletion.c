/* testentrycompletion.c
 * Copyright (C) 2004  Red Hat, Inc.
 * Author: Matthias Clasen
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
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>

static GtkWidget *window = NULL;


/* Creates a list model containing the completions */
static GListModel *
create_simple_completion_model (void)
{
  const char *strings[] = {
    "GNOME",
    "gnominious",
    "Gnomonic projection",
    "total",
    "totally",
    "toto",
    "tottery",
    "totterer",
    "Totten trust",
    "totipotent",
    "totipotency",
    "totemism",
    "totem pole",
    "Totara",
    "totalizer",
    "totalizator",
    "totalitarianism",
    "total parenteral nutrition",
    "total hysterectomy",
    "total eclipse",
    "Totipresence",
    "Totipalmi",
    "zombie",
    "a\303\246x",
    "a\303\246y",
    "a\303\246z",
    NULL
  };

  return G_LIST_MODEL (gtk_string_list_new (strings));
}

static char *
get_file_name (gpointer item)
{
  return g_strdup (g_file_info_get_display_name (G_FILE_INFO (item)));
}

static void
setup_item (GtkSignalListItemFactory *factory,
            GtkListItem              *item)
{
  GtkWidget *box;
  GtkWidget *icon;
  GtkWidget *label;

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
  icon = gtk_image_new ();
  label = gtk_label_new ("");
  gtk_label_set_xalign (GTK_LABEL (label), 0);
  gtk_box_append (GTK_BOX (box), icon);
  gtk_box_append (GTK_BOX (box), label);
  gtk_list_item_set_child (item, box);
}

static void
bind_item (GtkSignalListItemFactory *factory,
           GtkListItem              *item)
{
  GFileInfo *info = G_FILE_INFO (gtk_list_item_get_item (item));
  GtkWidget *box = gtk_list_item_get_child (item);
  GtkWidget *icon = gtk_widget_get_first_child (box);
  GtkWidget *label = gtk_widget_get_last_child (box);

  gtk_image_set_from_gicon (GTK_IMAGE (icon), g_file_info_get_icon (info));
  gtk_label_set_label (GTK_LABEL (label), g_file_info_get_display_name (info));
}

int
main (int argc, char *argv[])
{
  GtkWidget *vbox;
  GtkWidget *label;
  GtkWidget *entry;
  GtkEntryCompletion *completion;
  GListModel *model;
  char *cwd;
  GFile *file;
  GtkExpression *expression;
  GtkListItemFactory *factory;

  gtk_init ();

  window = gtk_window_new ();

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
  gtk_widget_set_margin_start (vbox, 5);
  gtk_widget_set_margin_end (vbox, 5);
  gtk_widget_set_margin_top (vbox, 5);
  gtk_widget_set_margin_bottom (vbox, 5);
  gtk_window_set_child (GTK_WINDOW (window), vbox);

  label = gtk_label_new (NULL);

  gtk_label_set_markup (GTK_LABEL (label), "Completion demo, try writing <b>total</b> or <b>gnome</b> for example.");
  gtk_box_append (GTK_BOX (vbox), label);

  entry = gtk_entry_new ();

  completion = gtk_entry_completion_new ();
  gtk_entry_completion_set_inline_completion (completion, TRUE);
  model = create_simple_completion_model ();
  gtk_entry_completion_set_model (completion, model);
  g_object_unref (model);

  gtk_entry_set_completion (GTK_ENTRY (entry), completion);
  g_object_unref (completion);

  gtk_box_append (GTK_BOX (vbox), entry);

  entry = gtk_entry_new ();

  completion = gtk_entry_completion_new ();
  gtk_entry_set_completion (GTK_ENTRY (entry), completion);

  gtk_box_append (GTK_BOX (vbox), entry);

  cwd = g_get_current_dir ();
  file = g_file_new_for_path (cwd);
  model = G_LIST_MODEL (gtk_directory_list_new ("standard::display-name,standard::content-type,standard::icon,standard::size", file));
  gtk_entry_completion_set_model (completion, model);
  g_object_unref (model);
  g_object_unref (file);
  g_free (cwd);

  expression = gtk_cclosure_expression_new (G_TYPE_STRING, NULL,
                                            0, NULL,
                                            (GCallback)get_file_name,
                                            NULL, NULL);
  gtk_entry_completion_set_expression (completion, expression);

  factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (factory, "setup", G_CALLBACK (setup_item), NULL);
  g_signal_connect (factory, "bind", G_CALLBACK (bind_item), NULL);
  gtk_entry_completion_set_factory (completion, factory);
  g_object_unref (factory);

  gtk_expression_unref (expression);

  g_object_unref (completion);

  gtk_window_present (GTK_WINDOW (window));

  while (g_list_model_get_n_items (gtk_window_get_toplevels ()) > 0)
    g_main_context_iteration (NULL, TRUE);

  gtk_window_destroy (GTK_WINDOW (window));

  return 0;
}


