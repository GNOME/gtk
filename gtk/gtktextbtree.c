/*
 * gtktextbtree.c --
 *
 *      This file contains code that manages the B-tree representation
 *      of text for the text buffer and implements character and
 *      toggle segment types.
 *
 * Copyright (c) 1992-1994 The Regents of the University of California.
 * Copyright (c) 1994-1995 Sun Microsystems, Inc.
 * Copyright (c) 2000      Red Hat, Inc.
 * Tk -> Gtk port by Havoc Pennington <hp@redhat.com>
 *
 * This software is copyrighted by the Regents of the University of
 * California, Sun Microsystems, Inc., and other parties.  The
 * following terms apply to all files associated with the software
 * unless explicitly disclaimed in individual files.
 * 
 * The authors hereby grant permission to use, copy, modify,
 * distribute, and license this software and its documentation for any
 * purpose, provided that existing copyright notices are retained in
 * all copies and that this notice is included verbatim in any
 * distributions. No written agreement, license, or royalty fee is
 * required for any of the authorized uses.  Modifications to this
 * software may be copyrighted by their authors and need not follow
 * the licensing terms described here, provided that the new terms are
 * clearly indicated on the first page of each file where they apply.
 * 
 * IN NO EVENT SHALL THE AUTHORS OR DISTRIBUTORS BE LIABLE TO ANY
 * PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL
 * DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE, ITS DOCUMENTATION,
 * OR ANY DERIVATIVES THEREOF, EVEN IF THE AUTHORS HAVE BEEN ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * THE AUTHORS AND DISTRIBUTORS SPECIFICALLY DISCLAIM ANY WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND
 * NON-INFRINGEMENT.  THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS,
 * AND THE AUTHORS AND DISTRIBUTORS HAVE NO OBLIGATION TO PROVIDE
 * MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 *
 * GOVERNMENT USE: If you are acquiring this software on behalf of the
 * U.S. government, the Government shall have only "Restricted Rights"
 * in the software and related documentation as defined in the Federal
 * Acquisition Regulations (FARs) in Clause 52.227.19 (c) (2).  If you
 * are acquiring the software on behalf of the Department of Defense,
 * the software shall be classified as "Commercial Computer Software"
 * and the Government shall have only "Restricted Rights" as defined
 * in Clause 252.227-7013 (c) (1) of DFARs.  Notwithstanding the
 * foregoing, the authors grant the U.S. Government and others acting
 * in its behalf permission to use and distribute the software in
 * accordance with the terms specified in this license.
 * 
 */

#include "gtktextbtree.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include "gtktexttag.h"
#include "gtktexttagtable.h"
#include "gtktextlayout.h"
#include "gtktextiterprivate.h"

/* Set this from the debugger */
gboolean gtk_text_view_debug_btree = FALSE;

/*
 * Types
 */


/*
 * The structure below is used to pass information between
 * gtk_text_btree_get_tags and inc_count:
 */

typedef struct TagInfo {
  int numTags;                  /* Number of tags for which there
                                 * is currently information in
                                 * tags and counts. */
  int arraySize;                        /* Number of entries allocated for
                                         * tags and counts. */
  GtkTextTag **tags;           /* Array of tags seen so far.
                                * Malloc-ed. */
  int *counts;                  /* Toggle count (so far) for each
                                 * entry in tags.  Malloc-ed. */
} TagInfo;


/*
 * This is used to store per-view width/height info at the tree nodes.
 */

typedef struct _NodeData NodeData;

struct _NodeData {
  gpointer view_id;
  NodeData *next;

  /* These node data fields mark our damage region for the btree */
  /* If -1, we must recalc width for this node */
  gint width;
  /* If -1, we must recalc height for this node */
  gint height;
};


/*
 * The data structure below keeps summary information about one tag as part
 * of the tag information in a node.
 */

typedef struct Summary {
  GtkTextTagInfo *info;                     /* Handle for tag. */
  int toggle_count;                     /* Number of transitions into or
                                         * out of this tag that occur in
                                         * the subtree rooted at this node. */
  struct Summary *next;         /* Next in list of all tags for same
                                 * node, or NULL if at end of list. */
} Summary;

/*
 * The data structure below defines a node in the B-tree.
 */

struct _GtkTextBTreeNode {
  GtkTextBTreeNode *parent;         /* Pointer to parent node, or NULL if
                                     * this is the root. */
  GtkTextBTreeNode *next;           /* Next in list of siblings with the
                                     * same parent node, or NULL for end
                                     * of list. */
  Summary *summary;             /* First in malloc-ed list of info
                                 * about tags in this subtree (NULL if
                                 * no tag info in the subtree). */
  int level;                            /* Level of this node in the B-tree.
                                         * 0 refers to the bottom of the tree
                                         * (children are lines, not nodes). */
  union {                               /* First in linked list of children. */
    struct _GtkTextBTreeNode *node;         /* Used if level > 0. */
    GtkTextLine *line;         /* Used if level == 0. */
  } children;
  int num_children;                     /* Number of children of this node. */
  int num_lines;                        /* Total number of lines (leaves) in
                                         * the subtree rooted here. */
  int num_chars;                        /* Number of chars below here */

  NodeData *node_data;
};


/*
 * Used to store the list of views in our btree
 */

typedef struct _BTreeView BTreeView;

struct _BTreeView {
  gpointer view_id;
  GtkTextLayout *layout;
  BTreeView *next;
  BTreeView *prev;
  GDestroyNotify line_data_destructor;
};

/*
 * And the tree itself
 */

struct _GtkTextBTree {
  GtkTextBTreeNode *root_node;          /* Pointer to root of B-tree. */
  GtkTextTagTable *table;
  GHashTable *mark_table;
  guint refcount;
  GtkTextLineSegment *insert_mark;
  GtkTextLineSegment *selection_bound_mark;
  GtkTextBuffer *buffer;
  BTreeView *views;
  GSList *tag_infos;
  guint tag_changed_handler;
  guint tag_removed_handler;
  /* Incremented when a segment with a byte size > 0
     is added to or removed from the tree (i.e. the
     length of a line may have changed, and lines may
     have been added or removed). This invalidates
     all outstanding iterators.
  */
  guint chars_changed_stamp;
  /* Incremented when any segments are added or deleted;
     this makes outstanding iterators recalculate their
     pointed-to segment and segment offset.
  */
  guint segments_changed_stamp;
};


/*
 * Upper and lower bounds on how many children a node may have:
 * rebalance when either of these limits is exceeded.  MAX_CHILDREN
 * should be twice MIN_CHILDREN and MIN_CHILDREN must be >= 2.
 */

/* Tk used MAX of 12 and MIN of 6. This makes the tree wide and
   shallow. It appears to be faster to locate a particular line number
   if the tree is narrow and deep, since it is more finely sorted.  I
   guess this may increase memory use though, and make it slower to
   walk the tree in order, or locate a particular byte index (which
   is done by walking the tree in order).
   
   There's basically a tradeoff here. However I'm thinking we want to
   add pixels, byte counts, and char counts to the tree nodes,
   at that point narrow and deep should speed up all operations,
   not just the line number searches.
*/

#if 1
#define MAX_CHILDREN 12
#define MIN_CHILDREN 6
#else
#define MAX_CHILDREN 6
#define MIN_CHILDREN 3
#endif

/*
 * Prototypes
 */

static BTreeView        *gtk_text_btree_get_view                 (GtkTextBTree     *tree,
                                                                  gpointer          view_id);
static void              gtk_text_btree_rebalance                (GtkTextBTree     *tree,
                                                                  GtkTextBTreeNode *node);
static GtkTextLine     * get_last_line                           (GtkTextBTree     *tree);
static void              post_insert_fixup                       (GtkTextBTree     *tree,
                                                                  GtkTextLine      *insert_line,
                                                                  gint              char_count_delta,
                                                                  gint              line_count_delta);
static void              gtk_text_btree_node_invalidate_upward   (GtkTextBTreeNode *node,
                                                                  gpointer          view_id);
static void              gtk_text_btree_node_adjust_toggle_count (GtkTextBTreeNode *node,
                                                                  GtkTextTagInfo   *info,
                                                                  gint              adjust);
static gboolean          gtk_text_btree_node_has_tag             (GtkTextBTreeNode *node,
                                                                  GtkTextTag       *tag);

static void             segments_changed                (GtkTextBTree     *tree);
static void             chars_changed                   (GtkTextBTree     *tree);
static void             summary_list_destroy            (Summary          *summary);
static GtkTextLine     *gtk_text_line_new               (void);
static void             gtk_text_line_destroy           (GtkTextBTree     *tree,
                                                         GtkTextLine      *line);
static void             gtk_text_line_set_parent        (GtkTextLine      *line,
                                                         GtkTextBTreeNode *node);
static void             gtk_text_btree_node_remove_data (GtkTextBTreeNode *node,
                                                         gpointer          view_id);


static NodeData         *node_data_new          (gpointer  view_id);
static void              node_data_destroy      (NodeData *nd);
static void              node_data_list_destroy (NodeData *nd);
static NodeData         *node_data_find         (NodeData *nd,
                                                 gpointer  view_id);

static GtkTextBTreeNode     *gtk_text_btree_node_new                 (void);
static void                  gtk_text_btree_node_invalidate_downward (GtkTextBTreeNode *node);
static void                  gtk_text_btree_node_invalidate_upward   (GtkTextBTreeNode *node,
                                                                      gpointer          view_id);
static void                  gtk_text_btree_node_remove_view         (BTreeView        *view,
                                                                      GtkTextBTreeNode *node,
                                                                      gpointer          view_id);
static void                  gtk_text_btree_node_destroy             (GtkTextBTree     *tree,
                                                                      GtkTextBTreeNode *node);
static NodeData         *    gtk_text_btree_node_ensure_data         (GtkTextBTreeNode *node,
                                                                      gpointer          view_id);
static void                  gtk_text_btree_node_remove_data         (GtkTextBTreeNode *node,
                                                                      gpointer          view_id);
static GtkTextLineData *     ensure_line_data                        (GtkTextLine      *line,
                                                                      GtkTextBTree     *tree,
                                                                      BTreeView        *view);
static void                  gtk_text_btree_node_get_size            (GtkTextBTreeNode *node,
                                                                      gpointer          view_id,
                                                                      GtkTextBTree     *tree,
                                                                      BTreeView        *view,
                                                                      gint             *width,
                                                                      gint             *height,
                                                                      GtkTextLine      *last_line);

static void get_tree_bounds       (GtkTextBTree     *tree,
                                   GtkTextIter      *start,
                                   GtkTextIter      *end);
static void tag_changed_cb        (GtkTextTagTable  *table,
                                   GtkTextTag       *tag,
                                   gboolean          size_changed,
                                   GtkTextBTree     *tree);
static void tag_removed_cb        (GtkTextTagTable  *table,
                                   GtkTextTag       *tag,
                                   GtkTextBTree     *tree);
static void cleanup_line          (GtkTextLine      *line);
static void recompute_node_counts (GtkTextBTreeNode *node);
static void inc_count             (GtkTextTag       *tag,
                                   int               inc,
                                   TagInfo          *tagInfoPtr);

static void gtk_text_btree_link_segment   (GtkTextLineSegment *seg,
                                           const GtkTextIter  *iter);
static void gtk_text_btree_unlink_segment (GtkTextBTree       *tree,
                                           GtkTextLineSegment *seg,
                                           GtkTextLine        *line);


static GtkTextTagInfo *gtk_text_btree_get_tag_info          (GtkTextBTree   *tree,
                                                             GtkTextTag     *tag);
static GtkTextTagInfo *gtk_text_btree_get_existing_tag_info (GtkTextBTree   *tree,
                                                             GtkTextTag     *tag);
static void            gtk_text_btree_remove_tag_info       (GtkTextBTree   *tree,
                                                             GtkTextTagInfo *info);


/* Inline thingies */

static inline void
segments_changed(GtkTextBTree *tree)
{
  tree->segments_changed_stamp += 1;
}

static inline void
chars_changed(GtkTextBTree *tree)
{
  tree->chars_changed_stamp += 1;
}

/*
 * BTree operations
 */

GtkTextBTree*
gtk_text_btree_new (GtkTextTagTable *table,
                    GtkTextBuffer *buffer)
{
  GtkTextBTree *tree;
  GtkTextBTreeNode *root_node;
  GtkTextLine *line, *line2;

  g_return_val_if_fail(GTK_IS_TEXT_VIEW_TAG_TABLE(table), NULL);
  g_return_val_if_fail(GTK_IS_TEXT_VIEW_BUFFER(buffer), NULL);
  
  /*
   * The tree will initially have two empty lines.  The second line
   * isn't actually part of the tree's contents, but its presence
   * makes several operations easier.  The tree will have one GtkTextBTreeNode,
   * which is also the root of the tree.
   */

  /* Create the root node. */
  
  root_node = gtk_text_btree_node_new();
  
  line = gtk_text_line_new();
  line2 = gtk_text_line_new();

  root_node->parent = NULL;
  root_node->next = NULL;
  root_node->summary = NULL;
  root_node->level = 0;
  root_node->children.line = line;
  root_node->num_children = 2;
  root_node->num_lines = 2;
  root_node->num_chars = 2;
  
  line->parent = root_node;
  line->next = line2;

  line->segments = char_segment_new("\n", 1);

  line2->parent = root_node;
  line2->next = NULL;
  line2->segments = char_segment_new("\n", 1);

  /* Create the tree itself */
  
  tree = g_new0(GtkTextBTree, 1);
  tree->root_node = root_node;
  tree->table = table;
  tree->views = NULL;

  /* Set these to values that are unlikely to be found
     in random memory garbage. */
  tree->chars_changed_stamp = 49;
  tree->segments_changed_stamp = 243;
  
  gtk_object_ref(GTK_OBJECT(tree->table));
  gtk_object_sink(GTK_OBJECT(tree->table));

  tree->tag_changed_handler = gtk_signal_connect(GTK_OBJECT(tree->table),
                                                 "tag_changed",
                                                 GTK_SIGNAL_FUNC(tag_changed_cb),
                                                 tree);

  tree->tag_removed_handler = gtk_signal_connect(GTK_OBJECT(tree->table),
                                                 "tag_removed",
                                                 GTK_SIGNAL_FUNC(tag_removed_cb),
                                                 tree);
  
  tree->mark_table = g_hash_table_new(g_str_hash, g_str_equal);

  /* We don't ref the buffer, since the buffer owns us;
     we'd have some circularity issues. The buffer always
     lasts longer than the BTree
  */
  tree->buffer = buffer;
  
  {
    GtkTextIter start;

    gtk_text_btree_get_iter_at_line_char(tree, &start, 0, 0);
    

    tree->insert_mark = gtk_text_btree_set_mark(tree,
                                                "insert",
                                                FALSE,
                                                &start,
                                                FALSE);

    tree->insert_mark->body.mark.visible = TRUE;
    
    tree->selection_bound_mark = gtk_text_btree_set_mark(tree,
                                                         "selection_bound",
                                                         FALSE,
                                                         &start,
                                                         FALSE);
    
    mark_segment_ref(tree->insert_mark);
    mark_segment_ref(tree->selection_bound_mark);
  }

  tree->refcount = 1;
  
  return tree;
}

void
gtk_text_btree_ref (GtkTextBTree *tree)
{
  g_return_if_fail(tree != NULL);
  g_return_if_fail(tree->refcount > 0);
  
  tree->refcount += 1;
}

static void
mark_destroy_foreach(gpointer key, gpointer value, gpointer user_data)
{
  mark_segment_unref(value);
}

void
gtk_text_btree_unref (GtkTextBTree *tree)
{
  g_return_if_fail(tree != NULL);
  g_return_if_fail(tree->refcount > 0);

  tree->refcount -= 1;

  if (tree->refcount == 0)
    {      
      gtk_text_btree_node_destroy(tree, tree->root_node);

      g_hash_table_foreach(tree->mark_table,
                           mark_destroy_foreach,
                           NULL);
      g_hash_table_destroy(tree->mark_table);
      
      mark_segment_unref(tree->insert_mark);
      mark_segment_unref(tree->selection_bound_mark);

      gtk_signal_disconnect(GTK_OBJECT(tree->table),
                            tree->tag_changed_handler);

      gtk_signal_disconnect(GTK_OBJECT(tree->table),
                            tree->tag_removed_handler);
      
      gtk_object_unref(GTK_OBJECT(tree->table));

      g_free(tree);
    }
}

GtkTextBuffer*
gtk_text_btree_get_buffer (GtkTextBTree *tree)
{
  return tree->buffer;
}

guint
gtk_text_btree_get_chars_changed_stamp (GtkTextBTree *tree)
{
  return tree->chars_changed_stamp;
}

guint
gtk_text_btree_get_segments_changed_stamp (GtkTextBTree *tree)
{
  return tree->segments_changed_stamp;
}

void
gtk_text_btree_segments_changed (GtkTextBTree *tree)
{
  g_return_if_fail(tree != NULL);
  segments_changed(tree);
}

/*
 * Indexable segment mutation
 */

