/* GAIL - The GNOME Accessibility Implementation Library
 * Copyright 2001 Sun Microsystems Inc.
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

#include <config.h>

#include <gtk/gtk.h>
#include "gailcalendar.h"

static void         gail_calendar_class_init          (GailCalendarClass *klass);
static void         gail_calendar_init                (GailCalendar      *calendar);

G_DEFINE_TYPE (GailCalendar, gail_calendar, GAIL_TYPE_WIDGET)

static void
gail_calendar_class_init (GailCalendarClass *klass)
{
}

static void
gail_calendar_init (GailCalendar *calendar)
{
}

AtkObject* 
gail_calendar_new (GtkWidget *widget)
{
  GObject *object;
  AtkObject *accessible;

  g_return_val_if_fail (GTK_IS_CALENDAR (widget), NULL);

  object = g_object_new (GAIL_TYPE_CALENDAR, NULL);

  accessible = ATK_OBJECT (object);
  atk_object_initialize (accessible, widget);

  accessible->role = ATK_ROLE_CALENDAR;

  return accessible;
}
