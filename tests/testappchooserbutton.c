/* testappchooserbutton.c
 * Copyright (C) 2010 Red Hat, Inc.
 * Authors: Cosimo Cecchi
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
#include <gtk/gtk.h>

#define CUSTOM_ITEM "custom-item"

static GtkWidget *toplevel, *combobox, *box;
static GtkWidget *sel_image, *sel_name;

static void
combo_changed_cb (GtkComboBox *cb,
                  gpointer     user_data)
{
  GAppInfo *app_info;

  app_info = gtk_app_chooser_get_app_info (GTK_APP_CHOOSER (cb));

  if (app_info == NULL)
    return;

  gtk_image_set_from_gicon (GTK_IMAGE (sel_image), g_app_info_get_icon (app_info),
                            GTK_ICON_SIZE_DIALOG);
  gtk_label_set_text (GTK_LABEL (sel_name), g_app_info_get_display_name (app_info));

  g_object_unref (app_info);
}

static void
special_item_activated_cb (GtkAppChooserButton *b,
                           const gchar *item_name,
                           gpointer user_data)
{
  gtk_image_set_from_gicon (GTK_IMAGE (sel_image), g_themed_icon_new ("face-smile"),
                            GTK_ICON_SIZE_DIALOG);
  gtk_label_set_text (GTK_LABEL (sel_name), "Special Item");
}

static void
action_cb (GtkAppChooserButton *b,
           const gchar *item_name,
           gpointer user_data)
{
  g_print ("Activated custom item %s\n", item_name);
}

int
main (int argc,
      char **argv)
{
  GtkWidget *w;

  gtk_init (&argc, &argv);

  toplevel = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_container_set_border_width (GTK_CONTAINER (toplevel), 12);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_container_add (GTK_CONTAINER (toplevel), box);

  combobox = gtk_app_chooser_button_new ("image/jpeg");
  gtk_box_pack_start (GTK_BOX (box), combobox, TRUE, TRUE, 0);

  g_signal_connect (combobox, "changed",
                    G_CALLBACK (combo_changed_cb), NULL);

  w = gtk_label_new (NULL);
  gtk_label_set_markup (GTK_LABEL (w), "<b>Selected app info</b>");
  gtk_box_pack_start (GTK_BOX (box), w, TRUE, TRUE, 0);

  w = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_box_pack_start (GTK_BOX (box), w, TRUE, TRUE, 0);

  sel_image = gtk_image_new ();
  gtk_box_pack_start (GTK_BOX (w), sel_image, TRUE, TRUE, 0);
  sel_name = gtk_label_new (NULL);
  gtk_box_pack_start (GTK_BOX (w), sel_name, TRUE, TRUE, 0);

  gtk_app_chooser_button_set_heading (GTK_APP_CHOOSER_BUTTON (combobox), "Choose one, <i>not</i> two");
  gtk_app_chooser_button_append_separator (GTK_APP_CHOOSER_BUTTON (combobox));
  gtk_app_chooser_button_append_custom_item (GTK_APP_CHOOSER_BUTTON (combobox),
                                             CUSTOM_ITEM,
                                             "Hey, I'm special!",
                                             g_themed_icon_new ("face-smile"));

  /* this one will trigger a warning, and will not be added */
  gtk_app_chooser_button_append_custom_item (GTK_APP_CHOOSER_BUTTON (combobox),
                                             CUSTOM_ITEM,
                                             "Hey, I'm fake!",
                                             g_themed_icon_new ("face-evil"));

  gtk_app_chooser_button_set_show_dialog_item (GTK_APP_CHOOSER_BUTTON (combobox),
                                               TRUE);
  gtk_app_chooser_button_set_show_default_item (GTK_APP_CHOOSER_BUTTON (combobox),
                                                TRUE);

  /* connect to the detailed signal */
  g_signal_connect (combobox, "custom-item-activated::" CUSTOM_ITEM,
                    G_CALLBACK (special_item_activated_cb), NULL);

  /* connect to the generic signal too */
  g_signal_connect (combobox, "custom-item-activated",
                    G_CALLBACK (action_cb), NULL);

  /* test refresh on a combo */
  gtk_app_chooser_refresh (GTK_APP_CHOOSER (combobox));

#if 0
  gtk_app_chooser_button_set_active_custom_item (GTK_APP_CHOOSER_BUTTON (combobox),
                                                 CUSTOM_ITEM);
#endif
  gtk_widget_show_all (toplevel);

  g_signal_connect (toplevel, "delete-event",
                    G_CALLBACK (gtk_main_quit), NULL);

  gtk_main ();

  return EXIT_SUCCESS;
}