void
gtk_text_btree_delete (GtkTextIter *start,
                       GtkTextIter *end)
{
  GtkTextLineSegment *prev_seg;             /* The segment just before the start
                                             * of the deletion range. */
  GtkTextLineSegment *last_seg;             /* The segment just after the end
                                             * of the deletion range. */
  GtkTextLineSegment *seg, *next;
  GtkTextLine *curline;
  GtkTextBTreeNode *curnode, *node;
  GtkTextBTree *tree;
  GtkTextLine *start_line;
  GtkTextLine *end_line;
  gint start_byte_offset;
  
  g_return_if_fail(start != NULL);
  g_return_if_fail(end != NULL);
  g_return_if_fail(gtk_text_iter_get_btree(start) ==
                   gtk_text_iter_get_btree(end));

  gtk_text_iter_reorder(start, end);
  
  tree = gtk_text_iter_get_btree(start);
  
  {
    /*
     * The code below is ugly, but it's needed to make sure there
     * is always a dummy empty line at the end of the text.  If the
     * final newline of the file (just before the dummy line) is being
     * deleted, then back up index to just before the newline.  If
     * there is a newline just before the first character being deleted,
     * then back up the first index too, so that an even number of lines
     * gets deleted.  Furthermore, remove any tags that are present on
     * the newline that isn't going to be deleted after all (this simulates
     * deleting the newline and then adding a "clean" one back again).
     */

    gint line1;
    gint line2;
    
    line1 = gtk_text_iter_get_line_number(start);
    line2 = gtk_text_iter_get_line_number(end);
    
    if (line2 == gtk_text_btree_line_count(tree))
      {
        GtkTextTag** tags;
        int array_size;
        GtkTextIter orig_end;
        
        orig_end = *end;
        gtk_text_iter_backward_char(end);
        
        --line2;

        if (gtk_text_iter_get_line_char(start) == 0 &&
            line1 != 0)
          {
            gtk_text_iter_backward_char(start);
            --line1;
          }
        
        tags = gtk_text_btree_get_tags(end,
                                       &array_size);
        
        if (tags != NULL)
          {
            int i;

            i = 0;
            while (i < array_size)
              {
                gtk_text_btree_tag(end, &orig_end, tags[i], FALSE);
                
                ++i;
              }

            g_free(tags);
          }
      }
  }
  
  /* Broadcast the need for redisplay before we break the iterators */
  gtk_text_btree_invalidate_region(tree, start, end);

  /* Save the byte offset so we can reset the iterators */
  start_byte_offset = gtk_text_iter_get_line_byte(start);
  
  start_line = gtk_text_iter_get_line(start);
  end_line = gtk_text_iter_get_line(end);
  
  /*
   * Split the start and end segments, so we have a place
   * to insert our new text.
   *
   * Tricky point:  split at end first;  otherwise the split
   * at end may invalidate seg and/or prev_seg. This allows
   * us to avoid invalidating segments for start.
   */

  last_seg = gtk_text_line_segment_split(end);
  if (last_seg != NULL)
    last_seg = last_seg->next;
  else
    last_seg = end_line->segments;

  prev_seg = gtk_text_line_segment_split(start);
  if (prev_seg != NULL)
    {
      seg = prev_seg->next;
      prev_seg->next = last_seg;
    }
  else
    {
      seg = start_line->segments;
      start_line->segments = last_seg;
    }

  /* notify iterators that their segments need recomputation,
     just for robustness. */
  segments_changed(tree);
  
  /*
   * Delete all of the segments between prev_seg and last_seg.
   */

  curline = start_line;
  curnode = curline->parent;
  while (seg != last_seg)
    {
      gint char_count = 0;
    
      if (seg == NULL)
        {
          GtkTextLine *nextline;

          /*
           * We just ran off the end of a line.  First find the
           * next line, then go back to the old line and delete it
           * (unless it's the starting line for the range).
           */

          nextline = gtk_text_line_next(curline);
          if (curline != start_line)
            {
              if (curnode == start_line->parent)
                start_line->next = curline->next;
              else
                curnode->children.line = curline->next;
              
              for (node = curnode; node != NULL;
                   node = node->parent)
                {
                  node->num_lines -= 1;
                }
              
              curnode->num_children -= 1;
              gtk_text_btree_node_invalidate_upward(curline->parent, NULL);
              gtk_text_line_destroy(tree, curline);
            }
          
          curline = nextline;
          seg = curline->segments;

          /*
           * If the GtkTextBTreeNode is empty then delete it and its parents,
           * recursively upwards until a non-empty GtkTextBTreeNode is found.
           */

          while (curnode->num_children == 0)
            {
              GtkTextBTreeNode *parent;

              parent = curnode->parent;
              if (parent->children.node == curnode)
                {
                  parent->children.node = curnode->next;
                }
              else
                {
                  GtkTextBTreeNode *prevnode = parent->children.node;
                  while (prevnode->next != curnode)
                    {
                      prevnode = prevnode->next;
                    }
                  prevnode->next = curnode->next;
                }
              parent->num_children--;
              g_free(curnode);
              curnode = parent;
            }
          curnode = curline->parent;
          continue;
        }

      next = seg->next;
      char_count = seg->char_count;
    
      if ((*seg->type->deleteFunc)(seg, curline, 0) != 0)
        {
          /*
           * This segment refuses to die.  Move it to prev_seg and
           * advance prev_seg if the segment has left gravity.
           */
        
          if (prev_seg == NULL)
            {
              seg->next = start_line->segments;
              start_line->segments = seg;
            }
          else
            {
              seg->next = prev_seg->next;
              prev_seg->next = seg;
            }
          if (seg->type->leftGravity)
            {
              prev_seg = seg;
            }
        }
      else
        {
          /* Segment is gone. Decrement the char count of the node and
             all its parents. */
          for (node = curnode; node != NULL;
               node = node->parent)
            {
              node->num_chars -= char_count;
            }
        }
    
      seg = next;
    }

  /*
   * If the beginning and end of the deletion range are in different
   * lines, join the two lines together and discard the ending line.
   */

  if (start_line != end_line)
    {
      GtkTextLine *prevline;

      for (seg = last_seg; seg != NULL;
           seg = seg->next)
        {
          if (seg->type->lineChangeFunc != NULL)
            {
              (*seg->type->lineChangeFunc)(seg, end_line);
            }
        }
      curnode = end_line->parent;
      for (node = curnode; node != NULL;
           node = node->parent)
        {
          node->num_lines--;
        }
      curnode->num_children--;
      prevline = curnode->children.line;
      if (prevline == end_line)
        {
          curnode->children.line = end_line->next;
        }
      else
        {
          while (prevline->next != end_line)
            {
              prevline = prevline->next;
            }
          prevline->next = end_line->next;
        }
      gtk_text_btree_node_invalidate_upward(end_line->parent, NULL);
      gtk_text_line_destroy(tree, end_line);
      gtk_text_btree_rebalance(tree, curnode);
    }

  /*
   * Cleanup the segments in the new line.
   */

  cleanup_line(start_line);

  /*
   * Lastly, rebalance the first GtkTextBTreeNode of the range.
   */

  gtk_text_btree_rebalance(tree, start_line->parent);
  
  /* Notify outstanding iterators that they
     are now hosed */
  chars_changed(tree);
  segments_changed(tree);

  if (gtk_text_view_debug_btree)
    gtk_text_btree_check(tree);

  /* Re-initialize our iterators */
  gtk_text_btree_get_iter_at_line(tree, start, start_line, start_byte_offset);
  *end = *start;
}

void
gtk_text_btree_insert (GtkTextIter *iter,
                       const gchar *text,
                       gint len)
{
  GtkTextLineSegment *prev_seg;     /* The segment just before the first
                                     * new segment (NULL means new segment
                                     * is at beginning of line). */
  GtkTextLineSegment *cur_seg;              /* Current segment;  new characters
                                             * are inserted just after this one. 
                                             * NULL means insert at beginning of
                                             * line. */
  GtkTextLine *line;           /* Current line (new segments are
                                * added to this line). */
  GtkTextLineSegment *seg;
  GtkTextLine *newline;
  int chunkSize;                        /* # characters in current chunk. */
  guint sol; /* start of line */
  guint eol;                      /* Pointer to character just after last
                                   * one in current chunk. */
  int line_count_delta;                /* Counts change to total number of
                                        * lines in file. */

  int char_count_delta;                /* change to number of chars */
  GtkTextBTree *tree;
  gint start_byte_index;
  GtkTextLine *start_line;
  
  g_return_if_fail(text != NULL);
  g_return_if_fail(iter != NULL);
  
  if (len < 0)
    len = strlen(text);

  /* extract iterator info */
  tree = gtk_text_iter_get_btree(iter);
  line = gtk_text_iter_get_line(iter);
  start_line = line;
  start_byte_index = gtk_text_iter_get_line_byte(iter);

  /* Get our insertion segment split */
  prev_seg = gtk_text_line_segment_split(iter);
  cur_seg = prev_seg;

  /* Invalidate all iterators */
  chars_changed(tree);
  segments_changed(tree);
  
  /*
   * Chop the text up into lines and create a new segment for
   * each line, plus a new line for the leftovers from the
   * previous line.
   */

  eol = 0;
  sol = 0;
  line_count_delta = 0;
  char_count_delta = 0;
  while (eol < len)
    {
      for (; eol < len; eol++)
        {
          if (text[eol] == '\n')
            {
              eol++;
              break;
            }
        }
      chunkSize = eol - sol;

      seg = char_segment_new(&text[sol], chunkSize);

      char_count_delta += seg->char_count;
    
      if (cur_seg == NULL)
        {
          seg->next = line->segments;
          line->segments = seg;
        }
      else
        {
          seg->next = cur_seg->next;
          cur_seg->next = seg;
        }

      if (text[eol-1] != '\n')
        {
          break;
        }

      /*
       * The chunk ended with a newline, so create a new GtkTextLine
       * and move the remainder of the old line to it.
       */

      newline = gtk_text_line_new();
      gtk_text_line_set_parent(newline, line->parent);
      newline->next = line->next;
      line->next = newline;
      newline->segments = seg->next;
      seg->next = NULL;
      line = newline;
      cur_seg = NULL;
      line_count_delta++;    
    
      sol = eol;
    }

  /*
   * Cleanup the starting line for the insertion, plus the ending
   * line if it's different.
   */

  cleanup_line(start_line);
  if (line != start_line)
    {
      cleanup_line(line);
    }

  post_insert_fixup(tree, line, line_count_delta, char_count_delta);
  
  /* Invalidate our region, and reset the iterator the user
     passed in to point to the end of the inserted text. */
  {
    GtkTextIter start;
    GtkTextIter end;


    gtk_text_btree_get_iter_at_line(tree,
                                    &start,
                                    start_line,
                                    start_byte_index);
    end = start;

    /* We could almost certainly be more efficient here
       by saving the information from the insertion loop
       above. FIXME */
    gtk_text_iter_forward_chars(&end, char_count_delta);

    gtk_text_btree_invalidate_region(tree,
                                     &start, &end);


    /* Convenience for the user */
    *iter = end;
  }
}

void
gtk_text_btree_insert_pixmap (GtkTextIter *iter,
                              GdkPixmap *pixmap,
                              GdkBitmap *mask)
{
  GtkTextLineSegment *seg;
  GtkTextIter start;
  GtkTextLineSegment *prevPtr;
  GtkTextLine *line;
  GtkTextBTree *tree;
  gint start_byte_offset;
  
  line = gtk_text_iter_get_line(iter);
  tree = gtk_text_iter_get_btree(iter);
  start_byte_offset = gtk_text_iter_get_line_byte(iter);
  
  seg = pixmap_segment_new(pixmap, mask);

  prevPtr = gtk_text_line_segment_split(iter);
  if (prevPtr == NULL)
    {
      seg->next = line->segments;
      line->segments = seg;
    }
  else
    {
      seg->next = prevPtr->next;
      prevPtr->next = seg;
    }

  post_insert_fixup(tree, line, 0, seg->char_count);

  chars_changed(tree);
  segments_changed(tree);

  /* reset *iter for the user, and invalidate tree nodes */
  
  gtk_text_btree_get_iter_at_line(tree, &start, line, start_byte_offset);

  *iter = start;
  gtk_text_iter_forward_char(iter); /* skip forward past the pixmap */

  gtk_text_btree_invalidate_region(tree, &start, iter);
}


/*
 * View stuff
 */

static GtkTextLine*
find_line_by_y(GtkTextBTree *tree, BTreeView *view,
               GtkTextBTreeNode *node, gint y, gint *line_top,
               GtkTextLine *last_line)
{
  gint current_y = 0;

  if (node->level == 0)
    {
      GtkTextLine *line;

      line = node->children.line;

      while (line != NULL && line != last_line)
        {
          GtkTextLineData *ld;

          ld = ensure_line_data(line, tree, view);

          g_assert(ld != NULL);
          g_assert(ld->height >= 0);

          if (y < (current_y + ld->height))
            {
              return line;
            }

          current_y += ld->height;
          *line_top += ld->height;
          
          line = line->next;
        }
      return NULL;
    }
  else
    {
      GtkTextBTreeNode *child;

      child = node->children.node;

      while (child != NULL)
        {
          gint width;
          gint height;

          gtk_text_btree_node_get_size(child, view->view_id,
                                       tree, view,
                                       &width, &height,
                                       last_line);

          if (y < (current_y + height))            
            return find_line_by_y(tree, view, child,
                                  y - current_y, line_top,
                                  last_line);

          current_y += height;
          *line_top += height;
          
          child = child->next;
        }
      
      return NULL;
    }
}

gpointer
gtk_text_btree_find_line_data_by_y (GtkTextBTree *tree,
                                    gpointer view_id,
                                    gint ypixel,
                                    gint *line_top)
{
  GtkTextLine *line;
  BTreeView *view;
  GtkTextLine *last_line;
  
  g_return_val_if_fail(line_top != NULL, NULL);
  
  view = gtk_text_btree_get_view(tree, view_id);

  *line_top = 0;
  
  g_return_val_if_fail(view != NULL, NULL);

  last_line = get_last_line(tree);
  
  line = find_line_by_y(tree, view, tree->root_node, ypixel, line_top,
                        last_line);

  return line ? gtk_text_line_get_data(line, view_id) : NULL;
}

static gint
find_line_top_in_line_list(GtkTextBTree *tree,
                           BTreeView *view,
                           GtkTextLine *line,
                           GtkTextLine *target_line,
                           gint y)
{
  while (line != NULL)
    {
      GtkTextLineData *ld;
              
      if (line == target_line)
        {
          return y;
        }
              
      ld = ensure_line_data(line, tree, view);
              
      g_assert(ld != NULL);
      g_assert(ld->height >= 0);
              
      y += ld->height;
              
      line = line->next;
    }
          
  g_assert_not_reached(); /* If we get here, our
                             target line didn't exist
                             under its parent node */
  return 0;
}

gint
gtk_text_btree_find_line_top (GtkTextBTree *tree,
                              GtkTextLine *target_line,
                              gpointer view_id)
{
  gint y = 0;
  BTreeView *view;
  GSList *nodes;
  GSList *iter;
  GtkTextBTreeNode *node;
  
  view = gtk_text_btree_get_view(tree, view_id);

  g_return_val_if_fail(view != NULL, 0);

  nodes = NULL;
  node = target_line->parent;
  while (node != NULL)
    {
      nodes = g_slist_prepend(nodes, node);
      node = node->parent;
    }

  iter = nodes;
  while (iter != NULL)
    {      
      node = iter->data;
      
      if (node->level == 0)
        {
          g_slist_free(nodes);
          return find_line_top_in_line_list(tree, view,
                                            node->children.line,
                                            target_line, y);
        }
      else
        {
          GtkTextBTreeNode *child;
          GtkTextBTreeNode *target_node;

          g_assert(iter->next != NULL); /* not at level 0 */
          target_node = iter->next->data;
          
          child = node->children.node;

          while (child != NULL)
            {
              gint width;
              gint height;

              if (child == target_node)
                break;
              else
                {
                  gtk_text_btree_node_get_size(child, view->view_id,
                                               tree, view,
                                               &width, &height,
                                               NULL);
                  y += height;
                }
              child = child->next;
            }
          g_assert(child != NULL); /* should have broken out before we
                                      ran out of nodes */
        }

      iter = g_slist_next(iter);
    }

  g_assert_not_reached(); /* we return when we find the target line */
  return 0;  
}

void
gtk_text_btree_add_view (GtkTextBTree *tree,
                         GtkTextLayout *layout,
                         GDestroyNotify line_data_destructor)
{
  BTreeView *view;

  g_return_if_fail(tree != NULL);
  
  view = g_new(BTreeView, 1);

  view->view_id = layout;
  view->layout = layout;
  view->line_data_destructor = line_data_destructor;
  
  view->next = tree->views;
  view->prev = NULL;

  tree->views = view;
}

void
gtk_text_btree_remove_view (GtkTextBTree *tree,
                            gpointer view_id)
{
  BTreeView *view;

  g_return_if_fail(tree != NULL);
  
  view = tree->views;
  
  while (view != NULL)
    {
      if (view->view_id == view_id)
        break;

      view = view->next;
    }
  
  g_return_if_fail(view != NULL);

  if (view->next)
    view->next->prev = view->prev;

  if (view->prev)
    view->prev->next = view->next;

  if (view == tree->views)
    tree->views = view->next;

  gtk_text_btree_node_remove_view(view, tree->root_node, view_id);
  
  g_free(view);
}

void
gtk_text_btree_invalidate_region (GtkTextBTree *tree,
                                  const GtkTextIter *start,
                                  const GtkTextIter *end)
{
  BTreeView *view;
  
  view = tree->views;

  while (view != NULL)
    {
      gtk_text_layout_invalidate(view->layout, start, end);

      view = view->next;
    }
}

void
gtk_text_btree_get_view_size (GtkTextBTree *tree,
                              gpointer view_id,
                              gint *width,
                              gint *height)
{
  g_return_if_fail(tree != NULL);
  g_return_if_fail(view_id != NULL);
  
  return gtk_text_btree_node_get_size(tree->root_node, view_id, tree, NULL,
                                      width, height, NULL);  
}

