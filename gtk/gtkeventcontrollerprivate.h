/* GTK - The GIMP Toolkit
 * Copyright (C) 2012, One Laptop Per Child.
 * Copyright (C) 2014, Red Hat, Inc.
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
 *
 * Author(s): Carlos Garnacho <carlosg@gnome.org>
 */
#ifndef __GTK_EVENT_CONTROLLER_PRIVATE_H__
#define __GTK_EVENT_CONTROLLER_PRIVATE_H__

#include "gtkeventcontroller.h"

struct _GtkEventController
{
  GObject parent_instance;
};

struct _GtkEventControllerClass
{
  GObjectClass parent_class;

  void     (* set_widget)   (GtkEventController *controller,
                             GtkWidget          *widget);
  void     (* unset_widget) (GtkEventController *controller);
  gboolean (* handle_event) (GtkEventController *controller,
                             const GdkEvent     *event,
                             double              x,
                             double              y);
  void     (* reset)        (GtkEventController *controller);

  void     (* handle_crossing) (GtkEventController    *controller,
                                const GtkCrossingData *crossing);

  /*<private>*/

  /* Tells whether the event is filtered out, %TRUE makes
   * the event unseen by the handle_event vfunc.
   */
  gboolean (* filter_event) (GtkEventController *controller,
                             const GdkEvent     *event);
  gpointer padding[10];
};

#endif /* __GTK_EVENT_CONTROLLER_PRIVATE_H__ */
