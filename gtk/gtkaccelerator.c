/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#include <ctype.h>
#include "gtkaccelerator.h"
#include "gtksignal.h"
#include "gtkwidget.h"


typedef struct _GtkAcceleratorEntry GtkAcceleratorEntry;

struct _GtkAcceleratorEntry
{
  guint8 modifiers;
  GtkObject *object;
  guint signal_id;
};


static void gtk_accelerator_table_init  (GtkAcceleratorTable *table);
static void gtk_accelerator_table_clean (GtkAcceleratorTable *table);


static GtkAcceleratorTable *default_table = NULL;
static GSList *tables = NULL;
static guint8 gtk_accelerator_table_default_mod_mask = (guint8) ~0;


GtkAcceleratorTable*
gtk_accelerator_table_new ()
{
  GtkAcceleratorTable *table;

  table = g_new (GtkAcceleratorTable, 1);
  gtk_accelerator_table_init (table);

  tables = g_slist_prepend (tables, table);

  return table;
}

GtkAcceleratorTable*
gtk_accelerator_table_find (GtkObject   *object,
			    const gchar *signal_name,
			    guchar       accelerator_key,
			    guint8       accelerator_mods)
{
  GtkAcceleratorTable *table;
  GtkAcceleratorEntry *entry;
  GSList *tmp_list;
  GList *entries;
  guint signal_id;
  guint hash;

  g_return_val_if_fail (object != NULL, NULL);
  g_return_val_if_fail (signal_name != NULL, NULL);

  signal_id = gtk_signal_lookup (signal_name, GTK_OBJECT_TYPE (object));
  hash = (guint) accelerator_key;

  tmp_list = tables;
  while (tmp_list)
    {
      table = tmp_list->data;
      tmp_list = tmp_list->next;

      entries = table->entries[hash];
      while (entries)
	{
	  entry = entries->data;
	  entries = entries->next;

	  if ((entry->object == object) &&
	      (entry->signal_id == signal_id) &&
	      ((entry->modifiers & table->modifier_mask) ==
	       (accelerator_mods & table->modifier_mask)))
	    return table;
	}
    }

  return NULL;
}

GtkAcceleratorTable*
gtk_accelerator_table_ref (GtkAcceleratorTable *table)
{
  g_return_val_if_fail (table != NULL, NULL);

  table->ref_count += 1;
  return table;
}

void
gtk_accelerator_table_unref (GtkAcceleratorTable *table)
{
  g_return_if_fail (table != NULL);

  table->ref_count -= 1;
  if (table->ref_count <= 0)
    {
      tables = g_slist_remove (tables, table);
      gtk_accelerator_table_clean (table);
      g_free (table);
    }
}

void
gtk_accelerator_table_install (GtkAcceleratorTable *table,
			       GtkObject           *object,
			       const gchar         *signal_name,
			       guchar               accelerator_key,
			       guint8               accelerator_mods)
{
  GtkAcceleratorEntry *entry;
  GList *entries;
  gchar *signame;
  guint signal_id;
  guint hash;

  g_return_if_fail (object != NULL);

  if (!table)
    {
      if (!default_table)
	default_table = gtk_accelerator_table_new ();
      table = default_table;
    }

  signal_id = gtk_signal_lookup (signal_name, GTK_OBJECT_TYPE (object));
  g_return_if_fail (signal_id != 0);

  hash = (guint) accelerator_key;
  entries = table->entries[hash];

  while (entries)
    {
      entry = entries->data;

      if ((entry->modifiers & table->modifier_mask) ==
          (accelerator_mods & table->modifier_mask))
	{
	  if (GTK_IS_WIDGET (entry->object))
	    {
	      signame = gtk_signal_name (entry->signal_id);
	      gtk_signal_emit_by_name (entry->object,
				       "remove_accelerator",
				       signame);
	    }

	  entry->modifiers = accelerator_mods;
	  entry->object = object;
	  entry->signal_id = signal_id;
	  return;
	}

      entries = entries->next;
    }

  entry = g_new (GtkAcceleratorEntry, 1);
  entry->modifiers = accelerator_mods;
  entry->object = object;
  entry->signal_id = signal_id;

  table->entries[hash] = g_list_prepend (table->entries[hash], entry);
}

