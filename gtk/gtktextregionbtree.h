/* gtktextregionbtree.h
 *
 * Copyright 2021 Christian Hergert <chergert@redhat.com>
 *
 * This file is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2.1 of the License, or (at your option)
 * any later version.
 *
 * This file is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef __GTK_TEXT_REGION_BTREE_H__
#define __GTK_TEXT_REGION_BTREE_H__

#include "gtktextregionprivate.h"

G_BEGIN_DECLS

/* The following set of macros are used to create a queue similar to a
 * double-ended linked list but using integers as indexes for items within the
 * queue. Doing so allows for inserting or removing items from a b+tree node
 * without having to memmove() data to maintain sorting orders.
 */
#define VAL_QUEUE_INVALID(Node) ((glib_typeof((Node)->head))-1)
#define VAL_QUEUE_LENGTH(Node) ((Node)->length)
#define VAL_QUEUE_EMPTY(Node) ((Node)->head == VAL_QUEUE_INVALID(Node))
#define VAL_QUEUE_PEEK_HEAD(Node) ((Node)->head)
#define VAL_QUEUE_PEEK_TAIL(Node) ((Node)->tail)
#define VAL_QUEUE_IS_VALID(Node, ID) ((ID) != VAL_QUEUE_INVALID(Node))
#define VAL_QUEUE_NODE(Type, N_Items)                                        \
  struct {                                                                   \
    Type length;                                                             \
    Type head;                                                               \
    Type tail;                                                               \
    struct {                                                                 \
      Type prev;                                                             \
      Type next;                                                             \
    } items[N_Items];                                                        \
  }
#define VAL_QUEUE_INIT(Node)                                                 \
  G_STMT_START {                                                             \
    (Node)->length = 0;                                                      \
    (Node)->head = VAL_QUEUE_INVALID(Node);                                  \
    (Node)->tail = VAL_QUEUE_INVALID(Node);                                  \
    for (guint _i = 0; _i < G_N_ELEMENTS ((Node)->items); _i++)              \
      {                                                                      \
        (Node)->items[_i].next = VAL_QUEUE_INVALID(Node);                    \
        (Node)->items[_i].prev = VAL_QUEUE_INVALID(Node);                    \
      }                                                                      \
  } G_STMT_END
#ifndef G_DISABLE_ASSERT
# define _VAL_QUEUE_VALIDATE(Node)                                           \
  G_STMT_START {                                                             \
    glib_typeof((Node)->head) count = 0;                                     \
                                                                             \
    if ((Node)->tail != VAL_QUEUE_INVALID(Node))                             \
      g_assert_cmpint((Node)->items[(Node)->tail].next, ==, VAL_QUEUE_INVALID(Node)); \
    if ((Node)->head != VAL_QUEUE_INVALID(Node))                             \
      g_assert_cmpint((Node)->items[(Node)->head].prev , ==, VAL_QUEUE_INVALID(Node)); \
                                                                             \
    for (glib_typeof((Node)->head) _viter = (Node)->head;                    \
         VAL_QUEUE_IS_VALID(Node, _viter);                                   \
         _viter = (Node)->items[_viter].next)                                \
    {                                                                        \
      count++;                                                               \
    }                                                                        \
                                                                             \
    g_assert_cmpint(count, ==, (Node)->length);                              \
  } G_STMT_END
#else
# define _VAL_QUEUE_VALIDATE(Node) G_STMT_START { } G_STMT_END
#endif
#define VAL_QUEUE_PUSH_HEAD(Node, ID)                                        \
  G_STMT_START {                                                             \
    (Node)->items[ID].prev = VAL_QUEUE_INVALID(Node);                        \
    (Node)->items[ID].next = (Node)->head;                                   \
    if (VAL_QUEUE_IS_VALID(Node, (Node)->head))                              \
      (Node)->items[(Node)->head].prev = ID;                                 \
    (Node)->head = ID;                                                       \
    if (!VAL_QUEUE_IS_VALID(Node, (Node)->tail))                             \
      (Node)->tail = ID;                                                     \
    (Node)->length++;                                                        \
    _VAL_QUEUE_VALIDATE(Node);                                               \
  } G_STMT_END
