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

#ifndef __GSEQUENCE_H__
#define __GSEQUENCE_H__


/* Warning
 *
 * Do not use this API. It is here, internal to gtk+, for 2.6 only.
 * In 2.8 the plan is to add a similar data structure to GLib. 
 */

typedef struct _GtkSequence      GtkSequence;
typedef struct _GtkSequenceNode *GtkSequencePtr;

/* GtkSequence */
GtkSequence *  _gtk_sequence_new              (GDestroyNotify     data_destroy);
void           _gtk_sequence_free             (GtkSequence       *seq);
void           _gtk_sequence_sort             (GtkSequence       *seq,
					       GCompareDataFunc   cmp_func,
					       gpointer           cmp_data);
void           _gtk_sequence_append           (GtkSequence       *seq,
					       gpointer           data);
void           _gtk_sequence_prepend          (GtkSequence       *seq,
					       gpointer           data);
GtkSequencePtr _gtk_sequence_insert           (GtkSequencePtr     ptr,
					       gpointer           data);
void           _gtk_sequence_remove           (GtkSequencePtr     ptr);
void	       _gtk_sequence_move             (GtkSequencePtr     ptr,
					       GtkSequencePtr	  pos);
void	       _gtk_sequence_swap             (GtkSequencePtr     a,
					       GtkSequencePtr	  b);
GtkSequencePtr _gtk_sequence_insert_sorted    (GtkSequence       *seq,
					       gpointer           data,
					       GCompareDataFunc   cmp_func,
					       gpointer           cmp_data);
void	       _gtk_sequence_set	      (GtkSequencePtr	  ptr,
					       gpointer		  data);
void           _gtk_sequence_insert_sequence  (GtkSequencePtr     ptr,
					       GtkSequence       *other_seq);
void           _gtk_sequence_concatenate      (GtkSequence       *seq1,
					       GtkSequence       *seq);
void           _gtk_sequence_remove_range     (GtkSequencePtr     begin,
					       GtkSequencePtr     end,
					       GtkSequence      **removed);
gint           _gtk_sequence_get_length       (GtkSequence       *seq);
GtkSequencePtr _gtk_sequence_get_end_ptr      (GtkSequence       *seq);
GtkSequencePtr _gtk_sequence_get_begin_ptr    (GtkSequence       *seq);
GtkSequencePtr _gtk_sequence_get_ptr_at_pos   (GtkSequence       *seq,
					       gint               pos);
void           _gtk_sequence_sort_changed     (GtkSequencePtr     ptr,
					       GCompareDataFunc   cmp_func,
					       gpointer           cmp_data);
void	       _gtk_sequence_foreach	      (GtkSequence	 *seq,
					       GFunc		  func,
					       gpointer		  data);

/* GtkSequencePtr */
gboolean       _gtk_sequence_ptr_is_end       (GtkSequencePtr     ptr);
gboolean       _gtk_sequence_ptr_is_begin     (GtkSequencePtr     ptr);
gint           _gtk_sequence_ptr_get_position (GtkSequencePtr     ptr);
GtkSequencePtr _gtk_sequence_ptr_next         (GtkSequencePtr     ptr);
GtkSequencePtr _gtk_sequence_ptr_prev         (GtkSequencePtr     ptr);
GtkSequencePtr _gtk_sequence_ptr_move         (GtkSequencePtr     ptr,
					       guint              leap);
gpointer       _gtk_sequence_ptr_get_data     (GtkSequencePtr     ptr);
GtkSequence   *_gtk_sequence_ptr_get_sequence (GtkSequencePtr	  ptr);

/* search */

/* return TRUE if you want to be called again with two
 * smaller segments
 */
typedef gboolean (* GtkSequenceSearchFunc) (GtkSequencePtr begin,
					     GtkSequencePtr end,
					     gpointer     data);

void         _gtk_sequence_search             (GtkSequence               *seq,
					       GtkSequenceSearchFunc      f,
					       gpointer                 data);

/* debug */
gint         _gtk_sequence_calc_tree_height   (GtkSequence               *seq);

#endif /* __GSEQUENCE_H__ */