void
gtk_accelerator_table_remove (GtkAcceleratorTable *table,
			      GtkObject           *object,
			      const gchar         *signal_name)
{
  GtkAcceleratorEntry *entry;
  GList *entries;
  GList *temp_list;
  guint signal_id;
  gint i;

  g_return_if_fail (object != NULL);

  if (!table)
    {
      if (!default_table)
	default_table = gtk_accelerator_table_new ();
      table = default_table;
    }

  signal_id = gtk_signal_lookup (signal_name, GTK_OBJECT_TYPE (object));
  g_return_if_fail (signal_id != 0);

  for (i = 0; i < 256; i++)
    {
      entries = table->entries[i];

      while (entries)
	{
	  entry = entries->data;

	  if ((entry->object == object) && (entry->signal_id == signal_id))
	    {
	      g_free (entry);

	      temp_list = entries;
	      if (entries->next)
		entries->next->prev = entries->prev;
	      if (entries->prev)
		entries->prev->next = entries->next;
	      if (table->entries[i] == entries)
		table->entries[i] = entries->next;

	      temp_list->next = NULL;
	      temp_list->prev = NULL;
	      g_list_free (temp_list);

	      return;
	    }

	  entries = entries->next;
	}
    }
}

gint
gtk_accelerator_table_check (GtkAcceleratorTable *table,
			     const guchar         accelerator_key,
			     guint8               accelerator_mods)
{
  GtkAcceleratorEntry *entry;
  GList *entries;
  guint hash;

  if (!table)
    {
      if (!default_table)
	default_table = gtk_accelerator_table_new ();
      table = default_table;
    }

  hash = (guint) accelerator_key;
  entries = table->entries[hash];

  while (entries)
    {
      entry = entries->data;

      if ((entry->modifiers & table->modifier_mask) ==
          (accelerator_mods & table->modifier_mask))
	{
	  gtk_signal_emit (entry->object, entry->signal_id);
	  return TRUE;
	}

      entries = entries->next;
    }

  if (!isupper (hash))
    {
      hash = toupper (hash);
      entries = table->entries[hash];

      while (entries)
	{
	  entry = entries->data;

	  if (((entry->modifiers & table->modifier_mask) ==
	       (accelerator_mods & table->modifier_mask)) &&
	      (GTK_IS_WIDGET (entry->object) &&
	       GTK_WIDGET_SENSITIVE (entry->object)))
	    {
	      gtk_signal_emit (entry->object, entry->signal_id);
	      return TRUE;
	    }

	  entries = entries->next;
	}
    }

  return FALSE;
}

void
gtk_accelerator_table_set_mod_mask (GtkAcceleratorTable *table,
                                    guint8               modifier_mask)
{
  if (table == NULL)
    {
      gtk_accelerator_table_default_mod_mask = modifier_mask;
    }
  else
    {
      table->modifier_mask = modifier_mask;
    }
}

static void
gtk_accelerator_table_init (GtkAcceleratorTable *table)
{
  gint i;

  g_return_if_fail (table != NULL);

  for (i = 0; i < 256; i++)
    table->entries[i] = NULL;

  table->ref_count = 1;
  table->modifier_mask = gtk_accelerator_table_default_mod_mask;
}

static void
gtk_accelerator_table_clean (GtkAcceleratorTable *table)
{
  GtkAcceleratorEntry *entry;
  GList *entries;
  gint i;

  g_return_if_fail (table != NULL);

  for (i = 0; i < 256; i++)
    {
      entries = table->entries[i];
      while (entries)
	{
	  entry = entries->data;
	  entries = entries->next;

	  g_free (entry);
	}

      g_list_free (table->entries[i]);
      table->entries[i] = NULL;
    }
}
