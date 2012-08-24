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

#include "config.h"

#include "gtkcsstypesprivate.h"

#include "gtkcssnumbervalueprivate.h"
#include "gtkstylecontextprivate.h"

typedef struct _GtkCssChangeTranslation GtkCssChangeTranslation;
struct _GtkCssChangeTranslation {
  GtkCssChange from;
  GtkCssChange to;
};

static GtkCssChange
gtk_css_change_translate (GtkCssChange                   match,
                         const GtkCssChangeTranslation *translations,
                         guint                         n_translations)
{
  GtkCssChange result = match;
  guint i;

  for (i = 0; i < n_translations; i++)
    {
      if (match & translations[i].from)
        {
          result &= ~translations[i].from;
          result |= translations[i].to;
        }
    }

  return result;
}

GtkCssChange
_gtk_css_change_for_sibling (GtkCssChange match)
{
  static const GtkCssChangeTranslation table[] = {
    { GTK_CSS_CHANGE_CLASS, GTK_CSS_CHANGE_SIBLING_CLASS },
    { GTK_CSS_CHANGE_NAME, GTK_CSS_CHANGE_SIBLING_NAME },
    { GTK_CSS_CHANGE_POSITION, GTK_CSS_CHANGE_SIBLING_POSITION },
    { GTK_CSS_CHANGE_STATE, GTK_CSS_CHANGE_SIBLING_STATE },
    { GTK_CSS_CHANGE_SOURCE, 0 },
    { GTK_CSS_CHANGE_ANIMATE, 0 }
  };

  return gtk_css_change_translate (match, table, G_N_ELEMENTS (table)); 
}

GtkCssChange
_gtk_css_change_for_child (GtkCssChange match)
{
  static const GtkCssChangeTranslation table[] = {
    { GTK_CSS_CHANGE_CLASS, GTK_CSS_CHANGE_PARENT_CLASS },
    { GTK_CSS_CHANGE_NAME, GTK_CSS_CHANGE_PARENT_NAME },
    { GTK_CSS_CHANGE_POSITION, GTK_CSS_CHANGE_PARENT_POSITION },
    { GTK_CSS_CHANGE_STATE, GTK_CSS_CHANGE_PARENT_STATE },
    { GTK_CSS_CHANGE_SIBLING_CLASS, GTK_CSS_CHANGE_PARENT_SIBLING_CLASS },
    { GTK_CSS_CHANGE_SIBLING_NAME, GTK_CSS_CHANGE_PARENT_SIBLING_NAME },
    { GTK_CSS_CHANGE_SIBLING_POSITION, GTK_CSS_CHANGE_PARENT_SIBLING_POSITION },
    { GTK_CSS_CHANGE_SIBLING_STATE, GTK_CSS_CHANGE_PARENT_SIBLING_STATE },
    { GTK_CSS_CHANGE_SOURCE, 0 },
    { GTK_CSS_CHANGE_ANIMATE, 0 }
  };

  return gtk_css_change_translate (match, table, G_N_ELEMENTS (table)); 
}

GtkCssDependencies
_gtk_css_dependencies_union (GtkCssDependencies first,
                             GtkCssDependencies second)
{
  return (first  & ~GTK_CSS_EQUALS_PARENT) | ((first  & GTK_CSS_EQUALS_PARENT) ? GTK_CSS_DEPENDS_ON_PARENT : 0)
       | (second & ~GTK_CSS_EQUALS_PARENT) | ((second & GTK_CSS_EQUALS_PARENT) ? GTK_CSS_DEPENDS_ON_PARENT : 0);
}

