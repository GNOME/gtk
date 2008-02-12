/* Copyright (C) 2006 Openismus GmbH
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GTK_IM_CONTEXT_MULTIPRESS_H
#define __GTK_IM_CONTEXT_MULTIPRESS_H

#include <gtk/gtkimcontext.h>

G_BEGIN_DECLS

#define GTK_TYPE_IM_CONTEXT_MULTIPRESS              (gtk_im_context_multipress_get_type())
#define gtk_im_context_multipress(obj)              (GTK_CHECK_CAST ((obj), GTK_TYPE_IM_CONTEXT_MULTIPRESS, GtkImContextMultipress))
#define gtk_im_context_multipress_CLASS(klass)      (GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_IM_CONTEXT_MULTIPRESS, GtkImContextMultipressClass))
#define GTK_IS_IM_CONTEXT_MULTIPRESS(obj)           (GTK_CHECK_TYPE ((obj), GTK_TYPE_IM_CONTEXT_MULTIPRESS))
#define GTK_IS_IM_CONTEXT_MULTIPRESS_CLASS(klass)   (GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_IM_CONTEXT_MULTIPRESS))
#define gtk_im_context_multipress_GET_CLASS(obj)    (GTK_CHECK_GET_CLASS ((obj), GTK_TYPE_IM_CONTEXT_MULTIPRESS, GtkImContextMultipressClass))


typedef struct _KeySequence KeySequence;

typedef struct _GtkImContextMultipress GtkImContextMultipress;

/** This input method allows multi-press character input, like that found on mobile phones.
 *
 * This is based on GtkImContextSimple, which allows "compose" based on sequences of characters.
 * But instead the character sequences are defined by lists of characters for a key,
 * so that repeated pressing of the same key can cycle through the possible output characters,
 * with automatic choosing of the character after a time delay.
 */   
struct _GtkImContextMultipress
{
  GtkIMContext parent;

  /* Sequence information, loading from the configuration file: */
  KeySequence** key_sequences; /* Built when we read the config file. Not null-terminated. */
  gsize key_sequences_count; /* Number of KeySequence struct instances. */


  /* The last character entered so far during a compose.
   * If this is NULL then we are not composing yet.
   */
  guint key_last_entered;
  
  /* The position of the compose in the possible sequence.
   *  For instance, this is 2 if aa has been pressed to show b (from abc0).
   */
  
  guint compose_count; 
  guint timeout_id;

  /* The character(s) that will be used if it the current character(s) is accepted: */
  const gchar* tentative_match;
};


typedef struct _GtkImContextMultipressClass  GtkImContextMultipressClass;

struct _GtkImContextMultipressClass
{
  GtkIMContextClass parent_class;
};

void gtk_im_context_multipress_register_type (GTypeModule* type_module);
GType gtk_im_context_multipress_get_type (void) G_GNUC_CONST;
GtkIMContext *gtk_im_context_multipress_new (void);

G_END_DECLS


#endif /* __GTK_IM_CONTEXT_MULTIPRESS_H */
