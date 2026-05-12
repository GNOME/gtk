/* gtkcelllayout.h
 * Copyright (C) 2003  Kristian Rietveld  <kris@gtk.org>
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <gtk/deprecated/gtkcelllayout.h>

gboolean _gtk_cell_layout_buildable_custom_tag_start (GtkBuildable       *buildable,
                                                      GtkBuilder         *builder,
                                                      GObject            *child,
                                                      const char         *tagname,
                                                      GtkBuildableParser *parser,
                                                      gpointer           *data);
gboolean _gtk_cell_layout_buildable_custom_tag_end   (GtkBuildable       *buildable,
                                                      GtkBuilder         *builder,
                                                      GObject            *child,
                                                      const char         *tagname,
                                                      gpointer           *data);
void     _gtk_cell_layout_buildable_add_child        (GtkBuildable       *buildable,
                                                      GtkBuilder         *builder,
                                                      GObject            *child,
                                                      const char         *type);