static gint
node_get_height_before_damage(GtkTextBTreeNode *node, gpointer view_id)
{
  gint height = 0;
  
  if (node->level == 0)
    {
      GtkTextLine *line;
      GtkTextLineData *ld;
      NodeData *nd;

      /* Don't recurse if this node is undamaged. */
      nd = node_data_find(node->node_data, view_id);
      if (nd != NULL && nd->height >= 0)
        return nd->height;
      
      line = node->children.line;

      while (line != NULL)
        {
          ld = gtk_text_line_get_data(line, view_id);

          if (ld != NULL && ld->height >= 0)
            height += ld->height;
          else
            {
              /* Found a damaged line. */
              return height;
            }
          
          line = line->next;
        }
      
      return height;
    }
  else
    {
      GtkTextBTreeNode *child;
      NodeData *nd;

      /* Don't recurse if this node is undamaged. */
      nd = node_data_find(node->node_data, view_id);
      if (nd != NULL && nd->height >= 0)
        return nd->height;

      /* Otherwise, count height of undamaged children occuring
         before the first damaged child. */
      child = node->children.node;
      while (child != NULL)
        {
          nd = node_data_find(child->node_data, view_id);

          if (nd != NULL && nd->height >= 0)
            height += nd->height;
          else
            {
              /* found a damaged child of the node. Add any undamaged
                 children of the child to our height, and return. */
              return height + node_get_height_before_damage(child, view_id);
            }
          child = child->next;
        }
      
      return height;
    }
}

static gint
node_get_height_after_damage(GtkTextBTreeNode *node, gpointer view_id)
{
  gint height = 0;
  
  if (node->level == 0)
    {
      GtkTextLine *line;
      GtkTextLineData *ld;
      NodeData *nd;

      /* Don't recurse if this node is undamaged. */
      nd = node_data_find(node->node_data, view_id);
      if (nd != NULL && nd->height >= 0)
        return nd->height;
      
      line = node->children.line;

      while (line != NULL)
        {
          ld = gtk_text_line_get_data(line, view_id);

          if (ld != NULL && ld->height >= 0)
            height += ld->height;
          else
            {
              /* Found a damaged line. Reset height after
                 damage to 0. */
              height = 0;
            }
          
          line = line->next;
        }
      
      return height;
    }
  else
    {
      GtkTextBTreeNode *child;
      NodeData *nd;
      GtkTextBTreeNode *last_damaged;
      
      /* Don't recurse if this node is undamaged. */
      nd = node_data_find(node->node_data, view_id);
      if (nd != NULL && nd->height >= 0)
        return nd->height;

      /* Otherwise, count height of undamaged children occuring
         before the first damaged child. */
      last_damaged = NULL;
      child = node->children.node;
      while (child != NULL)
        {
          nd = node_data_find(child->node_data, view_id);

          if (nd != NULL && nd->height >= 0)
            height += nd->height;
          else
            {
              /* found a damaged child of the node. reset height after
                 damage to 0, then add undamaged stuff in this damage
                 node. */
              height = 0;
              last_damaged = child;
            }
          child = child->next;
        }

      /* height is now the sum of all fully undamaged child node
         heights.  However we need to include undamaged child nodes of
         the last damaged node as well. */
      if (last_damaged != NULL)
        {
          height += node_get_height_after_damage(last_damaged, view_id);
        }
      
      return height;
    }
}

void
gtk_text_btree_get_damage_range    (GtkTextBTree      *tree,
                                    gpointer            view_id,
                                    gint               *top_undamaged_size,
                                    gint               *bottom_undamaged_size)
{
  g_return_if_fail(tree != NULL);
  
  *top_undamaged_size = node_get_height_before_damage(tree->root_node, view_id);
  *bottom_undamaged_size = node_get_height_after_damage(tree->root_node, view_id);
}

/*
 * Tag
 */

typedef struct {
  GtkTextIter *iters;
  guint count;
  guint alloced;
} IterStack;

static IterStack*
iter_stack_new(void)
{
  IterStack *stack;
  stack = g_new(IterStack, 1);
  stack->iters = NULL;
  stack->count = 0;
  stack->alloced = 0;
  return stack;
}

static void
iter_stack_push(IterStack *stack, const GtkTextIter *iter)
{
  stack->count += 1;
  if (stack->count > stack->alloced)
    {
      stack->alloced = stack->count*2;
      stack->iters = g_realloc(stack->iters,
                               stack->alloced*sizeof(GtkTextIter));
    }
  stack->iters[stack->count-1] = *iter;
}

static gboolean
iter_stack_pop(IterStack *stack, GtkTextIter *iter)
{
  if (stack->count == 0)
    return FALSE;
  else
    {
      stack->count -= 1;
      *iter = stack->iters[stack->count];
      return TRUE;
    }
}

static void
iter_stack_free(IterStack *stack)
{
  g_free(stack->iters);
  g_free(stack);
}

static void
iter_stack_invert(IterStack *stack)
{
  if (stack->count > 0)
    {
      guint i = 0;
      guint j = stack->count - 1;
      while (i < j)
        {
          GtkTextIter tmp;
      
          tmp = stack->iters[i];
          stack->iters[i] = stack->iters[j];
          stack->iters[j] = tmp;

          ++i;
          --j;
        }
    }
}

void
gtk_text_btree_tag (const GtkTextIter *start_orig,
                    const GtkTextIter *end_orig,
                    GtkTextTag *tag,
                    gboolean add)
{
  GtkTextLineSegment *seg, *prev;
  GtkTextLine *cleanupline;
  gboolean toggled_on;
  GtkTextLine *start_line;
  GtkTextLine *end_line;
  GtkTextIter iter;
  GtkTextIter start, end;
  GtkTextBTree *tree;  
  IterStack *stack;
  GtkTextTagInfo *info;
  
  g_return_if_fail(start_orig != NULL);
  g_return_if_fail(end_orig != NULL);
  g_return_if_fail(GTK_IS_TEXT_VIEW_TAG(tag));
  g_return_if_fail(gtk_text_iter_get_btree(start_orig) ==
                   gtk_text_iter_get_btree(end_orig));

#if 0
  printf("%s tag %s from %d to %d\n",
         add ? "Adding" : "Removing",
         tag->name,
         gtk_text_iter_get_char_index(start_orig),
         gtk_text_iter_get_char_index(end_orig));
#endif
  if (gtk_text_iter_equal(start_orig, end_orig))
    return;
  
  start = *start_orig;
  end = *end_orig;
  
  gtk_text_iter_reorder(&start, &end);
  
  tree = gtk_text_iter_get_btree(&start);

  info = gtk_text_btree_get_tag_info(tree, tag);
  
  start_line = gtk_text_iter_get_line(&start);
  end_line = gtk_text_iter_get_line(&end);

  /* Find all tag toggles in the region; we are going to delete them.
     We need to find them in advance, because
     forward_find_tag_toggle() won't work once we start playing around
     with the tree. */
  stack = iter_stack_new();
  iter = start;
  /* We don't want to delete a toggle that's at the start iterator. */
  gtk_text_iter_forward_char(&iter);
  while (gtk_text_iter_forward_find_tag_toggle(&iter, tag))
    {
      if (gtk_text_iter_compare(&iter, &end) >= 0)
        break;
      else
        iter_stack_push(stack, &iter);
    }

  /* We need to traverse the toggles in order. */
  iter_stack_invert(stack);
  
  /*
   * See whether the tag is present at the start of the range.  If
   * the state doesn't already match what we want then add a toggle
   * there.
   */

  toggled_on = gtk_text_iter_has_tag(&start, tag);
  if ( (add && !toggled_on) ||
       (!add && toggled_on) )
    {
      /* This could create a second toggle at the start position;
         cleanup_line() will remove it if so. */
      seg = toggle_segment_new(info, add);

      prev = gtk_text_line_segment_split(&start);
      if (prev == NULL)
        {
          seg->next = start_line->segments;
          start_line->segments = seg;
        }
      else
        {
          seg->next = prev->next;
          prev->next = seg;
        }
      
      /* cleanup_line adds the new toggle to the node counts. */
#if 0
      printf("added toggle at start\n");
#endif
      /* we should probably call segments_changed, but in theory
         any still-cached segments in the iters we are about to
         use are still valid, since they're in front
         of this spot. */
    }
  
  /*
   *
   * Scan the range of characters and delete any internal tag
   * transitions.  Keep track of what the old state was at the end
   * of the range, and add a toggle there if it's needed.
   *
   */

  cleanupline = start_line;
  while (iter_stack_pop(stack, &iter))
    {
      GtkTextLineSegment *indexable_seg;
      GtkTextLine *line;
      
      line = gtk_text_iter_get_line(&iter);
      seg = gtk_text_iter_get_any_segment(&iter);
      indexable_seg = gtk_text_iter_get_indexable_segment(&iter);

      g_assert(seg != NULL);
      g_assert(indexable_seg != NULL);
      g_assert(seg != indexable_seg);
      
      prev = line->segments;

      /* Find the segment that actually toggles this tag. */
      while (seg != indexable_seg)
        {
          g_assert(seg != NULL);
          g_assert(indexable_seg != NULL);
          g_assert(seg != indexable_seg);
      
          if ( (seg->type == &gtk_text_view_toggle_on_type ||
                seg->type == &gtk_text_view_toggle_off_type) && 
               (seg->body.toggle.info == info) )
            break;
          else
            seg = seg->next;
        }

      g_assert(seg != NULL);
      g_assert(indexable_seg != NULL);
      
      g_assert(seg != indexable_seg); /* If this happens, then
                                         forward_to_tag_toggle was
                                         full of shit. */
      g_assert(seg->body.toggle.info->tag == tag);

      /* If this happens, when previously tagging we didn't merge
         overlapping tags. */
      g_assert( (toggled_on && seg->type == &gtk_text_view_toggle_off_type) ||
                (!toggled_on && seg->type == &gtk_text_view_toggle_on_type) );
      
      toggled_on = !toggled_on;

#if 0
      printf("deleting %s toggle\n",
             seg->type == &gtk_text_view_toggle_on_type ? "on" : "off");
#endif 
      /* Remove toggle segment from the list. */
      if (prev == seg)
        {
          line->segments = seg->next;
        }
      else
        {
          while (prev->next != seg)
            {
              prev = prev->next;
            }
          prev->next = seg->next;
        }

      /* Inform iterators we've hosed them. This actually reflects a
         bit of inefficiency; if you have the same tag toggled on and
         off a lot in a single line, we keep having the rescan from
         the front of the line. Of course we have to do that to get
         "prev" anyway, but here we are doing it an additional
         time. FIXME */
      segments_changed(tree);

      /* Update node counts */
      if (seg->body.toggle.inNodeCounts)
        {
          change_node_toggle_count(line->parent,
                                   info, -1);
          seg->body.toggle.inNodeCounts = FALSE;
        }

      g_free(seg);

      /* We only clean up lines when we're done with them, saves some
         gratuitous line-segment-traversals */
      
      if (cleanupline != line)
        {
          cleanup_line(cleanupline);
          cleanupline = line;
        }
    }

  iter_stack_free(stack);
  
  /* toggled_on now reflects the toggle state _just before_ the
     end iterator. The end iterator could already have a toggle
     on or a toggle off. */
  if ( (add && !toggled_on) ||
       (!add && toggled_on) )
    {
      /* This could create a second toggle at the start position;
         cleanup_line() will remove it if so. */
      
      seg = toggle_segment_new(info, !add);

      prev = gtk_text_line_segment_split(&end);
      if (prev == NULL)
        {
          seg->next = end_line->segments;
          end_line->segments = seg;
        }
      else
        {
          seg->next = prev->next;
          prev->next = seg;
        }
      /* cleanup_line adds the new toggle to the node counts. */
      g_assert(seg->body.toggle.inNodeCounts == FALSE);
#if 0
      printf("added toggle at end\n");
#endif
    }

  /*
   * Cleanup cleanupline and the last line of the range, if
   * these are different.
   */

  cleanup_line(cleanupline);
  if (cleanupline != end_line)
    {
      cleanup_line(end_line);
    }

  segments_changed(tree);
  
  if (gtk_text_view_debug_btree)
    {
      gtk_text_btree_check(tree);
    }
}


/*
 * "Getters"
 */

GtkTextLine*
gtk_text_btree_get_line (GtkTextBTree *tree,
                         gint  line_number,
                         gint *real_line_number)
{
  GtkTextBTreeNode *node;
  GtkTextLine *line;
  int lines_left;
  int line_count;

  line_count = gtk_text_btree_line_count(tree);
  
  if (line_number < 0)
    {
      line_number = line_count;
    }
  else if (line_number > line_count)
    {
      line_number = line_count;
    }

  *real_line_number = line_number;
  
  node = tree->root_node;
  lines_left = line_number;

  /*
   * Work down through levels of the tree until a GtkTextBTreeNode is found at
   * level 0.
   */

  while (node->level != 0)
    {
      for (node = node->children.node;
           node->num_lines <= lines_left;
           node = node->next)
        {
#if 0
          if (node == NULL)
            {
              g_error("gtk_text_btree_find_line ran out of GtkTextBTreeNodes");
            }
#endif
          lines_left -= node->num_lines;
        }
    }

  /*
   * Work through the lines attached to the level-0 GtkTextBTreeNode.
   */

  for (line = node->children.line; lines_left > 0;
       line = line->next)
    {
#if 0
      if (line == NULL)
        {
          g_error("gtk_text_btree_find_line ran out of lines");
        }
#endif
      lines_left -= 1;
    }
  return line;  
}

GtkTextLine*
gtk_text_btree_get_line_at_char(GtkTextBTree      *tree,
                                gint                char_index,
                                gint               *line_start_index,
                                gint               *real_char_index)
{
  GtkTextBTreeNode *node;
  GtkTextLine *line;
  GtkTextLineSegment *seg;
  int chars_left;
  int chars_in_line;
  int bytes_in_line;
  
  node = tree->root_node;

  /* Clamp to valid indexes (-1 is magic for "highest index") */
  if (char_index < 0 || char_index >= node->num_chars)
    {
      char_index = node->num_chars - 1;
    }

  *real_char_index = char_index;
  
  /*
   * Work down through levels of the tree until a GtkTextBTreeNode is found at
   * level 0.
   */

  chars_left = char_index;
  while (node->level != 0)
    {
      for (node = node->children.node;
           chars_left >= node->num_chars;
           node = node->next)
        {
          chars_left -= node->num_chars;

          g_assert(chars_left >= 0);
        }
    }
  
  if (chars_left == 0)
    {
      /* Start of a line */

      *line_start_index = char_index;
      return node->children.line;
    }
  
  /*
   * Work through the lines attached to the level-0 GtkTextBTreeNode.
   */

  chars_in_line = 0;
  bytes_in_line = 0;
  seg = NULL;
  for (line = node->children.line; line != NULL; line = line->next)
    {
      seg = line->segments;
      while (seg != NULL)
        {
          if (chars_in_line + seg->char_count > chars_left)
            goto found; /* found our line/segment */
          
          chars_in_line += seg->char_count;
          
          seg = seg->next;
        }

      chars_left -= chars_in_line;
      
      chars_in_line = 0;
      seg = NULL;
    }

 found:
  
  g_assert(line != NULL); /* hosage, ran out of lines */
  g_assert(seg != NULL);

  *line_start_index = char_index - chars_left;
  return line;
}

GtkTextTag**
gtk_text_btree_get_tags (const GtkTextIter *iter,
                         gint *num_tags)
{
  GtkTextBTreeNode *node;
  GtkTextLine *siblingline;
  GtkTextLineSegment *seg;
  int src, dst, index;
  TagInfo tagInfo;
  GtkTextLine *line;
  GtkTextBTree *tree;
  gint byte_index;

#define NUM_TAG_INFOS 10
  
  line = gtk_text_iter_get_line(iter);
  tree = gtk_text_iter_get_btree(iter);
  byte_index = gtk_text_iter_get_line_byte(iter);

  tagInfo.numTags = 0;
  tagInfo.arraySize = NUM_TAG_INFOS;
  tagInfo.tags = g_new(GtkTextTag*, NUM_TAG_INFOS);
  tagInfo.counts = g_new(int, NUM_TAG_INFOS);

  /*
   * Record tag toggles within the line of indexPtr but preceding
   * indexPtr. Note that if this loop segfaults, your
   * byte_index probably points past the sum of all
   * seg->byte_count */

  for (index = 0, seg = line->segments;
       (index + seg->byte_count) <= byte_index;
       index += seg->byte_count, seg = seg->next)
    {
      if ((seg->type == &gtk_text_view_toggle_on_type)
          || (seg->type == &gtk_text_view_toggle_off_type))
        {
          inc_count(seg->body.toggle.info->tag, 1, &tagInfo);
        }
    }

  /*
   * Record toggles for tags in lines that are predecessors of
   * line but under the same level-0 GtkTextBTreeNode.
   */

  for (siblingline = line->parent->children.line;
       siblingline != line;
       siblingline = siblingline->next)
    {
      for (seg = siblingline->segments; seg != NULL;
           seg = seg->next)
        {
          if ((seg->type == &gtk_text_view_toggle_on_type)
              || (seg->type == &gtk_text_view_toggle_off_type))
            {
              inc_count(seg->body.toggle.info->tag, 1, &tagInfo);
            }
        }
    }

  /*
   * For each GtkTextBTreeNode in the ancestry of this line, record tag
   * toggles for all siblings that precede that GtkTextBTreeNode.
   */

  for (node = line->parent; node->parent != NULL;
       node = node->parent)
    {
      GtkTextBTreeNode *siblingPtr;
      Summary *summary;

      for (siblingPtr = node->parent->children.node; 
           siblingPtr != node; siblingPtr = siblingPtr->next)
        {
          for (summary = siblingPtr->summary; summary != NULL;
               summary = summary->next)
            {
              if (summary->toggle_count & 1)
                {
                  inc_count(summary->info->tag, summary->toggle_count,
                            &tagInfo);
                }
            }
        }
    }

  /*
   * Go through the tag information and squash out all of the tags
   * that have even toggle counts (these tags exist before the point
   * of interest, but not at the desired character itself).
   */

  for (src = 0, dst = 0; src < tagInfo.numTags; src++)
    {
      if (tagInfo.counts[src] & 1)
        {
          g_assert(GTK_IS_TEXT_VIEW_TAG(tagInfo.tags[src]));
          tagInfo.tags[dst] = tagInfo.tags[src];
          dst++;
        }
    }
  
  *num_tags = dst;
  g_free(tagInfo.counts);
  if (dst == 0)
    {
      g_free(tagInfo.tags);
      return NULL;
    }
  return tagInfo.tags;  
}

