/* GLIB - Library of useful routines for C programming
 * Copyright (C) 2002  Soeren Sandmann (sandmann@daimi.au.dk)
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <glib.h>

#include "gtksequence.h"
#include "gtkalias.h"

typedef struct _GtkSequenceNode GtkSequenceNode;

struct _GtkSequence {
  GtkSequenceNode *node;	/* does not necessarily point to the root.
				 * You can splay it if you want it to
				 */
  GDestroyNotify data_destroy_notify;
};

struct _GtkSequenceNode {
  guint is_end  : 1;
  gint  n_nodes : 31;		/* number of nodes below this node,
				 * including this node
				 */
  GtkSequenceNode *parent;
  GtkSequenceNode *left;
  GtkSequenceNode *right;
  
  GtkSequence *sequence;
  
  gpointer data;
};

static GtkSequenceNode *_gtk_sequence_node_new          (gpointer          data);
static GtkSequenceNode *_gtk_sequence_node_find_first   (GtkSequenceNode    *node);
static GtkSequenceNode *_gtk_sequence_node_find_last    (GtkSequenceNode    *node);
static GtkSequenceNode *_gtk_sequence_node_find_by_pos  (GtkSequenceNode    *node,
							 gint pos);
static GtkSequenceNode *_gtk_sequence_node_prev         (GtkSequenceNode    *node);
static GtkSequenceNode *_gtk_sequence_node_next         (GtkSequenceNode    *node);
static gint           _gtk_sequence_node_get_pos (GtkSequenceNode    *node);
static GtkSequence     *_gtk_sequence_node_get_sequence (GtkSequenceNode    *node);
static GtkSequenceNode *_gtk_sequence_node_find_closest (GtkSequenceNode    *node,
							 GtkSequenceNode    *other,
							 GCompareDataFunc  cmp,
							 gpointer          data);
static gint           _gtk_sequence_node_get_length   (GtkSequenceNode    *node);
static void           _gtk_sequence_node_free         (GtkSequenceNode    *node,
						       GDestroyNotify    destroy);
#if 0
static gboolean       _gtk_sequence_node_is_singleton (GtkSequenceNode    *node);
#endif
static void           _gtk_sequence_node_split        (GtkSequenceNode    *node,
						       GtkSequenceNode   **left,
						       GtkSequenceNode   **right);
static void           _gtk_sequence_node_insert_before (GtkSequenceNode *node,
							GtkSequenceNode *new);
static void           _gtk_sequence_node_remove        (GtkSequenceNode *node);
static void           _gtk_sequence_node_insert_sorted (GtkSequenceNode *node,
							GtkSequenceNode *new,
							GCompareDataFunc cmp_func,
							gpointer cmp_data);

/* GtkSequence */
GtkSequence *
_gtk_sequence_new                (GDestroyNotify           data_destroy)
{
  GtkSequence *seq = g_new (GtkSequence, 1);
  seq->data_destroy_notify = data_destroy;
  
  seq->node = _gtk_sequence_node_new (NULL);
  seq->node->is_end = TRUE;
  seq->node->sequence = seq;
  
  return seq;
}

void
_gtk_sequence_foreach	      (GtkSequence	 *seq,
			       GFunc		  func,
			       gpointer		  data)
{
  GtkSequencePtr ptr;
  
  g_return_if_fail (seq != NULL);
  g_return_if_fail (func != NULL);
  
  ptr = _gtk_sequence_get_begin_ptr (seq);
  
  while (!_gtk_sequence_ptr_is_end (ptr))
    {
      GtkSequenceNode *node = ptr;
      
      func (node->data, data);
      
      ptr = _gtk_sequence_ptr_next (ptr);
    }
  
}

void
_gtk_sequence_free               (GtkSequence               *seq)
{
  g_return_if_fail (seq != NULL);
  
  _gtk_sequence_node_free (seq->node, seq->data_destroy_notify);
  
  g_free (seq);
}

#if 0
static void
flatten_nodes (GtkSequenceNode *node, GList **list)
{
  g_print ("flatten %p\n", node);
  if (!node)
    return;
  else if (_gtk_sequence_node_is_singleton (node))
    *list = g_list_prepend (*list, node);
  else
    {
      GtkSequenceNode *left;
      GtkSequenceNode *right;
      
      _gtk_sequence_node_split (node, &left, &right);
      
      flatten_nodes (left, list);
      flatten_nodes (right, list);
    }
}
#endif