#define VAL_QUEUE_PUSH_TAIL(Node, ID)                                        \
  G_STMT_START {                                                             \
    (Node)->items[ID].prev = (Node)->tail;                                   \
    (Node)->items[ID].next = VAL_QUEUE_INVALID(Node);                        \
    if (VAL_QUEUE_IS_VALID (Node, (Node)->tail))                             \
      (Node)->items[(Node)->tail].next = ID;                                 \
    (Node)->tail = ID;                                                       \
    if (!VAL_QUEUE_IS_VALID(Node, (Node)->head))                             \
      (Node)->head = ID;                                                     \
    (Node)->length++;                                                        \
    _VAL_QUEUE_VALIDATE(Node);                                               \
  } G_STMT_END
#define VAL_QUEUE_INSERT(Node, Nth, Val)                                     \
  G_STMT_START {                                                             \
    g_assert_cmpint (VAL_QUEUE_LENGTH(Node),<,G_N_ELEMENTS((Node)->items));  \
                                                                             \
    if ((Nth) == 0)                                                          \
      {                                                                      \
        VAL_QUEUE_PUSH_HEAD(Node, Val);                                      \
      }                                                                      \
    else if ((Nth) == (Node)->length)                                        \
      {                                                                      \
        VAL_QUEUE_PUSH_TAIL(Node, Val);                                      \
      }                                                                      \
    else                                                                     \
      {                                                                      \
        glib_typeof((Node)->head) ID;                                        \
        glib_typeof((Node)->head) _nth;                                      \
                                                                             \
        g_assert_cmpint (VAL_QUEUE_LENGTH(Node), >, 0);                      \
        g_assert (VAL_QUEUE_IS_VALID(Node, (Node)->head));                   \
        g_assert (VAL_QUEUE_IS_VALID(Node, (Node)->tail));                   \
                                                                             \
        for (ID = (Node)->head, _nth = 0;                                    \
             _nth < (Nth) && VAL_QUEUE_IS_VALID(Node, ID);                   \
             ID = (Node)->items[ID].next, ++_nth)                            \
          { /* Do Nothing */ }                                               \
                                                                             \
        g_assert (VAL_QUEUE_IS_VALID(Node, ID));                             \
        g_assert (VAL_QUEUE_IS_VALID(Node, (Node)->items[ID].prev));         \
                                                                             \
        (Node)->items[Val].prev = (Node)->items[ID].prev;                    \
        (Node)->items[Val].next = ID;                                        \
        (Node)->items[(Node)->items[ID].prev].next = Val;                    \
        (Node)->items[ID].prev = Val;                                        \
                                                                             \
        (Node)->length++;                                                    \
                                                                             \
        _VAL_QUEUE_VALIDATE(Node);                                           \
      }                                                                      \
  } G_STMT_END
#define VAL_QUEUE_POP_HEAD(Node,_pos) VAL_QUEUE_POP_NTH((Node), 0, _pos)
#define VAL_QUEUE_POP_TAIL(Node,_pos) VAL_QUEUE_POP_NTH((Node), (Node)->length - 1, _pos)
#define VAL_QUEUE_POP_AT(Node, _pos)                                         \
  G_STMT_START {                                                             \
    g_assert (_pos != VAL_QUEUE_INVALID(Node));                              \
    g_assert (_pos < G_N_ELEMENTS ((Node)->items));                          \
                                                                             \
    if ((Node)->items[_pos].prev != VAL_QUEUE_INVALID(Node))                 \
      (Node)->items[(Node)->items[_pos].prev].next = (Node)->items[_pos].next; \
    if ((Node)->items[_pos].next != VAL_QUEUE_INVALID(Node))                 \
      (Node)->items[(Node)->items[_pos].next].prev = (Node)->items[_pos].prev; \
    if ((Node)->head == _pos)                                                \
      (Node)->head = (Node)->items[_pos].next;                               \
    if ((Node)->tail == _pos)                                                \
      (Node)->tail = (Node)->items[_pos].prev;                               \
                                                                             \
    (Node)->items[_pos].prev = VAL_QUEUE_INVALID((Node));                    \
    (Node)->items[_pos].next = VAL_QUEUE_INVALID((Node));                    \
                                                                             \
    (Node)->length--;                                                        \
                                                                             \
    _VAL_QUEUE_VALIDATE(Node);                                               \
  } G_STMT_END
#define VAL_QUEUE_POP_NTH(Node, Nth, _pos)                                   \
  G_STMT_START {                                                             \
    _pos = VAL_QUEUE_INVALID(Node);                                          \
                                                                             \
    if (Nth == 0)                                                            \
      _pos = (Node)->head;                                                   \
    else if (Nth >= (((Node)->length) - 1))                                  \
      _pos = (Node)->tail;                                                   \
    else                                                                     \
      VAL_QUEUE_NTH (Node, Nth, _pos);                                       \
                                                                             \
   if (_pos != VAL_QUEUE_INVALID(Node))                                      \
     VAL_QUEUE_POP_AT (Node, _pos);                                          \
  } G_STMT_END
