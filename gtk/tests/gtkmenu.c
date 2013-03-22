#include <gtk/gtk.h>

/* TestItem {{{1 */

/* This utility struct is used by both the RandomMenu and MirrorMenu
 * class implementations below.
 */
typedef struct {
  GHashTable *attributes;
  GHashTable *links;
} TestItem;

static TestItem *
test_item_new (GHashTable *attributes,
               GHashTable *links)
{
  TestItem *item;

  item = g_slice_new (TestItem);
  item->attributes = g_hash_table_ref (attributes);
  item->links = g_hash_table_ref (links);

  return item;
}

static void
test_item_free (gpointer data)
{
  TestItem *item = data;

  g_hash_table_unref (item->attributes);
  g_hash_table_unref (item->links);

  g_slice_free (TestItem, item);
}

/* RandomMenu {{{1 */
#define MAX_ITEMS 10
#define TOP_ORDER 4

typedef struct {
  GMenuModel parent_instance;

  GSequence *items;
  gint order;
} RandomMenu;

typedef GMenuModelClass RandomMenuClass;

static GType random_menu_get_type (void);
G_DEFINE_TYPE (RandomMenu, random_menu, G_TYPE_MENU_MODEL);

static gboolean
random_menu_is_mutable (GMenuModel *model)
{
  return TRUE;
}

static gint
random_menu_get_n_items (GMenuModel *model)
{
  RandomMenu *menu = (RandomMenu *) model;

  return g_sequence_get_length (menu->items);
}

static void
random_menu_get_item_attributes (GMenuModel  *model,
                                 gint         position,
                                 GHashTable **table)
{
  RandomMenu *menu = (RandomMenu *) model;
  TestItem *item;

  item = g_sequence_get (g_sequence_get_iter_at_pos (menu->items, position));
  *table = g_hash_table_ref (item->attributes);
}

static void
random_menu_get_item_links (GMenuModel  *model,
                            gint         position,
                            GHashTable **table)
{
  RandomMenu *menu = (RandomMenu *) model;
  TestItem *item;

  item = g_sequence_get (g_sequence_get_iter_at_pos (menu->items, position));
  *table = g_hash_table_ref (item->links);
}

static void
random_menu_finalize (GObject *object)
{
  RandomMenu *menu = (RandomMenu *) object;

  g_sequence_free (menu->items);

  G_OBJECT_CLASS (random_menu_parent_class)
    ->finalize (object);
}

static void
random_menu_init (RandomMenu *menu)
{
}

static void
random_menu_class_init (GMenuModelClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  class->is_mutable = random_menu_is_mutable;
  class->get_n_items = random_menu_get_n_items;
  class->get_item_attributes = random_menu_get_item_attributes;
  class->get_item_links = random_menu_get_item_links;

  object_class->finalize = random_menu_finalize;
}

static RandomMenu * random_menu_new (GRand *rand, gint order);

static void
random_menu_change (RandomMenu *menu,
                    GRand      *rand)
{
  gint position, removes, adds;
  GSequenceIter *point;
  gint n_items;
  gint i;

  n_items = g_sequence_get_length (menu->items);

  do
    {
      position = g_rand_int_range (rand, 0, n_items + 1);
      removes = g_rand_int_range (rand, 0, n_items - position + 1);
      adds = g_rand_int_range (rand, 0, MAX_ITEMS - (n_items - removes) + 1);
    }
  while (removes == 0 && adds == 0);

  point = g_sequence_get_iter_at_pos (menu->items, position + removes);

  if (removes)
    {
      GSequenceIter *start;

      start = g_sequence_get_iter_at_pos (menu->items, position);
      g_sequence_remove_range (start, point);
    }

  for (i = 0; i < adds; i++)
    {
      const gchar *label;
      GHashTable *links;
      GHashTable *attributes;

      attributes = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) g_variant_unref);
      links = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) g_object_unref);

      if (menu->order > 0 && g_rand_boolean (rand))
        {
          RandomMenu *child;
          const gchar *subtype;

          child = random_menu_new (rand, menu->order - 1);

          if (g_rand_boolean (rand))
            {
              subtype = G_MENU_LINK_SECTION;
              /* label some section headers */
              if (g_rand_boolean (rand))
                label = "Section";
              else
                label = NULL;
            }
          else
            {
              /* label all submenus */
              subtype = G_MENU_LINK_SUBMENU;
              label = "Submenu";
            }

          g_hash_table_insert (links, g_strdup (subtype), child);
        }
      else
        /* label all terminals */
        label = "Menu Item";

      if (label)
        g_hash_table_insert (attributes, g_strdup ("label"), g_variant_ref_sink (g_variant_new_string (label)));

      g_sequence_insert_before (point, test_item_new (attributes, links));
      g_hash_table_unref (links);
      g_hash_table_unref (attributes);
    }

  g_menu_model_items_changed (G_MENU_MODEL (menu), position, removes, adds);
}

static RandomMenu *
random_menu_new (GRand *rand,
                 gint   order)
{
  RandomMenu *menu;

  menu = g_object_new (random_menu_get_type (), NULL);
  menu->items = g_sequence_new (test_item_free);
  menu->order = order;

  random_menu_change (menu, rand);

  return menu;
}

/* Test cases {{{1 */
static void
test_bind_menu (void)
{
  RandomMenu *model;
  GtkWidget *menu;
  GRand *rand;
  gint i;

  gtk_init (0, 0);

  rand = g_rand_new_with_seed (g_test_rand_int ());
  model = random_menu_new (rand, TOP_ORDER);
  menu = gtk_menu_new_from_model (G_MENU_MODEL (model));
  g_object_ref_sink (menu);
  for (i = 0; i < 100; i++)
    {
      random_menu_change (model, rand);
      while (g_main_context_iteration (NULL, FALSE));
      g_print (".");
    }
  g_object_unref (model);
  gtk_widget_destroy (menu);
  g_object_unref (menu);
  g_rand_free (rand);
}
/* Epilogue {{{1 */
int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/gmenu/bind", test_bind_menu);

  return g_test_run ();
}
/* vim:set foldmethod=marker: */