static void
copy_segment(GString *string,
             gboolean include_hidden,
             gboolean include_nonchars,             
             const GtkTextIter *start,
             const GtkTextIter *end)
{
  GtkTextLineSegment *end_seg;
  GtkTextLineSegment *seg;
  
  if (gtk_text_iter_equal(start, end))
    return;

  seg = gtk_text_iter_get_indexable_segment(start);
  end_seg = gtk_text_iter_get_indexable_segment(end);
  
  if (seg->type == &gtk_text_view_char_type)
    {
      gboolean copy = TRUE;
      gint copy_bytes = 0;
      gint copy_start = 0;

      /* Don't copy if we're elided; segments are elided/not
         as a whole, no need to check each char */
      if (!include_hidden &&
          gtk_text_btree_char_is_invisible(start))
        {
          copy = FALSE;
          /* printf(" <elided>\n"); */
        }

      copy_start = gtk_text_iter_get_segment_byte(start);

      if (seg == end_seg)
        {
          /* End is in the same segment; need to copy fewer bytes. */
          gint end_byte = gtk_text_iter_get_segment_byte(end);

          copy_bytes = end_byte - copy_start;
        }
      else
        copy_bytes = seg->byte_count;

      g_assert(copy_bytes != 0); /* Due to iter equality check at
                                    front of this function. */

      if (copy)
        {
          g_assert((copy_start + copy_bytes) <= seg->byte_count);
          
          g_string_append_len(string,
                              seg->body.chars + copy_start,
                              copy_bytes);
        }
      
      /* printf("  :%s\n", string->str); */
    }
  else if (seg->type == &gtk_text_pixmap_type)
    {
      gboolean copy = TRUE;

      if (!include_nonchars)
        {
          copy = FALSE;
        }
      else if (!include_hidden &&
               gtk_text_btree_char_is_invisible(start))
        {
          copy = FALSE;
        }

      if (copy)
        {
          g_string_append_len(string,
                              gtk_text_unknown_char_utf8,
                              3);

        }
    }
}

gchar*
gtk_text_btree_get_text (const GtkTextIter *start_orig,
                         const GtkTextIter *end_orig,
                         gboolean include_hidden,
                         gboolean include_nonchars)
{
  GtkTextLineSegment *seg;
  GtkTextLineSegment *end_seg;
  GString *retval;
  GtkTextBTree *tree;
  gchar *str;
  GtkTextIter iter;
  GtkTextIter start;
  GtkTextIter end;
  
  g_return_val_if_fail(start_orig != NULL, NULL);
  g_return_val_if_fail(end_orig != NULL, NULL);
  g_return_val_if_fail(gtk_text_iter_get_btree(start_orig) ==
                       gtk_text_iter_get_btree(end_orig), NULL);

  start = *start_orig;
  end = *end_orig;

  gtk_text_iter_reorder(&start, &end);
  
  retval = g_string_new("");
  
  tree = gtk_text_iter_get_btree(&start);
  
  end_seg = gtk_text_iter_get_indexable_segment(&end);
  iter = start;
  seg = gtk_text_iter_get_indexable_segment(&iter);
  while (seg != end_seg)
    {          
      copy_segment(retval, include_hidden, include_nonchars,
                   &iter, &end);

      if (!gtk_text_iter_forward_indexable_segment(&iter))
        g_assert_not_reached(); /* must be able to go forward to
                                   end_seg, if end_seg still exists
                                   and was in front. */

      seg = gtk_text_iter_get_indexable_segment(&iter);
    }

  str = retval->str;
  g_string_free(retval, FALSE);
  return str;
}

gint
gtk_text_btree_line_count (GtkTextBTree *tree)
{
  /* Subtract bogus line at the end; we return a count
     of usable lines. */
  return tree->root_node->num_lines - 1;
}

gint
gtk_text_btree_char_count (GtkTextBTree *tree)
{
  /* Exclude newline in bogus last line */
  return tree->root_node->num_chars - 1;
}

#define LOTSA_TAGS 1000
gboolean
gtk_text_btree_char_is_invisible (const GtkTextIter *iter)
{
  gboolean invisible = FALSE;  /* if nobody says otherwise, it's visible */

  int deftagCnts[LOTSA_TAGS];
  int *tagCnts = deftagCnts;
  GtkTextTag *deftags[LOTSA_TAGS];
  GtkTextTag **tags = deftags;
  int numTags;
  GtkTextBTreeNode *node;
  GtkTextLine *siblingline;
  GtkTextLineSegment *seg;
  GtkTextTag *tag;
  int i, index;
  GtkTextLine *line;
  GtkTextBTree *tree;
  gint byte_index;
  
  line = gtk_text_iter_get_line(iter);
  tree = gtk_text_iter_get_btree(iter);
  byte_index = gtk_text_iter_get_line_byte(iter);
  
  numTags = gtk_text_tag_table_size(tree->table);
    
  /* almost always avoid malloc, so stay out of system calls */
  if (LOTSA_TAGS < numTags)
    {
      tagCnts = g_new(int, numTags);
      tags = g_new(GtkTextTag*, numTags);
    }
 
  for (i=0; i<numTags; i++)
    {
      tagCnts[i] = 0;
    }
  
  /*
   * Record tag toggles within the line of indexPtr but preceding
   * indexPtr.
   */

  for (index = 0, seg = line->segments;
       (index + seg->byte_count) <= byte_index; /* segfault here means invalid index */
       index += seg->byte_count, seg = seg->next)
    {    
      if ((seg->type == &gtk_text_view_toggle_on_type)
          || (seg->type == &gtk_text_view_toggle_off_type))
        {
          tag = seg->body.toggle.info->tag;
          if (tag->elide_set && tag->values->elide)
            {
              tags[tag->priority] = tag;
              tagCnts[tag->priority]++;
            }
        }
    }

  /*
   * Record toggles for tags in lines that are predecessors of
   * line but under the same level-0 GtkTextBTreeNode.
   */

  for (siblingline = line->parent->children.line;
       siblingline != line;
       siblingline = siblingline->next)
    {
      for (seg = siblingline->segments; seg != NULL;
           seg = seg->next)
        {
          if ((seg->type == &gtk_text_view_toggle_on_type)
              || (seg->type == &gtk_text_view_toggle_off_type))
            {
              tag = seg->body.toggle.info->tag;
              if (tag->elide_set && tag->values->elide)
                {
                  tags[tag->priority] = tag;
                  tagCnts[tag->priority]++;
                }
            }
        }
    }

  /*
   * For each GtkTextBTreeNode in the ancestry of this line, record tag toggles
   * for all siblings that precede that GtkTextBTreeNode.
   */

  for (node = line->parent; node->parent != NULL;
       node = node->parent)
    {
      GtkTextBTreeNode *siblingPtr;
      Summary *summary;

      for (siblingPtr = node->parent->children.node; 
           siblingPtr != node; siblingPtr = siblingPtr->next)
        {
          for (summary = siblingPtr->summary; summary != NULL;
               summary = summary->next)
            {
              if (summary->toggle_count & 1)
                {
                  tag = summary->info->tag;
                  if (tag->elide_set && tag->values->elide)
                    {
                      tags[tag->priority] = tag;
                      tagCnts[tag->priority] += summary->toggle_count;
                    }
                }
            }
        }
    }

  /*
   * Now traverse from highest priority to lowest, 
   * take elided value from first odd count (= on)
   */

  for (i = numTags-1; i >=0; i--)
    {
      if (tagCnts[i] & 1)
        {
          /* FIXME not sure this should be if 0 */
#if 0
#ifndef ALWAYS_SHOW_SELECTION
          /* who would make the selection elided? */
          if ((tag == tkxt->seltag)
              && !(tkxt->flags & GOT_FOCUS))
            {
              continue;
            }
#endif
#endif
          invisible = tags[i]->values->elide;
          break;
        }
    }

  if (LOTSA_TAGS < numTags)
    {
      g_free(tagCnts);
      g_free(tags);
    }

  return invisible;  
}


/*
 * Manipulate marks
 */

static void
redisplay_selected_region(GtkTextBTree *tree,
                          GtkTextLineSegment *mark)
{  
  if (mark == tree->insert_mark ||
      mark == tree->selection_bound_mark ||
      mark == NULL)
    {
      /* Selection does not affect the size of the wrapped lines, so
         we don't need to invalidate the lines, just repaint them. We
         used to invalidate, that's why this code is like this. Needs
         cleanup if you're reading this, I just wasn't sure when writing
         it that I'd leave it with just the redraw. */
      BTreeView *view;
      view = tree->views;
      while (view != NULL)
        {
          gtk_text_layout_need_repaint(view->layout, 0, 0,
                                       view->layout->width,
                                       view->layout->height);
          view = view->next;
        }
    }
}

static void
redisplay_mark(GtkTextLineSegment *mark)
{
  GtkTextIter iter;
  GtkTextIter end;
    
  gtk_text_btree_get_iter_at_mark(mark->body.mark.tree,
                                  &iter,
                                  mark);

  end = iter;
  gtk_text_iter_forward_char(&end);

  gtk_text_btree_invalidate_region(mark->body.mark.tree,
                                   &iter, &end);
}

static void
redisplay_mark_if_visible(GtkTextLineSegment *mark)
{
  if (!mark->body.mark.visible)
    return;
  else
    redisplay_mark(mark);
}

static void
ensure_not_off_end(GtkTextBTree *tree,
                   GtkTextLineSegment *mark,
                   GtkTextIter *iter)
{
  if (gtk_text_iter_get_line_number(iter) ==
      gtk_text_btree_line_count(tree))
    gtk_text_iter_backward_char(iter);
}

static GtkTextLineSegment*
real_set_mark(GtkTextBTree *tree,
              const gchar *name,
              gboolean left_gravity,
              const GtkTextIter *where,
              gboolean should_exist,
              gboolean redraw_selections)
{
  GtkTextLineSegment *mark;
  GtkTextIter iter;
  
  g_return_val_if_fail(tree != NULL, NULL);
  g_return_val_if_fail(name != NULL, NULL);
  g_return_val_if_fail(where != NULL, NULL);
  g_return_val_if_fail(gtk_text_iter_get_btree(where) == tree, NULL);
  
  mark = g_hash_table_lookup(tree->mark_table,
                             name);

  if (should_exist && mark == NULL)
    {
      g_warning("No mark `%s' exists!", name);
      return NULL;
    }

  /* OK if !should_exist and it does already exist, in that case
     we just move it. */

  iter = *where;
  
  if (mark != NULL)
    {
      if (redraw_selections)
        redisplay_selected_region(tree, mark);
      
      /*
       * don't let visible marks be after the final newline of the
       *  file.
       */
      
      if (mark->body.mark.visible)
        {
          ensure_not_off_end(tree, mark, &iter);
        }

      /* Redraw the mark's old location. */
      redisplay_mark_if_visible(mark);
      
      /* Unlink mark from its current location.
         This could hose our iterator... */
      gtk_text_btree_unlink_segment(tree, mark,
                                    mark->body.mark.line);
      mark->body.mark.line = gtk_text_iter_get_line(&iter);
      g_assert(mark->body.mark.line == gtk_text_iter_get_line(&iter));

      segments_changed(tree); /* make sure the iterator recomputes its
                                 segment */
    }
  else
    {
      mark = mark_segment_new(tree,
                              left_gravity,
                              name);

      mark->body.mark.line = gtk_text_iter_get_line(&iter);

      g_hash_table_insert(tree->mark_table,
                          mark->body.mark.name,
                          mark);
    }
  
  /* Link mark into new location */
  gtk_text_btree_link_segment(mark, &iter);
  
  /* Invalidate some iterators. */
  segments_changed(tree);
  
  /*
   * update the screen at the mark's new location.
   */

  redisplay_mark_if_visible(mark);

  if (redraw_selections)
    redisplay_selected_region(tree, mark);
  
  return mark;
}


GtkTextLineSegment*
gtk_text_btree_set_mark (GtkTextBTree *tree,
                         const gchar *name,
                         gboolean left_gravity,
                         const GtkTextIter *iter,
                         gboolean should_exist)
{
  return real_set_mark(tree, name, left_gravity, iter, should_exist,
                       TRUE);  
}

/* real_set_mark() is a relic from when we invalidated tree portions
   due to changed selection, now we just queue a draw for the
   onscreen bits since the layout size hasn't changed.  */
void
gtk_text_btree_place_cursor(GtkTextBTree *tree,
                            const GtkTextIter *iter)
{
  /* Redisplay what's selected now */
  /*   redisplay_selected_region(tree, NULL); */
  
  /* Move insert AND selection_bound before we redisplay */
  real_set_mark(tree, "insert", FALSE, iter, TRUE, FALSE);
  real_set_mark(tree, "selection_bound", FALSE, iter, TRUE, FALSE);

  redisplay_selected_region(tree, NULL);
}

void
gtk_text_btree_remove_mark_by_name (GtkTextBTree *tree,
                                    const gchar *name)
{
  GtkTextLineSegment *mark;

  g_return_if_fail(tree != NULL);
  g_return_if_fail(name != NULL);
  
  mark = g_hash_table_lookup(tree->mark_table,
                             name);

  gtk_text_btree_remove_mark(tree, mark);
}

void
gtk_text_btree_remove_mark (GtkTextBTree *tree,
                            GtkTextLineSegment *segment)
{
  g_return_if_fail(segment != NULL);
  g_return_if_fail(segment != tree->selection_bound_mark);
  g_return_if_fail(segment != tree->insert_mark);
  g_return_if_fail(tree != NULL);
  
  gtk_text_btree_unlink_segment(tree, segment, segment->body.mark.line);
  /* FIXME should probably cleanup_line but Tk didn't */
  g_hash_table_remove(tree->mark_table, segment->body.mark.name);
  mark_segment_unref(segment);
  segments_changed(tree);
}

gboolean
gtk_text_btree_mark_is_insert (GtkTextBTree *tree,
                               GtkTextLineSegment *segment)
{
  return segment == tree->insert_mark;
}

gboolean
gtk_text_btree_mark_is_selection_bound (GtkTextBTree *tree,
                                        GtkTextLineSegment *segment)
{
  return segment == tree->selection_bound_mark;
}

GtkTextLineSegment*
gtk_text_btree_get_mark_by_name (GtkTextBTree *tree,
                                 const gchar *name)
{
  g_return_val_if_fail(tree != NULL, NULL);
  g_return_val_if_fail(name != NULL, NULL);

  return g_hash_table_lookup(tree->mark_table, name);
}

void
gtk_text_mark_set_visible (GtkTextMark       *mark,
                           gboolean setting)
{
  GtkTextLineSegment *seg;
  
  g_return_if_fail(mark != NULL);
  
  seg = (GtkTextLineSegment*)mark;

  if (seg->body.mark.visible == setting)
    return;
  else
    {
      seg->body.mark.visible = setting;

      redisplay_mark(seg);
    }
}

GtkTextLine*
gtk_text_btree_first_could_contain_tag (GtkTextBTree *tree,
                                        GtkTextTag *tag)
{
  GtkTextBTreeNode *node;
  GtkTextTagInfo *info;
  
  g_return_val_if_fail(tree != NULL, NULL);

  if (tag != NULL)
    {      
      info = gtk_text_btree_get_existing_tag_info(tree, tag);

      if (info == NULL)
        return NULL;
      
      if (info->tag_root == NULL)
        return NULL;
      
      node = info->tag_root;
      /* We know the tag root has instances of the given
         tag below it */
    }
  else
    {
      info = NULL;
      
      node = tree->root_node;
      if (!gtk_text_btree_node_has_tag(node, tag))
        return NULL; /* no toggles of any tag in this tree */
    }

  g_assert(node != NULL);
  while (node->level > 0)
    {
      g_assert(node != NULL); /* Failure probably means bad tag summaries. */
      node = node->children.node;
      while (node != NULL)
        {
          if (gtk_text_btree_node_has_tag(node, tag))
            goto done;
          node = node->next;
        }
      g_assert(node != NULL);
    }

 done:
  
  g_assert(node != NULL); /* The tag summaries said some node had
                             tag toggles... */
  
  g_assert(node->level == 0);
  
  return node->children.line;
}

GtkTextLine*
gtk_text_btree_last_could_contain_tag (GtkTextBTree *tree,
                                       GtkTextTag *tag)
{
  GtkTextBTreeNode *node;
  GtkTextBTreeNode *last_node;
  GtkTextLine *line;
  GtkTextTagInfo *info;
  
  g_return_val_if_fail(tree != NULL, NULL);
  
  if (tag != NULL)
    {
      info = gtk_text_btree_get_existing_tag_info(tree, tag);
      
      if (info->tag_root == NULL)
        return NULL;
      
      node = info->tag_root;
      /* We know the tag root has instances of the given
         tag below it */
    }
  else
    {
      info = NULL;
      node = tree->root_node;
      if (!gtk_text_btree_node_has_tag(node, tag))
        return NULL; /* no instances of the target tag in this tree */
    }
      
  while (node->level > 0)
    {
      g_assert(node != NULL); /* Failure probably means bad tag summaries. */
      last_node = NULL;
      node = node->children.node;
      while (node != NULL)
        {
          if (gtk_text_btree_node_has_tag(node, tag))
            last_node = node;
          node = node->next;
        }

      node = last_node;
    }

  g_assert(node != NULL); /* The tag summaries said some node had
                             tag toggles... */
  
  g_assert(node->level == 0);
  
  /* Find the last line in this node */
  line = node->children.line;
  while (line->next != NULL)
    line = line->next;

  return line;
}


