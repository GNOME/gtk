/* GTK - The GIMP Toolkit
 * Copyright (C) 2011 Benjamin Otte <otte@gnome.org>
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

#ifndef __GTK_CSS_LOOKUP_PRIVATE_H__
#define __GTK_CSS_LOOKUP_PRIVATE_H__

#include <glib-object.h>
#include "gtk/gtkbitmaskprivate.h"
#include "gtk/gtkstylecontext.h"
#include "gtk/gtkstyleproperties.h"


G_BEGIN_DECLS

typedef struct _GtkCssLookup GtkCssLookup;

GtkCssLookup *          _gtk_css_lookup_new                     (void);
void                    _gtk_css_lookup_free                    (GtkCssLookup       *lookup);

const GtkBitmask *      _gtk_css_lookup_get_missing             (const GtkCssLookup *lookup);
gboolean                _gtk_css_lookup_is_missing              (const GtkCssLookup *lookup,
                                                                 guint               id);
void                    _gtk_css_lookup_set                     (GtkCssLookup       *lookup,
                                                                 guint               id,
                                                                 const GValue       *value);
GtkStyleProperties *    _gtk_css_lookup_resolve                 (GtkCssLookup       *lookup,
                                                                 GtkStyleContext    *context);


G_END_DECLS

#endif /* __GTK_CSS_LOOKUP_PRIVATE_H__ */
