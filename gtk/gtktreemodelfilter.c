/* gtktreemodelfilter.c
 * Copyright (C) 2000,2001  Red Hat, Inc., Jonathan Blandford <jrb@redhat.com>
 * Copyright (C) 2001-2003  Kristian Rietveld <kris@gtk.org>
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

#include <config.h>
#include "gtkalias.h"
#include "gtktreemodelfilter.h"
#include "gtkintl.h"
#include "gtktreednd.h"
#include <string.h>

/* ITER FORMAT:
 *
 * iter->stamp = filter->priv->stamp
 * iter->user_data = FilterLevel
 * iter->user_data2 = FilterElt
 */

/* all paths, iters, etc prefixed with c_ are paths, iters, etc relative to the
 * child model.
 */

typedef struct _FilterElt FilterElt;
typedef struct _FilterLevel FilterLevel;

struct _FilterElt
{
  GtkTreeIter iter;
  FilterLevel *children;
  gint offset;
  gint ref_count;
  gint zero_ref_count;
  gboolean visible;
};

struct _FilterLevel
{
  GArray *array;
  gint ref_count;

  FilterElt *parent_elt;
  FilterLevel *parent_level;
};

#define GTK_TREE_MODEL_FILTER_GET_PRIVATE(obj)  (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GTK_TYPE_TREE_MODEL_FILTER, GtkTreeModelFilterPrivate))

struct _GtkTreeModelFilterPrivate
{
  gpointer root;
  gint stamp;
  guint child_flags;
  GtkTreeModel *child_model;
  gint zero_ref_count;

  guint root_level_visible;

  GtkTreePath *virtual_root;

  GtkTreeModelFilterVisibleFunc visible_func;
  gpointer visible_data;
  GtkDestroyNotify visible_destroy;

  gint modify_n_columns;
  GType *modify_types;
  GtkTreeModelFilterModifyFunc modify_func;
  gpointer modify_data;
  gpointer modify_destroy;

  gint visible_column;

  gboolean visible_method_set;
  gboolean modify_func_set;

  /* signal ids */
  guint changed_id;
  guint inserted_id;
  guint has_child_toggled_id;
  guint deleted_id;
  guint reordered_id;
};

/* properties */
enum
{
  PROP_0,
  PROP_CHILD_MODEL,
  PROP_VIRTUAL_ROOT
};

#define GTK_TREE_MODEL_FILTER_CACHE_CHILD_ITERS(filter) \
        (((GtkTreeModelFilter *)filter)->priv->child_flags & GTK_TREE_MODEL_ITERS_PERSIST)

#define FILTER_ELT(filter_elt) ((FilterElt *)filter_elt)
#define FILTER_LEVEL(filter_level) ((FilterLevel *)filter_level)

/* general code (object/interface init, properties, etc) */
static void         gtk_tree_model_filter_init                            (GtkTreeModelFilter      *filter);
static void         gtk_tree_model_filter_class_init                      (GtkTreeModelFilterClass *filter_class);
static void         gtk_tree_model_filter_tree_model_init                 (GtkTreeModelIface       *iface);
static void         gtk_tree_model_filter_drag_source_init                (GtkTreeDragSourceIface  *iface);
static void         gtk_tree_model_filter_finalize                        (GObject                 *object);
static void         gtk_tree_model_filter_set_property                    (GObject                 *object,
                                                                           guint                    prop_id,
                                                                           const GValue            *value,
                                                                           GParamSpec              *pspec);
static void         gtk_tree_model_filter_get_property                    (GObject                 *object,
                                                                           guint                    prop_id,
                                                                           GValue                 *value,
                                                                           GParamSpec             *pspec);

/* signal handlers */
static void         gtk_tree_model_filter_row_changed                     (GtkTreeModel           *c_model,
                                                                           GtkTreePath            *c_path,
                                                                           GtkTreeIter            *c_iter,
                                                                           gpointer                data);
static void         gtk_tree_model_filter_row_inserted                    (GtkTreeModel           *c_model,
                                                                           GtkTreePath            *c_path,
                                                                           GtkTreeIter            *c_iter,
                                                                           gpointer                data);
static void         gtk_tree_model_filter_row_has_child_toggled           (GtkTreeModel           *c_model,
                                                                           GtkTreePath            *c_path,
                                                                           GtkTreeIter            *c_iter,
                                                                           gpointer                data);
static void         gtk_tree_model_filter_row_deleted                     (GtkTreeModel           *c_model,
                                                                           GtkTreePath            *c_path,
                                                                           gpointer                data);
static void         gtk_tree_model_filter_rows_reordered                  (GtkTreeModel           *c_model,
                                                                           GtkTreePath            *c_path,
                                                                           GtkTreeIter            *c_iter,
                                                                           gint                   *new_order,
                                                                           gpointer                data);

/* GtkTreeModel interface */
static GtkTreeModelFlags gtk_tree_model_filter_get_flags                       (GtkTreeModel           *model);
static gint         gtk_tree_model_filter_get_n_columns                   (GtkTreeModel           *model);
static GType        gtk_tree_model_filter_get_column_type                 (GtkTreeModel           *model,
                                                                           gint                    index);
static gboolean     gtk_tree_model_filter_get_iter                        (GtkTreeModel           *model,
                                                                           GtkTreeIter            *iter,
                                                                           GtkTreePath            *path);
static GtkTreePath *gtk_tree_model_filter_get_path                        (GtkTreeModel           *model,
                                                                           GtkTreeIter            *iter);
static void         gtk_tree_model_filter_get_value                       (GtkTreeModel           *model,
                                                                           GtkTreeIter            *iter,
                                                                           gint                    column,
                                                                           GValue                 *value);
static gboolean     gtk_tree_model_filter_iter_next                       (GtkTreeModel           *model,
                                                                           GtkTreeIter            *iter);
static gboolean     gtk_tree_model_filter_iter_children                   (GtkTreeModel           *model,
                                                                           GtkTreeIter            *iter,
                                                                           GtkTreeIter            *parent);
static gboolean     gtk_tree_model_filter_iter_has_child                  (GtkTreeModel           *model,
                                                                           GtkTreeIter            *iter);
static gint         gtk_tree_model_filter_iter_n_children                 (GtkTreeModel           *model,
                                                                           GtkTreeIter            *iter);
static gboolean     gtk_tree_model_filter_iter_nth_child                  (GtkTreeModel           *model,
                                                                           GtkTreeIter            *iter,
                                                                           GtkTreeIter            *parent,
                                                                           gint                    n);
static gboolean     gtk_tree_model_filter_iter_parent                     (GtkTreeModel           *model,
                                                                           GtkTreeIter            *iter,
                                                                           GtkTreeIter            *child);
static void         gtk_tree_model_filter_ref_node                        (GtkTreeModel           *model,
                                                                           GtkTreeIter            *iter);
static void         gtk_tree_model_filter_unref_node                      (GtkTreeModel           *model,
                                                                           GtkTreeIter            *iter);

/* TreeDragSource interface */
static gboolean    gtk_tree_model_filter_row_draggable                    (GtkTreeDragSource      *drag_source,
                                                                           GtkTreePath            *path);
static gboolean    gtk_tree_model_filter_drag_data_get                    (GtkTreeDragSource      *drag_source,
                                                                           GtkTreePath            *path,
                                                                           GtkSelectionData       *selection_data);
static gboolean    gtk_tree_model_filter_drag_data_delete                 (GtkTreeDragSource      *drag_source,
                                                                           GtkTreePath            *path);

/* private functions */
static void        gtk_tree_model_filter_build_level                      (GtkTreeModelFilter     *filter,
                                                                           FilterLevel            *parent_level,
                                                                           FilterElt              *parent_elt);
static void        gtk_tree_model_filter_free_level                       (GtkTreeModelFilter     *filter,
                                                                           FilterLevel            *filter_level);

static GtkTreePath *gtk_tree_model_filter_elt_get_path                    (FilterLevel            *level,
                                                                           FilterElt              *elt,
                                                                           GtkTreePath            *root);

static GtkTreePath *gtk_tree_model_filter_add_root                        (GtkTreePath            *src,
                                                                           GtkTreePath            *root);
static GtkTreePath *gtk_tree_model_filter_remove_root                     (GtkTreePath            *src,
                                                                           GtkTreePath            *root);

static void         gtk_tree_model_filter_increment_stamp                 (GtkTreeModelFilter     *filter);

static gboolean     gtk_tree_model_filter_visible                         (GtkTreeModelFilter     *filter,
                                                                           GtkTreeIter            *child_iter);
static void         gtk_tree_model_filter_clear_cache_helper              (GtkTreeModelFilter     *filter,
                                                                           FilterLevel            *level);

static void         gtk_tree_model_filter_real_unref_node                 (GtkTreeModel           *model,
                                                                           GtkTreeIter            *iter,
                                                                           gboolean                propagate_unref);

static void         gtk_tree_model_filter_set_model                       (GtkTreeModelFilter     *filter,
                                                                           GtkTreeModel           *child_model);
static void         gtk_tree_model_filter_set_root                        (GtkTreeModelFilter     *filter,
                                                                           GtkTreePath            *root);

static GtkTreePath *gtk_real_tree_model_filter_convert_child_path_to_path (GtkTreeModelFilter     *filter,
                                                                           GtkTreePath            *child_path,
                                                                           gboolean                build_levels,
                                                                           gboolean                fetch_children);

static FilterElt   *gtk_tree_model_filter_fetch_child                     (GtkTreeModelFilter     *filter,
                                                                           FilterLevel            *level,
                                                                           gint                    offset,
                                                                           gint                   *index);
static void         gtk_tree_model_filter_remove_node                     (GtkTreeModelFilter     *filter,
                                                                           GtkTreeIter            *iter,
                                                                           gboolean                emit_signal);
static void         gtk_tree_model_filter_update_children                 (GtkTreeModelFilter     *filter,
                                                                           FilterLevel            *level,
                                                                           FilterElt              *elt);
static FilterElt   *bsearch_elt_with_offset                               (GArray                 *array,
                                                                           gint                   offset,
                                                                           gint                  *index);


static GObjectClass *parent_class = NULL;

GType
gtk_tree_model_filter_get_type (void)
{
  static GType tree_model_filter_type = 0;

  if (!tree_model_filter_type)
    {
      static const GTypeInfo tree_model_filter_info =
        {
          sizeof (GtkTreeModelFilterClass),
          NULL, /* base_init */
          NULL, /* base_finalize */
          (GClassInitFunc) gtk_tree_model_filter_class_init,
          NULL, /* class_finalize */
          NULL, /* class_data */
          sizeof (GtkTreeModelFilter),
          0, /* n_preallocs */
          (GInstanceInitFunc) gtk_tree_model_filter_init
        };

      static const GInterfaceInfo tree_model_info =
        {
          (GInterfaceInitFunc) gtk_tree_model_filter_tree_model_init,
          NULL,
          NULL
        };

      static const GInterfaceInfo drag_source_info =
        {
          (GInterfaceInitFunc) gtk_tree_model_filter_drag_source_init,
          NULL,
          NULL
        };

      tree_model_filter_type = g_type_register_static (G_TYPE_OBJECT,
                                                       "GtkTreeModelFilter",
                                                       &tree_model_filter_info, 0);

      g_type_add_interface_static (tree_model_filter_type,
                                   GTK_TYPE_TREE_MODEL,
                                   &tree_model_info);

      g_type_add_interface_static (tree_model_filter_type,
                                   GTK_TYPE_TREE_DRAG_SOURCE,
                                   &drag_source_info);
    }

  return tree_model_filter_type;
}

