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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GTK_CSS_LOOKUP_PRIVATE_H__
#define __GTK_CSS_LOOKUP_PRIVATE_H__

#include <glib-object.h>

#include "gtk/gtkbitmaskprivate.h"

#include "gtk/css/gtkcsssection.h"
#include "gtk/gtkcssvalueprivate.h"


G_BEGIN_DECLS

typedef struct _GtkCssLookup GtkCssLookup;

typedef struct {
  GtkCssSection     *section;
  GtkCssValue       *value;
} GtkCssLookupValue;

struct _GtkCssLookup {
  int ref_count;
  GtkBitmask *set_values;
  GtkCssLookupValue  values[GTK_CSS_PROPERTY_N_PROPERTIES];
};

GtkCssLookup *           gtk_css_lookup_new (void);
GtkCssLookup *           gtk_css_lookup_ref (GtkCssLookup *lookup);
void                     gtk_css_lookup_unref (GtkCssLookup *lookup);

gboolean                 gtk_css_lookup_is_missing              (const GtkCssLookup         *lookup,
                                                                 guint                       id);
void                     gtk_css_lookup_set                     (GtkCssLookup               *lookup,
                                                                 guint                       id,
                                                                 GtkCssSection              *section,
                                                                 GtkCssValue                *value);

static inline const GtkBitmask *
gtk_css_lookup_get_set_values (const GtkCssLookup *lookup)
{
  return lookup->set_values;
}

static inline GtkCssLookupValue *
gtk_css_lookup_get (GtkCssLookup *lookup,
                    guint         id)
{
  return &lookup->values[id];
} 

G_END_DECLS

#endif /* __GTK_CSS_LOOKUP_PRIVATE_H__ */
