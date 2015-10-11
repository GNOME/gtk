/* gtkshortcutspage.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gtkshortcutspageprivate.h"

#include "gtkorientable.h"

struct _GtkShortcutsPage
{
  GtkBox parent_instance;
};

struct _GtkShortcutsPageClass
{
  GtkBoxClass parent_class;
};

G_DEFINE_TYPE (GtkShortcutsPage, gtk_shortcuts_page, GTK_TYPE_BOX)

static void
gtk_shortcuts_page_class_init (GtkShortcutsPageClass *klass)
{
}

static void
gtk_shortcuts_page_init (GtkShortcutsPage *self)
{
  gtk_orientable_set_orientation (GTK_ORIENTABLE (self), GTK_ORIENTATION_HORIZONTAL);
  gtk_box_set_spacing (GTK_BOX (self), 22);
}
