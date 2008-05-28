/* GAIL - The GNOME Accessibility Implementation Library
 * Copyright 2001 Sun Microsystems Inc.
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

#ifndef __GAIL_ENTRY_H__
#define __GAIL_ENTRY_H__

#include <gail/gailwidget.h>
#include <libgail-util/gailtextutil.h>

G_BEGIN_DECLS

#define GAIL_TYPE_ENTRY                      (gail_entry_get_type ())
#define GAIL_ENTRY(obj)                      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GAIL_TYPE_ENTRY, GailEntry))
#define GAIL_ENTRY_CLASS(klass)              (G_TYPE_CHECK_CLASS_CAST ((klass), GAIL_TYPE_ENTRY, GailEntryClass))
#define GAIL_IS_ENTRY(obj)                   (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GAIL_TYPE_ENTRY))
#define GAIL_IS_ENTRY_CLASS(klass)           (G_TYPE_CHECK_CLASS_TYPE ((klass), GAIL_TYPE_ENTRY))
#define GAIL_ENTRY_GET_CLASS(obj)            (G_TYPE_INSTANCE_GET_CLASS ((obj), GAIL_TYPE_ENTRY, GailEntryClass))

typedef struct _GailEntry              GailEntry;
typedef struct _GailEntryClass         GailEntryClass;

struct _GailEntry
{
  GailWidget parent;

  GailTextUtil *textutil;
  /*
   * These fields store information about text changed
   */
  gchar          *signal_name_insert;
  gchar          *signal_name_delete;
  gint           position_insert;
  gint           position_delete;
  gint           length_insert;
  gint           length_delete;
  gint           cursor_position;
  gint           selection_bound;

  gchar          *activate_description;
  gchar          *activate_keybinding;
  guint          action_idle_handler;
  guint          insert_idle_handler;
};

GType gail_entry_get_type (void);

struct _GailEntryClass
{
  GailWidgetClass parent_class;
};

G_END_DECLS

#endif /* __GAIL_ENTRY_H__ */