typedef struct SortInfo SortInfo;
struct SortInfo {
  GCompareDataFunc cmp;
  gpointer data;
};

static gint
node_compare (gconstpointer n1, gconstpointer n2, gpointer data)
{
  const SortInfo *info = data;
  const GtkSequenceNode *node1 = n1;
  const GtkSequenceNode *node2 = n2;
  gint retval;
  
  if (node1->is_end)
      return 1;

  if (node2->is_end)
      return -1;

  retval = (* info->cmp) (node1, node2, info->data);

  /* If the nodes are different, but the user-supplied compare function
   * compares them equal, then force an arbitrary (but consistent) order
   * on them, so that our sorts will be stable
   */
  if (retval != 0 || n1 == n2)
    return retval;
  
  if (n1 > n2)
    return 1;
  else
    return -1;
}

void
_gtk_sequence_append             (GtkSequence               *seq,
				  gpointer                 data)
{
  GtkSequenceNode *node, *last;
  
  g_return_if_fail (seq != NULL);
  
  node = _gtk_sequence_node_new (data);
  node->sequence = seq;
  last = _gtk_sequence_node_find_last (seq->node);
  _gtk_sequence_node_insert_before (last, node);
}
#if 0

void
_gtk_sequence_prepend            (GtkSequence               *seq,
				  gpointer                 data)
{
  GtkSequenceNode *node, *second;
  
  g_return_if_fail (seq != NULL);
  
  node = _gtk_sequence_node_new (data);
  node->sequence = seq;
  second = _gtk_sequence_node_next (_gtk_sequence_node_find_first (seq->node));
  
  _gtk_sequence_node_insert_before (second, node);
}
#endif

GtkSequencePtr 
_gtk_sequence_insert             (GtkSequencePtr             ptr,
				  gpointer                 data)
{
  GtkSequenceNode *node;
  
  g_return_val_if_fail (ptr != NULL, NULL);
  
  node = _gtk_sequence_node_new (data);
  node->sequence = ptr->sequence;

  _gtk_sequence_node_insert_before (ptr, node);

  return node;
}

static void
_gtk_sequence_unlink (GtkSequence *seq,
		      GtkSequenceNode *node)
{
  g_assert (!node->is_end);
  
  seq->node = _gtk_sequence_node_next (node);
  
  g_assert (seq->node);
  g_assert (seq->node != node);
  
  _gtk_sequence_node_remove (node);
}

void
_gtk_sequence_remove             (GtkSequencePtr             ptr)
{
  GtkSequence *seq;
  
  g_return_if_fail (ptr != NULL);
  g_return_if_fail (!ptr->is_end);
  
  seq = _gtk_sequence_node_get_sequence (ptr); 
  _gtk_sequence_unlink (seq, ptr);
  _gtk_sequence_node_free (ptr, seq->data_destroy_notify);
}

void
_gtk_sequence_sort               (GtkSequence               *seq,
				  GCompareDataFunc         cmp_func,
				  gpointer                 cmp_data)
{
  GtkSequence *tmp;
  GtkSequenceNode *begin, *end;
  
  g_return_if_fail (seq != NULL);
  g_return_if_fail (cmp_func != NULL);
  
  begin = _gtk_sequence_get_begin_ptr (seq);
  end   = _gtk_sequence_get_end_ptr (seq);
  
  _gtk_sequence_remove_range (begin, end, &tmp);
  
  while (_gtk_sequence_get_length (tmp) > 0)
    {
      GtkSequenceNode *node = _gtk_sequence_get_begin_ptr (tmp);
      _gtk_sequence_unlink (tmp, node);
      
      _gtk_sequence_node_insert_sorted (seq->node, node, cmp_func, cmp_data);
    }
  
  _gtk_sequence_free (tmp);
}

gpointer
_gtk_sequence_ptr_get_data           (GtkSequencePtr             ptr)
{
  g_return_val_if_fail (ptr != NULL, NULL);
  g_return_val_if_fail (!ptr->is_end, NULL);
  
  return ptr->data;
}

