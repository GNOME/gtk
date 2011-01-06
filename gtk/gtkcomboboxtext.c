/* GTK - The GIMP Toolkit
 *
 * Copyright (C) 2010 Christian Dywan
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gtkcomboboxtext.h"
#include "gtkcombobox.h"
#include "gtkcellrenderertext.h"
#include "gtkcelllayout.h"
#include "gtkalias.h"


/**
 * SECTION:gtkcomboboxtext
 * @Short_description: A simple, text-only combo box
 * @Title: GtkComboBoxText
 * @See_also: @GtkComboBox
 *
 * A GtkComboBoxText is a simple variant of #GtkComboBox that hides
 * the model-view complexity for simple text-only use cases.
 *
 * To create a GtkComboBoxText, use gtk_combo_box_text_new() or
 * gtk_combo_box_text_new_with_entry().
 *
 * You can add items to a GtkComboBoxText with
 * gtk_combo_box_text_append_text(), gtk_combo_box_text_insert_text()
 * or gtk_combo_box_text_prepend_text() and remove options with
 * gtk_combo_box_text_remove_text().
 *
 * If the GtkComboBoxText contains an entry (via the 'has-entry' property),
 * its contents can be retrieved using gtk_combo_box_text_get_active_text().
 * The entry itself can be accessed by calling gtk_bin_get_child() on the
 * combo box.
 */

G_DEFINE_TYPE (GtkComboBoxText, gtk_combo_box_text, GTK_TYPE_COMBO_BOX);

static GObject *
gtk_combo_box_text_constructor (GType                  type,
                                guint                  n_construct_properties,
                                GObjectConstructParam *construct_properties)
{
  GObject            *object;

  object = G_OBJECT_CLASS (gtk_combo_box_text_parent_class)->constructor
    (type, n_construct_properties, construct_properties);

  if (!gtk_combo_box_get_has_entry (GTK_COMBO_BOX (object)))
    {
      GtkCellRenderer *cell;

      cell = gtk_cell_renderer_text_new ();
      gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (object), cell, TRUE);
      gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (object), cell,
                                      "text", 0,
                                      NULL);
    }

  return object;
}

static void
gtk_combo_box_text_init (GtkComboBoxText *combo_box)
{
  GtkListStore *store;

  store = gtk_list_store_new (1, G_TYPE_STRING);
  gtk_combo_box_set_model (GTK_COMBO_BOX (combo_box), GTK_TREE_MODEL (store));
  g_object_unref (store);
}

static void
gtk_combo_box_text_class_init (GtkComboBoxTextClass *klass)
{
  GObjectClass *object_class;

  object_class = (GObjectClass *)klass;
  object_class->constructor = gtk_combo_box_text_constructor;
}


/**
 * gtk_combo_box_text_new:
 *
 * Creates a new #GtkComboBoxText, which is a #GtkComboBox just displaying
 * strings. See gtk_combo_box_entry_new_with_text().
 *
 * Return value: A new #GtkComboBoxText
 *
 * Since: 2.24
 */
GtkWidget *
gtk_combo_box_text_new (void)
{
  return g_object_new (GTK_TYPE_COMBO_BOX_TEXT,
                       "entry-text-column", 0,
                       NULL);
}

/**
 * gtk_combo_box_text_new_with_entry:
 *
 * Creates a new #GtkComboBoxText, which is a #GtkComboBox just displaying
 * strings. The combo box created by this function has an entry.
 *
 * Return value: a new #GtkComboBoxText
 *
 * Since: 2.24
 */
GtkWidget *
gtk_combo_box_text_new_with_entry (void)
{
  return g_object_new (GTK_TYPE_COMBO_BOX_TEXT,
                       "has-entry", TRUE,
                       "entry-text-column", 0,
                       NULL);
}

/**
 * gtk_combo_box_text_append_text:
 * @combo_box: A #GtkComboBoxText
 * @text: A string
 *
 * Appends @string to the list of strings stored in @combo_box.
 *
 * Since: 2.24
 */
void
gtk_combo_box_text_append_text (GtkComboBoxText *combo_box,
                                const gchar     *text)
{
  GtkListStore *store;
  GtkTreeIter iter;
  gint text_column;
  gint column_type;

  g_return_if_fail (GTK_IS_COMBO_BOX_TEXT (combo_box));
  g_return_if_fail (text != NULL);

  store = GTK_LIST_STORE (gtk_combo_box_get_model (GTK_COMBO_BOX (combo_box)));
  g_return_if_fail (GTK_IS_LIST_STORE (store));
  text_column = gtk_combo_box_get_entry_text_column (GTK_COMBO_BOX (combo_box));
  g_return_if_fail (text_column == 0);
  column_type = gtk_tree_model_get_column_type (GTK_TREE_MODEL (store), text_column);
  g_return_if_fail (column_type == G_TYPE_STRING);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, text_column, text, -1);
}

