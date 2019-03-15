/* GTK - The GIMP Toolkit
 * Copyright (C) 2019 Red Hat, Inc.
 *
 * Authors:
 * - Matthias Clasen <mclasen@redhat.com>
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

#ifndef __DEMO_TAGGED_ENTRY_H__
#define __DEMO_TAGGED_ENTRY_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define DEMO_TYPE_TAGGED_ENTRY                 (demo_tagged_entry_get_type ())
#define DEMO_TAGGED_ENTRY(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), DEMO_TYPE_TAGGED_ENTRY, DemoTaggedEntry))
#define DEMO_TAGGED_ENTRY_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), DEMO_TYPE_TAGGED_ENTRY, DemoTaggedEntryClass))
#define DEMO_IS_TAGGED_ENTRY(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), DEMO_TYPE_TAGGED_ENTRY))
#define DEMO_IS_TAGGED_ENTRY_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), DEMO_TYPE_TAGGED_ENTRY))
#define DEMO_TAGGED_ENTRY_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), DEMO_TYPE_TAGGED_ENTRY, DemoTaggedEntryClass))

typedef struct _DemoTaggedEntry       DemoTaggedEntry;
typedef struct _DemoTaggedEntryClass  DemoTaggedEntryClass;

struct _DemoTaggedEntry
{
  GtkWidget parent;
};

struct _DemoTaggedEntryClass
{
  GtkWidgetClass parent_class;
};

#define DEMO_TYPE_TAGGED_ENTRY_TAG                (demo_tagged_entry_tag_get_type ())
#define DEMO_TAGGED_ENTRY_TAG(obj)                (G_TYPE_CHECK_INSTANCE_CAST ((obj), DEMO_TYPE_TAGGED_ENTRY_TAG, DemoTaggedEntryTag))
#define DEMO_TAGGED_ENTRY_TAG_CLASS(klass)        (G_TYPE_CHECK_CLASS_CAST ((klass), DEMO_TYPE_TAGGED_ENTRY_TAG, DemoTaggedEntryTag))
#define DEMO_IS_TAGGED_ENTRY_TAG(obj)             (G_TYPE_CHECK_INSTANCE_TYPE ((obj), DEMO_TYPE_TAGGED_ENTRY_TAG))
#define DEMO_IS_TAGGED_ENTRY_TAG_CLASS(klass)     (G_TYPE_CHECK_CLASS_TYPE ((klass), DEMO_TYPE_TAGGED_ENTRY_TAG))
#define DEMO_TAGGED_ENTRY_TAG_GET_CLASS(obj)      (G_TYPE_INSTANCE_GET_CLASS ((obj), DEMO_TYPE_TAGGED_ENTRY_TAG, DemoTaggedEntryTagClass))

typedef struct _DemoTaggedEntryTag       DemoTaggedEntryTag;
typedef struct _DemoTaggedEntryTagClass  DemoTaggedEntryTagClass;


GType           demo_tagged_entry_get_type (void) G_GNUC_CONST;
GType           demo_tagged_entry_tag_get_type (void) G_GNUC_CONST;

GtkWidget *     demo_tagged_entry_new     (void);

void            demo_tagged_entry_add_tag (DemoTaggedEntry *entry,
                                           GtkWidget       *tag);

void            demo_tagged_entry_insert_tag_after (DemoTaggedEntry *entry,
                                                    GtkWidget       *tag,
                                                    GtkWidget       *sibling);

void            demo_tagged_entry_remove_tag (DemoTaggedEntry *entry,
                                              GtkWidget       *tag);

DemoTaggedEntryTag *
                demo_tagged_entry_tag_new (const char *label);

const char *    demo_tagged_entry_tag_get_label (DemoTaggedEntryTag *tag);

void            demo_tagged_entry_tag_set_label (DemoTaggedEntryTag *tag,
                                                 const char         *label);

gboolean        demo_tagged_entry_tag_get_has_close_button (DemoTaggedEntryTag *tag);

void            demo_tagged_entry_tag_set_has_close_button (DemoTaggedEntryTag *tag,
                                                            gboolean            has_close_button);

G_END_DECLS

#endif /* __DEMO_TAGGED_ENTRY_H__ */
