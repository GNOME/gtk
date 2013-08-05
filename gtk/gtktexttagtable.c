/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "config.h"

#include "gtktexttagtable.h"

#include "gtkbuildable.h"
#include "gtktexttagprivate.h"
#include "gtkmarshalers.h"
#include "gtktextbuffer.h" /* just for the lame notify_will_remove_tag hack */
#include "gtkintl.h"

#include <stdlib.h>


/**
 * SECTION:gtktexttagtable
 * @Short_description: Collection of tags that can be used together
 * @Title: GtkTextTagTable
 *
 * You may wish to begin by reading the <link linkend="TextWidget">text widget
 * conceptual overview</link> which gives an overview of all the objects and
 * data types related to the text widget and how they work together.
 *
 * <refsect2 id="GtkTextTagTable-BUILDER-UI">
 * <title>GtkTextTagTables as GtkBuildable</title>
 * <para>
 * The GtkTextTagTable implementation of the GtkBuildable interface
 * supports adding tags by specifying "tag" as the "type"
 * attribute of a &lt;child&gt; element.
 *
 * <example>
 * <title>A UI definition fragment specifying tags</title>
 * <programlisting><![CDATA[
 * <object class="GtkTextTagTable">
 *  <child type="tag">
 *    <object class="GtkTextTag"/>
 *  </child>
 * </object>
 * ]]></programlisting>
 * </example>
 * </para>
 * </refsect2>
 */

struct _GtkTextTagTablePrivate
{
  GHashTable *hash;
  GSList     *anonymous;
  GSList     *buffers;

  gint anon_count;
};

enum {
  TAG_CHANGED,
  TAG_ADDED,
  TAG_REMOVED,
  LAST_SIGNAL
};

static void gtk_text_tag_table_finalize                 (GObject             *object);

static void gtk_text_tag_table_buildable_interface_init (GtkBuildableIface   *iface);
static void gtk_text_tag_table_buildable_add_child      (GtkBuildable        *buildable,
							 GtkBuilder          *builder,
							 GObject             *child,
							 const gchar         *type);

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_CODE (GtkTextTagTable, gtk_text_tag_table, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (GtkTextTagTable)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                                gtk_text_tag_table_buildable_interface_init))

static void
gtk_text_tag_table_class_init (GtkTextTagTableClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gtk_text_tag_table_finalize;

  /**
   * GtkTextTagTable::tag-changed:
   * @texttagtable: the object which received the signal.
   * @tag: the changed tag.
   * @size_changed: whether the size has been changed.
   */
  signals[TAG_CHANGED] =
    g_signal_new (I_("tag-changed"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkTextTagTableClass, tag_changed),
                  NULL, NULL,
                  _gtk_marshal_VOID__OBJECT_BOOLEAN,
                  G_TYPE_NONE,
                  2,
                  GTK_TYPE_TEXT_TAG,
                  G_TYPE_BOOLEAN);  

  /**
   * GtkTextTagTable::tag-added:
   * @texttagtable: the object which received the signal.
   * @tag: the added tag.
   */
  signals[TAG_ADDED] =
    g_signal_new (I_("tag-added"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkTextTagTableClass, tag_added),
                  NULL, NULL,
                  _gtk_marshal_VOID__OBJECT,
                  G_TYPE_NONE,
                  1,
                  GTK_TYPE_TEXT_TAG);

  /**
   * GtkTextTagTable::tag-removed:
   * @texttagtable: the object which received the signal.
   * @tag: the removed tag.
   */
  signals[TAG_REMOVED] =
    g_signal_new (I_("tag-removed"),  
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkTextTagTableClass, tag_removed),
                  NULL, NULL,
                  _gtk_marshal_VOID__OBJECT,
                  G_TYPE_NONE,
                  1,
                  GTK_TYPE_TEXT_TAG);
}

static void
gtk_text_tag_table_init (GtkTextTagTable *table)
{
  table->priv = gtk_text_tag_table_get_instance_private (table);
  table->priv->hash = g_hash_table_new (g_str_hash, g_str_equal);
}

/**
 * gtk_text_tag_table_new:
 * 
 * Creates a new #GtkTextTagTable. The table contains no tags by
 * default.
 * 
 * Return value: a new #GtkTextTagTable
 **/