GtkSequencePtr
_gtk_sequence_insert_sorted      (GtkSequence               *seq,
				  gpointer                 data,
				  GCompareDataFunc         cmp_func,
				  gpointer                 cmp_data)
{
  GtkSequenceNode *new_node = _gtk_sequence_node_new (data);
  new_node->sequence = seq;
  _gtk_sequence_node_insert_sorted (seq->node, new_node, cmp_func, cmp_data);
  return new_node;
}

void
_gtk_sequence_insert_sequence    (GtkSequencePtr             ptr,
				  GtkSequence               *other_seq)
{
  GtkSequenceNode *last;
  
  g_return_if_fail (other_seq != NULL);
  g_return_if_fail (ptr != NULL);
  
  last = _gtk_sequence_node_find_last (other_seq->node);
  _gtk_sequence_node_insert_before (ptr, last);
  _gtk_sequence_node_remove (last);
  _gtk_sequence_node_free (last, NULL);
  other_seq->node = NULL;
  _gtk_sequence_free (other_seq);
}

void
_gtk_sequence_concatenate        (GtkSequence               *seq1,
				  GtkSequence               *seq2)
{
  GtkSequenceNode *last;
  
  g_return_if_fail (seq1 != NULL);
  g_return_if_fail (seq2 != NULL);
  
  last = _gtk_sequence_node_find_last (seq1->node);
  _gtk_sequence_insert_sequence (last, seq2);
}

/*
 * The new sequence inherits the destroy notify from the sequence that
 * beign and end comes from
 */
void
_gtk_sequence_remove_range       (GtkSequencePtr             begin,
				  GtkSequencePtr             end,
				  GtkSequence              **removed)
{
  GtkSequence *seq;
  GtkSequenceNode *s1, *s2, *s3;
  
  seq = _gtk_sequence_node_get_sequence (begin);
  
  g_assert (end != NULL);
  
  g_return_if_fail (seq == _gtk_sequence_node_get_sequence (end));
  
  _gtk_sequence_node_split (begin, &s1, &s2);
  _gtk_sequence_node_split (end, NULL, &s3);
  
  if (s1)
    _gtk_sequence_node_insert_before (s3, s1);
  
  seq->node = s3;
  
  if (removed)
    {
      *removed = _gtk_sequence_new (seq->data_destroy_notify);
      _gtk_sequence_node_insert_before ((*removed)->node, s2);
    }
  else
    {
      _gtk_sequence_node_free (s2, seq->data_destroy_notify);
    }
}

gint
_gtk_sequence_get_length         (GtkSequence               *seq)
{
  return _gtk_sequence_node_get_length (seq->node) - 1;
}

GtkSequencePtr
_gtk_sequence_get_end_ptr        (GtkSequence               *seq)
{
  g_return_val_if_fail (seq != NULL, NULL);
  return _gtk_sequence_node_find_last (seq->node);
}

GtkSequencePtr
_gtk_sequence_get_begin_ptr      (GtkSequence               *seq)
{
  g_return_val_if_fail (seq != NULL, NULL);
  return _gtk_sequence_node_find_first (seq->node);
}

/*
 * if pos > number of items or -1, will return end pointer
 */
GtkSequencePtr
_gtk_sequence_get_ptr_at_pos     (GtkSequence               *seq,
				  gint                     pos)
{
  gint len;
  
  g_return_val_if_fail (seq != NULL, NULL);
  
  len = _gtk_sequence_get_length (seq);
  
  if (pos > len || pos == -1)
    pos = len;
  
  return _gtk_sequence_node_find_by_pos (seq->node, pos);
}


/* GtkSequencePtr */
gboolean
_gtk_sequence_ptr_is_end         (GtkSequencePtr             ptr)
{
  g_return_val_if_fail (ptr != NULL, FALSE);
  return ptr->is_end;
}

gboolean
_gtk_sequence_ptr_is_begin       (GtkSequencePtr             ptr)
{
  return (_gtk_sequence_node_prev (ptr) == ptr);
}

/* If you call this on an end pointer you'll get
 * the length of the sequence
 */
