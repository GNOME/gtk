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

static void assert_menu_equality (GtkContainer *container, GMenuModel   *model);

static const gchar *
get_label (GtkMenuItem *item)
{
  GList *children = gtk_container_get_children (GTK_CONTAINER (item));
  const gchar *label = NULL;

  while (children)
    {
      if (GTK_IS_CONTAINER (children->data))
        children = g_list_concat (children, gtk_container_get_children (children->data));
      else if (GTK_IS_LABEL (children->data))
        label = gtk_label_get_text (children->data);

      children = g_list_delete_link (children, children);
    }

  return label;
}

/* a bit complicated with the separators...
 *
 * with_separators are if subsections of this GMenuModel should have
 * separators inserted between them (ie: in the same sense as the
 * 'with_separators' argument to gtk_menu_shell_bind_model().
 *
 * needs_separator is true if this particular section needs to have a
 * separator before it in the case that it is non-empty.  this will be
 * defined for all subsections of a with_separators menu (except the
 * first) or in case section_header is non-%NULL.
 *
 * section_header is the label that must be inside that separator, if it
 * exists.  section_header is only non-%NULL if needs_separator is also
 * TRUE.
 */
static void
assert_section_equality (GSList      **children,
                         gboolean      with_separators,
                         gboolean      needs_separator,
                         const gchar  *section_header,
                         GMenuModel   *model)
{
  gboolean has_separator;
  GSList *our_children;
  gint i, n;

  /* Assuming that we have the possibility of showing a separator, there
   * are two valid situations:
   *
   *  - we have a separator and we have other children
   *
   *  - we have no separator and no children
   *
   * If we see a separator, we suppose that it is ours and that we will
   * encounter children.  In the case that we have no children, the
   * separator may not be ours but may rather belong to a later section.
   *
   * We therefore keep our own copy of the children GSList.  If we
   * encounter children, we will delete the links that this section is
   * responsible for and update the pass-by-reference value.  Otherwise,
   * we will leave everything alone and let the separator be accounted
   * for by a following section.
   */
  our_children = *children;
  if (needs_separator && GTK_IS_SEPARATOR_MENU_ITEM (our_children->data))
    {
       /* We accounted for the separator, at least for now, so remove it
       * from the list.
       *
       * We will check later if we should have actually had a separator
       * and compare the result to has_separator.
       */
      our_children = our_children->next;
      has_separator = TRUE;
    }
  else
    has_separator = FALSE;

  /* Now, iterate the model checking that the items in the GSList line
   * up with our expectations. */
  n = g_menu_model_get_n_items (model);
  for (i = 0; i < n; i++)
    {
      GMenuModel *subsection;
      GMenuModel *submenu;
      gchar *label = NULL;

      subsection = g_menu_model_get_item_link (model, i, G_MENU_LINK_SECTION);
      submenu = g_menu_model_get_item_link (model, i, G_MENU_LINK_SUBMENU);
      g_menu_model_get_item_attribute (model, i, G_MENU_ATTRIBUTE_LABEL, "s", &label);

      if (subsection)
        {
          g_assert (!submenu);
          assert_section_equality (&our_children,
                                   FALSE,                                /* with_separators */
                                   label || (with_separators && i > 0),  /* needs_separator */
                                   label,                                /* section_header */
                                   subsection);
          g_object_unref (subsection);
        }
      else
        {
          GtkWidget *submenu_widget;
          GtkMenuItem *item;

          /* This is a normal item.  Make sure the label is right. */
          item = our_children->data;
          our_children = g_slist_remove (our_children, item);

          /* get_label() returns "" when it ought to return NULL */
          g_assert_cmpstr (get_label (item), ==, label ? label : "");
          submenu_widget = gtk_menu_item_get_submenu (item);

          if (submenu)
            {
              g_assert (submenu_widget != NULL);
              assert_menu_equality (GTK_CONTAINER (submenu_widget), submenu);
              g_object_unref (submenu);
            }
          else
            g_assert (!submenu_widget);
        }

      g_free (label);
    }

  /* If we found a separator but visited no children then the separator
   * was not for us.  Patch that up.
   */
  if (has_separator && our_children == (*children)->next)
    {
      /* Rewind our_children to put the separator we tentatively
       * consumed back into the list.
       */
      our_children = *children;
      has_separator = FALSE;
    }

  if (our_children == *children)
    /* If we had no children then we didn't really need a separator. */
    needs_separator = FALSE;

  g_assert (needs_separator == has_separator);

  if (has_separator)
    {
      GtkWidget *contents;
      const gchar *label;

      /* We needed and had a separator and we visited a child.
       *
       * Make sure that separator was valid.
       */
      contents = gtk_bin_get_child ((*children)->data);
      if (GTK_IS_LABEL (contents))
        label = gtk_label_get_label (GTK_LABEL (contents));
      else
        label = "";

      /* get_label() returns "" when it ought to return NULL */
      g_assert_cmpstr (label, ==, section_header ? section_header : "");

      /* our_children has already gone (possibly far) past *children, so
       * we need to free up the link that we left behind for the
       * separator in case we wanted to rewind.
       */
      g_slist_free_1 (*children);
    }

  *children = our_children;
}

/* We want to use a GSList here instead of a GList because the ->prev
 * pointer updates cause trouble with the way we speculatively deal with
 * separators by skipping over them and coming back to clean up later.
 */
static void
get_children_into_slist (GtkWidget *widget,
                         gpointer   user_data)
{
  GSList **list_ptr = user_data;

  *list_ptr = g_slist_prepend (*list_ptr, widget);
}

static void
assert_menu_equality (GtkContainer *container,
                      GMenuModel   *model)
{
  GSList *children = NULL;

  gtk_container_foreach (container, get_children_into_slist, &children);
  children = g_slist_reverse (children);

  assert_section_equality (&children, TRUE, FALSE, NULL, model);
  g_assert (children == NULL);
}

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
  assert_menu_equality (GTK_CONTAINER (menu), G_MENU_MODEL (model));
  for (i = 0; i < 100; i++)
    {
      random_menu_change (model, rand);
      while (g_main_context_iteration (NULL, FALSE));
      assert_menu_equality (GTK_CONTAINER (menu), G_MENU_MODEL (model));
    }
  g_object_unref (model);
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