#define VAL_QUEUE_NTH(Node, Nth, _iter)                                      \
  G_STMT_START {                                                             \
    glib_typeof((Node)->head) _nth;                                          \
    if (Nth == 0)                                                            \
      _iter = (Node)->head;                                                  \
    else if (Nth >= (((Node)->length) - 1))                                  \
      _iter = (Node)->tail;                                                  \
    else                                                                     \
      {                                                                      \
        for (_iter = (Node)->head, _nth = 0;                                 \
             _nth < (Nth);                                                   \
             _iter = (Node)->items[_iter].next, ++_nth)                      \
          { /* Do Nothing */ }                                               \
      }                                                                      \
  } G_STMT_END
#define _VAL_QUEUE_MOVE(Node, Old, New)                                      \
  G_STMT_START {                                                             \
    (Node)->items[New] = (Node)->items[Old];                                 \
    if ((Node)->items[New].prev != VAL_QUEUE_INVALID(Node))                  \
      (Node)->items[(Node)->items[New].prev].next = New;                     \
    if ((Node)->items[New].next != VAL_QUEUE_INVALID(Node))                  \
      (Node)->items[(Node)->items[New].next].prev = New;                     \
    if ((Node)->head == Old)                                                 \
      (Node)->head = New;                                                    \
    if ((Node)->tail == Old)                                                 \
      (Node)->tail = New;                                                    \
  } G_STMT_END
/*
 * SORTED_ARRAY_FIELD:
 * @TYPE: The type of the structure used by elements in the array
 * @N_ITEMS: The maximum number of items in the array
 *
 * This creates a new inline structure that can be embedded within
 * other super-structures.
 *
 * @N_ITEMS must be <= 254 or this macro will fail.
 */
#define SORTED_ARRAY_FIELD(TYPE,N_ITEMS)                                     \
  struct {                                                                   \
    TYPE items[N_ITEMS];                                                     \
    VAL_QUEUE_NODE(guint8, N_ITEMS) q;                                       \
  }
/*
 * SORTED_ARRAY_INIT:
 * @FIELD: A pointer to a SortedArray
 *
 * This will initialize a node that has been previously registered
 * using %SORTED_ARRAY_FIELD(). You must call this macro before
 * using the SortedArray structure.
 */
#define SORTED_ARRAY_INIT(FIELD)                                             \
  G_STMT_START {                                                             \
    G_STATIC_ASSERT (G_N_ELEMENTS((FIELD)->items) < 255);                    \
    VAL_QUEUE_INIT(&(FIELD)->q);                                             \
  } G_STMT_END
/*
 * SORTED_ARRAY_LENGTH:
 * @FIELD: A pointer to the SortedArray field.
 *
 * This macro will evaluate to the number of items inserted into
 * the SortedArray.
 */
#define SORTED_ARRAY_LENGTH(FIELD) (VAL_QUEUE_LENGTH(&(FIELD)->q))
/*
 * SORTED_ARRAY_CAPACITY:
 * @FIELD: A pointer to the SortedArray field.
 *
 * This macro will evaluate to the number of elements in the SortedArray.
 * This is dependent on how the SortedArray was instantiated using
 * the %SORTED_ARRAY_FIELD() macro.
 */
#define SORTED_ARRAY_CAPACITY(FIELD) (G_N_ELEMENTS((FIELD)->items))
/*
 * SORTED_ARRAY_IS_FULL:
 * @FIELD: A pointer to the SortedArray field.
 *
 * This macro will evaluate to 1 if the SortedArray is at capacity.
 * Otherwise, the macro will evaluate to 0.
 */
#define SORTED_ARRAY_IS_FULL(FIELD) (SORTED_ARRAY_LENGTH(FIELD) == SORTED_ARRAY_CAPACITY(FIELD))
/*
 * SORTED_ARRAY_IS_EMPTY:
 * @FIELD: A SortedArray field
 *
 * This macro will evaluate to 1 if the SortedArray contains zero children.
 */
