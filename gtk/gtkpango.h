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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#ifndef __GTK_PANGO_H__
#define __GTK_PANGO_H__

#include <pango2/pangocairo.h>
#include  "gtkbuildable.h"

G_BEGIN_DECLS

Pango2AttrList *_gtk_pango_attr_list_merge (Pango2AttrList *into,
                                           Pango2AttrList *from) G_GNUC_WARN_UNUSED_RESULT;

gboolean gtk_buildable_attribute_tag_start (GtkBuildable       *buildable,
                                            GtkBuilder         *builder,
                                            GObject            *child,
                                            const char         *tagname,
                                            GtkBuildableParser *parser,
                                            gpointer           *data);

typedef struct {
  GtkBuilder    *builder;
  GObject       *object;
  Pango2AttrList *attrs;
} GtkPangoAttributeParserData;

void
gtk_pango_attribute_start_element (GtkBuildableParseContext  *context,
                                   const char                *element_name,
                                   const char               **names,
                                   const char               **values,
                                   gpointer                   user_data,
                                   GError                   **error);

G_END_DECLS

#endif /* __GTK_PANGO_H__ */
