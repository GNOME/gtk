/* GTK - The GIMP Toolkit
 * Copyright (C) 2019 Red Hat, Inc.
 *
 * Authors:
 * - MAtthias Clasen <mclasen@redhat.com>
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GTK_TAGGED_ENTRY_H__
#define __GTK_TAGGED_ENTRY_H__

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtkentry.h>

G_BEGIN_DECLS

#define GTK_TYPE_TAGGED_ENTRY                 (gtk_tagged_entry_get_type ())
#define GTK_TAGGED_ENTRY(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_TAGGED_ENTRY, GtkTaggedEntry))
#define GTK_TAGGED_ENTRY_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_TAGGED_ENTRY, GtkTaggedEntryClass))
#define GTK_IS_TAGGED_ENTRY(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_TAGGED_ENTRY))
#define GTK_IS_TAGGED_ENTRY_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_TAGGED_ENTRY))
#define GTK_TAGGED_ENTRY_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_TAGGED_ENTRY, GtkTaggedEntryClass))

typedef struct _GtkTaggedEntry       GtkTaggedEntry;
typedef struct _GtkTaggedEntryClass  GtkTaggedEntryClass;

struct _GtkTaggedEntry
{
  GtkWidget parent;
};

struct _GtkTaggedEntryClass
{
  GtkWidgetClass parent_class;
};

#define GTK_TYPE_ENTRY_TAG                (gtk_entry_tag_get_type ())
#define GTK_ENTRY_TAG(obj)                (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_ENTRY_TAG, GtkEntryTag))
#define GTK_ENTRY_TAG_CLASS(klass)        (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_ENTRY_TAG, GtkEntryTag))
#define GTK_IS_ENTRY_TAG(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_ENTRY_TAG))
#define GTK_IS_ENTRY_TAG_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_ENTRY_TAG))
#define GTK_ENTRY_TAG_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_ENTRY_TAG, GtkEntryTagClass))

typedef struct _GtkEntryTag       GtkEntryTag;
typedef struct _GtkEntryTagClass  GtkEntryTagClass;


GDK_AVAILABLE_IN_ALL
GType           gtk_tagged_entry_get_type (void) G_GNUC_CONST;
GDK_AVAILABLE_IN_ALL
GType           gtk_entry_tag_get_type (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_ALL
GtkWidget *     gtk_tagged_entry_new      (void);


GDK_AVAILABLE_IN_ALL
void            gtk_tagged_entry_add_tag (GtkTaggedEntry *entry,
                                          GtkWidget      *widget);

GDK_AVAILABLE_IN_ALL
void            gtk_tagged_entry_insert_tag (GtkTaggedEntry *entry,
                                             GtkWidget      *widget,
                                             int             position);

GDK_AVAILABLE_IN_ALL
void            gtk_tagged_entry_remove_tag (GtkTaggedEntry *entry,
                                             GtkWidget      *widget);

GDK_AVAILABLE_IN_ALL
GtkEntryTag *   gtk_entry_tag_new (const char *label);

GDK_AVAILABLE_IN_ALL
const char *    gtk_entry_tag_get_label (GtkEntryTag *tag);

GDK_AVAILABLE_IN_ALL
void            gtk_entry_tag_set_label (GtkEntryTag *tag,
                                         const char  *label);

GDK_AVAILABLE_IN_ALL
gboolean        gtk_entry_tag_get_has_close_button (GtkEntryTag *tag);

GDK_AVAILABLE_IN_ALL
void            gtk_entry_tag_set_has_close_button (GtkEntryTag *tag,
                                                    gboolean     has_close_button);

G_END_DECLS

#endif /* __GTK_TAGGED_ENTRY_H__ */