/*
 * Lines
 */

gint
gtk_text_line_get_number (GtkTextLine *line)
{
  GtkTextLine *line2;
  GtkTextBTreeNode *node, *parent, *node2;
  int index;

  /*
   * First count how many lines precede this one in its level-0
   * GtkTextBTreeNode.
   */

  node = line->parent;
  index = 0;
  for (line2 = node->children.line; line2 != line;
       line2 = line2->next)
    {
      if (line2 == NULL)
        {
          g_error("gtk_text_btree_line_number couldn't find line");
        }
      index += 1;
    }

  /*
   * Now work up through the levels of the tree one at a time,
   * counting how many lines are in GtkTextBTreeNodes preceding the current
   * GtkTextBTreeNode.
   */

  for (parent = node->parent ; parent != NULL;
       node = parent, parent = parent->parent)
    {
      for (node2 = parent->children.node; node2 != node;
           node2 = node2->next)
        {
          if (node2 == NULL)
            {
              g_error("gtk_text_btree_line_number couldn't find GtkTextBTreeNode");
            }
          index += node2->num_lines;
        }
    }
  return index;  
}

static GtkTextLineSegment*
find_toggle_segment_before_char(GtkTextLine *line,
                                gint char_in_line,
                                GtkTextTag *tag)
{
  GtkTextLineSegment *seg;
  GtkTextLineSegment *toggle_seg;
  int index;

  toggle_seg = NULL;
  index = 0;
  seg = line->segments;
  while ( (index + seg->char_count) <= char_in_line )
    {
      if (((seg->type == &gtk_text_view_toggle_on_type)
           || (seg->type == &gtk_text_view_toggle_off_type))
          && (seg->body.toggle.info->tag == tag))
        toggle_seg = seg;

      index += seg->char_count;
      seg = seg->next;
    }

  return toggle_seg;
}

static GtkTextLineSegment*
find_toggle_segment_before_byte(GtkTextLine *line,
                                gint byte_in_line,
                                GtkTextTag *tag)
{
  GtkTextLineSegment *seg;
  GtkTextLineSegment *toggle_seg;
  int index;

  toggle_seg = NULL;
  index = 0;
  seg = line->segments;
  while ( (index + seg->byte_count) <= byte_in_line )
    {
      if (((seg->type == &gtk_text_view_toggle_on_type)
           || (seg->type == &gtk_text_view_toggle_off_type))
          && (seg->body.toggle.info->tag == tag))
        toggle_seg = seg;

      index += seg->byte_count;
      seg = seg->next;
    }

  return toggle_seg;
}

static gboolean
find_toggle_outside_current_line(GtkTextLine *line,
                                 GtkTextBTree *tree,
                                 GtkTextTag *tag)
{
  GtkTextBTreeNode *node;
  GtkTextLine *sibling_line;
  GtkTextLineSegment *seg;
  GtkTextLineSegment *toggle_seg;
  int toggles;
  GtkTextTagInfo *info = NULL;
  
  /*
   * No toggle in this line.  Look for toggles for the tag in lines
   * that are predecessors of line but under the same
   * level-0 GtkTextBTreeNode.
   */
  toggle_seg = NULL;
  sibling_line = line->parent->children.line;
  while (sibling_line != line)
    {
      seg = sibling_line->segments;
      while (seg != NULL)
        {
          if (((seg->type == &gtk_text_view_toggle_on_type)
               || (seg->type == &gtk_text_view_toggle_off_type))
              && (seg->body.toggle.info->tag == tag))
            toggle_seg = seg;

          seg = seg->next;
        }
      
      sibling_line = sibling_line->next;
    }

  if (toggle_seg != NULL)
    return (toggle_seg->type == &gtk_text_view_toggle_on_type);
  
  /*
   * No toggle in this GtkTextBTreeNode.  Scan upwards through the ancestors of
   * this GtkTextBTreeNode, counting the number of toggles of the given tag in
   * siblings that precede that GtkTextBTreeNode.
   */

  info = gtk_text_btree_get_existing_tag_info(tree, tag);

  if (info == NULL)
    return FALSE;
  
  toggles = 0;
  node = line->parent;
  while (node->parent != NULL)
    {
      GtkTextBTreeNode *sibling_node;
      
      sibling_node = node->parent->children.node;
      while (sibling_node != node)
        {
          Summary *summary;
          
          summary = sibling_node->summary;
          while (summary != NULL)
            {
              if (summary->info == info)
                toggles += summary->toggle_count;

              summary = summary->next;
            }

          sibling_node = sibling_node->next;
        }
      
      if (node == info->tag_root)
        break;

      node = node->parent;
    }

  /*
   * An odd number of toggles means that the tag is present at the
   * given point.
   */

  return (toggles & 1) != 0;  
}

/* FIXME this function is far too slow, for no good reason. */
gboolean
gtk_text_line_char_has_tag (GtkTextLine *line,
                            GtkTextBTree *tree,
                            gint char_in_line,
                            GtkTextTag *tag)
{
  GtkTextLineSegment *toggle_seg;

  g_return_val_if_fail(line != NULL, FALSE);
  
  /* 
   * Check for toggles for the tag in the line but before
   * the char.  If there is one, its type indicates whether or
   * not the character is tagged.
   */

  toggle_seg = find_toggle_segment_before_char(line, char_in_line, tag);
  
  if (toggle_seg != NULL)
    return (toggle_seg->type == &gtk_text_view_toggle_on_type);
  else
    return find_toggle_outside_current_line(line, tree, tag);
}

gboolean
gtk_text_line_byte_has_tag (GtkTextLine *line,
                            GtkTextBTree *tree,
                            gint byte_in_line,
                            GtkTextTag *tag)
{
  GtkTextLineSegment *toggle_seg;

  g_return_val_if_fail(line != NULL, FALSE);
  
  /* 
   * Check for toggles for the tag in the line but before
   * the char.  If there is one, its type indicates whether or
   * not the character is tagged.
   */

  toggle_seg = find_toggle_segment_before_byte(line, byte_in_line, tag);
  
  if (toggle_seg != NULL)
    return (toggle_seg->type == &gtk_text_view_toggle_on_type);
  else
    return find_toggle_outside_current_line(line, tree, tag);
}

GtkTextLine*
gtk_text_line_next (GtkTextLine *line)
{
  GtkTextBTreeNode *node;

  if (line->next != NULL)
    return line->next;
  else
    {
      /*
       * This was the last line associated with the particular parent
       * GtkTextBTreeNode.  Search up the tree for the next GtkTextBTreeNode,
       * then search down from that GtkTextBTreeNode to find the first
       * line.
       */

      node = line->parent;
      while (node != NULL && node->next == NULL)
        node = node->parent;      

      if (node == NULL)
        return NULL;

      node = node->next;
      while (node->level > 0)
        {
          node = node->children.node;
        }

      g_assert(node->children.line != line);
      
      return node->children.line;
    }  
}

GtkTextLine*
gtk_text_line_previous (GtkTextLine *line)
{
  GtkTextBTreeNode *node;
  GtkTextBTreeNode *node2;
  GtkTextLine *prev;

  /*
   * Find the line under this GtkTextBTreeNode just before the starting line.
   */
  prev = line->parent->children.line;        /* First line at leaf */
  while (prev != line)
    {
      if (prev->next == line)
        return prev;

      prev = prev->next;

      if (prev == NULL)
        g_error("gtk_text_btree_previous_line ran out of lines");
    }

  /*
   * This was the first line associated with the particular parent
   * GtkTextBTreeNode.  Search up the tree for the previous GtkTextBTreeNode,
   * then search down from that GtkTextBTreeNode to find its last line.
   */
  for (node = line->parent; ; node = node->parent)
    {
      if (node == NULL || node->parent == NULL)
        return NULL;
      else if (node != node->parent->children.node)
        break;
    }
  
  for (node2 = node->parent->children.node; ; 
       node2 = node2->children.node)
    {
      while (node2->next != node)
        node2 = node2->next;

      if (node2->level == 0)
        break;

      node = NULL;
    }

  for (prev = node2->children.line ; ; prev = prev->next)
    {
      if (prev->next == NULL)
        return prev;
    }

  g_assert_not_reached();
  return NULL;
}

void
gtk_text_line_add_data (GtkTextLine *line,
                        GtkTextLineData *data)
{
  g_return_if_fail(line != NULL);
  g_return_if_fail(data != NULL);
  g_return_if_fail(data->view_id != NULL);
  
  if (line->views)
    {
      data->next = line->views;
      line->views = data;
    }
  else
    {
      line->views = data;
    }
}

gpointer
gtk_text_line_remove_data (GtkTextLine *line,
                           gpointer view_id)
{
  GtkTextLineData *prev;
  GtkTextLineData *iter;
  
  g_return_val_if_fail(line != NULL, NULL);
  g_return_val_if_fail(view_id != NULL, NULL);

  prev = NULL;
  iter = line->views;
  while (iter != NULL)
    {
      if (iter->view_id == view_id)
        break;
      prev = iter;
      iter = iter->next;
    }

  if (iter)
    {
      if (prev)
        prev->next = iter->next;
      else
        line->views = iter->next;

      return iter;
    }
  else
    return NULL;
}

gpointer
gtk_text_line_get_data (GtkTextLine *line,
                        gpointer view_id)
{
  GtkTextLineData *iter;
  
  g_return_val_if_fail(line != NULL, NULL);
  g_return_val_if_fail(view_id != NULL, NULL);
  
  iter = line->views;
  while (iter != NULL)
    {
      if (iter->view_id == view_id)
        break;
      iter = iter->next;
    }

  return iter;
}

void
gtk_text_line_invalidate_wrap (GtkTextLine *line,
                               GtkTextLineData *ld)
{
  /* For now this is totally unoptimized. FIXME?
     
     If we kept an "invalid" flag separate from the
     width/height fields (i.e. didn't use -1 as the flag),
     we could probably optimize the case where the width removed
     is less than the max width for the parent node,
     and the case where the height is unchanged when we re-wrap.
  */
  
  g_return_if_fail(ld != NULL);
  
  ld->width = -1;
  ld->height = -1;

  gtk_text_btree_node_invalidate_upward(line->parent, ld->view_id);
}

gint
gtk_text_line_char_count (GtkTextLine *line)
{
  GtkTextLineSegment *seg;
  gint size;
  
  size = 0;
  seg = line->segments;
  while (seg != NULL)
    {
      size += seg->char_count;
      seg = seg->next;
    }
  return size;
}

gint
gtk_text_line_byte_count (GtkTextLine *line)
{
  GtkTextLineSegment *seg;
  gint size;
  
  size = 0;
  seg = line->segments;
  while (seg != NULL)
    {
      size += seg->byte_count;
      seg = seg->next;
    }
  
  return size;
}

gint
gtk_text_line_char_index (GtkTextLine *target_line)
{
  GSList *node_stack = NULL;
  GtkTextBTreeNode *iter;
  GtkTextLine *line;
  gint num_chars;
  
  /* Push all our parent nodes onto a stack */
  iter = target_line->parent;

  g_assert(iter != NULL);
  
  while (iter != NULL)
    {
      node_stack = g_slist_prepend(node_stack, iter);

      iter = iter->parent;
    }

  /* Check that we have the root node on top of the stack. */
  g_assert(node_stack != NULL &&
           node_stack->data != NULL &&
           ((GtkTextBTreeNode*)node_stack->data)->parent == NULL);

  /* Add up chars in all nodes before the nodes in our stack.
   */

  num_chars = 0;
  iter = node_stack->data;
  while (iter != NULL)
    {
      GtkTextBTreeNode *child_iter;
      GtkTextBTreeNode *next_node;
      
      next_node = node_stack->next ?
        node_stack->next->data : NULL;
      node_stack = g_slist_remove(node_stack, node_stack->data);
      
      if (iter->level == 0)
        {
          /* stack should be empty when we're on the last node */
          g_assert(node_stack == NULL);
          break; /* Our children are now lines */
        }

      g_assert(next_node != NULL);
      g_assert(iter != NULL);
      g_assert(next_node->parent == iter);

      /* Add up chars before us in the tree */
      child_iter = iter->children.node;      
      while (child_iter != next_node)
        {
          g_assert(child_iter != NULL);
          
          num_chars += child_iter->num_chars;
          
          child_iter = child_iter->next;
        }

      iter = next_node;
    }

  g_assert(iter != NULL);
  g_assert(iter == target_line->parent);

  /* Since we don't store char counts in lines, only in segments, we
     have to iterate over the lines adding up segment char counts
     until we find our line.  */
  line = iter->children.line;
  while (line != target_line)
    {
      g_assert(line != NULL);

      num_chars += gtk_text_line_char_count(line);

      line = line->next;
    }

  g_assert(line == target_line);

  return num_chars;
}

GtkTextLineSegment*
gtk_text_line_byte_to_segment (GtkTextLine *line,
                               gint byte_offset,
                               gint *seg_offset)
{
  GtkTextLineSegment *seg;
  int offset;

  g_return_val_if_fail(line != NULL, NULL);
  
  offset = byte_offset;
  seg = line->segments;

  while (offset >= seg->byte_count)
    {
      g_assert(seg != NULL); /* means an invalid byte index */
      offset -= seg->byte_count;
      seg = seg->next;
    }

  if (seg_offset)
    *seg_offset = offset;

  return seg;
}

GtkTextLineSegment*
gtk_text_line_char_to_segment (GtkTextLine *line,
                               gint char_offset,
                               gint *seg_offset)
{
  GtkTextLineSegment *seg;
  int offset;

  g_return_val_if_fail(line != NULL, NULL);
  
  offset = char_offset;
  seg = line->segments;

  while (offset >= seg->char_count)
    {
      g_assert(seg != NULL); /* means an invalid char index */
      offset -= seg->char_count;
      seg = seg->next;
    }

  if (seg_offset)
    *seg_offset = offset;

  return seg;  
}

GtkTextLineSegment*
gtk_text_line_byte_to_any_segment (GtkTextLine *line,
                                   gint byte_offset,
                                   gint *seg_offset)
{
  GtkTextLineSegment *seg;
  int offset;

  g_return_val_if_fail(line != NULL, NULL);
  
  offset = byte_offset;
  seg = line->segments;

  while (offset > 0 && offset >= seg->byte_count)
    {
      g_assert(seg != NULL); /* means an invalid byte index */
      offset -= seg->byte_count;
      seg = seg->next;
    }

  if (seg_offset)
    *seg_offset = offset;

  return seg;
}

GtkTextLineSegment*
gtk_text_line_char_to_any_segment (GtkTextLine *line,
                                   gint char_offset,
                                   gint *seg_offset)
{
  GtkTextLineSegment *seg;
  int offset;

  g_return_val_if_fail(line != NULL, NULL);
  
  offset = char_offset;
  seg = line->segments;

  while (offset > 0 && offset >= seg->char_count)
    {
      g_assert(seg != NULL); /* means an invalid byte index */
      offset -= seg->char_count;
      seg = seg->next;
    }

  if (seg_offset)
    *seg_offset = offset;

  return seg;
}

gint
gtk_text_line_byte_to_char (GtkTextLine *line,
                            gint byte_offset)
{
  gint char_offset;
  GtkTextLineSegment *seg;

  g_return_val_if_fail(line != NULL, 0);
  g_return_val_if_fail(byte_offset >= 0, 0);
  
  char_offset = 0;
  seg = line->segments;
  while (byte_offset >= seg->byte_count) /* while (we need to go farther than
                                            the next segment) */
    {
      g_assert(seg != NULL); /* our byte_index was bogus if this happens */
      
      byte_offset -= seg->byte_count;
      char_offset += seg->char_count;
      
      seg = seg->next;
    }

  g_assert(seg != NULL);

  /* Now byte_offset is the offset into the current segment,
     and char_offset is the start of the current segment.
     Optimize the case where no chars use > 1 byte */
  if (seg->byte_count == seg->char_count)
    return char_offset + byte_offset;
  else
    {
      if (seg->type == &gtk_text_view_char_type)
        return char_offset + gtk_text_view_num_utf_chars(seg->body.chars, byte_offset);
      else
        {
          g_assert(seg->char_count == 1);
          g_assert(byte_offset == 0);

          return char_offset;
        }
    }
}

gint
gtk_text_line_char_to_byte (GtkTextLine *line,
                            gint char_offset)
{
  g_warning("FIXME not implemented");
}


/* FIXME sync with char_locate (or figure out a clean
   way to merge the two functions) */
void
gtk_text_line_byte_locate (GtkTextLine *line,
                           gint byte_offset,
                           GtkTextLineSegment **segment,
                           GtkTextLineSegment **any_segment,
                           gint *seg_byte_offset,
                           gint *line_byte_offset)
{
  GtkTextLineSegment *seg;
  GtkTextLineSegment *after_prev_indexable;
  GtkTextLineSegment *after_last_indexable;
  GtkTextLineSegment *last_indexable;
  gint offset;
  gint bytes_in_line;
  
  g_return_if_fail(line != NULL);

  if (byte_offset < 0)
    {    
      /* -1 means end of line; we here assume no line is
         longer than 1 bazillion bytes, of course we assumed
         that anyway since we'd wrap around... */

      byte_offset = G_MAXINT;
    }
  
  *segment = NULL;
  *any_segment = NULL;
  bytes_in_line = 0;
  
  offset = byte_offset;

  last_indexable = NULL;
  after_last_indexable = line->segments;
  after_prev_indexable = line->segments;
  seg = line->segments;

  /* The loop ends when we're inside a segment;
     last_indexable refers to the last segment
     we passed entirely. */
  while (seg && offset >= seg->byte_count)
    {          
      if (seg->char_count > 0)
        {          
          offset -= seg->byte_count;
          bytes_in_line += seg->byte_count;
          last_indexable = seg;
          after_prev_indexable = after_last_indexable;
          after_last_indexable = last_indexable->next;
        }
      
      seg = seg->next;
    }

  if (seg == NULL)
    {
      /* We went off the end of the line */
      *segment = last_indexable;
      *any_segment = after_prev_indexable;
      /* subtracting 1 is OK, we know it's a newline at the end. */
      offset = (*segment)->byte_count - 1;
      bytes_in_line -= (*segment)->byte_count;
    }
  else
    {
      *segment = seg;
      if (after_last_indexable != NULL)
        *any_segment = after_last_indexable;
      else
        *any_segment = *segment;
    }
  
  /* Override any_segment if we're in the middle of a segment. */
  if (offset > 0)
    *any_segment = *segment;

  *seg_byte_offset = offset;

  g_assert(*segment != NULL);
  g_assert(*any_segment != NULL);
  g_assert(*seg_byte_offset < (*segment)->byte_count);
  
  *line_byte_offset = bytes_in_line + *seg_byte_offset;
}

