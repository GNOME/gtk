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

/* This file implements most of the work of the ICCCM selection protocol.
 * The code was written after an intensive study of the equivalent part
 * of John Ousterhout’s Tk toolkit, and does many things in much the 
 * same way.
 *
 * The one thing in the ICCCM that isn’t fully supported here (or in Tk)
 * is side effects targets. For these to be handled properly, MULTIPLE
 * targets need to be done in the order specified. This cannot be
 * guaranteed with the way we do things, since if we are doing INCR
 * transfers, the order will depend on the timing of the requestor.
 *
 * By Owen Taylor <owt1@cornell.edu>	      8/16/97
 */

/* Terminology note: when not otherwise specified, the term "incr" below
 * refers to the _sending_ part of the INCR protocol. The receiving
 * portion is referred to just as “retrieval”. (Terminology borrowed
 * from Tk, because there is no good opposite to “retrieval” in English.
 * “send” can’t be made into a noun gracefully and we’re already using
 * “emission” for something else ....)
 */

/* The MOTIF entry widget seems to ask for the TARGETS target, then
   (regardless of the reply) ask for the TEXT target. It's slightly
   possible though that it somehow thinks we are responding negatively
   to the TARGETS request, though I don't really think so ... */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

/**
 * SECTION:gtkselection
 * @Title: Selections
 * @Short_description: Functions for handling inter-process communication
 *     via selections
 * @See_also: #GtkWidget - Much of the operation of selections happens via
 *     signals for #GtkWidget. In particular, if you are using the functions
 *     in this section, you may need to pay attention to
 *     #GtkWidget::selection-get, #GtkWidget::selection-received and
 *     #GtkWidget::selection-clear-event signals
 *
 * The selection mechanism provides the basis for different types
 * of communication between processes. In particular, drag and drop and
 * #GtkClipboard work via selections. You will very seldom or
 * never need to use most of the functions in this section directly;
 * #GtkClipboard provides a nicer interface to the same functionality.
 *
 * Some of the datatypes defined this section are used in
 * the #GtkClipboard and drag-and-drop API’s as well. The
 * #GtkTargetList object represents
 * lists of data types that are supported when sending or
 * receiving data. The #GtkSelectionData object is used to
 * store a chunk of data along with the data type and other
 * associated information.
 */

#include "config.h"

#include "gdkcontentformats.h"
#include "gdkcontentformatsprivate.h"

#include "gdkproperty.h"

struct _GtkTargetList
{
  /*< private >*/
  GList *list;
  guint ref_count;
};


/**
 * gtk_target_list_new:
 * @targets: (array length=ntargets) (allow-none): Pointer to an array
 *   of char *
 * @ntargets: number of entries in @targets.
 * 
 * Creates a new #GtkTargetList from an array of mime types.
 * 
 * Returns: (transfer full): the new #GtkTargetList.
 **/
GtkTargetList *
gtk_target_list_new (const char **targets,
		     guint        ntargets)
{
  GtkTargetList *result = g_slice_new (GtkTargetList);
  result->list = NULL;
  result->ref_count = 1;

  if (targets)
    gtk_target_list_add_table (result, targets, ntargets);
  
  return result;
}

/**
 * gtk_target_list_ref:
 * @list:  a #GtkTargetList
 * 
 * Increases the reference count of a #GtkTargetList by one.
 *
 * Returns: the passed in #GtkTargetList.
 **/
GtkTargetList *
gtk_target_list_ref (GtkTargetList *list)
{
  g_return_val_if_fail (list != NULL, NULL);

  list->ref_count++;

  return list;
}

/**
 * gtk_target_list_unref:
 * @list: a #GtkTargetList
 * 
 * Decreases the reference count of a #GtkTargetList by one.
 * If the resulting reference count is zero, frees the list.
 **/
