/*
 * Copyright © 2018 Benjamin Otte
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
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#ifndef __GTK_GST_MEDIA_FILE_H__
#define __GTK_GST_MEDIA_FILE_H__

#include <gtk/gtkmediafile.h>

G_BEGIN_DECLS

#define GTK_TYPE_GST_MEDIA_FILE (gtk_gst_media_file_get_type ())

G_DECLARE_FINAL_TYPE (GtkGstMediaFile, gtk_gst_media_file, GTK, GST_MEDIA_FILE, GtkMediaFile)

G_END_DECLS

#endif /* __GTK_GST_MEDIA_FILE_H__ */