#define SORTED_ARRAY_IS_EMPTY(FIELD) (SORTED_ARRAY_LENGTH(FIELD) == 0)
/*
 * SORTED_ARRAY_INSERT_VAL:
 * @FIELD: A pointer to a SortedArray field.
 * @POSITION: the logical position at which to insert
 * @ELEMENT: The element to insert
 *
 * This will insert a new item into the array. It is invalid API use
 * to call this function while the SortedArray is at capacity. Check
 * SORTED_ARRAY_IS_FULL() before using this function to be certain.
 */
#define SORTED_ARRAY_INSERT_VAL(FIELD,POSITION,ELEMENT)                      \
  G_STMT_START {                                                             \
    guint8 _pos;                                                             \
                                                                             \
    g_assert (POSITION >= 0);                                                \
    g_assert (POSITION <= SORTED_ARRAY_LENGTH(FIELD));                       \
                                                                             \
    _pos = VAL_QUEUE_LENGTH(&(FIELD)->q);                                    \
    g_assert (_pos != VAL_QUEUE_INVALID(&(FIELD)->q));                       \
    (FIELD)->items[_pos] = ELEMENT;                                          \
    VAL_QUEUE_INSERT(&(FIELD)->q, POSITION, _pos);                           \
  } G_STMT_END
#define SORTED_ARRAY_REMOVE_INDEX(FIELD,POSITION,_ele)                       \
  G_STMT_START {                                                             \
    guint8 _pos;                                                             \
    guint8 _len;                                                             \
                                                                             \
    VAL_QUEUE_POP_NTH(&(FIELD)->q, POSITION, _pos);                          \
    _ele = (FIELD)->items[_pos];                                             \
    _len = VAL_QUEUE_LENGTH(&(FIELD)->q);                                    \
                                                                             \
    /* We must preserve our invariant of having no empty gaps                \
     * in the array so that se can place new items always at the             \
     * end (to avoid scanning for an empty spot).                            \
     * Therefore we move our tail item into the removed slot and             \
     * adjust the iqueue positions (which are all O(1).                      \
     */                                                                      \
                                                                             \
    if (_pos < _len)                                                         \
      {                                                                      \
        (FIELD)->items[_pos] = (FIELD)->items[_len];                         \
        _VAL_QUEUE_MOVE(&(FIELD)->q, _len, _pos);                            \
      }                                                                      \
  } G_STMT_END
/* SORTED_ARRAY_FOREACH_REMOVE:
 *
 * This a form of SORTED_ARRAY_REMOVE_INDEX but to be used when you
 * are within a SORTED_ARRAY_FOREACH() to avoid extra scanning.
 */
#define SORTED_ARRAY_FOREACH_REMOVE(FIELD)                                   \
  G_STMT_START {                                                             \
    guint8 _pos = _current;                                                  \
    guint8 _len = VAL_QUEUE_LENGTH(&(FIELD)->q);                             \
                                                                             \
    g_assert (_len > 0);                                                     \
    g_assert (_pos < _len);                                                  \
    VAL_QUEUE_POP_AT(&(FIELD)->q, _pos);                                     \
    g_assert (VAL_QUEUE_LENGTH(&(FIELD)->q) == _len-1);                      \
    _len--;                                                                  \
                                                                             \
    /* We must preserve our invariant of having no empty gaps                \
     * in the array so that se can place new items always at the             \
     * end (to avoid scanning for an empty spot).                            \
     * Therefore we move our tail item into the removed slot and             \
     * adjust the iqueue positions (which are all O(1).                      \
     */                                                                      \
                                                                             \
    if (_pos < _len)                                                         \
      {                                                                      \
        (FIELD)->items[_pos] = (FIELD)->items[_len];                         \
        _VAL_QUEUE_MOVE(&(FIELD)->q, _len, _pos);                            \
                                                                             \
        /* We might need to change the iter if next position moved */        \
        if (_aiter == _len)                                                  \
          _aiter = _pos;                                                     \
      }                                                                      \
                                                                             \
  } G_STMT_END
/*
 * SORTED_ARRAY_FOREACH:
 * @FIELD: A pointer to a SortedArray
 * @Element: The type of the elements in @FIELD
 * @Name: the name for a pointer of type @Element
 * @LABlock: a {} tyle block to execute for each item. You may use
 *    "break" to exit the foreach.
 *
 * Calls @Block for every element stored in @FIELD. A pointer to
 * each element will be provided as a variable named @Name.
 */
