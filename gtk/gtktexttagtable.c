
#include "gtktexttagtable.h"
#include "gtksignal.h"

#include <stdlib.h>

enum {
  TAG_CHANGED,
  TAG_ADDED,
  TAG_REMOVED,
  LAST_SIGNAL
};

enum {
  LAST_ARG
};

static void gtk_text_tag_table_init (GtkTextTagTable *table);
static void gtk_text_tag_table_class_init (GtkTextTagTableClass *klass);
static void gtk_text_tag_table_destroy (GtkObject *object);
static void gtk_text_tag_table_finalize (GObject *object);
static void gtk_text_tag_table_set_arg (GtkObject *object, GtkArg *arg, guint arg_id);
static void gtk_text_tag_table_get_arg (GtkObject *object, GtkArg *arg, guint arg_id);

static GtkObjectClass *parent_class = NULL;
static guint signals[LAST_SIGNAL] = { 0 };

GtkType
gtk_text_tag_table_get_type (void)
{
  static GtkType our_type = 0;

  if (our_type == 0)
    {
      static const GtkTypeInfo our_info =
      {
        "GtkTextTagTable",
        sizeof (GtkTextTagTable),
        sizeof (GtkTextTagTableClass),
        (GtkClassInitFunc) gtk_text_tag_table_class_init,
        (GtkObjectInitFunc) gtk_text_tag_table_init,
        /* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL
      };

      our_type = gtk_type_unique (GTK_TYPE_OBJECT, &our_info);
    }

  return our_type;
}

static void
gtk_text_tag_table_class_init (GtkTextTagTableClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);

  parent_class = gtk_type_class (GTK_TYPE_OBJECT);

  signals[TAG_CHANGED] =
    gtk_signal_new ("tag_changed",
                    GTK_RUN_LAST,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkTextTagTableClass, tag_changed),
                    gtk_marshal_VOID__OBJECT_BOOLEAN,
                    GTK_TYPE_NONE,
                    2,
                    G_TYPE_OBJECT,
                    GTK_TYPE_BOOL);

  signals[TAG_ADDED] =
    gtk_signal_new ("tag_added",
                    GTK_RUN_LAST,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkTextTagTableClass, tag_added),
                    gtk_marshal_VOID__OBJECT,
                    GTK_TYPE_NONE,
                    1,
                    G_TYPE_OBJECT);

  signals[TAG_REMOVED] =
    gtk_signal_new ("tag_removed",
                    GTK_RUN_LAST,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkTextTagTableClass, tag_removed),
                    gtk_marshal_VOID__OBJECT,
                    GTK_TYPE_NONE,
                    1,
                    G_TYPE_OBJECT);


  gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);

  object_class->set_arg = gtk_text_tag_table_set_arg;
  object_class->get_arg = gtk_text_tag_table_get_arg;

  object_class->destroy = gtk_text_tag_table_destroy;
  gobject_class->finalize = gtk_text_tag_table_finalize;
}

void
gtk_text_tag_table_init (GtkTextTagTable *table)
{
  table->hash = g_hash_table_new (g_str_hash, g_str_equal);
}

/**
 * gtk_text_tag_table_new:
 * Creates a new #GtkTextTagTable. The table contains no tags by
 * default.
 * 
 * Return value: a new #GtkTextTagTable
 **/
GtkTextTagTable*
gtk_text_tag_table_new (void)
{
  GtkTextTagTable *table;

  table = GTK_TEXT_TAG_TABLE (gtk_type_new (gtk_text_tag_table_get_type ()));

  return table;
}

static void
gtk_text_tag_table_destroy (GtkObject *object)
{
  GtkTextTagTable *table;

  table = GTK_TEXT_TAG_TABLE (object);

  (* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

static void
foreach_unref (GtkTextTag *tag, gpointer data)
{
  /* We don't want to emit the remove signal here; so we just unparent
   * and unref the tag.
   */

  tag->table = NULL;
  g_object_unref (G_OBJECT (tag));
}

static void
gtk_text_tag_table_finalize (GObject *object)
{
  GtkTextTagTable *table;

  table = GTK_TEXT_TAG_TABLE (object);

  gtk_text_tag_table_foreach (table, foreach_unref, NULL);

  g_hash_table_destroy (table->hash);
  g_slist_free (table->anonymous);

  (* G_OBJECT_CLASS (parent_class)->finalize) (object);
}

static void
gtk_text_tag_table_set_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
  GtkTextTagTable *table;

  table = GTK_TEXT_TAG_TABLE (object);

  switch (arg_id)
    {

    default:
      g_assert_not_reached ();
      break;
    }
}

static void
gtk_text_tag_table_get_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
  GtkTextTagTable *table;

  table = GTK_TEXT_TAG_TABLE (object);

  switch (arg_id)
    {

    default:
      arg->type = GTK_TYPE_INVALID;
      break;
    }
}

/**
 * gtk_text_tag_table_add:
 * @table: a #GtkTextTagTable
 * @tag: a #GtkTextTag
 *
 * Add a tag to the table. The tag is assigned the highest priority
 * in the table.
 * 
 **/
void
gtk_text_tag_table_add (GtkTextTagTable *table,
                        GtkTextTag      *tag)
{
  guint size;

  g_return_if_fail (GTK_IS_TEXT_TAG_TABLE (table));
  g_return_if_fail (GTK_IS_TEXT_TAG (tag));
  g_return_if_fail (tag->name == NULL ||
                    g_hash_table_lookup (table->hash, tag->name) == NULL);
  g_return_if_fail (tag->table == NULL);

  gtk_object_ref (GTK_OBJECT (tag));
  gtk_object_sink (GTK_OBJECT (tag));

  if (tag->name)
    g_hash_table_insert (table->hash, tag->name, tag);
  else
    {
      table->anonymous = g_slist_prepend (table->anonymous, tag);
      table->anon_count += 1;
    }

  tag->table = table;

  /* We get the highest tag priority, as the most-recently-added
     tag. Note that we do NOT use gtk_text_tag_set_priority,
     as it assumes the tag is already in the table. */
  size = gtk_text_tag_table_size (table);
  g_assert (size > 0);
  tag->priority = size - 1;

  gtk_signal_emit (GTK_OBJECT (table), signals[TAG_ADDED], tag);
}

/**
 * gtk_text_tag_table_lookup:
 * @table: a #GtkTextTagTable 
 * @name: name of a tag
 * 
 * Look up a named tag.
 * 
 * Return value: The tag, or %NULL if none by that name is in the table.
 **/
GtkTextTag*
gtk_text_tag_table_lookup (GtkTextTagTable *table,
                           const gchar     *name)
{
  g_return_val_if_fail (GTK_IS_TEXT_TAG_TABLE (table), NULL);
  g_return_val_if_fail (name != NULL, NULL);

  return g_hash_table_lookup (table->hash, name);
}

/**
 * gtk_text_tag_table_remove:
 * @table: a #GtkTextTagTable
 * @tag: a #GtkTextTag
 * 
 * Remove a tag from the table. This will remove the table's
 * reference to the tag, so be careful - the tag will end
 * up destroyed if you don't have a reference to it.
 **/
void
gtk_text_tag_table_remove (GtkTextTagTable *table,
                           GtkTextTag      *tag)
{
  g_return_if_fail (GTK_IS_TEXT_TAG_TABLE (table));
  g_return_if_fail (GTK_IS_TEXT_TAG (tag));
  g_return_if_fail (tag->table == table);

  /* Set ourselves to the highest priority; this means
     when we're removed, there won't be any gaps in the
     priorities of the tags in the table. */
  gtk_text_tag_set_priority (tag, gtk_text_tag_table_size (table) - 1);

  tag->table = NULL;

  if (tag->name)
    g_hash_table_remove (table->hash, tag->name);
  else
    {
      table->anonymous = g_slist_remove (table->anonymous, tag);
      table->anon_count -= 1;
    }

  gtk_signal_emit (GTK_OBJECT (table), signals[TAG_REMOVED], tag);

  gtk_object_unref (GTK_OBJECT (tag));
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
 * @func: a function to call on each tag
 * @data: user data
 *
 * Calls @func on each tag in @table, with user data @data.
 * 
 **/
void
gtk_text_tag_table_foreach (GtkTextTagTable       *table,
                            GtkTextTagTableForeach func,
                            gpointer               data)
{
  struct ForeachData d;

  g_return_if_fail (GTK_IS_TEXT_TAG_TABLE (table));
  g_return_if_fail (func != NULL);

  d.func = func;
  d.data = data;

  g_hash_table_foreach (table->hash, hash_foreach, &d);
  g_slist_foreach (table->anonymous, list_foreach, &d);
}

/**
 * gtk_text_tag_table_size:
 * @table: a #GtkTextTagTable
 * 
 * Returns the size of the table (number of tags)
 * 
 * Return value: number of tags in @table
 **/
gint
gtk_text_tag_table_size (GtkTextTagTable *table)
{
  g_return_val_if_fail (GTK_IS_TEXT_TAG_TABLE (table), 0);

  return g_hash_table_size (table->hash) + table->anon_count;
}
