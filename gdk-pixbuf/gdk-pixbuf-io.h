/* GdkPixbuf library - Io handling
 *
 * Copyright (C) 1999 The Free Software Foundation
 *
 * Authors: Mark Crichton <crichton@gimp.org>
 *          Miguel de Icaza <miguel@gnu.org>
 *          Federico Mena-Quintero <federico@gimp.org>
 *          Jonathan Blandford <jrb@redhat.com>
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
#include <gmodule.h>
#include "gdk-pixbuf.h"
#include <stdio.h>

typedef struct _ModuleType ModuleType;
struct _ModuleType {
	char *module_name;
	gboolean (* format_check) (guchar *buffer, int size);
	GModule *module;
	GdkPixbuf *(* load) (FILE *f);
        GdkPixbuf *(* load_xpm_data) (const gchar **data);
};


ModuleType *gdk_pixbuf_get_module (gchar *buffer, gint size);