/* FIXME sync with byte_locate (or figure out a clean
   way to merge the two functions) */
void
gtk_text_line_char_locate     (GtkTextLine     *line,
                               gint              char_offset,
                               GtkTextLineSegment **segment,
                               GtkTextLineSegment **any_segment,
                               gint             *seg_char_offset,
                               gint             *line_char_offset)
{
  GtkTextLineSegment *seg;
  GtkTextLineSegment *after_prev_indexable;
  GtkTextLineSegment *after_last_indexable;
  GtkTextLineSegment *last_indexable;
  gint offset;
  gint chars_in_line;
  
  g_return_if_fail(line != NULL);

  if (char_offset < 0)
    {    
      /* -1 means end of line; we here assume no line is
         longer than 1 bazillion chars, of course we assumed
         that anyway since we'd wrap around... */

      char_offset = G_MAXINT;
    }
  
  *segment = NULL;
  *any_segment = NULL;
  chars_in_line = 0;
  
  offset = char_offset;

  last_indexable = NULL;
  after_last_indexable = line->segments;
  after_prev_indexable = line->segments;
  seg = line->segments;

  /* The loop ends when we're inside a segment;
     last_indexable refers to the last segment
     we passed entirely. */
  while (seg && offset >= seg->char_count)
    {          
      if (seg->char_count > 0)
        {          
          offset -= seg->char_count;
          chars_in_line += seg->char_count;
          last_indexable = seg;
          after_prev_indexable = after_last_indexable;
          after_last_indexable = last_indexable->next;
        }
      
      seg = seg->next;
    }

  if (seg == NULL)
    {
      /* We went off the end of the line */
      *segment = last_indexable;
      *any_segment = after_prev_indexable;
      /* subtracting 1 is OK, we know it's a newline at the end. */
      offset = (*segment)->char_count - 1;
      chars_in_line -= (*segment)->char_count;
    }
  else
    {
      *segment = seg;
      if (after_last_indexable != NULL)
        *any_segment = after_last_indexable;
      else
        *any_segment = *segment;
    }
  
  /* Override any_segment if we're in the middle of a segment. */
  if (offset > 0)
    *any_segment = *segment;

  *seg_char_offset = offset;

  g_assert(*segment != NULL);
  g_assert(*any_segment != NULL);
  g_assert(*seg_char_offset < (*segment)->char_count);
  
  *line_char_offset = chars_in_line + *seg_char_offset;
}

void
gtk_text_line_byte_to_char_offsets(GtkTextLine *line,
                                   gint byte_offset,
                                   gint *line_char_offset,
                                   gint *seg_char_offset)
{
  GtkTextLineSegment *seg;
  int offset;

  g_return_if_fail(line != NULL);
  g_return_if_fail(byte_offset >= 0);

  *line_char_offset = 0;
  
  offset = byte_offset;
  seg = line->segments;

  while (offset >= seg->byte_count)
    {
      offset -= seg->byte_count;
      *line_char_offset += seg->char_count;
      seg = seg->next;
      g_assert(seg != NULL); /* means an invalid char offset */
    }

  g_assert(seg->char_count > 0); /* indexable. */
  
  /* offset is now the number of bytes into the current segment we
     want to go. Count chars into the current segment. */

  if (seg->type == &gtk_text_view_char_type)
    {
      *seg_char_offset = gtk_text_view_num_utf_chars(seg->body.chars,
                                                     offset);

      g_assert(*seg_char_offset < seg->char_count);
      
      *line_char_offset += *seg_char_offset;
    }
  else
    {
      g_assert(offset == 0);
      *seg_char_offset = 0;
    }
}

void
gtk_text_line_char_to_byte_offsets(GtkTextLine *line,
                                   gint char_offset,
                                   gint *line_byte_offset,
                                   gint *seg_byte_offset)
{
  GtkTextLineSegment *seg;
  int offset;

  g_return_if_fail(line != NULL);
  g_return_if_fail(char_offset >= 0);

  *line_byte_offset = 0;
  
  offset = char_offset;
  seg = line->segments;

  while (offset >= seg->char_count)
    {
      offset -= seg->char_count;
      *line_byte_offset += seg->byte_count;
      seg = seg->next;
      g_assert(seg != NULL); /* means an invalid char offset */
    }

  g_assert(seg->char_count > 0); /* indexable. */
  
  /* offset is now the number of chars into the current segment we
     want to go. Count bytes into the current segment. */

  if (seg->type == &gtk_text_view_char_type)
    {
      *seg_byte_offset = 0;
      while (offset > 0)
        {
          GtkTextUniChar ch;
          gint bytes;
          
          bytes = gtk_text_utf_to_unichar(seg->body.chars + *seg_byte_offset,
                                          &ch);
          *seg_byte_offset += bytes;
          offset -= 1;
        }

      g_assert(*seg_byte_offset < seg->byte_count);
      
      *line_byte_offset += *seg_byte_offset;
    }
  else
    {
      g_assert(offset == 0);
      *seg_byte_offset = 0;
    }
}

/* remember that tag == NULL means "any tag" */
GtkTextLine*
gtk_text_line_next_could_contain_tag(GtkTextLine *line,
                                     GtkTextBTree *tree,
                                     GtkTextTag  *tag)
{
  GtkTextBTreeNode *node;
  GtkTextTagInfo *info;
  
  g_return_val_if_fail(line != NULL, NULL);
  
  /* Our tag summaries only have node precision, not line
     precision. This means that if any line under a node could contain a
     tag, then any of the others could also contain a tag.

     In the future we could have some mechanism to keep track of how
     many toggles we've found under a node so far, since we have a
     count of toggles under the node. But for now I'm going with KISS.
  */

  /* return same-node line, if any. */
  if (line->next)
    return line->next;

  info = gtk_text_btree_get_existing_tag_info(tree, tag);
  if (info == NULL)
    return NULL;
  
  /* We need to go up out of this node, and on to the next one with
     toggles for the target tag. */
  
  node = line->parent;

  while (TRUE)
    {
      /* If there's no next node in our list, go up in the tree.
         If we reach the tag root or run out of tree, return. */
      while (node->next == NULL)
        {
          if (tag && node == info->tag_root)
            return NULL; /* No more tag toggle summaries above this node. */
          else if (node->parent == NULL)
            return NULL; /* Nowhere else to go */

          node = node->parent;
        }

      g_assert(node != NULL);
      node = node->next;
      g_assert(node != NULL);

      if (gtk_text_btree_node_has_tag(node, tag))
        break;
    }

  g_assert(node != NULL);
  
  /* We have to find the first sub-node of this node that contains
     the target tag. */

  while (node->level > 0)
    {
      g_assert(node != NULL); /* If this fails, it likely means an
                                 incorrect tag summary led us on a
                                 wild goose chase down this branch of
                                 the tree. */
      node = node->children.node;
      while (node != NULL)
        {
          if (gtk_text_btree_node_has_tag(node, tag))
            goto done;
          node = node->next;
        }
      g_assert(node != NULL);
    }

 done:
  g_assert(node != NULL);
  g_assert(node->level == 0);
  
  return node->children.line;
}
      
GtkTextLine*
gtk_text_line_previous_could_contain_tag(GtkTextLine *line,
                                         GtkTextBTree *tree,
                                         GtkTextTag  *tag)
{
  g_warning("FIXME");

}

/*
 * Non-public function implementations */

static void
summary_list_destroy(Summary *summary)
{
  Summary *next;
  while (summary != NULL)
    {
      next = summary->next;
      g_free(summary);
      summary = next;
    }
}

static GtkTextLine*
get_last_line(GtkTextBTree *tree)
{
  gint n_lines;
  GtkTextLine *line;
  gint real_line;
  
  n_lines = gtk_text_btree_line_count(tree);

  g_assert(n_lines >= 1); /* num_lines doesn't return bogus last line. */

  line = gtk_text_btree_get_line(tree, n_lines, &real_line);
  
  return line;
}

/*
 * Lines
 */

static GtkTextLine*
gtk_text_line_new(void)
{
  GtkTextLine *line;

  line = g_new0(GtkTextLine, 1);

  return line;
}

static void
gtk_text_line_destroy(GtkTextBTree *tree, GtkTextLine *line)
{
  GtkTextLineData *ld;
  GtkTextLineData *next;
  
  g_return_if_fail(line != NULL);

  ld = line->views;
  while (ld != NULL)
    {
      BTreeView *view;
      
      view = gtk_text_btree_get_view(tree, ld->view_id);

      g_assert(view != NULL);

      next = ld->next;
      (* view->line_data_destructor) (ld);
      
      ld = next;
    }
  
  g_free(line);
}

static void
gtk_text_line_set_parent(GtkTextLine *line,
                         GtkTextBTreeNode *node)
{
  if (line->parent == node)
    return;
  line->parent = node;
  gtk_text_btree_node_invalidate_upward(node, NULL);
}

static void
cleanup_line(GtkTextLine *line)
{
  GtkTextLineSegment *seg, **prev_p;
  gboolean changed;

  /*
   * Make a pass over all of the segments in the line, giving each
   * a chance to clean itself up.  This could potentially change
   * the structure of the line, e.g. by merging two segments
   * together or having two segments cancel themselves;  if so,
   * then repeat the whole process again, since the first structure
   * change might make other structure changes possible.  Repeat
   * until eventually there are no changes.
   */

  changed = TRUE;
  while (changed)
    {
      changed = FALSE;
      for (prev_p = &line->segments, seg = *prev_p;
           seg != NULL;
           prev_p = &(*prev_p)->next, seg = *prev_p)
        {
          if (seg->type->cleanupFunc != NULL)
            {
              *prev_p = (*seg->type->cleanupFunc)(seg, line);
              if (seg != *prev_p)
                changed = TRUE;
            }
        }
    }
}

/*
 * Nodes
 */

static NodeData*
node_data_new(gpointer view_id)
{
  NodeData *nd;

  nd = g_new(NodeData, 1);

  nd->view_id = view_id;
  nd->next = NULL;
  nd->width = -1;
  nd->height = -1;

  return nd;
}

static void
node_data_destroy(NodeData *nd)
{
  
  g_free(nd);
}

static void
node_data_list_destroy(NodeData *nd)
{
  NodeData *iter;
  NodeData *next;
  
  iter = nd;
  while (iter != NULL)
    {
      next = iter->next;
      node_data_destroy(iter);
      iter = next;
    }
}

static NodeData*
node_data_find(NodeData *nd, gpointer view_id)
{
  while (nd != NULL)
    {
      if (nd->view_id == view_id)
        break;
      nd = nd->next;
    }
  return nd;
}

static GtkTextBTreeNode*
gtk_text_btree_node_new(void)
{
  GtkTextBTreeNode *node;

  node = g_new(GtkTextBTreeNode, 1);

  node->node_data = NULL;
  
  return node;
}

static void
gtk_text_btree_node_adjust_toggle_count (GtkTextBTreeNode  *node,
                                         GtkTextTagInfo  *info,
                                         gint adjust)
{
  Summary *summary;
  
  summary = node->summary;
  while (summary != NULL)
    {
      if (summary->info == info)
        {
          summary->toggle_count += adjust;
          break;
        }

      summary = summary->next;
    }

  if (summary == NULL)
    {
      /* didn't find a summary for our tag. */
      g_return_if_fail(adjust > 0);
      summary = g_new(Summary, 1);
      summary->info = info;
      summary->toggle_count = adjust;
      summary->next = node->summary;
      node->summary = summary;
    }
}

static gboolean
gtk_text_btree_node_has_tag(GtkTextBTreeNode *node, GtkTextTag *tag)
{
  Summary *summary;
  
  summary = node->summary;
  while (summary != NULL)
    {
      if (tag == NULL ||
          summary->info->tag == tag)
        return TRUE;
      
      summary = summary->next;
    }
  return FALSE;
}

/* Add node and all children to the damage region. */
static void
gtk_text_btree_node_invalidate_downward(GtkTextBTreeNode *node)
{
  NodeData *nd;

  nd = node->node_data;
  while (nd != NULL)
    {
      nd->width = -1;
      nd->height = -1;

      nd = nd->next;
    }
  
  if (node->level == 0)
    {
      GtkTextLine *line;

      line = node->children.line;
      while (line != NULL)
        {
          GtkTextLineData *ld;

          ld = line->views;
          while (ld != NULL)
            {
              ld->width = -1;
              ld->height = -1;
              ld = ld->next;
            }
          
          line = line->next;
        }
    }
  else
    {
      GtkTextBTreeNode *child;
      
      child = node->children.node;
      
      while (child != NULL)
        {
          gtk_text_btree_node_invalidate_downward(child);
          
          child = child->next;
        }
    }
}

static void
gtk_text_btree_node_invalidate_upward(GtkTextBTreeNode *node, gpointer view_id)
{
  GtkTextBTreeNode *iter;
  
  iter = node;
  while (iter != NULL)
    {
      NodeData *nd;

      if (view_id)
        {
          nd = node_data_find(iter->node_data, view_id);
          
          if (nd != NULL)
            {
              if (nd->height < 0)
                break; /* Once a node is -1, we know its parents are as well. */
              
              nd->width = -1;
              nd->height = -1;
            }
        }
      else
        {
          gboolean should_continue = FALSE;
          
          nd = iter->node_data;
          while (nd != NULL)
            {
              if (nd->width > 0 ||
                  nd->height > 0)
                should_continue = TRUE;

              nd->width = -1;
              nd->height = -1;

              nd = nd->next;
            }

          if (!should_continue)
            break; /* This node was totally invalidated, so are its
                      parents */
        }
      
      iter = iter->parent;
    }
}

static void
gtk_text_btree_node_remove_view(BTreeView *view, GtkTextBTreeNode *node, gpointer view_id)
{
  if (node->level == 0)
    {
      GtkTextLine *line;

      line = node->children.line;
      while (line != NULL)
        {
          GtkTextLineData *ld;

          ld = gtk_text_line_remove_data(line, view_id);

          if (ld)
            {
              if (view->line_data_destructor)
                (*view->line_data_destructor)(ld);
            }
          
          line = line->next;
        }
    }
  else
    {
      GtkTextBTreeNode *child;
      
      child = node->children.node;
      
      while (child != NULL)
        {
          /* recurse */
          gtk_text_btree_node_remove_view(view, child, view_id);
          
          child = child->next;
        }
    }

  gtk_text_btree_node_remove_data(node, view_id);
}

static void
gtk_text_btree_node_destroy(GtkTextBTree *tree, GtkTextBTreeNode *node)
{
  if (node->level == 0)
    {
      GtkTextLine *line;
      GtkTextLineSegment *seg;
      
      while (node->children.line != NULL)
        {
          line = node->children.line;
          node->children.line = line->next;
          while (line->segments != NULL)
            {
              seg = line->segments;
              line->segments = seg->next;
              (*seg->type->deleteFunc)(seg, line, 1);
            }
          gtk_text_line_destroy(tree, line);
        }
    }
  else
    {
      GtkTextBTreeNode *childPtr;
      
      while (node->children.node != NULL)
        {
          childPtr = node->children.node;
          node->children.node = childPtr->next;
          gtk_text_btree_node_destroy(tree, childPtr);
        }
    }
  summary_list_destroy(node->summary);
  node_data_list_destroy(node->node_data);
  g_free(node);
}

static NodeData*
gtk_text_btree_node_ensure_data(GtkTextBTreeNode *node, gpointer view_id)
{
  NodeData *nd;
  
  nd = node->node_data;
  while (nd != NULL)
    {
      if (nd->view_id == view_id)
        break;

      nd = nd->next;
    }

  if (nd == NULL)
    {
      nd = node_data_new(view_id);

      if (node->node_data)
        nd->next = node->node_data;

      node->node_data = nd;
    }

  return nd;
}

static void
gtk_text_btree_node_remove_data(GtkTextBTreeNode *node, gpointer view_id)
{
  NodeData *nd;
  NodeData *prev;

  prev = NULL;
  nd = node->node_data;
  while (nd != NULL)
    {
      if (nd->view_id == view_id)
        break;

      nd = nd->next;
    }
  
  if (nd == NULL)
    return;

  if (prev != NULL)
    prev->next = nd->next;

  if (node->node_data == nd)
    node->node_data = nd->next;
  
  nd->next = NULL;
  
  node_data_destroy(nd);
}

static GtkTextLineData*
ensure_line_data(GtkTextLine *line, GtkTextBTree *tree, BTreeView *view)
{
  GtkTextLineData *ld;
  
  ld = gtk_text_line_get_data(line, view->view_id);
  
  if (ld == NULL ||
      ld->height < 0 ||
      ld->width < 0)
    {
      /* This function should return the passed-in line data,
         OR remove the existing line data from the line, and
         return a NEW line data after adding it to the line.
         That is, invariant after calling the callback is that
         there should be exactly one line data for this view
         stored on the btree line. */
      ld = gtk_text_layout_wrap(view->layout, line, ld);
    }

  return ld;
}