GtkTextTagTable*
gtk_text_tag_table_new (void)
{
  GtkTextTagTable *table;

  table = g_object_new (GTK_TYPE_TEXT_TAG_TABLE, NULL);

  return table;
}

static void
foreach_unref (GtkTextTag *tag, gpointer data)
{
  GtkTextTagTable *table = GTK_TEXT_TAG_TABLE (tag->priv->table);
  GtkTextTagTablePrivate *priv = table->priv;
  GSList *l;

  /* We don't want to emit the remove signal here; so we just unparent
   * and unref the tag.
   */

  for (l = priv->buffers; l != NULL; l = l->next)
    _gtk_text_buffer_notify_will_remove_tag (GTK_TEXT_BUFFER (l->data),
                                             tag);

  tag->priv->table = NULL;
  g_object_unref (tag);
}

static void
gtk_text_tag_table_finalize (GObject *object)
{
  GtkTextTagTable *table = GTK_TEXT_TAG_TABLE (object);
  GtkTextTagTablePrivate *priv = table->priv;

  gtk_text_tag_table_foreach (table, foreach_unref, NULL);

  g_hash_table_destroy (priv->hash);
  g_slist_free (priv->anonymous);
  g_slist_free (priv->buffers);

  G_OBJECT_CLASS (gtk_text_tag_table_parent_class)->finalize (object);
}

static void
gtk_text_tag_table_buildable_interface_init (GtkBuildableIface   *iface)
{
  iface->add_child = gtk_text_tag_table_buildable_add_child;
}

static void
gtk_text_tag_table_buildable_add_child (GtkBuildable        *buildable,
					GtkBuilder          *builder,
					GObject             *child,
					const gchar         *type)
{
  if (type && strcmp (type, "tag") == 0)
    gtk_text_tag_table_add (GTK_TEXT_TAG_TABLE (buildable),
			    GTK_TEXT_TAG (child));
}

/**
 * gtk_text_tag_table_add:
 * @table: a #GtkTextTagTable
 * @tag: a #GtkTextTag
 *
 * Add a tag to the table. The tag is assigned the highest priority
 * in the table.
 *
 * @tag must not be in a tag table already, and may not have
 * the same name as an already-added tag.
 **/
void
gtk_text_tag_table_add (GtkTextTagTable *table,
                        GtkTextTag      *tag)
{
  GtkTextTagTablePrivate *priv;
  guint size;

  g_return_if_fail (GTK_IS_TEXT_TAG_TABLE (table));
  g_return_if_fail (GTK_IS_TEXT_TAG (tag));
  g_return_if_fail (tag->priv->table == NULL);

  priv = table->priv;

  if (tag->priv->name && g_hash_table_lookup (priv->hash, tag->priv->name))
    {
      g_warning ("A tag named '%s' is already in the tag table.",
                 tag->priv->name);
      return;
    }
  
  g_object_ref (tag);

  if (tag->priv->name)
    g_hash_table_insert (priv->hash, tag->priv->name, tag);
  else
    {
      priv->anonymous = g_slist_prepend (priv->anonymous, tag);
      priv->anon_count++;
    }

  tag->priv->table = table;

  /* We get the highest tag priority, as the most-recently-added
     tag. Note that we do NOT use gtk_text_tag_set_priority,
     as it assumes the tag is already in the table. */
  size = gtk_text_tag_table_get_size (table);
  g_assert (size > 0);
  tag->priv->priority = size - 1;

  g_signal_emit (table, signals[TAG_ADDED], 0, tag);
}

/**
 * gtk_text_tag_table_lookup:
 * @table: a #GtkTextTagTable 
 * @name: name of a tag
 * 
 * Look up a named tag.
 * 
 * Return value: (transfer none): The tag, or %NULL if none by that name is in the table.
 **/
GtkTextTag*
gtk_text_tag_table_lookup (GtkTextTagTable *table,
                           const gchar     *name)
{
  GtkTextTagTablePrivate *priv;

  g_return_val_if_fail (GTK_IS_TEXT_TAG_TABLE (table), NULL);
  g_return_val_if_fail (name != NULL, NULL);

  priv = table->priv;

  return g_hash_table_lookup (priv->hash, name);
}

/**
 * gtk_text_tag_table_remove:
 * @table: a #GtkTextTagTable
 * @tag: a #GtkTextTag
 *
 * Remove a tag from the table. If a #GtkTextBuffer has @table as its tag table,
 * the tag is removed from the buffer. The table's reference to the tag is
 * removed, so the tag will end up destroyed if you don't have a reference to
 * it.
 **/