gint
_gtk_sequence_ptr_get_position   (GtkSequencePtr             ptr)
{
  g_return_val_if_fail (ptr != NULL, -1);
  
  return _gtk_sequence_node_get_pos (ptr);
}

GtkSequencePtr
_gtk_sequence_ptr_next           (GtkSequencePtr             ptr)
{
  g_return_val_if_fail (ptr != NULL, NULL);
  
  return _gtk_sequence_node_next (ptr);
}

GtkSequencePtr
_gtk_sequence_ptr_prev           (GtkSequencePtr             ptr)
{
  g_return_val_if_fail (ptr != NULL, NULL);
  
  return _gtk_sequence_node_prev (ptr);
}

GtkSequencePtr
_gtk_sequence_ptr_move           (GtkSequencePtr             ptr,
				  guint                    delta)
{
  gint new_pos;
  
  g_return_val_if_fail (ptr != NULL, NULL);
  
  new_pos = _gtk_sequence_node_get_pos (ptr) + delta;
  
  return _gtk_sequence_node_find_by_pos (ptr, new_pos);
}

void
_gtk_sequence_sort_changed  (GtkSequencePtr	     ptr,
			     GCompareDataFunc	     cmp_func,
			     gpointer		     cmp_data)
  
{
  GtkSequence *seq;

  g_return_if_fail (ptr != NULL);
  g_return_if_fail (!ptr->is_end);

  seq = _gtk_sequence_node_get_sequence (ptr);
  _gtk_sequence_unlink (seq, ptr);
  _gtk_sequence_node_insert_sorted (seq->node, ptr, cmp_func, cmp_data);
}

/* search
 *
 * The only restriction on the search function is that it
 * must not delete any nodes. It is permitted to insert new nodes,
 * but the caller should "know what he is doing"
 */
void
_gtk_sequence_search             (GtkSequence               *seq,
				  GtkSequenceSearchFunc      f,
				  gpointer                 data)
{
  GQueue *intervals = g_queue_new ();
  
  g_queue_push_tail (intervals, _gtk_sequence_node_find_first (seq->node));
  g_queue_push_tail (intervals, _gtk_sequence_node_find_last (seq->node));
  
  while (!g_queue_is_empty (intervals))
    {
      GtkSequenceNode *begin = g_queue_pop_head (intervals);
      GtkSequenceNode *end   = g_queue_pop_head (intervals);
      
      if (f (begin, end, data))
	{
	  gint begin_pos = _gtk_sequence_node_get_pos (begin);
	  gint end_pos   = _gtk_sequence_node_get_pos (end);
	  
	  if (end_pos - begin_pos > 1)
	    {
	      GtkSequenceNode *mid;
	      gint mid_pos;
	      
	      mid_pos = begin_pos + (end_pos - begin_pos) / 2;
	      mid = _gtk_sequence_node_find_by_pos (begin, mid_pos);
	      
	      g_queue_push_tail (intervals, begin);
	      g_queue_push_tail (intervals, mid);
	      
	      g_queue_push_tail (intervals, mid);
	      g_queue_push_tail (intervals, end);
	    }
	}
    }
  
  g_queue_free (intervals);
}

#if 0
/* aggregates */
void
_gtk_sequence_add_aggregate      (GtkSequence               *seq,
				  const gchar             *aggregate,
				  GtkSequenceAggregateFunc   f,
				  gpointer                 data,
				  GDestroyNotify           destroy)
{
  /* FIXME */
}

void
_gtk_sequence_remove_aggregate   (GtkSequence               *seq,
				  const gchar              aggregate)
{
  /* FIXME */
  
}

void
_gtk_sequence_set_aggregate_data (GtkSequencePtr             ptr,
				  const gchar             *aggregate,
				  gpointer                 data)
{
  /* FIXME */
  
}

gpointer
_gtk_sequence_get_aggregate_data (GtkSequencePtr             begin,
				  GtkSequencePtr             end,
				  const gchar             *aggregate)
{
  g_assert_not_reached();
  return NULL;
}
#endif


/* Nodes
 */
static void
_gtk_sequence_node_update_fields (GtkSequenceNode *node)
{
  g_assert (node != NULL);
  
  node->n_nodes = 1;
  
  if (node->left)
    node->n_nodes += node->left->n_nodes;
  
  if (node->right)
    node->n_nodes += node->right->n_nodes;
  
#if 0
  if (node->left || node->right)
    g_assert (node->n_nodes > 1);
#endif
}

