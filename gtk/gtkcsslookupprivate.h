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
  guint                id;
  GtkCssValue         *value;
  GtkCssSection       *section;
} GtkCssLookupValue;

struct _GtkCssLookup {
  int ref_count;
  GtkBitmask *set_values;
  GPtrArray *values;
};

GtkCssLookup *           gtk_css_lookup_new (void);
GtkCssLookup *           gtk_css_lookup_ref (GtkCssLookup *lookup);
void                     gtk_css_lookup_unref (GtkCssLookup *lookup);

void                     gtk_css_lookup_merge                   (GtkCssLookup               *lookup,
                                                                 GtkCssLookupValue          *values,
                                                                 guint                       n_values);
GtkCssSection *          gtk_css_lookup_get_section             (GtkCssLookup               *lookup,
                                                                 guint                       id);

static inline const GtkBitmask *
gtk_css_lookup_get_set_values (const GtkCssLookup *lookup)
{
  return lookup->set_values;
}

static inline GtkCssLookupValue *
gtk_css_lookup_get (GtkCssLookup *lookup,
                    guint         id)
{
  if (_gtk_bitmask_get (lookup->set_values, id))
    {
      int i;

      for (i = 0; i < lookup->values->len; i++)
        {
          GtkCssLookupValue *value = g_ptr_array_index (lookup->values, i);
          if (value->id == id)
            return value;
        }
    }

  return NULL;
} 

void gtk_css_lookup_register (GtkCssLookup *lookup);

G_END_DECLS

#endif /* __GTK_CSS_LOOKUP_PRIVATE_H__ */