void
gtk_text_tag_table_remove (GtkTextTagTable *table,
                           GtkTextTag      *tag)
{
  GtkTextTagTablePrivate *priv;
  GSList *l;

  g_return_if_fail (GTK_IS_TEXT_TAG_TABLE (table));
  g_return_if_fail (GTK_IS_TEXT_TAG (tag));
  g_return_if_fail (tag->priv->table == table);

  priv = table->priv;

  /* Our little bad hack to be sure buffers don't still have the tag
   * applied to text in the buffer
   */
  for (l = priv->buffers; l != NULL; l = l->next)
    _gtk_text_buffer_notify_will_remove_tag (GTK_TEXT_BUFFER (l->data),
                                             tag);

  /* Set ourselves to the highest priority; this means
     when we're removed, there won't be any gaps in the
     priorities of the tags in the table. */
  gtk_text_tag_set_priority (tag, gtk_text_tag_table_get_size (table) - 1);

  tag->priv->table = NULL;

  if (tag->priv->name)
    g_hash_table_remove (priv->hash, tag->priv->name);
  else
    {
      priv->anonymous = g_slist_remove (priv->anonymous, tag);
      priv->anon_count--;
    }

  g_signal_emit (table, signals[TAG_REMOVED], 0, tag);

  g_object_unref (tag);
}

struct ForeachData
{
  GtkTextTagTableForeach func;
  gpointer data;
};

static void
hash_foreach (gpointer key, gpointer value, gpointer data)
{
  struct ForeachData *fd = data;

  g_return_if_fail (GTK_IS_TEXT_TAG (value));

  (* fd->func) (value, fd->data);
}

static void
list_foreach (gpointer data, gpointer user_data)
{
  struct ForeachData *fd = user_data;

  g_return_if_fail (GTK_IS_TEXT_TAG (data));

  (* fd->func) (data, fd->data);
}

/**
 * gtk_text_tag_table_foreach:
 * @table: a #GtkTextTagTable
 * @func: (scope call): a function to call on each tag
 * @data: user data
 *
 * Calls @func on each tag in @table, with user data @data.
 * Note that the table may not be modified while iterating 
 * over it (you can't add/remove tags).
 **/
void
gtk_text_tag_table_foreach (GtkTextTagTable       *table,
                            GtkTextTagTableForeach func,
                            gpointer               data)
{
  GtkTextTagTablePrivate *priv;
  struct ForeachData d;

  g_return_if_fail (GTK_IS_TEXT_TAG_TABLE (table));
  g_return_if_fail (func != NULL);

  priv = table->priv;

  d.func = func;
  d.data = data;

  g_hash_table_foreach (priv->hash, hash_foreach, &d);
  g_slist_foreach (priv->anonymous, list_foreach, &d);
}

/**
 * gtk_text_tag_table_get_size:
 * @table: a #GtkTextTagTable
 * 
 * Returns the size of the table (number of tags)
 * 
 * Return value: number of tags in @table
 **/
gint
gtk_text_tag_table_get_size (GtkTextTagTable *table)
{
  GtkTextTagTablePrivate *priv;

  g_return_val_if_fail (GTK_IS_TEXT_TAG_TABLE (table), 0);

  priv = table->priv;

  return g_hash_table_size (priv->hash) + priv->anon_count;
}

void
_gtk_text_tag_table_add_buffer (GtkTextTagTable *table,
                                gpointer         buffer)
{
  GtkTextTagTablePrivate *priv = table->priv;

  priv->buffers = g_slist_prepend (priv->buffers, buffer);
}

static void
foreach_remove_tag (GtkTextTag *tag, gpointer data)
{
  GtkTextBuffer *buffer;

  buffer = GTK_TEXT_BUFFER (data);

  _gtk_text_buffer_notify_will_remove_tag (buffer, tag);
}

void
_gtk_text_tag_table_remove_buffer (GtkTextTagTable *table,
                                   gpointer         buffer)
{
  GtkTextTagTablePrivate *priv = table->priv;

  gtk_text_tag_table_foreach (table, foreach_remove_tag, buffer);

  priv->buffers = g_slist_remove (priv->buffers, buffer);
}