#define NODE_LEFT_CHILD(n)  (((n)->parent) && ((n)->parent->left) == (n))
#define NODE_RIGHT_CHILD(n) (((n)->parent) && ((n)->parent->right) == (n))

static void
_gtk_sequence_node_rotate (GtkSequenceNode *node)
{
  GtkSequenceNode *tmp, *old;
  
  g_assert (node->parent);
  g_assert (node->parent != node);
  
  if (NODE_LEFT_CHILD (node))
    {
      /* rotate right */
      tmp = node->right;
      
      node->right = node->parent;
      node->parent = node->parent->parent;
      if (node->parent)
	{
	  if (node->parent->left == node->right)
	    node->parent->left = node;
	  else
	    node->parent->right = node;
	}
      
      g_assert (node->right);
      
      node->right->parent = node;
      node->right->left = tmp;
      
      if (node->right->left)
	node->right->left->parent = node->right;
      
      old = node->right;
    }
  else
    {
      /* rotate left */
      tmp = node->left;
      
      node->left = node->parent;
      node->parent = node->parent->parent;
      if (node->parent)
	{
	  if (node->parent->right == node->left)
	    node->parent->right = node;
	  else
	    node->parent->left = node;
	}
      
      g_assert (node->left);
      
      node->left->parent = node;
      node->left->right = tmp;
      
      if (node->left->right)
	node->left->right->parent = node->left;
      
      old = node->left;
    }
  
  _gtk_sequence_node_update_fields (old);
  _gtk_sequence_node_update_fields (node);
}

static GtkSequenceNode *
splay (GtkSequenceNode *node)
{
  while (node->parent)
    {
      if (!node->parent->parent)
	{
	  /* zig */
	  _gtk_sequence_node_rotate (node);
	}
      else if ((NODE_LEFT_CHILD (node) && NODE_LEFT_CHILD (node->parent)) ||
	       (NODE_RIGHT_CHILD (node) && NODE_RIGHT_CHILD (node->parent)))
	{
	  /* zig-zig */
	  _gtk_sequence_node_rotate (node->parent);
	  _gtk_sequence_node_rotate (node);
	}
      else
	{
	  /* zig-zag */
	  _gtk_sequence_node_rotate (node);
	  _gtk_sequence_node_rotate (node);
	}
    }
  
  return node;
}

static GtkSequenceNode *
_gtk_sequence_node_new (gpointer          data)
{
  GtkSequenceNode *node = g_new0 (GtkSequenceNode, 1);
  
  node->parent = NULL;
  node->left = NULL;
  node->right = NULL;
  
  node->data = data;
  node->is_end = FALSE;
  node->n_nodes = 1;
  
  return node;
}

static GtkSequenceNode *
find_min (GtkSequenceNode *node)
{
  splay (node);
  
  while (node->left)
    node = node->left;
  
  return node;
}

static GtkSequenceNode *
find_max (GtkSequenceNode *node)
{
  splay (node);
  
  while (node->right)
    node = node->right;
  
  return node;
}

static GtkSequenceNode *
_gtk_sequence_node_find_first   (GtkSequenceNode    *node)
{
  return splay (find_min (node));
}

static GtkSequenceNode *
_gtk_sequence_node_find_last    (GtkSequenceNode    *node)
{
  return splay (find_max (node));
}

static gint
get_n_nodes (GtkSequenceNode *node)
{
  if (node)
    return node->n_nodes;
  else
    return 0;
}

static GtkSequenceNode *
_gtk_sequence_node_find_by_pos  (GtkSequenceNode    *node,
				 gint              pos)
{
  gint i;
  
  g_assert (node != NULL);
  
  splay (node);
  
  while ((i = get_n_nodes (node->left)) != pos)
    {
      if (i < pos)
	{
	  node = node->right;
	  pos -= (i + 1);
	}
      else
	{
	  node = node->left;
	  g_assert (node->parent != NULL);
	}
    }
  
  return splay (node);
}

