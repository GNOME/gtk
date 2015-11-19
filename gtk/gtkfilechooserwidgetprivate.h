/* gtkfilechooserwidgetprivate.h
 *
 * Copyright (C) 2015 Red Hat
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Matthias Clasen
 */

#ifndef __GTK_FILE_CHOOSER_WIDGET_PRIVATE_H__
#define __GTK_FILE_CHOOSER_WIDGET_PRIVATE_H__

#include <glib.h>
#include "gtkfilechooserwidget.h"

G_BEGIN_DECLS

void
gtk_file_chooser_widget_set_save_entry (GtkFileChooserWidget *chooser,
                                        GtkWidget            *entry);

G_END_DECLS

#endif /* __GTK_FILE_CHOOSER_WIDGET_PRIVATE_H__ */
