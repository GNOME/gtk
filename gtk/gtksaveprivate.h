/*
 * Copyright © Red Hat
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the licence, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <gio/gio.h>
#include <gtk/gtksave.h>

G_BEGIN_DECLS

typedef void   (*GtkSaveCallback)              (GVariant        *result,
                                                gpointer         data);

GtkSave*         gtk_save_new                  (GtkSaveCallback  callback,
                                                gpointer         data);

G_END_DECLS