static GtkSequenceNode *
_gtk_sequence_node_prev         (GtkSequenceNode    *node)
{
  splay (node);
  
  if (node->left)
    {
      node = node->left;
      while (node->right)
	node = node->right;
    }
  
  return splay (node);
}

static GtkSequenceNode *
_gtk_sequence_node_next         (GtkSequenceNode    *node)
{
  splay (node);
  
  if (node->right)
    {
      node = node->right;
      while (node->left)
	node = node->left;
    }
  
  return splay (node);
}

static gint
_gtk_sequence_node_get_pos (GtkSequenceNode    *node)
{
  splay (node);
  
  return get_n_nodes (node->left);
}

static GtkSequence *
_gtk_sequence_node_get_sequence (GtkSequenceNode    *node)
{
  splay (node);
  
  return node->sequence;
}

static GtkSequenceNode *
_gtk_sequence_node_find_closest (GtkSequenceNode    *node,
				 GtkSequenceNode    *other,
				 GCompareDataFunc  cmp,
				 gpointer          data)
{
  GtkSequenceNode *best;
  gint c;
  
  splay (node);

  do
    {
      best = node;

      if ((c = cmp (node, other, data)) != 0)
	{
	  if (c < 0)
	    node = node->right;
	  else
	    node = node->left;
	}
    }
  while (c != 0 && node != NULL);
  
  return best;
}

static void
_gtk_sequence_node_free         (GtkSequenceNode    *node,
				 GDestroyNotify    destroy)
{
  /* FIXME:
   *
   * This is to avoid excessively deep recursions. A splay tree is not necessarily
   * balanced at all.
   *
   * I _think_ this is still linear in the number of nodes, but I'd like to
   * do something more efficient.
   */
  
  while (node)
    {
      GtkSequenceNode *next;
      
      node = splay (find_min (node));
      next = node->right;
      if (next)
	next->parent = NULL;
      
      if (destroy && !node->is_end)
	destroy (node->data);
      g_free (node);
      
      node = next;
    }
}

#if 0
static gboolean
_gtk_sequence_node_is_singleton (GtkSequenceNode    *node)
{
  splay (node);
  
  if (node->left || node->right)
    return FALSE;
  
  return TRUE;
}
#endif

static void
_gtk_sequence_node_split        (GtkSequenceNode    *node,
				 GtkSequenceNode   **left,
				 GtkSequenceNode   **right)
{
  GtkSequenceNode *left_tree;
  
  splay (node);
  
  left_tree = node->left;
  if (left_tree)
    {
      left_tree->parent = NULL;
      _gtk_sequence_node_update_fields (left_tree);
    }
  
  node->left = NULL;
  _gtk_sequence_node_update_fields (node);
  
  if (left)
    *left = left_tree;
  
  if (right)
    *right = node;
}

static void
_gtk_sequence_node_insert_before (GtkSequenceNode *node,
				  GtkSequenceNode *new)
{
  g_assert (node != NULL);
  g_assert (new != NULL);
  
  splay (node);
  
  new = splay (find_min (new));
  g_assert (new->left == NULL);
  
  if (node->left)
    node->left->parent = new;
  
  new->left = node->left;
  new->parent = node;
  
  node->left = new;
  
  _gtk_sequence_node_update_fields (new);
  _gtk_sequence_node_update_fields (node);
}

static gint
_gtk_sequence_node_get_length (GtkSequenceNode    *node)
{
  g_assert (node != NULL);
  
  splay (node);
  return node->n_nodes;
}

static void
_gtk_sequence_node_remove        (GtkSequenceNode *node)
{
  GtkSequenceNode *right, *left;
  
  splay (node);
  
  left = node->left;
  right = node->right;
  
  node->left = node->right = NULL;
  
  if (right)
    {
      right->parent = NULL;
      
      right = _gtk_sequence_node_find_first (right);
      g_assert (right->left == NULL);
      
      right->left = left;
      if (left)
	{
	  left->parent = right;
	  _gtk_sequence_node_update_fields (right);
	}
    }
  else if (left)
    left->parent = NULL;
}