/* This is the function that results in wrapping lines
   and repairing the damage region of the tree. */
static void
gtk_text_btree_node_get_size(GtkTextBTreeNode *node, gpointer view_id,
                             GtkTextBTree *tree, BTreeView *view,
                             gint *width, gint *height, GtkTextLine *last_line)
{
  NodeData *nd;
  
  g_return_if_fail(width != NULL);
  g_return_if_fail(height != NULL);

  if (last_line == NULL)
    {
      last_line = get_last_line(tree);
    }
  
  nd = gtk_text_btree_node_ensure_data(node, view_id);
  
  if (nd->width >= 0 &&
      nd->height >= 0)
    {
      *width = nd->width;
      *height = nd->height;
      return;
    }

  if (view == NULL)
    {
      view = gtk_text_btree_get_view(tree, view_id);
      g_assert(view != NULL);
    }
  
  if (node->level == 0)
    {
      GtkTextLine *line;
      
      nd->width = 0;
      nd->height = 0;
      
      line = node->children.line;
      while (line != NULL && line != last_line)
        {
          GtkTextLineData *ld;

          ld = ensure_line_data(line, tree, view);

          g_assert(ld != NULL);
          g_assert(ld->height >= 0);
          
          nd->width = MAX(ld->width, nd->width);
          nd->height += ld->height;
          
          line = line->next;
        }
    }
  else
    {
      GtkTextBTreeNode *child;
      
      nd->width = 0;
      nd->height = 0;

      child = node->children.node;
      while (child != NULL)
        {
          gint child_width;
          gint child_height;

          gtk_text_btree_node_get_size(child, view_id, tree, view,
                                       &child_width, &child_height,
                                       last_line);

          nd->width = MAX(child_width, nd->width);
          nd->height += child_height;
          
          child = child->next;
        }
    }

  *width = nd->width;
  *height = nd->height;
}

/*
 * BTree
 */

static BTreeView*
gtk_text_btree_get_view(GtkTextBTree *tree, gpointer view_id)
{
  BTreeView *view;

  view = tree->views;
  while (view != NULL)
    {
      if (view->view_id == view_id)
        break;
      view = view->next;
    }
  
  return view;
}

static void
get_tree_bounds(GtkTextBTree *tree,
                GtkTextIter *start,
                GtkTextIter *end)
{
  gtk_text_btree_get_iter_at_line_char(tree, start, 0, 0);
  gtk_text_btree_get_last_iter(tree, end);
}
  
static void
tag_changed_cb(GtkTextTagTable *table,
               GtkTextTag *tag,
               gboolean size_changed,
               GtkTextBTree *tree)
{
  if (size_changed)
    {
      /* We need to queue a redisplay on all regions that are tagged with
         this tag. */

      GtkTextIter start;
      GtkTextIter end;

      if (gtk_text_btree_get_iter_at_first_toggle(tree, &start, tag))
        {
          /* Must be a last toggle if there was a first one. */
          gtk_text_btree_get_iter_at_last_toggle(tree, &end, tag);
          gtk_text_btree_invalidate_region(tree,
                                           &start, &end);
               
        }
    }
  else
    {
      BTreeView *view;
      
      view = tree->views;
      
      while (view != NULL)
        {
          gtk_text_layout_need_repaint(view->layout, 0, 0,
                                       view->layout->width,
                                       view->layout->height);

          view = view->next;
        }
    }
}

static void
tag_removed_cb(GtkTextTagTable *table,
               GtkTextTag *tag,
               GtkTextBTree *tree)
{
  /* Remove the tag from the tree */

  GtkTextIter start;
  GtkTextIter end;

  get_tree_bounds(tree, &start, &end);

  gtk_text_btree_tag(&start, &end, tag, FALSE);
}


/* Rebalance the out-of-whack node "node" */
static void
gtk_text_btree_rebalance(GtkTextBTree *tree,
                         GtkTextBTreeNode *node)
{
  /*
   * Loop over the entire ancestral chain of the GtkTextBTreeNode, working
   * up through the tree one GtkTextBTreeNode at a time until the root
   * GtkTextBTreeNode has been processed.
   */

  while (node != NULL)
    {
      GtkTextBTreeNode *new_node, *child;
      GtkTextLine *line;
      int i;

      /*
       * Check to see if the GtkTextBTreeNode has too many children.  If it does,
       * then split off all but the first MIN_CHILDREN into a separate
       * GtkTextBTreeNode following the original one.  Then repeat until the
       * GtkTextBTreeNode has a decent size.
       */

      if (node->num_children > MAX_CHILDREN)
        {
          while (1)
            {
              /*
               * If the GtkTextBTreeNode being split is the root
               * GtkTextBTreeNode, then make a new root GtkTextBTreeNode above
               * it first.
               */
    
              if (node->parent == NULL)
                {
                  new_node = gtk_text_btree_node_new();
                  new_node->parent = NULL;
                  new_node->next = NULL;
                  new_node->summary = NULL;
                  new_node->level = node->level + 1;
                  new_node->children.node = node;
                  new_node->num_children = 1;
                  new_node->num_lines = node->num_lines;
                  new_node->num_chars = node->num_chars;
                  recompute_node_counts(new_node);
                  tree->root_node = new_node;
                }
              new_node = gtk_text_btree_node_new();
              new_node->parent = node->parent;
              new_node->next = node->next;
              node->next = new_node;
              new_node->summary = NULL;
              new_node->level = node->level;
              new_node->num_children = node->num_children - MIN_CHILDREN;
              if (node->level == 0)
                {
                  for (i = MIN_CHILDREN-1,
                         line = node->children.line;
                       i > 0; i--, line = line->next)
                    {
                      /* Empty loop body. */
                    }
                  new_node->children.line = line->next;
                  line->next = NULL;
                }
              else
                {
                  for (i = MIN_CHILDREN-1,
                         child = node->children.node;
                       i > 0; i--, child = child->next)
                    {
                      /* Empty loop body. */
                    }
                  new_node->children.node = child->next;
                  child->next = NULL;
                }
              recompute_node_counts(node);
              node->parent->num_children++;
              node = new_node;
              if (node->num_children <= MAX_CHILDREN)
                {
                  recompute_node_counts(node);
                  break;
                }
            }
        }

      while (node->num_children < MIN_CHILDREN)
        {
          GtkTextBTreeNode *other;
          GtkTextBTreeNode *halfwaynode = NULL; /* Initialization needed only */
          GtkTextLine *halfwayline = NULL; /* to prevent cc warnings. */
          int total_children, first_children, i;

          /*
           * Too few children for this GtkTextBTreeNode.  If this is the root then,
           * it's OK for it to have less than MIN_CHILDREN children
           * as long as it's got at least two.  If it has only one
           * (and isn't at level 0), then chop the root GtkTextBTreeNode out of
           * the tree and use its child as the new root.
           */

          if (node->parent == NULL)
            {
              if ((node->num_children == 1) && (node->level > 0))
                {
                  tree->root_node = node->children.node;
                  tree->root_node->parent = NULL;
                  summary_list_destroy(node->summary);
                  g_free(node);
                }
              return;
            }

          /*
           * Not the root.  Make sure that there are siblings to
           * balance with.
           */

          if (node->parent->num_children < 2)
            {
              gtk_text_btree_rebalance(tree, node->parent);
              continue;
            }

          /*
           * Find a sibling neighbor to borrow from, and arrange for
           * node to be the earlier of the pair.
           */

          if (node->next == NULL)
            {
              for (other = node->parent->children.node;
                   other->next != node;
                   other = other->next)
                {
                  /* Empty loop body. */
                }
              node = other;
            }
          other = node->next;

          /*
           * We're going to either merge the two siblings together
           * into one GtkTextBTreeNode or redivide the children among them to
           * balance their loads.  As preparation, join their two
           * child lists into a single list and remember the half-way
           * point in the list.
           */

          total_children = node->num_children + other->num_children;
          first_children = total_children/2;
          if (node->children.node == NULL)
            {
              node->children = other->children;
              other->children.node = NULL;
              other->children.line = NULL;
            }
          if (node->level == 0)
            {
              GtkTextLine *line;

              for (line = node->children.line, i = 1;
                   line->next != NULL;
                   line = line->next, i++)
                {
                  if (i == first_children)
                    {
                      halfwayline = line;
                    }
                }
              line->next = other->children.line;
              while (i <= first_children)
                {
                  halfwayline = line;
                  line = line->next;
                  i++;
                }
            }
          else
            {
              GtkTextBTreeNode *child;

              for (child = node->children.node, i = 1;
                   child->next != NULL;
                   child = child->next, i++)
                {
                  if (i <= first_children)
                    {
                      if (i == first_children)
                        {
                          halfwaynode = child;
                        }
                    }
                }
              child->next = other->children.node;
              while (i <= first_children)
                {
                  halfwaynode = child;
                  child = child->next;
                  i++;
                }
            }

          /*
           * If the two siblings can simply be merged together, do it.
           */

          if (total_children <= MAX_CHILDREN)
            {
              recompute_node_counts(node);
              node->next = other->next;
              node->parent->num_children--;
              summary_list_destroy(other->summary);
              g_free(other);
              continue;
            }

          /*
           * The siblings can't be merged, so just divide their
           * children evenly between them.
           */

          if (node->level == 0)
            {
              other->children.line = halfwayline->next;
              halfwayline->next = NULL;
            }
          else
            {
              other->children.node = halfwaynode->next;
              halfwaynode->next = NULL;
            }

          recompute_node_counts(node);
          recompute_node_counts(other);
        }

      node = node->parent;
    }
}

static void
post_insert_fixup(GtkTextBTree *tree,
                  GtkTextLine *line,
                  gint line_count_delta,
                  gint char_count_delta)

{
  GtkTextBTreeNode *node;
  
  /*
   * Increment the line counts in all the parent GtkTextBTreeNodes of the insertion
   * point, then rebalance the tree if necessary.
   */

  for (node = line->parent ; node != NULL;
       node = node->parent)
    {
      node->num_lines += line_count_delta;
      node->num_chars += char_count_delta;
    }
  node = line->parent;
  node->num_children += line_count_delta;

  if (node->num_children > MAX_CHILDREN)
    {
      gtk_text_btree_rebalance(tree, node);
    }

  if (gtk_text_view_debug_btree)
    {
      gtk_text_btree_check(tree);
    }
}

static GtkTextTagInfo*
gtk_text_btree_get_existing_tag_info(GtkTextBTree *tree,
                                     GtkTextTag   *tag)
{
  GtkTextTagInfo *info;
  GSList *list;


  list = tree->tag_infos;
  while (list != NULL)
    {
      info = list->data;
      if (info->tag == tag)
        return info;

      list = g_slist_next(list);
    }

  return NULL;
}

static GtkTextTagInfo*
gtk_text_btree_get_tag_info(GtkTextBTree *tree,
                            GtkTextTag   *tag)
{
  GtkTextTagInfo *info;

  info = gtk_text_btree_get_existing_tag_info(tree, tag);

  if (info == NULL)
    {
      /* didn't find it, create. */

      info = g_new(GtkTextTagInfo, 1);

      info->tag = tag;
      gtk_object_ref(GTK_OBJECT(tag));
      info->tag_root = NULL;
      info->toggle_count = 0;

      tree->tag_infos = g_slist_prepend(tree->tag_infos, info);
    }
  
  return info;
}

static void
gtk_text_btree_remove_tag_info(GtkTextBTree *tree,
                               GtkTextTagInfo *target_info)
{
  GtkTextTagInfo *info;
  GSList *list;
  GSList *prev;

  prev = NULL;
  list = tree->tag_infos;
  while (list != NULL)
    {
      info = list->data;
      if (info == target_info)
        {
          if (prev != NULL)
            {
              prev->next = list->next;
            }
          else
            {
              tree->tag_infos = list->next;
            }
          list->next = NULL;
          g_slist_free(list);
          
          gtk_object_unref(GTK_OBJECT(info->tag));

          g_free(info);
          return;
        }

      list = g_slist_next(list);
    }
  
  g_assert_not_reached();
  return;
}

static void
recompute_level_zero_tag_counts(GtkTextBTreeNode *node)
{
  GtkTextLine *line;
  GtkTextLineSegment *seg;

  g_assert(node->level == 0);
  
  line = node->children.line;
  while (line != NULL)
    {
      node->num_children++;
      node->num_lines++;

      if (line->parent != node)
        gtk_text_line_set_parent(line, node);

      seg = line->segments;
      while (seg != NULL)
        {

          node->num_chars += seg->char_count;
        
          if (((seg->type != &gtk_text_view_toggle_on_type)
               && (seg->type != &gtk_text_view_toggle_off_type))
              || !(seg->body.toggle.inNodeCounts))
            {
              ; /* nothing */
            }
          else
            {
              GtkTextTagInfo *info;
              
              info = seg->body.toggle.info;

              gtk_text_btree_node_adjust_toggle_count(node, info, 1);
            }
              
          seg = seg->next;
        }

      line = line->next;
    }
}