#define SORTED_ARRAY_FOREACH(FIELD, Element, Name, LABlock)                  \
  G_STMT_START {                                                             \
    for (glib_typeof((FIELD)->q.head) _aiter = (FIELD)->q.head;              \
         _aiter != VAL_QUEUE_INVALID(&(FIELD)->q);                           \
         /* Do Nothing */)                                                   \
      {                                                                      \
        G_GNUC_UNUSED glib_typeof((FIELD)->q.head) _current = _aiter;        \
        Element * Name = &(FIELD)->items[_aiter];                            \
        _aiter = (FIELD)->q.items[_aiter].next;                              \
        LABlock                                                              \
      }                                                                      \
  } G_STMT_END
#define SORTED_ARRAY_FOREACH_REVERSE(FIELD, Element, Name, LABlock)          \
  G_STMT_START {                                                             \
    for (glib_typeof((FIELD)->q.head) _aiter = (FIELD)->q.tail;              \
         _aiter != VAL_QUEUE_INVALID(&(FIELD)->q);                           \
         /* Do Nothing */)                                                   \
      {                                                                      \
        G_GNUC_UNUSED glib_typeof((FIELD)->q.head) _current = _aiter;        \
        Element * Name = &(FIELD)->items[_aiter];                            \
        _aiter = (FIELD)->q.items[_aiter].prev;                              \
        LABlock                                                              \
      }                                                                      \
  } G_STMT_END
#define SORTED_ARRAY_FOREACH_PEEK(FIELD)                                     \
  (((FIELD)->q.items[_current].next != VAL_QUEUE_INVALID(&(FIELD)->q))       \
    ? &(FIELD)->items[(FIELD)->q.items[_current].next] : NULL)
#define SORTED_ARRAY_SPLIT(FIELD, SPLIT)                                     \
  G_STMT_START {                                                             \
    guint8 _mid;                                                             \
                                                                             \
    SORTED_ARRAY_INIT(SPLIT);                                                \
                                                                             \
    _mid = SORTED_ARRAY_LENGTH(FIELD) / 2;                                   \
                                                                             \
    for (guint8 _z = 0; _z < _mid; _z++)                                     \
      {                                                                      \
        glib_typeof((FIELD)->items[0]) ele;                                  \
        SORTED_ARRAY_POP_TAIL(FIELD, ele);                                   \
        SORTED_ARRAY_PUSH_HEAD(SPLIT, ele);                                  \
      }                                                                      \
  } G_STMT_END
#define SORTED_ARRAY_SPLIT2(FIELD, LEFT, RIGHT)                              \
  G_STMT_START {                                                             \
    guint8 mid;                                                              \
                                                                             \
    SORTED_ARRAY_INIT(LEFT);                                                 \
    SORTED_ARRAY_INIT(RIGHT);                                                \
                                                                             \
    mid = SORTED_ARRAY_LENGTH(FIELD) / 2;                                    \
                                                                             \
    for (guint8 i = 0; i < mid; i++)                                         \
      {                                                                      \
        glib_typeof((FIELD)->items[0]) ele;                                  \
        SORTED_ARRAY_POP_TAIL(FIELD, ele);                                   \
        SORTED_ARRAY_PUSH_HEAD(RIGHT, ele);                                  \
      }                                                                      \
                                                                             \
    while (!SORTED_ARRAY_IS_EMPTY(FIELD))                                    \
      {                                                                      \
        glib_typeof((FIELD)->items[0]) ele;                                  \
        SORTED_ARRAY_POP_TAIL(FIELD, ele);                                   \
        SORTED_ARRAY_PUSH_HEAD(LEFT, ele);                                   \
      }                                                                      \
  } G_STMT_END
#define SORTED_ARRAY_PEEK_HEAD(FIELD) ((FIELD)->items[VAL_QUEUE_PEEK_HEAD(&(FIELD)->q)])
#define SORTED_ARRAY_POP_HEAD(FIELD,_ele) SORTED_ARRAY_REMOVE_INDEX(FIELD, 0, _ele)
#define SORTED_ARRAY_POP_TAIL(FIELD,_ele) SORTED_ARRAY_REMOVE_INDEX(FIELD, SORTED_ARRAY_LENGTH(FIELD)-1, _ele)
#define SORTED_ARRAY_PUSH_HEAD(FIELD, ele)                                   \
  G_STMT_START {                                                             \
    guint8 _pos = VAL_QUEUE_LENGTH(&(FIELD)->q);                             \
    g_assert_cmpint (_pos, <, G_N_ELEMENTS ((FIELD)->items));                \
    (FIELD)->items[_pos] = ele;                                              \
    VAL_QUEUE_PUSH_HEAD(&(FIELD)->q, _pos);                                  \
  } G_STMT_END