static void
gtk_tree_model_filter_init (GtkTreeModelFilter *filter)
{
  filter->priv = GTK_TREE_MODEL_FILTER_GET_PRIVATE (filter);

  filter->priv->visible_column = -1;
  filter->priv->zero_ref_count = 0;
  filter->priv->visible_method_set = FALSE;
  filter->priv->modify_func_set = FALSE;
}

static void
gtk_tree_model_filter_class_init (GtkTreeModelFilterClass *filter_class)
{
  GObjectClass *object_class;

  object_class = (GObjectClass *) filter_class;
  parent_class = g_type_class_peek_parent (filter_class);

  object_class->set_property = gtk_tree_model_filter_set_property;
  object_class->get_property = gtk_tree_model_filter_get_property;

  object_class->finalize = gtk_tree_model_filter_finalize;

  /* Properties -- FIXME: disabled translations for now, until I can come up with a
   * better description
   */
  g_object_class_install_property (object_class,
                                   PROP_CHILD_MODEL,
                                   g_param_spec_object ("child_model",
                                                        ("The child model"),
                                                        ("The model for the filtermodel to filter"),
                                                        GTK_TYPE_TREE_MODEL,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class,
                                   PROP_VIRTUAL_ROOT,
                                   g_param_spec_boxed ("virtual_root",
                                                       ("The virtual root"),
                                                       ("The virtual root (relative to the child model) for this filtermodel"),
                                                       GTK_TYPE_TREE_PATH,
                                                       G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  g_type_class_add_private (object_class, sizeof (GtkTreeModelFilterPrivate));
}

static void
gtk_tree_model_filter_tree_model_init (GtkTreeModelIface *iface)
{
  iface->get_flags = gtk_tree_model_filter_get_flags;
  iface->get_n_columns = gtk_tree_model_filter_get_n_columns;
  iface->get_column_type = gtk_tree_model_filter_get_column_type;
  iface->get_iter = gtk_tree_model_filter_get_iter;
  iface->get_path = gtk_tree_model_filter_get_path;
  iface->get_value = gtk_tree_model_filter_get_value;
  iface->iter_next = gtk_tree_model_filter_iter_next;
  iface->iter_children = gtk_tree_model_filter_iter_children;
  iface->iter_has_child = gtk_tree_model_filter_iter_has_child;
  iface->iter_n_children = gtk_tree_model_filter_iter_n_children;
  iface->iter_nth_child = gtk_tree_model_filter_iter_nth_child;
  iface->iter_parent = gtk_tree_model_filter_iter_parent;
  iface->ref_node = gtk_tree_model_filter_ref_node;
  iface->unref_node = gtk_tree_model_filter_unref_node;
}

static void
gtk_tree_model_filter_drag_source_init (GtkTreeDragSourceIface *iface)
{
  iface->row_draggable = gtk_tree_model_filter_row_draggable;
  iface->drag_data_delete = gtk_tree_model_filter_drag_data_delete;
  iface->drag_data_get = gtk_tree_model_filter_drag_data_get;
}


static void
gtk_tree_model_filter_finalize (GObject *object)
{
  GtkTreeModelFilter *filter = (GtkTreeModelFilter *) object;

  gtk_tree_model_filter_set_model (filter, NULL);

  if (filter->priv->virtual_root)
    gtk_tree_path_free (filter->priv->virtual_root);

  if (filter->priv->root)
    gtk_tree_model_filter_free_level (filter, filter->priv->root);

  if (filter->priv->modify_types)
    g_free (filter->priv->modify_types);

  /* must chain up */
  parent_class->finalize (object);
}

static void
gtk_tree_model_filter_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  GtkTreeModelFilter *filter = GTK_TREE_MODEL_FILTER (object);

  switch (prop_id)
    {
      case PROP_CHILD_MODEL:
        gtk_tree_model_filter_set_model (filter, g_value_get_object (value));
        break;
      case PROP_VIRTUAL_ROOT:
        gtk_tree_model_filter_set_root (filter, g_value_get_boxed (value));
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
gtk_tree_model_filter_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  GtkTreeModelFilter *filter = GTK_TREE_MODEL_FILTER (object);

  switch (prop_id)
    {
      case PROP_CHILD_MODEL:
        g_value_set_object (value, filter->priv->child_model);
        break;
      case PROP_VIRTUAL_ROOT:
        g_value_set_boxed (value, filter->priv->virtual_root);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

/* helpers */

static void
gtk_tree_model_filter_build_level (GtkTreeModelFilter *filter,
                                   FilterLevel        *parent_level,
                                   FilterElt          *parent_elt)
{
  GtkTreeIter iter;
  GtkTreeIter root;
  FilterLevel *new_level;
  gint length = 0;
  gint i;

  g_assert (filter->priv->child_model != NULL);

  if (!parent_level)
    {
      if (filter->priv->virtual_root)
        {
          if (gtk_tree_model_get_iter (filter->priv->child_model, &root, filter->priv->virtual_root) == FALSE)
            return;
          length = gtk_tree_model_iter_n_children (filter->priv->child_model, &root);

          if (gtk_tree_model_iter_children (filter->priv->child_model, &iter, &root) == FALSE)
            return;
        }
      else
        {
          if (!gtk_tree_model_get_iter_first (filter->priv->child_model, &iter))
            return;
          length = gtk_tree_model_iter_n_children (filter->priv->child_model, NULL);
        }
    }
  else
    {
      GtkTreeIter parent_iter;
      GtkTreeIter child_parent_iter;

      parent_iter.stamp = filter->priv->stamp;
      parent_iter.user_data = parent_level;
      parent_iter.user_data2 = parent_elt;

      gtk_tree_model_filter_convert_iter_to_child_iter (filter,
                                                        &child_parent_iter,
                                                        &parent_iter);
      if (gtk_tree_model_iter_children (filter->priv->child_model, &iter, &child_parent_iter) == FALSE)
        return;

      /* stamp may have changed */
      gtk_tree_model_filter_convert_iter_to_child_iter (filter,
                                                        &child_parent_iter,
                                                        &parent_iter);
      length = gtk_tree_model_iter_n_children (filter->priv->child_model, &child_parent_iter);
    }

  g_return_if_fail (length > 0);

  new_level = g_new (FilterLevel, 1);
  new_level->array = g_array_sized_new (FALSE, FALSE,
                                        sizeof (FilterElt),
                                        length);
  new_level->ref_count = 0;
  new_level->parent_elt = parent_elt;
  new_level->parent_level = parent_level;

  if (parent_elt)
    parent_elt->children = new_level;
  else
    filter->priv->root = new_level;

  /* increase the count of zero ref_counts */
  while (parent_level)
    {
      parent_elt->zero_ref_count++;

      parent_elt = parent_level->parent_elt;
      parent_level = parent_level->parent_level;
    }
  filter->priv->zero_ref_count++;

  i = 0;

  if (!new_level->parent_level)
    filter->priv->root_level_visible = 0;

  do
    {
      if (gtk_tree_model_filter_visible (filter, &iter))
        {
          FilterElt filter_elt;

          filter_elt.offset = i;
          filter_elt.zero_ref_count = 0;
          filter_elt.ref_count = 0;
          filter_elt.children = NULL;
          filter_elt.visible = TRUE;

          if (GTK_TREE_MODEL_FILTER_CACHE_CHILD_ITERS (filter))
            filter_elt.iter = iter;

          g_array_append_val (new_level->array, filter_elt);

          if (!new_level->parent_level)
            filter->priv->root_level_visible++;
        }
      i++;
    }
  while (gtk_tree_model_iter_next (filter->priv->child_model, &iter));
}

static void
gtk_tree_model_filter_free_level (GtkTreeModelFilter *filter,
                                  FilterLevel        *filter_level)
{
  gint i;

  g_assert (filter_level);

  if (filter_level->ref_count == 0)
    {
      FilterLevel *parent_level = filter_level->parent_level;
      FilterElt *parent_elt = filter_level->parent_elt;

      do
        {
          if (parent_elt)
            parent_elt->zero_ref_count--;

          if (parent_level)
            {
              parent_elt = parent_level->parent_elt;
              parent_level = parent_level->parent_level;
            }
        }
      while (parent_level);
      filter->priv->zero_ref_count--;
    }

  for (i = 0; i < filter_level->array->len; i++)
    {
      if (g_array_index (filter_level->array, FilterElt, i).children)
        gtk_tree_model_filter_free_level (filter,
                                          FILTER_LEVEL (g_array_index (filter_level->array, FilterElt, i).children));
    }

  if (!filter_level->parent_level)
    filter->priv->root_level_visible = 0;

  if (filter_level->parent_elt)
    filter_level->parent_elt->children = NULL;
  else
    filter->priv->root = NULL;

  g_array_free (filter_level->array, TRUE);
  filter_level->array = NULL;

  g_free (filter_level);
  filter_level = NULL;
}

static GtkTreePath *
gtk_tree_model_filter_elt_get_path (FilterLevel *level,
                                    FilterElt   *elt,
                                    GtkTreePath *root)
{
  FilterLevel *walker = level;
  FilterElt *walker2 = elt;
  GtkTreePath *path;
  GtkTreePath *real_path;

  g_return_val_if_fail (level != NULL, NULL);
  g_return_val_if_fail (elt != NULL, NULL);

  path = gtk_tree_path_new ();

  while (walker)
    {
      gtk_tree_path_prepend_index (path, walker2->offset);

      walker2 = walker->parent_elt;
      walker = walker->parent_level;
    }

  if (root)
    {
      real_path = gtk_tree_model_filter_add_root (path, root);
      gtk_tree_path_free (path);
      return real_path;
    }

  return path;
}

static GtkTreePath *
gtk_tree_model_filter_add_root (GtkTreePath *src,
                                GtkTreePath *root)
{
  GtkTreePath *retval;
  gint i;

  retval = gtk_tree_path_copy (root);

  for (i = 0; i < gtk_tree_path_get_depth (src); i++)
    gtk_tree_path_append_index (retval, gtk_tree_path_get_indices (src)[i]);

  return retval;
}

static GtkTreePath *
gtk_tree_model_filter_remove_root (GtkTreePath *src,
                                   GtkTreePath *root)
{
  GtkTreePath *retval;
  gint i;
  gint depth;
  gint *indices;

  if (gtk_tree_path_get_depth (src) <= gtk_tree_path_get_depth (root))
    return NULL;

  depth = gtk_tree_path_get_depth (src);
  indices = gtk_tree_path_get_indices (src);

  for (i = 0; i < gtk_tree_path_get_depth (root); i++)
    if (indices[i] != gtk_tree_path_get_indices (root)[i])
      return NULL;

  retval = gtk_tree_path_new ();

  for (; i < depth; i++)
    gtk_tree_path_append_index (retval, indices[i]);

  return retval;
}

static void
gtk_tree_model_filter_increment_stamp (GtkTreeModelFilter *filter)
{
  do
    {
      filter->priv->stamp++;
    }
  while (filter->priv->stamp == 0);

  gtk_tree_model_filter_clear_cache (filter);
}

static gboolean
gtk_tree_model_filter_visible (GtkTreeModelFilter *filter,
                               GtkTreeIter        *child_iter)
{
  if (filter->priv->visible_func)
    {
      return (filter->priv->visible_func (filter->priv->child_model,
                                    child_iter,
                                    filter->priv->visible_data));
    }
  else if (filter->priv->visible_column >= 0)
   {
     GValue val = {0, };

     gtk_tree_model_get_value (filter->priv->child_model, child_iter,
                               filter->priv->visible_column, &val);

     if (g_value_get_boolean (&val))
       {
         g_value_unset (&val);
         return TRUE;
       }

     g_value_unset (&val);
     return FALSE;
   }

  /* no filter thing set, so always visible */
  return TRUE;
}

static void
gtk_tree_model_filter_clear_cache_helper (GtkTreeModelFilter *filter,
                                          FilterLevel        *level)
{
  gint i;

  g_assert (level);

  for (i = 0; i < level->array->len; i++)
    {
      if (g_array_index (level->array, FilterElt, i).zero_ref_count > 0)
        gtk_tree_model_filter_clear_cache_helper (filter, g_array_index (level->array, FilterElt, i).children);
     }

  if (level->ref_count == 0 && level != filter->priv->root)
    {
      gtk_tree_model_filter_free_level (filter, level);
      return;
    }
}

static FilterElt *
gtk_tree_model_filter_fetch_child (GtkTreeModelFilter *filter,
                                   FilterLevel        *level,
                                   gint                offset,
                                   gint               *index)
{
  gint i = 0;
  gint start, middle, end;
  gint len;
  GtkTreePath *c_path = NULL;
  GtkTreeIter c_iter;
  GtkTreePath *c_parent_path = NULL;
  GtkTreeIter c_parent_iter;
  FilterElt elt;

  /* check if child exists and is visible */
  if (level->parent_elt)
    {
      c_parent_path =
        gtk_tree_model_filter_elt_get_path (level->parent_level,
                                            level->parent_elt,
                                            filter->priv->virtual_root);
      if (!c_parent_path)
        return NULL;
    }
  else
    {
      if (filter->priv->virtual_root)
        c_parent_path = gtk_tree_path_copy (filter->priv->virtual_root);
      else
        c_parent_path = NULL;
    }

  if (c_parent_path)
    {
      gtk_tree_model_get_iter (filter->priv->child_model,
                               &c_parent_iter,
                               c_parent_path);
      len = gtk_tree_model_iter_n_children (filter->priv->child_model,
                                            &c_parent_iter);

      c_path = gtk_tree_path_copy (c_parent_path);
      gtk_tree_path_free (c_parent_path);
    }
  else
    {
      len = gtk_tree_model_iter_n_children (filter->priv->child_model, NULL);
      c_path = gtk_tree_path_new ();
    }

  gtk_tree_path_append_index (c_path, offset);
  gtk_tree_model_get_iter (filter->priv->child_model, &c_iter, c_path);
  gtk_tree_path_free (c_path);

  if (offset >= len || !gtk_tree_model_filter_visible (filter, &c_iter))
    return NULL;

  /* add child */
  elt.offset = offset;
  elt.zero_ref_count = 0;
  elt.ref_count = 0;
  elt.children = NULL;
  /* visibility should be FALSE as we don't emit row_inserted */
  elt.visible = FALSE;

  if (GTK_TREE_MODEL_FILTER_CACHE_CHILD_ITERS (filter))
    elt.iter = c_iter;

  /* find index (binary search on offset) */
  start = 0;
  end = level->array->len;

  if (start != end)
    {
      while (start != end)
        {
          middle = (start + end) / 2;

          if (g_array_index (level->array, FilterElt, middle).offset <= offset)
            start = middle + 1;
          else
            end = middle;
        }

      if (g_array_index (level->array, FilterElt, middle).offset <= offset)
        i = middle + 1;
      else
        i = middle;
    }
  else
    i = 0;

  g_array_insert_val (level->array, i, elt);
  *index = i;

  for (i = MAX (--i, 0); i < level->array->len; i++)
    {
      FilterElt *e = &(g_array_index (level->array, FilterElt, i));
      if (e->children)
        e->children->parent_elt = e;
    }

  return &g_array_index (level->array, FilterElt, *index);
}

static void
gtk_tree_model_filter_remove_node (GtkTreeModelFilter *filter,
                                   GtkTreeIter        *iter,
                                   gboolean            emit_signal)
{
  FilterElt *elt, *parent;
  FilterLevel *level, *parent_level;
  gint offset, i, length, level_refcount;

  /* FIXME: this function is very ugly. I need to rethink and
   * rewrite it someday.
   */

  level = FILTER_LEVEL (iter->user_data);
  elt = FILTER_ELT (iter->user_data2);

  parent = level->parent_elt;
  parent_level = level->parent_level;
  length = level->array->len;
  offset = elt->offset;

  /* ref counting */
  while (elt->ref_count > 0)
    gtk_tree_model_filter_real_unref_node (GTK_TREE_MODEL (filter),
                                           iter, FALSE);

  level_refcount = level->ref_count;

  /* do the ref counting first! this touches the stamp */
  if (emit_signal)
    {
      GtkTreePath *path;

      path = gtk_tree_model_get_path (GTK_TREE_MODEL (filter), iter);
      gtk_tree_model_filter_increment_stamp (filter);
      gtk_tree_model_row_deleted (GTK_TREE_MODEL (filter), path);
      gtk_tree_path_free (path);
    }

  if ((length == 1 || level_refcount == 0) &&
      emit_signal && iter->user_data != filter->priv->root)
    {
      /* above code destroyed the level */
      goto emit_has_child_toggled;
    }

  if (length == 1)
    {
      /* kill the level */
      gtk_tree_model_filter_free_level (filter, level);

      if (!filter->priv->root)
        /* we killed the root */
        return;
    }
  else
    {
      FilterElt *tmp;

      /* remove the node */
      tmp = bsearch_elt_with_offset (level->array, elt->offset, &i);

      if (tmp)
        {
          g_array_remove_index (level->array, i);

          for (i = MAX (--i, 0); i < level->array->len; i++)
            {
              /* NOTE: here we do *not* decrease offsets, because the node was
               * not removed from the child model
               */
              elt = &g_array_index (level->array, FilterElt, i);
              if (elt->children)
                elt->children->parent_elt = elt;
            }
        }
    }

emit_has_child_toggled:
  /* children are being handled first, so we can check it this way
   *
   * yes this if-statement is ugly
   */
  if ((parent && parent->children && parent->children->array->len <= 1) ||
      (length == 1 && emit_signal && iter->user_data != filter->priv->root))
    {
      /* latest child has been removed, level has been destroyed */
      GtkTreeIter piter;
      GtkTreePath *ppath;

      piter.stamp = filter->priv->stamp;
      piter.user_data = parent_level;
      piter.user_data2 = parent;

      ppath = gtk_tree_model_get_path (GTK_TREE_MODEL (filter), &piter);

      gtk_tree_model_row_has_child_toggled (GTK_TREE_MODEL (filter),
                                            ppath, &piter);
      gtk_tree_path_free (ppath);
    }
}

static void
gtk_tree_model_filter_update_children (GtkTreeModelFilter *filter,
				       FilterLevel        *level,
				       FilterElt          *elt)
{
  GtkTreeIter c_iter;
  GtkTreeIter iter;

  if (!elt->visible)
    return;

  iter.stamp = filter->priv->stamp;
  iter.user_data = level;
  iter.user_data2 = elt;

  gtk_tree_model_filter_convert_iter_to_child_iter (filter, &c_iter, &iter);

  if (gtk_tree_model_iter_has_child (filter->priv->child_model, &c_iter))
    {
      GtkTreePath *path = gtk_tree_model_get_path (GTK_TREE_MODEL (filter),
                                                   &iter);
      gtk_tree_model_row_has_child_toggled (GTK_TREE_MODEL (filter),
                                            path,
                                            &iter);
      if (path)
        gtk_tree_path_free (path);
    }
}

static FilterElt *
bsearch_elt_with_offset (GArray *array,
                         gint    offset,
                         gint   *index)
{
  gint start, middle, end;
  FilterElt *elt;

  start = 0;
  end = array->len;

  if (array->len < 1)
    return NULL;

  if (start == end)
    {
      elt = &g_array_index (array, FilterElt, 0);

      if (elt->offset == offset)
        {
          *index = 0;
          return elt;
        }
      else
        return NULL;
    }

  do
    {
      middle = (start + end) / 2;

      elt = &g_array_index (array, FilterElt, middle);

      if (elt->offset < offset)
        start = middle + 1;
      else if (elt->offset > offset)
        end = middle;
      else
        break;
    }
  while (start != end);

  if (elt->offset == offset)
    {
      *index = middle;
      return elt;
    }

  return NULL;
}

/* TreeModel signals */
static void
gtk_tree_model_filter_row_changed (GtkTreeModel *c_model,
                                   GtkTreePath  *c_path,
                                   GtkTreeIter  *c_iter,
                                   gpointer      data)
{
  GtkTreeModelFilter *filter = GTK_TREE_MODEL_FILTER (data);
  GtkTreeIter iter;
  GtkTreeIter children;
  GtkTreeIter real_c_iter;
  GtkTreePath *path = NULL;

  FilterElt *elt;
  FilterLevel *level;

  gboolean requested_state;
  gboolean current_state;
  gboolean free_c_path = FALSE;

  g_return_if_fail (c_path != NULL || c_iter != NULL);

  if (!c_path)
    {
      c_path = gtk_tree_model_get_path (c_model, c_iter);
      free_c_path = TRUE;
    }

  if (c_iter)
    real_c_iter = *c_iter;
  else
    gtk_tree_model_get_iter (c_model, &real_c_iter, c_path);

  /* is this node above the virtual root? */
  if (filter->priv->virtual_root
      && (gtk_tree_path_get_depth (filter->priv->virtual_root)
          >= gtk_tree_path_get_depth (c_path)))
    goto done;

  /* what's the requested state? */
  requested_state = gtk_tree_model_filter_visible (filter, &real_c_iter);

  /* now, let's see whether the item is there */
  path = gtk_real_tree_model_filter_convert_child_path_to_path (filter,
                                                                c_path,
                                                                FALSE,
                                                                FALSE);

  if (path)
    {
      gtk_tree_model_get_iter (GTK_TREE_MODEL (filter), &iter, path);
      current_state = FILTER_ELT (iter.user_data2)->visible;
    }
  else
    current_state = FALSE;

  if (current_state == FALSE && requested_state == FALSE)
    /* no changes required */
    goto done;

  if (current_state == TRUE && requested_state == FALSE)
    {
      /* get rid of this node */
      gtk_tree_model_get_iter (GTK_TREE_MODEL (filter), &iter, path);

      level = FILTER_LEVEL (iter.user_data);

      if (!level->parent_level)
        filter->priv->root_level_visible--;

      gtk_tree_model_filter_remove_node (filter, &iter, TRUE);

      goto done;
    }

  if (current_state == TRUE && requested_state == TRUE)
    {
      /* progate the signal */
      gtk_tree_model_get_iter (GTK_TREE_MODEL (filter), &iter, path);
      gtk_tree_model_row_changed (GTK_TREE_MODEL (filter), path, &iter);

      level = FILTER_LEVEL (iter.user_data);
      elt = FILTER_ELT (iter.user_data2);

      /* and update the children */
      if (gtk_tree_model_iter_children (c_model, &children, &real_c_iter))
        gtk_tree_model_filter_update_children (filter, level, elt);

      goto done;
    }

  /* only current == FALSE and requested == TRUE is left,
   * pull in the child
   */
  g_return_if_fail (current_state == FALSE && requested_state == TRUE);

  /* make sure the new item has been pulled in */
  if (!filter->priv->root)
    {
      gint i;
      FilterLevel *root;

      gtk_tree_model_filter_build_level (filter, NULL, NULL);

      root = FILTER_LEVEL (filter->priv->root);

      if (root)
        {
          for (i = 0; i < root->array->len; i++)
            g_array_index (root->array, FilterElt, i).visible = FALSE;
          filter->priv->root_level_visible = 0;
        }
    }

  gtk_tree_model_filter_increment_stamp (filter);

  if (!path)
    path = gtk_real_tree_model_filter_convert_child_path_to_path (filter,
                                                                  c_path,
                                                                  TRUE,
                                                                  TRUE);

  if (!path)
    /* parent is probably being filtered out */
    goto done;    

  gtk_tree_model_get_iter (GTK_TREE_MODEL (filter), &iter, path);

  level = FILTER_LEVEL (iter.user_data);
  elt = FILTER_ELT (iter.user_data2);

  elt->visible = TRUE;

  if (!level->parent_level)
    filter->priv->root_level_visible++;

  /* update stamp */
  gtk_tree_model_row_inserted (GTK_TREE_MODEL (filter), path, &iter);

  if (gtk_tree_model_iter_children (c_model, &children, c_iter))
    gtk_tree_model_filter_update_children (filter, level, elt);

done:
  if (path)
    gtk_tree_path_free (path);

  if (free_c_path)
    gtk_tree_path_free (c_path);
}

static void
gtk_tree_model_filter_row_inserted (GtkTreeModel *c_model,
                                    GtkTreePath  *c_path,
                                    GtkTreeIter  *c_iter,
                                    gpointer      data)
{
  GtkTreeModelFilter *filter = GTK_TREE_MODEL_FILTER (data);
  GtkTreePath *path = NULL;
  GtkTreePath *real_path = NULL;
  GtkTreeIter iter;

  GtkTreeIter real_c_iter;

  FilterElt *elt;
  FilterLevel *level;
  FilterLevel *parent_level;

  gint i = 0, offset, index = -1;

  gboolean free_c_path = FALSE;

  g_return_if_fail (c_path != NULL || c_iter != NULL);

  if (!c_path)
    {
      c_path = gtk_tree_model_get_path (c_model, c_iter);
      free_c_path = TRUE;
    }

  if (c_iter)
    real_c_iter = *c_iter;
  else
    gtk_tree_model_get_iter (c_model, &real_c_iter, c_path);

  /* the row has already been inserted. so we need to fixup the
   * virtual root here first
   */
  if (filter->priv->virtual_root)
    {
      if (gtk_tree_path_get_depth (filter->priv->virtual_root) >=
          gtk_tree_path_get_depth (c_path))
        {
          gint level;
          gint *v_indices, *c_indices;

          level = gtk_tree_path_get_depth (c_path) - 1;
          v_indices = gtk_tree_path_get_indices (filter->priv->virtual_root);
          c_indices = gtk_tree_path_get_indices (c_path);

          if (v_indices[level] >= c_indices[level])
            (v_indices[level])++;
        }
    }

  if (!filter->priv->root)
    {
      gtk_tree_model_filter_build_level (filter, NULL, NULL);
      /* that already put the inserted iter in the level */

      goto done_and_emit;
    }

  parent_level = level = FILTER_LEVEL (filter->priv->root);

  /* subtract virtual root if necessary */
  if (filter->priv->virtual_root)
    {
      real_path = gtk_tree_model_filter_remove_root (c_path,
                                                     filter->priv->virtual_root);
      /* not our kiddo */
      if (!real_path)
        goto done;
    }
  else
    real_path = gtk_tree_path_copy (c_path);

  if (gtk_tree_path_get_depth (real_path) - 1 >= 1)
    {
      /* find the parent level */
      while (i < gtk_tree_path_get_depth (real_path) - 1)
        {
          gint j;

          if (!level)
            /* we don't cover this signal */
            goto done;

          elt = bsearch_elt_with_offset (level->array,
                                         gtk_tree_path_get_indices (real_path)[i],
                                         &j);

          if (!elt)
            /* parent is probably being filtered out */
            goto done;

          if (!elt->children)
            {
              GtkTreePath *tmppath;
              GtkTreeIter  tmpiter;

              tmpiter.stamp = filter->priv->stamp;
              tmpiter.user_data = level;
              tmpiter.user_data2 = elt;

              tmppath = gtk_tree_model_get_path (GTK_TREE_MODEL (data),
                                                 &tmpiter);

              if (tmppath)
                {
                  gtk_tree_model_row_has_child_toggled (GTK_TREE_MODEL (data),
                                                        tmppath, &tmpiter);
                  gtk_tree_path_free (tmppath);
                }

              /* not covering this signal */
              goto done;
            }

          level = elt->children;
          parent_level = level;
          i++;
        }
    }

  if (!parent_level)
    goto done;

  /* let's try to insert the value */
  offset = gtk_tree_path_get_indices (real_path)[gtk_tree_path_get_depth (real_path) - 1];

  /* update the offsets, yes if we didn't insert the node above, there will
   * be a gap here. This will be filled with the node (via fetch_child) when
   * it becomes visible
   */
  for (i = 0; i < level->array->len; i++)
    {
      FilterElt *e = &g_array_index (level->array, FilterElt, i);
      if ((e->offset >= offset))
        e->offset++;
    }

  /* only insert when visible */
  if (gtk_tree_model_filter_visible (filter, &real_c_iter))
    {
      FilterElt felt;

      if (GTK_TREE_MODEL_FILTER_CACHE_CHILD_ITERS (filter))
        felt.iter = real_c_iter;

      felt.offset = offset;
      felt.zero_ref_count = 0;
      felt.ref_count = 0;
      felt.visible = TRUE;
      felt.children = NULL;

      for (i = 0; i < level->array->len; i++)
        if (g_array_index (level->array, FilterElt, i).offset > offset)
          break;

      g_array_insert_val (level->array, i, felt);
      index = i;

      if (!level->parent_level)
        filter->priv->root_level_visible++;
    }

  /* another iteration to update the references of children to parents. */
  for (i = 0; i < level->array->len; i++)
    {
      FilterElt *e = &g_array_index (level->array, FilterElt, i);
      if (e->children)
        e->children->parent_elt = e;
    }

  /* don't emit the signal if we aren't visible */
  if (!gtk_tree_model_filter_visible (filter, &real_c_iter))
    goto done;

done_and_emit:
  /* NOTE: pass c_path here and NOT real_path. This function does
   * root subtraction itself
   */
  path = gtk_real_tree_model_filter_convert_child_path_to_path (filter,
                                                                c_path,
                                                                FALSE, TRUE);

  if (!path)
    goto done;

  gtk_tree_model_filter_increment_stamp (filter);

  gtk_tree_model_get_iter (GTK_TREE_MODEL (data), &iter, path);
  gtk_tree_model_row_inserted (GTK_TREE_MODEL (data), path, &iter);

  gtk_tree_path_free (path);

done:
  if (real_path)
    gtk_tree_path_free (real_path);

  if (free_c_path)
    gtk_tree_path_free (c_path);
}

static void
gtk_tree_model_filter_row_has_child_toggled (GtkTreeModel *c_model,
                                             GtkTreePath  *c_path,
                                             GtkTreeIter  *c_iter,
                                             gpointer      data)
{
  GtkTreeModelFilter *filter = GTK_TREE_MODEL_FILTER (data);
  GtkTreePath *path;
  GtkTreeIter iter;

  g_return_if_fail (c_path != NULL && c_iter != NULL);

  /* FIXME: does this code work? */

  if (!gtk_tree_model_filter_visible (filter, c_iter))
    return;

  path = gtk_real_tree_model_filter_convert_child_path_to_path (filter,
                                                                c_path,
                                                                FALSE,
                                                                TRUE);
  if (!path)
    return;

  gtk_tree_model_get_iter (GTK_TREE_MODEL (data), &iter, path);
  gtk_tree_model_row_has_child_toggled (GTK_TREE_MODEL (data), path, &iter);

  gtk_tree_path_free (path);
}

static void
gtk_tree_model_filter_row_deleted (GtkTreeModel *c_model,
                                   GtkTreePath  *c_path,
                                   gpointer      data)
{
  GtkTreeModelFilter *filter = GTK_TREE_MODEL_FILTER (data);
  GtkTreePath *path;
  GtkTreeIter iter;
  FilterElt *elt;
  FilterLevel *level;
  gint offset;
  gboolean emit_signal = TRUE;
  gint i;

  g_return_if_fail (c_path != NULL);

  /* special case the deletion of an ancestor of the virtual root */
  if (filter->priv->virtual_root &&
      (gtk_tree_path_is_ancestor (c_path, filter->priv->virtual_root) ||
       !gtk_tree_path_compare (c_path, filter->priv->virtual_root)))
    {
      gint i;
      GtkTreePath *path;
      FilterLevel *level = FILTER_LEVEL (filter->priv->root);

      if (!level)
        return;

      /* remove everything in the filter model
       *
       * For now, we just iterate over the root level and emit a
       * row_deleted for each FilterElt. Not sure if this is correct.
       */

      gtk_tree_model_filter_increment_stamp (filter);
      path = gtk_tree_path_new ();
      gtk_tree_path_append_index (path, 0);

      for (i = 0; i < level->array->len; i++)
        gtk_tree_model_row_deleted (GTK_TREE_MODEL (data), path);

      gtk_tree_path_free (path);
      gtk_tree_model_filter_free_level (filter, filter->priv->root);

      return;
    }

  /* fixup virtual root */
  if (filter->priv->virtual_root)
    {
      if (gtk_tree_path_get_depth (filter->priv->virtual_root) >=
          gtk_tree_path_get_depth (c_path))
        {
          gint level;
          gint *v_indices, *c_indices;

          level = gtk_tree_path_get_depth (c_path) - 1;
          v_indices = gtk_tree_path_get_indices (filter->priv->virtual_root);
          c_indices = gtk_tree_path_get_indices (c_path);

          if (v_indices[level] > c_indices[level])
            (v_indices[level])--;
        }
    }

  path = gtk_real_tree_model_filter_convert_child_path_to_path (filter,
                                                                c_path,
                                                                FALSE,
                                                                FALSE);

  if (!path)
    {
      /* fixup the offsets */
      GtkTreePath *real_path;

      if (!filter->priv->root)
        return;

      level = FILTER_LEVEL (filter->priv->root);

      /* subtract vroot if necessary */
      if (filter->priv->virtual_root)
        {
          real_path = gtk_tree_model_filter_remove_root (c_path,
                                                         filter->priv->virtual_root);
          /* we don't handle this */
          if (!real_path)
            return;
        }
      else
        real_path = gtk_tree_path_copy (c_path);

      i = 0;
      if (gtk_tree_path_get_depth (real_path) - 1 >= 1)
        {
          while (i < gtk_tree_path_get_depth (real_path) - 1)
            {
              gint j;

              if (!level)
                {
                  /* we don't cover this */
                  gtk_tree_path_free (real_path);
                  return;
                }

              elt = bsearch_elt_with_offset (level->array,
                                             gtk_tree_path_get_indices (real_path)[i],
                                             &j);

              if (!elt || !elt->children)
                {
                  /* parent is filtered out, so no level */
                  gtk_tree_path_free (real_path);
                  return;
                }

              level = elt->children;
              i++;
            }
        }

      offset = gtk_tree_path_get_indices (real_path)[gtk_tree_path_get_depth (real_path) - 1];
      gtk_tree_path_free (real_path);

      if (!level)
        return;

      /* we need:
       * - the offset of the removed item
       * - the level
       */
      for (i = 0; i < level->array->len; i++)
        {
          elt = &g_array_index (level->array, FilterElt, i);
          if (elt->offset > offset)
            elt->offset--;
          if (elt->children)
            elt->children->parent_elt = elt;
        }

      return;
    }

  gtk_tree_model_get_iter (GTK_TREE_MODEL (data), &iter, path);

  level = FILTER_LEVEL (iter.user_data);
  elt = FILTER_ELT (iter.user_data2);
  offset = elt->offset;

  if (!level->parent_level && elt->visible)
    filter->priv->root_level_visible--;

  if (emit_signal)
    {
      if (level->ref_count == 0 && level != filter->priv->root)
        {
          gtk_tree_model_filter_increment_stamp (filter);
          gtk_tree_model_row_deleted (GTK_TREE_MODEL (data), path);

          gtk_tree_path_free (path);
          return;
        }

      gtk_tree_model_filter_increment_stamp (filter);
      gtk_tree_model_row_deleted (GTK_TREE_MODEL (data), path);
      iter.stamp = filter->priv->stamp;

      while (elt->ref_count > 0)
        gtk_tree_model_filter_real_unref_node (GTK_TREE_MODEL (data), &iter,
                                               FALSE);
    }

  if (level->array->len == 1)
    {
      /* kill level */
      gtk_tree_model_filter_free_level (filter, level);
    }
  else
    {
      FilterElt *tmp;

      /* remove the row */
      tmp = bsearch_elt_with_offset (level->array, elt->offset, &i);

      offset = tmp->offset;
      g_array_remove_index (level->array, i);

      for (i = MAX (--i, 0); i < level->array->len; i++)
        {
          elt = &g_array_index (level->array, FilterElt, i);
          if (elt->offset > offset)
            elt->offset--;
          if (elt->children)
            elt->children->parent_elt = elt;
        }
    }

  gtk_tree_path_free (path);
}

static void
gtk_tree_model_filter_rows_reordered (GtkTreeModel *c_model,
                                      GtkTreePath  *c_path,
                                      GtkTreeIter  *c_iter,
                                      gint         *new_order,
                                      gpointer      data)
{
  FilterElt *elt;
  FilterLevel *level;
  GtkTreeModelFilter *filter = GTK_TREE_MODEL_FILTER (data);

  GtkTreePath *path;
  GtkTreeIter iter;

  gint *tmp_array;
  gint i, j, elt_count;
  gint length;

  GArray *new_array;

  g_return_if_fail (new_order != NULL);

  if (c_path == NULL || gtk_tree_path_get_indices (c_path) == NULL)
    {
      if (!filter->priv->root)
        return;

      length = gtk_tree_model_iter_n_children (c_model, NULL);

      if (filter->priv->virtual_root)
        {
          gint new_pos = -1;

          /* reorder root level of path */
          for (i = 0; i < length; i++)
            if (new_order[i] == gtk_tree_path_get_indices (filter->priv->virtual_root)[0])
              new_pos = i;

          if (new_pos < 0)
            return;

          gtk_tree_path_get_indices (filter->priv->virtual_root)[0] = new_pos;
          return;
        }

      path = gtk_tree_path_new ();
      level = FILTER_LEVEL (filter->priv->root);
    }
  else
    {
      GtkTreeIter child_iter;

      /* virtual root anchor reordering */
      if (filter->priv->virtual_root &&
          gtk_tree_path_get_depth (c_path) <
          gtk_tree_path_get_depth (filter->priv->virtual_root))
        {
          gint new_pos = -1;
          gint length;
          gint level;
          GtkTreeIter real_c_iter;

          level = gtk_tree_path_get_depth (c_path);

          if (c_iter)
            real_c_iter = *c_iter;
          else
            gtk_tree_model_get_iter (c_model, &real_c_iter, c_path);

          length = gtk_tree_model_iter_n_children (c_model, &real_c_iter);

          for (i = 0; i < length; i++)
            if (new_order[i] == gtk_tree_path_get_indices (filter->priv->virtual_root)[level])
              new_pos = i;

          if (new_pos < 0)
            return;

          gtk_tree_path_get_indices (filter->priv->virtual_root)[level] = new_pos;
          return;
        }

      path = gtk_real_tree_model_filter_convert_child_path_to_path (filter,
                                                                    c_path,
                                                                    FALSE,
                                                                    FALSE);
      if (!path && filter->priv->virtual_root &&
          gtk_tree_path_compare (c_path, filter->priv->virtual_root))
        return;

      if (!path && !filter->priv->virtual_root)
        return;

      if (!path)
        {
          /* root level mode */
          if (!c_iter)
            gtk_tree_model_get_iter (c_model, c_iter, c_path);
          length = gtk_tree_model_iter_n_children (c_model, c_iter);
          path = gtk_tree_path_new ();
          level = FILTER_LEVEL (filter->priv->root);
        }
      else
        {
          gtk_tree_model_get_iter (GTK_TREE_MODEL (data), &iter, path);

          level = FILTER_LEVEL (iter.user_data);
          elt = FILTER_ELT (iter.user_data2);

          if (!elt->children)
            {
              gtk_tree_path_free (path);
              return;
            }

          level = elt->children;

          gtk_tree_model_filter_convert_iter_to_child_iter (GTK_TREE_MODEL_FILTER (filter), &child_iter, &iter);
          length = gtk_tree_model_iter_n_children (c_model, &child_iter);
        }
    }

  if (level->array->len < 1)
    {
      gtk_tree_path_free (path);
      return;
    }

  /* NOTE: we do not bail out here if level->array->len < 2 like
   * GtkTreeModelSort does. This because we do some special tricky
   * reordering.
   */

  /* construct a new array */
  new_array = g_array_sized_new (FALSE, FALSE, sizeof (FilterElt),
                                 level->array->len);
  tmp_array = g_new (gint, level->array->len);

  for (i = 0, elt_count = 0; i < length; i++)
    {
      FilterElt *e = NULL;
      gint old_offset = -1;

      for (j = 0; j < level->array->len; j++)
        if (g_array_index (level->array, FilterElt, j).offset == new_order[i])
          {
            e = &g_array_index (level->array, FilterElt, j);
            old_offset = j;
            break;
          }

      if (!e)
        continue;

      tmp_array[elt_count] = old_offset;
      g_array_append_val (new_array, *e);
      g_array_index (new_array, FilterElt, elt_count).offset = i;
      elt_count++;
    }

  g_array_free (level->array, TRUE);
  level->array = new_array;

  /* fix up stuff */
  for (i = 0; i < level->array->len; i++)
    {
      FilterElt *e = &g_array_index (level->array, FilterElt, i);
      if (e->children)
        e->children->parent_elt = e;
    }

  /* emit rows_reordered */
  if (!gtk_tree_path_get_indices (path))
    gtk_tree_model_rows_reordered (GTK_TREE_MODEL (data), path, NULL,
                                   tmp_array);
  else
    gtk_tree_model_rows_reordered (GTK_TREE_MODEL (data), path, &iter,
                                   tmp_array);

  /* done */
  g_free (tmp_array);
  gtk_tree_path_free (path);
}

/* TreeModelIface implementation */
static GtkTreeModelFlags
gtk_tree_model_filter_get_flags (GtkTreeModel *model)
{
  GtkTreeModelFlags flags;

  g_return_val_if_fail (GTK_IS_TREE_MODEL_FILTER (model), 0);
  g_return_val_if_fail (GTK_TREE_MODEL_FILTER (model)->priv->child_model != NULL, 0);

  flags = gtk_tree_model_get_flags (GTK_TREE_MODEL_FILTER (model)->priv->child_model);

  if ((flags & GTK_TREE_MODEL_LIST_ONLY) == GTK_TREE_MODEL_LIST_ONLY)
    return GTK_TREE_MODEL_LIST_ONLY;

  return 0;
}

static gint
gtk_tree_model_filter_get_n_columns (GtkTreeModel *model)
{
  GtkTreeModelFilter *filter = (GtkTreeModelFilter *)model;

  g_return_val_if_fail (GTK_IS_TREE_MODEL_FILTER (model), 0);
  g_return_val_if_fail (filter->priv->child_model != NULL, 0);

  if (filter->priv->child_model == NULL)
    return 0;

  /* so we can't modify the modify func after this ... */
  filter->priv->modify_func_set = TRUE;

  if (filter->priv->modify_n_columns > 0)
    return filter->priv->modify_n_columns;

  return gtk_tree_model_get_n_columns (filter->priv->child_model);
}

static GType
gtk_tree_model_filter_get_column_type (GtkTreeModel *model,
                                       gint          index)
{
  GtkTreeModelFilter *filter = (GtkTreeModelFilter *)model;

  g_return_val_if_fail (GTK_IS_TREE_MODEL_FILTER (model), G_TYPE_INVALID);
  g_return_val_if_fail (filter->priv->child_model != NULL, G_TYPE_INVALID);

  /* so we can't modify the modify func after this ... */
  filter->priv->modify_func_set = TRUE;

  if (filter->priv->modify_types)
    {
      g_return_val_if_fail (index < filter->priv->modify_n_columns, G_TYPE_INVALID);

      return filter->priv->modify_types[index];
    }

  return gtk_tree_model_get_column_type (filter->priv->child_model, index);
}

static gboolean
gtk_tree_model_filter_get_iter (GtkTreeModel *model,
                                GtkTreeIter  *iter,
                                GtkTreePath  *path)
{
  GtkTreeModelFilter *filter = (GtkTreeModelFilter *)model;
  gint *indices;
  FilterLevel *level;
  gint depth, i;

  g_return_val_if_fail (GTK_IS_TREE_MODEL_FILTER (model), FALSE);
  g_return_val_if_fail (filter->priv->child_model != NULL, FALSE);

  indices = gtk_tree_path_get_indices (path);

  if (filter->priv->root == NULL)
    gtk_tree_model_filter_build_level (filter, NULL, NULL);
  level = FILTER_LEVEL (filter->priv->root);

  depth = gtk_tree_path_get_depth (path);
  if (!depth)
    {
      iter->stamp = 0;
      return FALSE;
    }

  for (i = 0; i < depth - 1; i++)
    {
      if (!level || indices[i] >= level->array->len)
        {
          return FALSE;
        }

      if (!g_array_index (level->array, FilterElt, indices[i]).children)
        gtk_tree_model_filter_build_level (filter, level,
                                           &g_array_index (level->array,
                                                           FilterElt,
                                                           indices[i]));
      level = g_array_index (level->array, FilterElt, indices[i]).children;
    }

  if (!level || indices[i] >= level->array->len)
    {
      iter->stamp = 0;
      return FALSE;
    }

  iter->stamp = filter->priv->stamp;
  iter->user_data = level;
  iter->user_data2 = &g_array_index (level->array, FilterElt,
                                     indices[depth - 1]);

  return TRUE;
}

static GtkTreePath *
gtk_tree_model_filter_get_path (GtkTreeModel *model,
                                GtkTreeIter  *iter)
{
  GtkTreePath *retval;
  FilterLevel *level;
  FilterElt *elt;

  g_return_val_if_fail (GTK_IS_TREE_MODEL_FILTER (model), NULL);
  g_return_val_if_fail (GTK_TREE_MODEL_FILTER (model)->priv->child_model != NULL, NULL);
  g_return_val_if_fail (GTK_TREE_MODEL_FILTER (model)->priv->stamp == iter->stamp, NULL);

  retval = gtk_tree_path_new ();
  level = iter->user_data;
  elt = iter->user_data2;

  while (level)
    {
      gtk_tree_path_prepend_index (retval,
                                   elt - FILTER_ELT (level->array->data));
      elt = level->parent_elt;
      level = level->parent_level;
    }

  return retval;
}

static void
gtk_tree_model_filter_get_value (GtkTreeModel *model,
                                 GtkTreeIter  *iter,
                                 gint          column,
                                 GValue       *value)
{
  GtkTreeIter child_iter;
  GtkTreeModelFilter *filter = GTK_TREE_MODEL_FILTER (model);

  g_return_if_fail (GTK_IS_TREE_MODEL_FILTER (model));
  g_return_if_fail (GTK_TREE_MODEL_FILTER (model)->priv->child_model != NULL);
  g_return_if_fail (GTK_TREE_MODEL_FILTER (model)->priv->stamp == iter->stamp);

  if (filter->priv->modify_func)
    {
      g_return_if_fail (column < filter->priv->modify_n_columns);

      g_value_init (value, filter->priv->modify_types[column]);
      filter->priv->modify_func (model,
                           iter,
                           value,
                           column,
                           filter->priv->modify_data);

      return;
    }

  gtk_tree_model_filter_convert_iter_to_child_iter (GTK_TREE_MODEL_FILTER (model), &child_iter, iter);
  gtk_tree_model_get_value (GTK_TREE_MODEL_FILTER (model)->priv->child_model,
                            &child_iter, column, value);
}

static gboolean
gtk_tree_model_filter_iter_next (GtkTreeModel *model,
                                 GtkTreeIter  *iter)
{
  FilterLevel *level;
  FilterElt *elt;

  g_return_val_if_fail (GTK_IS_TREE_MODEL_FILTER (model), FALSE);
  g_return_val_if_fail (GTK_TREE_MODEL_FILTER (model)->priv->child_model != NULL, FALSE);
  g_return_val_if_fail (GTK_TREE_MODEL_FILTER (model)->priv->stamp == iter->stamp, FALSE);

  level = iter->user_data;
  elt = iter->user_data2;

  if (elt - FILTER_ELT (level->array->data) >= level->array->len - 1)
    {
      iter->stamp = 0;
      return FALSE;
    }

  iter->user_data2 = elt + 1;

  return TRUE;
}

static gboolean
gtk_tree_model_filter_iter_children (GtkTreeModel *model,
                                     GtkTreeIter  *iter,
                                     GtkTreeIter  *parent)
{
  GtkTreeModelFilter *filter = (GtkTreeModelFilter *)model;
  FilterLevel *level;

  iter->stamp = 0;
  g_return_val_if_fail (GTK_IS_TREE_MODEL_FILTER (model), FALSE);
  g_return_val_if_fail (filter->priv->child_model != NULL, FALSE);
  if (parent)
    g_return_val_if_fail (filter->priv->stamp == parent->stamp, FALSE);

  if (!parent)
    {
      if (!filter->priv->root)
        gtk_tree_model_filter_build_level (filter, NULL, NULL);
      if (!filter->priv->root)
        return FALSE;

      level = filter->priv->root;
      iter->stamp = filter->priv->stamp;
      iter->user_data = level;
      iter->user_data2 = level->array->data;
    }
  else
    {
      if (FILTER_ELT (parent->user_data2)->children == NULL)
        gtk_tree_model_filter_build_level (filter,
                                           FILTER_LEVEL (parent->user_data),
                                           FILTER_ELT (parent->user_data2));
      if (FILTER_ELT (parent->user_data2)->children == NULL)
        return FALSE;

      /* empty array? */
      if (FILTER_ELT (parent->user_data2)->children->array->len <= 0)
        return FALSE;

      iter->stamp = filter->priv->stamp;
      iter->user_data = FILTER_ELT (parent->user_data2)->children;
      iter->user_data2 = FILTER_LEVEL (iter->user_data)->array->data;
    }

  return TRUE;
}

static gboolean
gtk_tree_model_filter_iter_has_child (GtkTreeModel *model,
                                      GtkTreeIter  *iter)
{
  GtkTreeIter child_iter;
  GtkTreeModelFilter *filter = (GtkTreeModelFilter *)model;
  FilterElt *elt;

  g_return_val_if_fail (GTK_IS_TREE_MODEL_FILTER (model), FALSE);
  g_return_val_if_fail (filter->priv->child_model != NULL, FALSE);
  g_return_val_if_fail (filter->priv->stamp == iter->stamp, FALSE);

  filter = GTK_TREE_MODEL_FILTER (model);

  gtk_tree_model_filter_convert_iter_to_child_iter (GTK_TREE_MODEL_FILTER (model), &child_iter, iter);
  elt = FILTER_ELT (iter->user_data2);

  /* we need to build the level to check if not all children are filtered
   * out
   */
  if (!elt->children
      && gtk_tree_model_iter_has_child (filter->priv->child_model, &child_iter))
    gtk_tree_model_filter_build_level (filter, FILTER_LEVEL (iter->user_data),
                                       elt);

  /* FIXME: we should prolly count the visible nodes here, just like in
   * _iter_n_children.
   */
  if (elt->children && elt->children->array->len > 0)
    return TRUE;

  return FALSE;
}

static gint
gtk_tree_model_filter_iter_n_children (GtkTreeModel *model,
                                       GtkTreeIter  *iter)
{
  GtkTreeIter child_iter;
  GtkTreeModelFilter *filter = (GtkTreeModelFilter *)model;
  FilterElt *elt;

  g_return_val_if_fail (GTK_IS_TREE_MODEL_FILTER (model), 0);
  g_return_val_if_fail (filter->priv->child_model != NULL, 0);
  if (iter)
    g_return_val_if_fail (filter->priv->stamp == iter->stamp, 0);

  if (!iter)
    {
      if (!filter->priv->root)
        gtk_tree_model_filter_build_level (filter, NULL, NULL);

      /* count visible nodes */
      return filter->priv->root_level_visible;
    }

  elt = FILTER_ELT (iter->user_data2);
  gtk_tree_model_filter_convert_iter_to_child_iter (GTK_TREE_MODEL_FILTER (model), &child_iter, iter);

  if (!elt->children &&
      gtk_tree_model_iter_has_child (filter->priv->child_model, &child_iter))
    gtk_tree_model_filter_build_level (filter,
                                       FILTER_LEVEL (iter->user_data),
                                       elt);

  if (elt->children && elt->children->array->len)
    {
      int i = 0;
      int count = 0;
      GArray *a = elt->children->array;

      /* count visible nodes */
      for (i = 0; i < a->len; i++)
        if (g_array_index (a, FilterElt, i).visible)
          count++;

      return count;
    }

  return 0;
}

static gboolean
gtk_tree_model_filter_iter_nth_child (GtkTreeModel *model,
                                      GtkTreeIter  *iter,
                                      GtkTreeIter  *parent,
                                      gint          n)
{
  FilterLevel *level;
  GtkTreeIter children;

  g_return_val_if_fail (GTK_IS_TREE_MODEL_FILTER (model), FALSE);
  if (parent)
    g_return_val_if_fail (GTK_TREE_MODEL_FILTER (model)->priv->stamp == parent->stamp, FALSE);

  /* use this instead of has_Child to force us to build the level, if needed */
  if (gtk_tree_model_filter_iter_children (model, &children, parent) == FALSE)
    {
      iter->stamp = 0;
      return FALSE;
    }

  level = children.user_data;
  if (n >= level->array->len)
    {
      iter->stamp = 0;
      return FALSE;
    }

  iter->stamp = GTK_TREE_MODEL_FILTER (model)->priv->stamp;
  iter->user_data = level;
  iter->user_data2 = &g_array_index (level->array, FilterElt, n);

  return TRUE;
}

static gboolean
gtk_tree_model_filter_iter_parent (GtkTreeModel *model,
                                   GtkTreeIter  *iter,
                                   GtkTreeIter  *child)
{
  FilterLevel *level;

  iter->stamp = 0;
  g_return_val_if_fail (GTK_IS_TREE_MODEL_FILTER (model), FALSE);
  g_return_val_if_fail (GTK_TREE_MODEL_FILTER (model)->priv->child_model != NULL, FALSE);
  g_return_val_if_fail (GTK_TREE_MODEL_FILTER (model)->priv->stamp == child->stamp, FALSE);

  level = child->user_data;

  if (level->parent_level)
    {
      iter->stamp = GTK_TREE_MODEL_FILTER (model)->priv->stamp;
      iter->user_data = level->parent_level;
      iter->user_data2 = level->parent_elt;

      return TRUE;
    }

  return FALSE;
}

static void
gtk_tree_model_filter_ref_node (GtkTreeModel *model,
                                GtkTreeIter  *iter)
{
  GtkTreeModelFilter *filter = (GtkTreeModelFilter *)model;
  GtkTreeIter child_iter;
  FilterLevel *level;
  FilterElt *elt;

  g_return_if_fail (GTK_IS_TREE_MODEL_FILTER (model));
  g_return_if_fail (GTK_TREE_MODEL_FILTER (model)->priv->child_model != NULL);
  g_return_if_fail (GTK_TREE_MODEL_FILTER (model)->priv->stamp == iter->stamp);

  gtk_tree_model_filter_convert_iter_to_child_iter (GTK_TREE_MODEL_FILTER (model), &child_iter, iter);

  gtk_tree_model_ref_node (filter->priv->child_model, &child_iter);

  level = iter->user_data;
  elt = iter->user_data2;

  elt->ref_count++;
  level->ref_count++;
  if (level->ref_count == 1)
    {
      FilterLevel *parent_level = level->parent_level;
      FilterElt *parent_elt = level->parent_elt;

      /* we were at zero -- time to decrease the zero_ref_count val */
      do
        {
          if (parent_elt)
            parent_elt->zero_ref_count--;

          if (parent_level)
            {
              parent_elt = parent_level->parent_elt;
              parent_level = parent_level->parent_level;
            }
        }
      while (parent_level);
      filter->priv->zero_ref_count--;
    }
}

static void
gtk_tree_model_filter_unref_node (GtkTreeModel *model,
                                  GtkTreeIter  *iter)
{
  gtk_tree_model_filter_real_unref_node (model, iter, TRUE);
}

static void
gtk_tree_model_filter_real_unref_node (GtkTreeModel *model,
                                       GtkTreeIter  *iter,
                                       gboolean      propagate_unref)
{
  GtkTreeModelFilter *filter = (GtkTreeModelFilter *)model;
  FilterLevel *level;
  FilterElt *elt;

  g_return_if_fail (GTK_IS_TREE_MODEL_FILTER (model));
  g_return_if_fail (filter->priv->child_model != NULL);
  g_return_if_fail (filter->priv->stamp == iter->stamp);

  if (propagate_unref)
    {
      GtkTreeIter child_iter;
      gtk_tree_model_filter_convert_iter_to_child_iter (GTK_TREE_MODEL_FILTER (model), &child_iter, iter);
      gtk_tree_model_unref_node (filter->priv->child_model, &child_iter);
    }

  level = iter->user_data;
  elt = iter->user_data2;

  g_return_if_fail (elt->ref_count > 0);

  elt->ref_count--;
  level->ref_count--;
  if (level->ref_count == 0)
    {
      FilterLevel *parent_level = level->parent_level;
      FilterElt *parent_elt = level->parent_elt;

      /* we are at zero -- time to increase the zero_ref_count val */
      while (parent_level)
        {
          parent_elt->zero_ref_count++;

          parent_elt = parent_level->parent_elt;
          parent_level = parent_level->parent_level;
        }
      filter->priv->zero_ref_count++;
    }
}

/* TreeDragSource interface implementation */
static gboolean
gtk_tree_model_filter_row_draggable (GtkTreeDragSource *drag_source,
                                     GtkTreePath       *path)
{
  GtkTreeModelFilter *tree_model_filter = (GtkTreeModelFilter *)drag_source;
  GtkTreePath *child_path;
  gboolean draggable;

  g_return_val_if_fail (GTK_IS_TREE_MODEL_FILTER (drag_source), FALSE);
  g_return_val_if_fail (path != NULL, FALSE);

  child_path = gtk_tree_model_filter_convert_path_to_child_path (tree_model_filter, path);
  draggable = gtk_tree_drag_source_row_draggable (GTK_TREE_DRAG_SOURCE (tree_model_filter->priv->child_model), child_path);
  gtk_tree_path_free (child_path);

  return draggable;
}

static gboolean
gtk_tree_model_filter_drag_data_get (GtkTreeDragSource *drag_source,
                                     GtkTreePath       *path,
                                     GtkSelectionData  *selection_data)
{
  GtkTreeModelFilter *tree_model_filter = (GtkTreeModelFilter *)drag_source;
  GtkTreePath *child_path;
  gboolean gotten;

  g_return_val_if_fail (GTK_IS_TREE_MODEL_FILTER (drag_source), FALSE);
  g_return_val_if_fail (path != NULL, FALSE);

  child_path = gtk_tree_model_filter_convert_path_to_child_path (tree_model_filter, path);
  gotten = gtk_tree_drag_source_drag_data_get (GTK_TREE_DRAG_SOURCE (tree_model_filter->priv->child_model), child_path, selection_data);
  gtk_tree_path_free (child_path);

  return gotten;
}

static gboolean
gtk_tree_model_filter_drag_data_delete (GtkTreeDragSource *drag_source,
                                        GtkTreePath       *path)
{
  GtkTreeModelFilter *tree_model_filter = (GtkTreeModelFilter *)drag_source;
  GtkTreePath *child_path;
  gboolean deleted;

  g_return_val_if_fail (GTK_IS_TREE_MODEL_FILTER (drag_source), FALSE);
  g_return_val_if_fail (path != NULL, FALSE);

  child_path = gtk_tree_model_filter_convert_path_to_child_path (tree_model_filter, path);
  deleted = gtk_tree_drag_source_drag_data_delete (GTK_TREE_DRAG_SOURCE (tree_model_filter->priv->child_model), child_path);
  gtk_tree_path_free (child_path);

  return deleted;
}

/* bits and pieces */
static void
gtk_tree_model_filter_set_model (GtkTreeModelFilter *filter,
                                 GtkTreeModel       *child_model)
{
  g_return_if_fail (GTK_IS_TREE_MODEL_FILTER (filter));

  if (filter->priv->child_model)
    {
      g_signal_handler_disconnect (filter->priv->child_model,
                                   filter->priv->changed_id);
      g_signal_handler_disconnect (filter->priv->child_model,
                                   filter->priv->inserted_id);
      g_signal_handler_disconnect (filter->priv->child_model,
                                   filter->priv->has_child_toggled_id);
      g_signal_handler_disconnect (filter->priv->child_model,
                                   filter->priv->deleted_id);
      g_signal_handler_disconnect (filter->priv->child_model,
                                   filter->priv->reordered_id);

      /* reset our state */
      if (filter->priv->root)
        gtk_tree_model_filter_free_level (filter, filter->priv->root);

      filter->priv->root = NULL;
      g_object_unref (filter->priv->child_model);
      filter->priv->visible_column = -1;
      /* FIXME: destroy more crack here? the funcs? */
    }

  filter->priv->child_model = child_model;

  if (child_model)
    {
      g_object_ref (filter->priv->child_model);
      filter->priv->changed_id =
        g_signal_connect (child_model, "row_changed",
                          G_CALLBACK (gtk_tree_model_filter_row_changed),
                          filter);
      filter->priv->inserted_id =
        g_signal_connect (child_model, "row_inserted",
                          G_CALLBACK (gtk_tree_model_filter_row_inserted),
                          filter);
      filter->priv->has_child_toggled_id =
        g_signal_connect (child_model, "row_has_child_toggled",
                          G_CALLBACK (gtk_tree_model_filter_row_has_child_toggled),
                          filter);
      filter->priv->deleted_id =
        g_signal_connect (child_model, "row_deleted",
                          G_CALLBACK (gtk_tree_model_filter_row_deleted),
                          filter);
      filter->priv->reordered_id =
        g_signal_connect (child_model, "rows_reordered",
                          G_CALLBACK (gtk_tree_model_filter_rows_reordered),
                          filter);

      filter->priv->child_flags = gtk_tree_model_get_flags (child_model);
      filter->priv->stamp = g_random_int ();
    }
}

static void
gtk_tree_model_filter_set_root (GtkTreeModelFilter *filter,
                                GtkTreePath        *root)
{
  g_return_if_fail (GTK_IS_TREE_MODEL_FILTER (filter));

  if (!root)
    filter->priv->virtual_root = NULL;
  else
    filter->priv->virtual_root = gtk_tree_path_copy (root);
}

/* public API */

/**
 * gtk_tree_model_filter_new:
 * @child_model: A #GtkTreeModel.
 * @root: A #GtkTreePath or %NULL.
 *
 * Creates a new #GtkTreeModel, with @child_model as the child_model
 * and @root as the virtual root.
 *
 * Return value: A new #GtkTreeModel.
 *
 * Since: 2.4
 */
GtkTreeModel *
gtk_tree_model_filter_new (GtkTreeModel *child_model,
                           GtkTreePath  *root)
{
  GtkTreeModel *retval;

  g_return_val_if_fail (GTK_IS_TREE_MODEL (child_model), NULL);

  retval = g_object_new (GTK_TYPE_TREE_MODEL_FILTER, 
			 "child_model", child_model,
			 "virtual_root", root,
			 NULL);

  return retval;
}

/**
 * gtk_tree_model_filter_get_model:
 * @filter: A #GtkTreeModelFilter.
 *
 * Returns a pointer to the child model of @filter.
 *
 * Return value: A pointer to a #GtkTreeModel.
 *
 * Since: 2.4
 */
GtkTreeModel *
gtk_tree_model_filter_get_model (GtkTreeModelFilter *filter)
{
  g_return_val_if_fail (GTK_IS_TREE_MODEL_FILTER (filter), NULL);

  return filter->priv->child_model;
}

/**
 * gtk_tree_model_filter_set_visible_func:
 * @filter: A #GtkTreeModelFilter.
 * @func: A #GtkTreeModelFilterVisibleFunc, the visible function.
 * @data: User data to pass to the visible function, or %NULL.
 * @destroy: Destroy notifier of @data, or %NULL.
 *
 * Sets the visible function used when filtering the @filter to be @func. The
 * function should return %TRUE if the given row should be visible and
 * %FALSE otherwise.
 *
 * Since: 2.4
 */
void
gtk_tree_model_filter_set_visible_func (GtkTreeModelFilter            *filter,
                                        GtkTreeModelFilterVisibleFunc  func,
                                        gpointer                       data,
                                        GtkDestroyNotify               destroy)
{
  g_return_if_fail (GTK_IS_TREE_MODEL_FILTER (filter));
  g_return_if_fail (func != NULL);
  g_return_if_fail (filter->priv->visible_method_set == FALSE);

  if (filter->priv->visible_func)
    {
      GtkDestroyNotify d = filter->priv->visible_destroy;

      filter->priv->visible_destroy = NULL;
      d (filter->priv->visible_data);
    }

  filter->priv->visible_func = func;
  filter->priv->visible_data = data;
  filter->priv->visible_destroy = destroy;

  filter->priv->visible_method_set = TRUE;
}

/**
 * gtk_tree_model_filter_set_modify_func:
 * @filter: A #GtkTreeModelFilter.
 * @n_columns: The number of columns in the filter model.
 * @types: The #GType<!-- -->s of the columns.
 * @func: A #GtkTreeModelFilterModifyFunc
 * @data: User data to pass to the modify function, or %NULL.
 * @destroy: Destroy notifier of @data, or %NULL.
 *
 * With the @n_columns and @types parameters, you give an array of column
 * types for this model (which will be exposed to the parent model/view).
 * The @func, @data and @destroy parameters are for specifying the modify
 * function. The modify function will get called for <emphasis>each</emphasis>
 * data access, the goal of the modify function is to return the data which 
 * should be displayed at the location specified using the parameters of the 
 * modify function.
 *
 * Since: 2.4
 */
void
gtk_tree_model_filter_set_modify_func (GtkTreeModelFilter           *filter,
                                       gint                          n_columns,
                                       GType                        *types,
                                       GtkTreeModelFilterModifyFunc  func,
                                       gpointer                      data,
                                       GtkDestroyNotify              destroy)
{
  g_return_if_fail (GTK_IS_TREE_MODEL_FILTER (filter));
  g_return_if_fail (func != NULL);
  g_return_if_fail (filter->priv->modify_func_set == FALSE);

  if (filter->priv->modify_destroy)
    {
      GtkDestroyNotify d = filter->priv->modify_destroy;

      filter->priv->modify_destroy = NULL;
      d (filter->priv->modify_data);
    }

  filter->priv->modify_n_columns = n_columns;
  filter->priv->modify_types = g_new0 (GType, n_columns);
  memcpy (filter->priv->modify_types, types, sizeof (GType) * n_columns);
  filter->priv->modify_func = func;
  filter->priv->modify_data = data;
  filter->priv->modify_destroy = destroy;

  filter->priv->modify_func_set = TRUE;
}

/**
 * gtk_tree_model_filter_set_visible_column:
 * @filter: A #GtkTreeModelFilter.
 * @column: A #gint which is the column containing the visible information.
 *
 * Sets @column of the child_model to be the column where @filter should
 * look for visibility information. @columns should be a column of type
 * %G_TYPE_BOOLEAN, where %TRUE means that a row is visible, and %FALSE
 * if not.
 *
 * Since: 2.4
 */
void
gtk_tree_model_filter_set_visible_column (GtkTreeModelFilter *filter,
                                          gint column)
{
  g_return_if_fail (GTK_IS_TREE_MODEL_FILTER (filter));
  g_return_if_fail (column >= 0);
  g_return_if_fail (filter->priv->visible_method_set == FALSE);

  filter->priv->visible_column = column;

  filter->priv->visible_method_set = TRUE;
}

/* conversion */

/**
 * gtk_tree_model_filter_convert_child_iter_to_iter:
 * @filter: A #GtkTreeModelFilter.
 * @filter_iter: An uninitialized #GtkTreeIter.
 * @child_iter: A valid #GtkTreeIter pointing to a row on the child model.
 *
 * Sets @filter_iter to point to the row in @filter that corresponds to the
 * row pointed at by @child_iter.
 *
 * Since: 2.4
 */
void
gtk_tree_model_filter_convert_child_iter_to_iter (GtkTreeModelFilter *filter,
                                                  GtkTreeIter        *filter_iter,
                                                  GtkTreeIter        *child_iter)
{
  GtkTreePath *child_path, *path;

  g_return_if_fail (GTK_IS_TREE_MODEL_FILTER (filter));
  g_return_if_fail (filter->priv->child_model != NULL);
  g_return_if_fail (filter_iter != NULL);
  g_return_if_fail (child_iter != NULL);

  filter_iter->stamp = 0;

  child_path = gtk_tree_model_get_path (filter->priv->child_model, child_iter);
  g_return_if_fail (child_path != NULL);

  path = gtk_tree_model_filter_convert_child_path_to_path (filter,
                                                           child_path);
  gtk_tree_path_free (child_path);
  g_return_if_fail (path != NULL);

  gtk_tree_model_get_iter (GTK_TREE_MODEL (filter), filter_iter, path);
  gtk_tree_path_free (path);
}

/**
 * gtk_tree_model_filter_convert_iter_to_child_iter:
 * @filter: A #GtkTreeModelFilter.
 * @child_iter: An uninitialized #GtkTreeIter.
 * @filter_iter: A valid #GtkTreeIter pointing to a row on @filter.
 *
 * Sets @child_iter to point to the row pointed to by @filter_iter.
 *
 * Since: 2.4
 */
void
gtk_tree_model_filter_convert_iter_to_child_iter (GtkTreeModelFilter *filter,
                                                  GtkTreeIter        *child_iter,
                                                  GtkTreeIter        *filter_iter)
{
  g_return_if_fail (GTK_IS_TREE_MODEL_FILTER (filter));
  g_return_if_fail (filter->priv->child_model != NULL);
  g_return_if_fail (child_iter != NULL);
  g_return_if_fail (filter_iter != NULL);
  g_return_if_fail (filter_iter->stamp == filter->priv->stamp);

  if (GTK_TREE_MODEL_FILTER_CACHE_CHILD_ITERS (filter))
    {
      *child_iter = FILTER_ELT (filter_iter->user_data2)->iter;
    }
  else
    {
      GtkTreePath *path;

      path = gtk_tree_model_filter_elt_get_path (filter_iter->user_data,
                                                 filter_iter->user_data2,
                                                 filter->priv->virtual_root);
      gtk_tree_model_get_iter (filter->priv->child_model, child_iter, path);
      gtk_tree_path_free (path);
    }
}

static GtkTreePath *
gtk_real_tree_model_filter_convert_child_path_to_path (GtkTreeModelFilter *filter,
                                                       GtkTreePath        *child_path,
                                                       gboolean            build_levels,
                                                       gboolean            fetch_children)
{
  gint *child_indices;
  GtkTreePath *retval;
  GtkTreePath *real_path;
  FilterLevel *level;
  FilterElt *tmp;
  gint i;

  g_return_val_if_fail (GTK_IS_TREE_MODEL_FILTER (filter), NULL);
  g_return_val_if_fail (filter->priv->child_model != NULL, NULL);
  g_return_val_if_fail (child_path != NULL, NULL);

  if (!filter->priv->virtual_root)
    real_path = gtk_tree_path_copy (child_path);
  else
    real_path = gtk_tree_model_filter_remove_root (child_path,
                                                   filter->priv->virtual_root);

  if (!real_path)
    return NULL;

  retval = gtk_tree_path_new ();
  child_indices = gtk_tree_path_get_indices (real_path);

  if (filter->priv->root == NULL && build_levels)
    gtk_tree_model_filter_build_level (filter, NULL, NULL);
  level = FILTER_LEVEL (filter->priv->root);

  for (i = 0; i < gtk_tree_path_get_depth (real_path); i++)
    {
      gint j;
      gboolean found_child = FALSE;

      if (!level)
        {
          gtk_tree_path_free (real_path);
          gtk_tree_path_free (retval);
          return NULL;
        }

      tmp = bsearch_elt_with_offset (level->array, child_indices[i], &j);
      if (tmp)
        {
          gtk_tree_path_append_index (retval, j);
          if (!tmp->children && build_levels)
            gtk_tree_model_filter_build_level (filter, level, tmp);
          level = tmp->children;
          found_child = TRUE;
        }

      if (!found_child && fetch_children)
        {
          tmp = gtk_tree_model_filter_fetch_child (filter, level,
                                                   child_indices[i],
                                                   &j);

          /* didn't find the child, let's try to bring it back */
          if (!tmp || tmp->offset != child_indices[i])
            {
              /* not there */
              gtk_tree_path_free (real_path);
              gtk_tree_path_free (retval);
              return NULL;
            }

          gtk_tree_path_append_index (retval, j);
          if (!tmp->children && build_levels)
            gtk_tree_model_filter_build_level (filter, level, tmp);
          level = tmp->children;
          found_child = TRUE;
        }
      else if (!found_child && !fetch_children)
        {
          /* no path */
          gtk_tree_path_free (real_path);
          gtk_tree_path_free (retval);
          return NULL;
        }
    }

  gtk_tree_path_free (real_path);
  return retval;
}

/**
 * gtk_tree_model_filter_convert_child_path_to_path:
 * @filter: A #GtkTreeModelFilter.
 * @child_path: A #GtkTreePath to convert.
 *
 * Converts @child_path to a path relative to @filter. That is, @child_path
 * points to a path in the child model. The rerturned path will point to the
 * same row in the filtered model. If @child_path isn't a valid path on the
 * child model, then %NULL is returned.
 *
 * Return value: A newly allocated #GtkTreePath, or %NULL.
 *
 * Since: 2.4
 */
GtkTreePath *
gtk_tree_model_filter_convert_child_path_to_path (GtkTreeModelFilter *filter,
                                                  GtkTreePath        *child_path)
{
  /* this function does the sanity checks */
  return gtk_real_tree_model_filter_convert_child_path_to_path (filter,
                                                                child_path,
                                                                TRUE,
                                                                TRUE);
}

/**
 * gtk_tree_model_filter_convert_path_to_child_path:
 * @filter: A #GtkTreeModelFilter.
 * @filter_path: A #GtkTreePath to convert.
 *
 * Converts @filter_path to a path on the child model of @filter. That is,
 * @filter_path points to a location in @filter. The returned path will
 * point to the same location in the model not being filtered. If @filter_path
 * does not point to a location in the child model, %NULL is returned.
 *
 * Return value: A newly allocated #GtkTreePath, or %NULL.
 *
 * Since: 2.4
 */
GtkTreePath *
gtk_tree_model_filter_convert_path_to_child_path (GtkTreeModelFilter *filter,
                                                  GtkTreePath        *filter_path)
{
  gint *filter_indices;
  GtkTreePath *retval;
  FilterLevel *level;
  gint i;

  g_return_val_if_fail (GTK_IS_TREE_MODEL_FILTER (filter), NULL);
  g_return_val_if_fail (filter->priv->child_model != NULL, NULL);
  g_return_val_if_fail (filter_path != NULL, NULL);

  /* convert path */
  retval = gtk_tree_path_new ();
  filter_indices = gtk_tree_path_get_indices (filter_path);
  if (!filter->priv->root)
    gtk_tree_model_filter_build_level (filter, NULL, NULL);
  level = FILTER_LEVEL (filter->priv->root);

  for (i = 0; i < gtk_tree_path_get_depth (filter_path); i++)
    {
      gint count = filter_indices[i];

      if (!level || level->array->len <= filter_indices[i])
        {
          gtk_tree_path_free (retval);
          return NULL;
        }

      if (g_array_index (level->array, FilterElt, count).children == NULL)
        gtk_tree_model_filter_build_level (filter, level, &g_array_index (level->array, FilterElt, count));

      if (!level || level->array->len <= filter_indices[i])
        {
          gtk_tree_path_free (retval);
          return NULL;
        }

      gtk_tree_path_append_index (retval, g_array_index (level->array, FilterElt, count).offset);
      level = g_array_index (level->array, FilterElt, count).children;
    }

  /* apply vroot */

  if (filter->priv->virtual_root)
    {
      GtkTreePath *real_retval;

      real_retval = gtk_tree_model_filter_add_root (retval,
                                                    filter->priv->virtual_root);
      gtk_tree_path_free (retval);

      return real_retval;
    }

  return retval;
}

static gboolean
gtk_tree_model_filter_refilter_helper (GtkTreeModel *model,
                                       GtkTreePath  *path,
                                       GtkTreeIter  *iter,
                                       gpointer      data)
{
  /* evil, don't try this at home, but certainly speeds things up */
  gtk_tree_model_filter_row_changed (model, path, iter, data);

  return FALSE;
}

/**
 * gtk_tree_model_filter_refilter:
 * @filter: A #GtkTreeModelFilter.
 *
 * Emits ::row_changed for each row in the child model, which causes
 * the filter to re-evaluate whether a row is visible or not.
 *
 * Since: 2.4
 */
void
gtk_tree_model_filter_refilter (GtkTreeModelFilter *filter)
{
  g_return_if_fail (GTK_IS_TREE_MODEL_FILTER (filter));

  /* S L O W */
  gtk_tree_model_foreach (filter->priv->child_model,
                          gtk_tree_model_filter_refilter_helper,
                          filter);
}

/**
 * gtk_tree_model_filter_clear_cache:
 * @filter: A #GtkTreeModelFilter.
 *
 * This function should almost never be called. It clears the @filter
 * of any cached iterators that haven't been reffed with
 * gtk_tree_model_ref_node(). This might be useful if the child model
 * being filtered is static (and doesn't change often) and there has been
 * a lot of unreffed access to nodes. As a side effect of this function,
 * all unreffed itters will be invalid.
 *
 * Since: 2.4
 */
void
gtk_tree_model_filter_clear_cache (GtkTreeModelFilter *filter)
{
  g_return_if_fail (GTK_IS_TREE_MODEL_FILTER (filter));

  if (filter->priv->zero_ref_count)
    gtk_tree_model_filter_clear_cache_helper (filter,
                                              FILTER_LEVEL (filter->priv->root));
}
