/* GTK - The GIMP Toolkit
 * Copyright (C) 2017, Red Hat, Inc.
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

#ifndef __GTK_EVENT_CONTROLLER_LEGACY_H__
#define __GTK_EVENT_CONTROLLER_LEGACY_H__

#include "gtkeventcontrollerprivate.h"
#include "gtkwidgetprivate.h"

G_BEGIN_DECLS

#define GTK_TYPE_EVENT_CONTROLLER_LEGACY         (_gtk_event_controller_legacy_get_type ())
#define GTK_EVENT_CONTROLLER_LEGACY(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GTK_TYPE_EVENT_CONTROLLER_LEGACY, GtkEventControllerLegacy))
#define GTK_EVENT_CONTROLLER_LEGACY_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), GTK_TYPE_EVENT_CONTROLLER_LEGACY, GtkEventControllerLegacyClass))
#define GTK_IS_EVENT_CONTROLLER_LEGACY(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GTK_TYPE_EVENT_CONTROLLER_LEGACY))
#define GTK_IS_EVENT_CONTROLLER_LEGACY_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GTK_TYPE_EVENT_CONTROLLER_LEGACY))
#define GTK_EVENT_CONTROLLER_LEGACY_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GTK_TYPE_EVENT_CONTROLLER_LEGACY, GtkEventControllerLegacyClass))

typedef struct _GtkEventControllerLegacy GtkEventControllerLegacy;
typedef struct _GtkEventControllerLegacyClass GtkEventControllerLegacyClass;

struct _GtkEventControllerLegacy
{
  GtkEventController parent_instance;
};

struct _GtkEventControllerLegacyClass
{
  GtkEventControllerClass parent_class;
};

GType               _gtk_event_controller_legacy_get_type  (void) G_GNUC_CONST;
GtkEventController *_gtk_event_controller_legacy_new       (GtkWidget *widget);

#endif /* __GTK_EVENT_CONTROLLER_LEGACY_H__ */