static void
recompute_level_nonzero_tag_counts(GtkTextBTreeNode *node)
{
  Summary *summary;
  GtkTextBTreeNode *child;

  g_assert(node->level > 0);
  
  child = node->children.node;
  while (child != NULL)
    {
      node->num_children += 1;
      node->num_lines += child->num_lines;
      node->num_chars += child->num_chars;

      if (child->parent != node)
        {
          child->parent = node;
          gtk_text_btree_node_invalidate_upward(node, NULL);
        }

      summary = child->summary;
      while (summary != NULL)
        {
          gtk_text_btree_node_adjust_toggle_count(node,
                                                  summary->info,
                                                  summary->toggle_count);
          
          summary = summary->next;
        }

      child = child->next;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * recompute_node_counts --
 *
 *      This procedure is called to recompute all the counts in a GtkTextBTreeNode
 *      (tags, child information, etc.) by scanning the information in
 *      its descendants.  This procedure is called during rebalancing
 *      when a GtkTextBTreeNode's child structure has changed.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      The tag counts for node are modified to reflect its current
 *      child structure, as are its num_children, num_lines, num_chars fields.
 *      Also, all of the childrens' parent fields are made to point
 *      to node.
 *
 *----------------------------------------------------------------------
 */

static void
recompute_node_counts(GtkTextBTreeNode *node)
{
  Summary *summary, *summary2;

  /*
   * Zero out all the existing counts for the GtkTextBTreeNode, but don't delete
   * the existing Summary records (most of them will probably be reused).
   */

  summary = node->summary;
  while (summary != NULL)
    {
      summary->toggle_count = 0;
      summary = summary->next;
    }

  node->num_children = 0;
  node->num_lines = 0;
  node->num_chars = 0;
  
  /*
   * Scan through the children, adding the childrens' tag counts into
   * the GtkTextBTreeNode's tag counts and adding new Summary structures if
   * necessary.
   */

  if (node->level == 0)
    recompute_level_zero_tag_counts(node);
  else
    recompute_level_nonzero_tag_counts(node);

  /*
   * Scan through the GtkTextBTreeNode's tag records again and delete any Summary
   * records that still have a zero count, or that have all the toggles.
   * The GtkTextBTreeNode with the children that account for all the tags toggles
   * have no summary information, and they become the tag_root for the tag.
   */

  summary2 = NULL;
  for (summary = node->summary; summary != NULL; )
    {
      if (summary->toggle_count > 0 && 
          summary->toggle_count < summary->info->toggle_count)
        {
          if (node->level == summary->info->tag_root->level)
            {
              /*
               * The tag's root GtkTextBTreeNode split and some toggles left.
               * The tag root must move up a level.
               */
              summary->info->tag_root = node->parent;
            }
          summary2 = summary;
          summary = summary->next;
          continue;
        }
      if (summary->toggle_count == summary->info->toggle_count)
        {
          /*
           * A GtkTextBTreeNode merge has collected all the toggles under
           * one GtkTextBTreeNode.  Push the root down to this level.
           */
          summary->info->tag_root = node;
        }
      if (summary2 != NULL)
        {
          summary2->next = summary->next;
          g_free(summary);
          summary = summary2->next;
        }
      else
        {
          node->summary = summary->next;
          g_free(summary);
          summary = node->summary;
        }
    }
}

void
change_node_toggle_count(GtkTextBTreeNode *node,
                         GtkTextTagInfo *info,
                         gint delta) /* may be negative */
{
  Summary *summary, *prevPtr;
  GtkTextBTreeNode *node2Ptr;
  int rootLevel;                        /* Level of original tag root */

  info->toggle_count += delta;

  if (info->tag_root == (GtkTextBTreeNode *) NULL)
    {
      info->tag_root = node;
      return;
    }

  /*
   * Note the level of the existing root for the tag so we can detect
   * if it needs to be moved because of the toggle count change.
   */

  rootLevel = info->tag_root->level;

  /*
   * Iterate over the GtkTextBTreeNode and its ancestors up to the tag root, adjusting
   * summary counts at each GtkTextBTreeNode and moving the tag's root upwards if
   * necessary.
   */

  for ( ; node != info->tag_root; node = node->parent)
    {
      /*
       * See if there's already an entry for this tag for this GtkTextBTreeNode.  If so,
       * perhaps all we have to do is adjust its count.
       */
    
      for (prevPtr = NULL, summary = node->summary;
           summary != NULL;
           prevPtr = summary, summary = summary->next)
        {
          if (summary->info == info)
            {
              break;
            }
        }
      if (summary != NULL)
        {
          summary->toggle_count += delta;
          if (summary->toggle_count > 0 &&
              summary->toggle_count < info->toggle_count)
            {
              continue;
            }
          if (summary->toggle_count != 0)
            {
              /*
               * Should never find a GtkTextBTreeNode with max toggle count at this
               * point (there shouldn't have been a summary entry in the
               * first place).
               */

              g_error("change_node_toggle_count: bad toggle count (%d) max (%d)",
                      summary->toggle_count, info->toggle_count);
            }
    
          /*
           * Zero toggle count;  must remove this tag from the list.
           */

          if (prevPtr == NULL)
            {
              node->summary = summary->next;
            }
          else
            {
              prevPtr->next = summary->next;
            }
          g_free(summary);
        }
      else
        {
          /*
           * This tag isn't currently in the summary information list.
           */
    
          if (rootLevel == node->level)
            {
    
              /*
               * The old tag root is at the same level in the tree as this
               * GtkTextBTreeNode, but it isn't at this GtkTextBTreeNode.  Move the tag root up
               * a level, in the hopes that it will now cover this GtkTextBTreeNode
               * as well as the old root (if not, we'll move it up again
               * the next time through the loop).  To push it up one level
               * we copy the original toggle count into the summary
               * information at the old root and change the root to its
               * parent GtkTextBTreeNode.
               */
    
              GtkTextBTreeNode *rootnode = info->tag_root;
              summary = (Summary *) g_malloc(sizeof(Summary));
              summary->info = info;
              summary->toggle_count = info->toggle_count - delta;
              summary->next = rootnode->summary;
              rootnode->summary = summary;
              rootnode = rootnode->parent;
              rootLevel = rootnode->level;
              info->tag_root = rootnode;
            }
          summary = (Summary *) g_malloc(sizeof(Summary));
          summary->info = info;
          summary->toggle_count = delta;
          summary->next = node->summary;
          node->summary = summary;
        }
    }

  /*
   * If we've decremented the toggle count, then it may be necessary
   * to push the tag root down one or more levels.
   */

  if (delta >= 0)
    {
      return;
    }
  if (info->toggle_count == 0)
    {
      info->tag_root = (GtkTextBTreeNode *) NULL;
      return;
    }
  node = info->tag_root;
  while (node->level > 0)
    {
      /*
       * See if a single child GtkTextBTreeNode accounts for all of the tag's
       * toggles.  If so, push the root down one level.
       */

      for (node2Ptr = node->children.node;
           node2Ptr != (GtkTextBTreeNode *)NULL ;
           node2Ptr = node2Ptr->next)
        {
          for (prevPtr = NULL, summary = node2Ptr->summary;
               summary != NULL;
               prevPtr = summary, summary = summary->next)
            {
              if (summary->info == info)
                {
                  break;
                }
            }
          if (summary == NULL)
            {
              continue;
            }
          if (summary->toggle_count != info->toggle_count)
            {
              /*
               * No GtkTextBTreeNode has all toggles, so the root is still valid.
               */

              return;
            }

          /*
           * This GtkTextBTreeNode has all the toggles, so push down the root.
           */

          if (prevPtr == NULL)
            {
              node2Ptr->summary = summary->next;
            }
          else
            {
              prevPtr->next = summary->next;
            }
          g_free(summary);
          info->tag_root = node2Ptr;
          break;
        }
      node = info->tag_root;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * inc_count --
 *
 *      This is a utility procedure used by gtk_text_btree_get_tags.  It
 *      increments the count for a particular tag, adding a new
 *      entry for that tag if there wasn't one previously.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      The information at *tagInfoPtr may be modified, and the arrays
 *      may be reallocated to make them larger.
 *
 *----------------------------------------------------------------------
 */

static void
inc_count(GtkTextTag *tag, int inc, TagInfo *tagInfoPtr)
{
  GtkTextTag **tag_p;
  int count;

  for (tag_p = tagInfoPtr->tags, count = tagInfoPtr->numTags;
       count > 0; tag_p++, count--)
    {
      if (*tag_p == tag)
        {
          tagInfoPtr->counts[tagInfoPtr->numTags-count] += inc;
          return;
        }
    }

  /*
   * There isn't currently an entry for this tag, so we have to
   * make a new one.  If the arrays are full, then enlarge the
   * arrays first.
   */

  if (tagInfoPtr->numTags == tagInfoPtr->arraySize)
    {
      GtkTextTag **newTags;
      int *newCounts, newSize;

      newSize = 2*tagInfoPtr->arraySize;
      newTags = (GtkTextTag **) g_malloc((unsigned)
                                         (newSize*sizeof(GtkTextTag *)));
      memcpy((void *) newTags, (void *) tagInfoPtr->tags,
             tagInfoPtr->arraySize  *sizeof(GtkTextTag *));
      g_free((char *) tagInfoPtr->tags);
      tagInfoPtr->tags = newTags;
      newCounts = (int *) g_malloc((unsigned) (newSize*sizeof(int)));
      memcpy((void *) newCounts, (void *) tagInfoPtr->counts,
             tagInfoPtr->arraySize  *sizeof(int));
      g_free((char *) tagInfoPtr->counts);
      tagInfoPtr->counts = newCounts;
      tagInfoPtr->arraySize = newSize;
    }

  tagInfoPtr->tags[tagInfoPtr->numTags] = tag;
  tagInfoPtr->counts[tagInfoPtr->numTags] = inc;
  tagInfoPtr->numTags++;
}

static void
gtk_text_btree_link_segment(GtkTextLineSegment *seg,
                            const GtkTextIter *iter)
{
  GtkTextLineSegment *prev;
  GtkTextLine *line;
  GtkTextBTree *tree;
  
  line = gtk_text_iter_get_line(iter);
  tree = gtk_text_iter_get_btree(iter);
  
  prev = gtk_text_line_segment_split(iter);
  if (prev == NULL)
    {
      seg->next = line->segments;
      line->segments = seg;
    }
  else
    {
      seg->next = prev->next;
      prev->next = seg;
    }
  cleanup_line(line);
  segments_changed(tree);
  
  if (gtk_text_view_debug_btree)
    {
      gtk_text_btree_check(tree);
    }
}

static void
gtk_text_btree_unlink_segment(GtkTextBTree *tree,
                              GtkTextLineSegment *seg,
                              GtkTextLine *line)
{
  GtkTextLineSegment *prev;

  if (line->segments == seg)
    {
      line->segments = seg->next;
    }
  else
    {
      for (prev = line->segments; prev->next != seg;
           prev = prev->next)
        {
          /* Empty loop body. */
        }
      prev->next = seg->next;
    }
  cleanup_line(line);
  segments_changed(tree);
}

/*
 * This is here because it requires BTree internals, it logically
 * belongs in gtktextsegment.c
 */


/*
 *--------------------------------------------------------------
 *
 * toggle_segment_check_func --
 *
 *      This procedure is invoked to perform consistency checks
 *      on toggle segments.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      If a consistency problem is found the procedure g_errors.
 *
 *--------------------------------------------------------------
 */

void
toggle_segment_check_func(GtkTextLineSegment *segPtr,
                          GtkTextLine *line)
{
  Summary *summary;
  int needSummary;

  if (segPtr->byte_count != 0)
    {
      g_error("toggle_segment_check_func: segment had non-zero size");
    }
  if (!segPtr->body.toggle.inNodeCounts)
    {
      g_error("toggle_segment_check_func: toggle counts not updated in GtkTextBTreeNodes");
    }
  needSummary = (segPtr->body.toggle.info->tag_root != line->parent);
  for (summary = line->parent->summary; ;
       summary = summary->next)
    {
      if (summary == NULL)
        {
          if (needSummary)
            {
              g_error("toggle_segment_check_func: tag not present in GtkTextBTreeNode");
            }
          else
            {
              break;
            }
        }
      if (summary->info == segPtr->body.toggle.info)
        {
          if (!needSummary)
            {
              g_error("toggle_segment_check_func: tag present in root GtkTextBTreeNode summary");
            }
          break;
        }
    }
}

/*
 * Debug
 */


static void
gtk_text_btree_node_check_consistency(GtkTextBTreeNode *node)
{
  GtkTextBTreeNode *childnode;
  Summary *summary, *summary2;
  GtkTextLine *line;
  GtkTextLineSegment *segPtr;
  int num_children, num_lines, num_chars, toggle_count, min_children;
  GtkTextLineData *ld;
  NodeData *nd;
  
  if (node->parent != NULL)
    {
      min_children = MIN_CHILDREN;
    }
  else if (node->level > 0)
    {
      min_children = 2;
    }
  else  {
    min_children = 1;
  }
  if ((node->num_children < min_children)
      || (node->num_children > MAX_CHILDREN))
    {
      g_error("gtk_text_btree_node_check_consistency: bad child count (%d)",
              node->num_children);
    }

  nd = node->node_data;
  while (nd != NULL)
    {
      /* Make sure we don't segv doing this. */
      nd = nd->next;
    }
  
  num_children = 0;
  num_lines = 0;
  num_chars = 0;
  if (node->level == 0)
    {
      for (line = node->children.line; line != NULL;
           line = line->next)
        {
          if (line->parent != node)
            {
              g_error("gtk_text_btree_node_check_consistency: line doesn't point to parent");
            }
          if (line->segments == NULL)
            {
              g_error("gtk_text_btree_node_check_consistency: line has no segments");
            }

          ld = line->views;
          while (ld != NULL)
            {
              /* Just ensuring we don't segv while doing this loop */
          
              ld = ld->next;
            }
      
          for (segPtr = line->segments; segPtr != NULL; segPtr = segPtr->next)
            {
              if (segPtr->type->checkFunc != NULL)
                {
                  (*segPtr->type->checkFunc)(segPtr, line);
                }
              if ((segPtr->byte_count == 0) && (!segPtr->type->leftGravity)
                  && (segPtr->next != NULL)
                  && (segPtr->next->byte_count == 0)
                  && (segPtr->next->type->leftGravity))
                {
                  g_error("gtk_text_btree_node_check_consistency: wrong segment order for gravity");
                }
              if ((segPtr->next == NULL)
                  && (segPtr->type != &gtk_text_view_char_type))
                {
                  g_error("gtk_text_btree_node_check_consistency: line ended with wrong type");
                }
          
              num_chars += segPtr->char_count;
            }
      
          num_children++;
          num_lines++;
        }
    }
  else
    {
      for (childnode = node->children.node; childnode != NULL;
           childnode = childnode->next)
        {
          if (childnode->parent != node)
            {
              g_error("gtk_text_btree_node_check_consistency: GtkTextBTreeNode doesn't point to parent");
            }
          if (childnode->level != (node->level-1))
            {
              g_error("gtk_text_btree_node_check_consistency: level mismatch (%d %d)",
                      node->level, childnode->level);
            }
          gtk_text_btree_node_check_consistency(childnode);
          for (summary = childnode->summary; summary != NULL;
               summary = summary->next)
            {
              for (summary2 = node->summary; ;
                   summary2 = summary2->next)
                {
                  if (summary2 == NULL)
                    {
                      if (summary->info->tag_root == node)
                        {
                          break;
                        }
                      g_error("gtk_text_btree_node_check_consistency: GtkTextBTreeNode tag \"%s\" not %s",
                              summary->info->tag->name,
                              "present in parent summaries");
                    }
                  if (summary->info == summary2->info)
                    {
                      break;
                    }
                }
            }
          num_children++;
          num_lines += childnode->num_lines;
          num_chars += childnode->num_chars;
        }
    }
  if (num_children != node->num_children)
    {
      g_error("gtk_text_btree_node_check_consistency: mismatch in num_children (%d %d)",
              num_children, node->num_children);
    }
  if (num_lines != node->num_lines)
    {
      g_error("gtk_text_btree_node_check_consistency: mismatch in num_lines (%d %d)",
              num_lines, node->num_lines);
    }
  if (num_chars != node->num_chars)
    {
      g_error("%s: mismatch in num_chars (%d %d)",
              __FUNCTION__, num_chars, node->num_chars);
    }

  for (summary = node->summary; summary != NULL;
       summary = summary->next)
    {
      if (summary->info->toggle_count == summary->toggle_count)
        {
          g_error("gtk_text_btree_node_check_consistency: found unpruned root for \"%s\"",
                  summary->info->tag->name);
        }
      toggle_count = 0;
      if (node->level == 0)
        {
          for (line = node->children.line; line != NULL;
               line = line->next)
            {
              for (segPtr = line->segments; segPtr != NULL;
                   segPtr = segPtr->next)
                {
                  if ((segPtr->type != &gtk_text_view_toggle_on_type)
                      && (segPtr->type != &gtk_text_view_toggle_off_type))
                    {
                      continue;
                    }
                  if (segPtr->body.toggle.info == summary->info)
                    {
                      toggle_count ++;
                    }
                }
            }
        }
      else
        {
          for (childnode = node->children.node;
               childnode != NULL;
               childnode = childnode->next)
            {
              for (summary2 = childnode->summary;
                   summary2 != NULL;
                   summary2 = summary2->next)
                {
                  if (summary2->info == summary->info)
                    {
                      toggle_count += summary2->toggle_count;
                    }
                }
            }
        }
      if (toggle_count != summary->toggle_count)
        {
          g_error("gtk_text_btree_node_check_consistency: mismatch in toggle_count (%d %d)",
                  toggle_count, summary->toggle_count);
        }
      for (summary2 = summary->next; summary2 != NULL;
           summary2 = summary2->next)
        {
          if (summary2->info == summary->info)
            {
              g_error("gtk_text_btree_node_check_consistency: duplicated GtkTextBTreeNode tag: %s",
                      summary->info->tag->name);
            }
        }
    }
}

static void
listify_foreach(gpointer key, gpointer value, gpointer user_data)
{
  GSList** listp = user_data;

  *listp = g_slist_prepend(*listp, value);
}

static GSList*
list_of_tags(GtkTextTagTable *table)
{
  GSList *list = NULL;
  
  gtk_text_tag_table_foreach(table, listify_foreach, &list);
  
  return list;
}

void
gtk_text_btree_check (GtkTextBTree *tree)
{
  Summary *summary;
  GtkTextBTreeNode *node;
  GtkTextLine *line;
  GtkTextLineSegment *seg;
  GtkTextTag *tag;
  GSList *taglist = NULL;
  int count;
  GtkTextTagInfo *info;
  
  /*
   * Make sure that the tag toggle counts and the tag root pointers are OK.
   */
  for (taglist = list_of_tags(tree->table);
       taglist != NULL ; taglist = taglist->next)
    {
      tag = taglist->data;
      info = gtk_text_btree_get_existing_tag_info(tree, tag);
      if (info != NULL)
        {
          node = info->tag_root;
          if (node == NULL)
            {
              if (info->toggle_count != 0)
                {
                  g_error("gtk_text_btree_check found \"%s\" with toggles (%d) but no root",
                          tag->name, info->toggle_count);
                }
              continue;         /* no ranges for the tag */
            }
          else if (info->toggle_count == 0)
            {
              g_error("gtk_text_btree_check found root for \"%s\" with no toggles",
                      tag->name);
            }
          else if (info->toggle_count & 1)
            {
              g_error("gtk_text_btree_check found odd toggle count for \"%s\" (%d)",
                      tag->name, info->toggle_count);
            }
          for (summary = node->summary; summary != NULL;
               summary = summary->next)
            {
              if (summary->info->tag == tag)
                {
                  g_error("gtk_text_btree_check found root GtkTextBTreeNode with summary info");
                }
            }
          count = 0;
          if (node->level > 0)
            {
              for (node = node->children.node ; node != NULL ;
                   node = node->next)
                {
                  for (summary = node->summary; summary != NULL;
                       summary = summary->next)
                    {
                      if (summary->info->tag == tag)
                        {
                          count += summary->toggle_count;
                        }
                    }
                }
            }
          else
            {
              for (line = node->children.line ; line != NULL ;
                   line = line->next)
                {
                  for (seg = line->segments; seg != NULL;
                       seg = seg->next)
                    {
                      if ((seg->type == &gtk_text_view_toggle_on_type ||
                           seg->type == &gtk_text_view_toggle_off_type) &&
                          seg->body.toggle.info->tag == tag)
                        {
                          count++;
                        }
                    }
                }
            }
          if (count != info->toggle_count)
            {
              g_error("gtk_text_btree_check toggle_count (%d) wrong for \"%s\" should be (%d)",
                      info->toggle_count, tag->name, count);
            }
        }
    }

  g_slist_free(taglist);
  taglist = NULL;

  /*
   * Call a recursive procedure to do the main body of checks.
   */

  node = tree->root_node;
  gtk_text_btree_node_check_consistency(tree->root_node);

  /*
   * Make sure that there are at least two lines in the text and
   * that the last line has no characters except a newline.
   */

  if (node->num_lines < 2)
    {
      g_error("gtk_text_btree_check: less than 2 lines in tree");
    }
  if (node->num_chars < 2)
    {
      g_error("%s: less than 2 chars in tree", __FUNCTION__);
    }
  while (node->level > 0)
    {
      node = node->children.node;
      while (node->next != NULL)
        {
          node = node->next;
        }
    }
  line = node->children.line;
  while (line->next != NULL)
    {
      line = line->next;
    }
  seg = line->segments;
  while ((seg->type == &gtk_text_view_toggle_off_type)
         || (seg->type == &gtk_text_view_right_mark_type)
         || (seg->type == &gtk_text_view_left_mark_type))
    {
      /*
       * It's OK to toggle a tag off in the last line, but
       * not to start a new range.  It's also OK to have marks
       * in the last line.
       */

      seg = seg->next;
    }
  if (seg->type != &gtk_text_view_char_type)
    {
      g_error("gtk_text_btree_check: last line has bogus segment type");
    }
  if (seg->next != NULL)
    {
      g_error("gtk_text_btree_check: last line has too many segments");
    }
  if (seg->byte_count != 1)
    {
      g_error("gtk_text_btree_check: last line has wrong # characters: %d",
              seg->byte_count);
    }
  if ((seg->body.chars[0] != '\n') || (seg->body.chars[1] != 0))
    {
      g_error("gtk_text_btree_check: last line had bad value: %s",
              seg->body.chars);
    }
}

void
gtk_text_btree_spew (GtkTextBTree *tree)
{
  g_warning("FIXME copy spew back in from old stuff");

}