/**
 * gtk_combo_box_text_insert_text:
 * @combo_box: A #GtkComboBoxText
 * @position: An index to insert @text
 * @text: A string
 *
 * Inserts @string at @position in the list of strings stored in @combo_box.
 *
 * Since: 2.24
 */
void
gtk_combo_box_text_insert_text (GtkComboBoxText *combo_box,
                                gint             position,
                                const gchar     *text)
{
  GtkListStore *store;
  GtkTreeIter iter;
  gint text_column;
  gint column_type;

  g_return_if_fail (GTK_IS_COMBO_BOX_TEXT (combo_box));
  g_return_if_fail (position >= 0);
  g_return_if_fail (text != NULL);

  store = GTK_LIST_STORE (gtk_combo_box_get_model (GTK_COMBO_BOX (combo_box)));
  g_return_if_fail (GTK_IS_LIST_STORE (store));
  text_column = gtk_combo_box_get_entry_text_column (GTK_COMBO_BOX (combo_box));
  column_type = gtk_tree_model_get_column_type (GTK_TREE_MODEL (store), text_column);
  g_return_if_fail (column_type == G_TYPE_STRING);

  gtk_list_store_insert (store, &iter, position);
  gtk_list_store_set (store, &iter, text_column, text, -1);
}

/**
 * gtk_combo_box_text_prepend_text:
 * @combo_box: A #GtkComboBox
 * @text: A string
 *
 * Prepends @string to the list of strings stored in @combo_box.
 *
 * Since: 2.24
 */
void
gtk_combo_box_text_prepend_text (GtkComboBoxText *combo_box,
                                 const gchar     *text)
{
  GtkListStore *store;
  GtkTreeIter iter;
  gint text_column;
  gint column_type;

  g_return_if_fail (GTK_IS_COMBO_BOX_TEXT (combo_box));
  g_return_if_fail (text != NULL);

  store = GTK_LIST_STORE (gtk_combo_box_get_model (GTK_COMBO_BOX (combo_box)));
  g_return_if_fail (GTK_IS_LIST_STORE (store));
  text_column = gtk_combo_box_get_entry_text_column (GTK_COMBO_BOX (combo_box));
  column_type = gtk_tree_model_get_column_type (GTK_TREE_MODEL (store), text_column);
  g_return_if_fail (column_type == G_TYPE_STRING);

  gtk_list_store_prepend (store, &iter);
  gtk_list_store_set (store, &iter, text_column, text, -1);
}

/**
 * gtk_combo_box_text_remove:
 * @combo_box: A #GtkComboBox
 * @position: Index of the item to remove
 *
 * Removes the string at @position from @combo_box.
 *
 * Since: 2.24
 */
void
gtk_combo_box_text_remove (GtkComboBoxText *combo_box,
                           gint             position)
{
  GtkTreeModel *model;
  GtkListStore *store;
  GtkTreeIter iter;

  g_return_if_fail (GTK_IS_COMBO_BOX_TEXT (combo_box));
  g_return_if_fail (position >= 0);

  model = gtk_combo_box_get_model (GTK_COMBO_BOX (combo_box));
  store = GTK_LIST_STORE (model);
  g_return_if_fail (GTK_IS_LIST_STORE (store));

  if (gtk_tree_model_iter_nth_child (model, &iter, NULL, position))
    gtk_list_store_remove (store, &iter);
}

/**
 * gtk_combo_box_text_get_active_text:
 * @combo_box: A #GtkComboBoxText
 *
 * Returns the currently active string in @combo_box or %NULL if none
 * is selected. If @combo_box contains an entry, this function will return
 * its contents (which will not necessarily be an item from the list).
 *
 * Returns: a newly allocated string containing the currently active text.
 *     Must be freed with g_free().
 *
 * Since: 2.24
 */
gchar *
gtk_combo_box_text_get_active_text (GtkComboBoxText *combo_box)
{
  GtkTreeIter iter;
  gchar *text = NULL;

  g_return_val_if_fail (GTK_IS_COMBO_BOX_TEXT (combo_box), NULL);

  if (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (combo_box), &iter))
    {
      GtkTreeModel *model;
      gint text_column;
      gint column_type;

      model = gtk_combo_box_get_model (GTK_COMBO_BOX (combo_box));
      g_return_val_if_fail (GTK_IS_LIST_STORE (model), NULL);
      text_column = gtk_combo_box_get_entry_text_column (GTK_COMBO_BOX (combo_box));
      column_type = gtk_tree_model_get_column_type (model, text_column);
      g_return_val_if_fail (column_type == G_TYPE_STRING, NULL);
      gtk_tree_model_get (model, &iter, text_column, &text, -1);
    }

  return text;
}

#define __GTK_COMBO_BOX_TEXT_C__
#include "gtkaliasdef.c"