#define SORTED_ARRAY_PUSH_TAIL(FIELD, ele)                                   \
  G_STMT_START {                                                             \
    guint8 _pos = VAL_QUEUE_LENGTH(&(FIELD)->q);                             \
    g_assert_cmpint (_pos, <, G_N_ELEMENTS ((FIELD)->items));                \
    (FIELD)->items[_pos] = ele;                                              \
    VAL_QUEUE_PUSH_TAIL(&(FIELD)->q, _pos);                                  \
  } G_STMT_END

#define GTK_TEXT_REGION_MAX_BRANCHES 26
#define GTK_TEXT_REGION_MIN_BRANCHES (GTK_TEXT_REGION_MAX_BRANCHES/3)
#define GTK_TEXT_REGION_MAX_RUNS     26
#define GTK_TEXT_REGION_MIN_RUNS     (GTK_TEXT_REGION_MAX_RUNS/3)

typedef union  _GtkTextRegionNode   GtkTextRegionNode;
typedef struct _GtkTextRegionBranch GtkTextRegionBranch;
typedef struct _GtkTextRegionLeaf   GtkTextRegionLeaf;
typedef struct _GtkTextRegionChild  GtkTextRegionChild;

struct _GtkTextRegionChild
{
  GtkTextRegionNode *node;
  gsize              length;
};

struct _GtkTextRegionBranch
{
  GtkTextRegionNode *tagged_parent;
  GtkTextRegionNode *prev;
  GtkTextRegionNode *next;
  SORTED_ARRAY_FIELD (GtkTextRegionChild, GTK_TEXT_REGION_MAX_BRANCHES) children;
};

struct _GtkTextRegionLeaf
{
  GtkTextRegionNode *tagged_parent;
  GtkTextRegionNode *prev;
  GtkTextRegionNode *next;
  SORTED_ARRAY_FIELD (GtkTextRegionRun, GTK_TEXT_REGION_MAX_RUNS) runs;
};

union _GtkTextRegionNode
{
  /* pointer to the parent, low bit 0x1 means leaf node */
  GtkTextRegionNode *tagged_parent;
  struct _GtkTextRegionLeaf leaf;
  struct _GtkTextRegionBranch branch;
};

struct _GtkTextRegion
{
  GtkTextRegionNode root;
  GtkTextRegionJoinFunc join_func;
  GtkTextRegionSplitFunc split_func;
  gsize length;
  GtkTextRegionNode *cached_result;
  gsize cached_result_offset;
};

#define TAG(ptr,val) GSIZE_TO_POINTER(GPOINTER_TO_SIZE(ptr)|(gsize)val)
#define UNTAG(ptr)   GSIZE_TO_POINTER(GPOINTER_TO_SIZE(ptr) & ~(gsize)1)

static inline GtkTextRegionNode *
gtk_text_region_node_get_parent (GtkTextRegionNode *node)
{
  if (node == NULL)
    return NULL;
  return UNTAG (node->tagged_parent);
}

static inline gboolean
gtk_text_region_node_is_leaf (GtkTextRegionNode *node)
{
  GtkTextRegionNode *parent = gtk_text_region_node_get_parent (node);

  return parent != NULL && node->tagged_parent != parent;
}

static inline void
gtk_text_region_node_set_parent (GtkTextRegionNode *node,
                                 GtkTextRegionNode *parent)
{
  node->tagged_parent = TAG (parent, gtk_text_region_node_is_leaf (node));
}

static inline gsize
gtk_text_region_node_length (GtkTextRegionNode *node)
{
  gsize length = 0;

  g_assert (node != NULL);

  if (gtk_text_region_node_is_leaf (node))
    {
      SORTED_ARRAY_FOREACH (&node->leaf.runs, GtkTextRegionRun, run, {
        length += run->length;
      });
    }
  else
    {
      SORTED_ARRAY_FOREACH (&node->branch.children, GtkTextRegionChild, child, {
        length += child->length;
      });
    }

  return length;
}

static inline GtkTextRegionNode *
_gtk_text_region_get_first_leaf (GtkTextRegion *self)
{
  for (GtkTextRegionNode *iter = &self->root;
       iter;
       iter = SORTED_ARRAY_PEEK_HEAD (&iter->branch.children).node)
    {
      if (gtk_text_region_node_is_leaf (iter))
        return iter;
    }

  g_assert_not_reached ();
}

G_END_DECLS

#endif /* __GTK_TEXT_REGION_BTREE_H__ */
