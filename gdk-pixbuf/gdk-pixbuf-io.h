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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef GDK_PIXBUF_IO_H
#define GDK_PIXBUF_IO_H

#include <gmodule.h>
#include <stdio.h>
#include "gdk-pixbuf.h"

#ifdef __cplusplus
extern "C" {
#endif



typedef void (* ModulePreparedNotifyFunc) (GdkPixbuf *pixbuf, gpointer user_data);
typedef void (* ModuleUpdatedNotifyFunc) (GdkPixbuf *pixbuf,
					  guint x, guint y,
					  guint width, guint height,
					  gpointer user_data);
/* Needed only for animated images. */
typedef void (* ModuleFrameDoneNotifyFunc) (GdkPixbufFrame *frame,
					    gpointer user_data);
typedef void (* ModuleAnimationDoneNotifyFunc) (GdkPixbuf *pixbuf,
						gpointer user_data);

typedef struct _GdkPixbufModule GdkPixbufModule;
struct _GdkPixbufModule {
	char *module_name;
	gboolean (* format_check) (guchar *buffer, int size);
	GModule *module;
	GdkPixbuf *(* load) (FILE *f);
        GdkPixbuf *(* load_xpm_data) (const char **data);

        /* Incremental loading */

        gpointer (* begin_load) (ModulePreparedNotifyFunc prepare_func,
				 ModuleUpdatedNotifyFunc update_func,
				 ModuleFrameDoneNotifyFunc frame_done_func,
				 ModuleAnimationDoneNotifyFunc anim_done_func,
				 gpointer user_data);
        void (* stop_load) (gpointer context);
        gboolean (* load_increment) (gpointer context, const guchar *buf, guint size);

	/* Animation loading */
	GdkPixbufAnimation *(* load_animation) (FILE *f);
};


GdkPixbufModule *gdk_pixbuf_get_module (guchar *buffer, guint size);
void gdk_pixbuf_load_module (GdkPixbufModule *image_module);



#ifdef __cplusplus
}
#endif

#endif