void               
gtk_target_list_unref (GtkTargetList *list)
{
  g_return_if_fail (list != NULL);
  g_return_if_fail (list->ref_count > 0);

  list->ref_count--;
  if (list->ref_count > 0)
    return;

  g_list_free (list->list);
  g_slice_free (GtkTargetList, list);
}

/**
 * gtk_target_list_add:
 * @list:  a #GtkTargetList
 * @target: the mime type of the target
 * 
 * Appends another target to a #GtkTargetList.
 **/
void 
gtk_target_list_add (GtkTargetList *list,
		     const char    *target)
{
  g_return_if_fail (list != NULL);
  
  list->list = g_list_append (list->list, (gpointer) gdk_atom_intern (target, FALSE));
}

/**
 * gtk_target_list_merge:
 * @target: the #GtkTargetList to merge into
 * @source: the #GtkTargeList to merge from
 *
 * Merges all targets from @source into @target.
 */
void
gtk_target_list_merge (GtkTargetList       *target,
                       const GtkTargetList *source)
{
  GList *l;

  g_return_if_fail (target != NULL);
  g_return_if_fail (source != NULL);

  for (l = source->list; l; l = l->next)
    {
      target->list = g_list_prepend (target->list, l->data);
    }
}

/**
 * gtk_target_list_intersects:
 * @first: the primary #GtkTargetList to intersect
 * @second: the #GtkTargeList to intersect with
 *
 * Finds the first element from @first that is also contained
 * in @second.
 *
 * Returns: The first matching #GdkAtom or %NULL if the lists
 *     do not intersect.
 */
GdkAtom
gtk_target_list_intersects (const GtkTargetList *first,
                            const GtkTargetList *second)
{
  GList *l;

  g_return_val_if_fail (first != NULL, NULL);
  g_return_val_if_fail (second != NULL, NULL);

  for (l = first->list; l; l = l->next)
    {
      if (g_list_find (second->list, l->data))
        return l->data;
    }

  return NULL;
}

/**
 * gtk_target_list_add_table:
 * @list: a #GtkTargetList
 * @targets: (array length=ntargets): the table of #GtkTargetEntry
 * @ntargets: number of targets in the table
 * 
 * Prepends a table of #GtkTargetEntry to a target list.
 **/
void               
gtk_target_list_add_table (GtkTargetList  *list,
			   const char    **targets,
			   guint           ntargets)
{
  gint i;

  for (i=ntargets-1; i >= 0; i--)
    {
      list->list = g_list_prepend (list->list, (gpointer) gdk_atom_intern (targets[i], FALSE));
    }
}

/**
 * gtk_target_list_remove:
 * @list: a #GtkTargetList
 * @target: the interned atom representing the target
 * 
 * Removes a target from a target list.
 **/
void 
gtk_target_list_remove (GtkTargetList *list,
			GdkAtom        target)
{
  g_return_if_fail (list != NULL);

  list->list = g_list_remove (list->list, (gpointer) target);
}

/**
 * gtk_target_list_find:
 * @list: a #GtkTargetList
 * @target: a string representing the target to search for
 *
 * Looks up a given target in a #GtkTargetList.
 *
 * Returns: %TRUE if the target was found, otherwise %FALSE
 **/
gboolean
gtk_target_list_find (GtkTargetList *list,
		      const char    *target)
{
  g_return_val_if_fail (list != NULL, FALSE);
  g_return_val_if_fail (target != NULL, FALSE);

  return g_list_find (list->list, (gpointer) gdk_atom_intern (target, FALSE)) != NULL;
}

GdkAtom *
gtk_target_list_get_atoms (GtkTargetList *list,
                           guint         *n_atoms)
{
  GdkAtom *atoms;
  GList *l;
  guint i, n;

  n = g_list_length (list->list);
  atoms = g_new (GdkAtom, n);

  i = 0;
  for (l = list->list; l; l = l->next)
    atoms[i++] = l->data;

  if (n_atoms)
    *n_atoms = n;

  return atoms;
}

