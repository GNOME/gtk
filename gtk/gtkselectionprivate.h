/* GTK - The GIMP Toolkit
 *
 * Copyright (C) 2010 Javier Jardón
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */


/* The contents of a selection are returned in a GtkSelectionData
 * structure. selection/target identify the request.  type specifies
 * the type of the return; if length < 0, and the data should be
 * ignored. This structure has object semantics - no fields should be
 * modified directly, they should not be created directly, and
 * pointers to them should not be stored beyond the duration of a
 * callback. (If the last is changed, we’ll need to add reference
 * counting.) The time field gives the timestamp at which the data was
 * sent.
 */

#ifndef __GTK_SELECTION_PRIVATE_H__
#define __GTK_SELECTION_PRIVATE_H__

#include "gtkselection.h"

G_BEGIN_DECLS

struct _GtkSelectionData
{
  /*< private >*/
  GdkAtom       selection;
  GdkAtom       target;
  GdkAtom       type;
  gint          format;
  guchar       *data;
  gint          length;
  GdkDisplay   *display;
};

struct _GtkTargetList
{
  /*< private >*/
  GList *list;
  guint ref_count;
 };

gboolean _gtk_selection_clear           (GtkWidget         *widget,
                                         GdkEventSelection *event);
gboolean _gtk_selection_request         (GtkWidget         *widget,
                                         GdkEventSelection *event);
gboolean _gtk_selection_incr_event      (GdkWindow         *window,
                                         GdkEventProperty  *event);
gboolean _gtk_selection_notify          (GtkWidget         *widget,
                                         GdkEventSelection *event);
gboolean _gtk_selection_property_notify (GtkWidget         *widget,
                                         GdkEventProperty  *event);

G_END_DECLS

#endif /* __GTK_SELECTION_PRIVATE_H__ */