#if 0
/* debug func */
static gint
_gtk_sequence_node_calc_height (GtkSequenceNode *node)
{
  /* breadth first traversal */
  gint height = 0;
  GQueue *nodes = g_queue_new ();
  
  g_queue_push_tail (nodes, node);
  
  while (!g_queue_is_empty (nodes))
    {
      GQueue *tmp = g_queue_new ();
      
      height++;
      while (!g_queue_is_empty (nodes))
	{
	  GtkSequenceNode *node = g_queue_pop_head (nodes);
	  if (node->left)
	    g_queue_push_tail (tmp, node->left);
	  if (node->right)
	    g_queue_push_tail (tmp, node->right);
	}
      
      g_queue_free (nodes);
      
      nodes = tmp;
    }
  g_queue_free (nodes);
  
  return height;
}
#endif

static void
_gtk_sequence_node_insert_sorted (GtkSequenceNode *node,
				  GtkSequenceNode *new,
				  GCompareDataFunc cmp_func,
				  gpointer cmp_data)
{
  SortInfo info;
  GtkSequenceNode *closest;
  info.cmp = cmp_func;
  info.data = cmp_data;
  
  closest =
    _gtk_sequence_node_find_closest (node, new, node_compare, &info);

  g_assert (closest != new);
  
  if (node_compare (new, closest, &info) > 0)
    closest = _gtk_sequence_node_next (closest);

  _gtk_sequence_node_insert_before (closest, new);
}

static gint
_gtk_sequence_node_calc_height (GtkSequenceNode *node)
{
  gint left_height;
  gint right_height;
  
  if (node)
    {
      left_height = 0;
      right_height = 0;
      
      if (node->left)
	left_height = _gtk_sequence_node_calc_height (node->left);
      
      if (node->right)
	right_height = _gtk_sequence_node_calc_height (node->right);
      
      return MAX (left_height, right_height) + 1;
    }
  
  return 0;
}

gint
_gtk_sequence_calc_tree_height   (GtkSequence               *seq)
{
  GtkSequenceNode *node = seq->node;
  gint r, l;
  while (node->parent)
    node = node->parent;
  
  if (node)
    {
      r = _gtk_sequence_node_calc_height (node->right);
      l = _gtk_sequence_node_calc_height (node->left);
      
      return MAX (r, l) + 1;
    }
  else
    return 0;
}

GtkSequence   *
_gtk_sequence_ptr_get_sequence (GtkSequencePtr	  ptr)
{
  GtkSequenceNode *node = ptr;
  
  return node->sequence;
}

void
_gtk_sequence_swap (GtkSequencePtr a,
		    GtkSequencePtr b)
{
  GtkSequenceNode *leftmost, *rightmost, *rightmost_next;
  int a_pos, b_pos;
  
  g_return_if_fail (!_gtk_sequence_ptr_is_end (a));
  g_return_if_fail (!_gtk_sequence_ptr_is_end (b));

  if (a == b)
      return;

  a_pos = _gtk_sequence_ptr_get_position (a);
  b_pos = _gtk_sequence_ptr_get_position (b);

  if (a_pos > b_pos)
    {
      leftmost = b;
      rightmost = a;
    }
  else
    {
      leftmost = a;
      rightmost = b;
    }

  rightmost_next = _gtk_sequence_node_next (rightmost);

  /* Situation now:  ..., leftmost, ......., rightmost, rightmost_next, ... */
  
  _gtk_sequence_move (rightmost, leftmost);
  _gtk_sequence_move (leftmost, rightmost_next);
}

void
_gtk_sequence_move (GtkSequencePtr ptr,
		    GtkSequencePtr new_pos)
{
  g_return_if_fail (ptr != NULL);
  g_return_if_fail (new_pos != NULL);
  
  if (ptr == new_pos)
    return;
  
  _gtk_sequence_unlink (ptr->sequence, ptr);
  _gtk_sequence_node_insert_before (new_pos, ptr);
}

/* Overwrites the existing pointer. */
void
_gtk_sequence_set	      (GtkSequencePtr	  ptr,
			       gpointer		  data)
{
  GtkSequence *seq;

  g_return_if_fail (!_gtk_sequence_ptr_is_end (ptr));
  
  seq = _gtk_sequence_node_get_sequence (ptr);
  if (seq->data_destroy_notify)
    seq->data_destroy_notify (ptr->data);
  ptr->data = data;
}
