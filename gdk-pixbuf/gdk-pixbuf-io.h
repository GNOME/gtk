/* GdkPixbuf library - Io handling.  This is an internal header for gdk-pixbuf.
 * You should never use it unless you are doing developement for gdkpixbuf itself.
 *
 * Copyright (C) 1999 The Free Software Foundation
 *
 * Authors: Mark Crichton <crichton@gimp.org>
 *          Miguel de Icaza <miguel@gnu.org>
 *          Federico Mena-Quintero <federico@gimp.org>
 *          Jonathan Blandford <jrb@redhat.com>
 *          Michael Fulbright <drmike@redhat.com>
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

#ifndef GDK_PIXBUF_IO_H
#define GDK_PIXBUF_IO_H

#include <gmodule.h>
#include <stdio.h>
#include "gdk-pixbuf.h"
#include "gdk-pixbuf-i18n.h"

G_BEGIN_DECLS



typedef void (* ModuleSizeFunc)           (gint *width, 
				           gint *height, 
				           gpointer user_data);

typedef void (* ModulePreparedNotifyFunc) (GdkPixbuf          *pixbuf,
                                           GdkPixbufAnimation *anim,
                                           gpointer            user_data);
typedef void (* ModuleUpdatedNotifyFunc)  (GdkPixbuf *pixbuf,
                                           int        x,
                                           int        y,
                                           int        width,
                                           int        height,
                                           gpointer   user_data);

typedef struct _GdkPixbufModule GdkPixbufModule;
struct _GdkPixbufModule {
	char *module_name;
	gboolean (* format_check) (guchar *buffer, int size);
	GModule *module;
        GdkPixbuf *(* load) (FILE    *f,
                             GError **error);
        GdkPixbuf *(* load_xpm_data) (const char **data);

        /* Incremental loading */

        gpointer (* begin_load)     (ModuleSizeFunc size_func,
                                     ModulePreparedNotifyFunc prepare_func,
                                     ModuleUpdatedNotifyFunc update_func,
                                     gpointer user_data,
                                     GError **error);
        gboolean (* stop_load)      (gpointer context,
                                     GError **error);
        gboolean (* load_increment) (gpointer      context,
                                     const guchar *buf,
                                     guint         size,
                                     GError      **error);

	/* Animation loading */
	GdkPixbufAnimation *(* load_animation) (FILE    *f,
                                                GError **error);

        gboolean (* save) (FILE      *f,
                           GdkPixbuf *pixbuf,
                           gchar    **param_keys,
                           gchar    **param_values,
                           GError   **error);
};

typedef void (* ModuleFillVtableFunc) (GdkPixbufModule *module);

GdkPixbufModule *_gdk_pixbuf_get_module (guchar *buffer, guint size,
                                         const gchar *filename,
                                         GError **error);
GdkPixbufModule *_gdk_pixbuf_get_named_module (const char *name,
                                               GError **error);
gboolean _gdk_pixbuf_load_module (GdkPixbufModule *image_module,
                                  GError **error);

GdkPixbuf *_gdk_pixbuf_generic_image_load (GdkPixbufModule *image_module,
					   FILE *f,
					   GError **error);



G_END_DECLS

#endif
