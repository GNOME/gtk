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

GtkCssChange
_gtk_css_change_for_sibling (GtkCssChange match)
{
#define BASE_STATES ( GTK_CSS_CHANGE_CLASS \
                    | GTK_CSS_CHANGE_NAME \
                    | GTK_CSS_CHANGE_ID \
                    | GTK_CSS_CHANGE_FIRST_CHILD \
                    | GTK_CSS_CHANGE_LAST_CHILD \
                    | GTK_CSS_CHANGE_NTH_CHILD \
                    | GTK_CSS_CHANGE_NTH_LAST_CHILD \
                    | GTK_CSS_CHANGE_STATE )

#define SIBLING_SHIFT 8

  return (match & ~(BASE_STATES|GTK_CSS_CHANGE_SOURCE|GTK_CSS_CHANGE_PARENT_STYLE)) | ((match & BASE_STATES) << SIBLING_SHIFT);

#undef BASE_STATES
#undef SIBLING_SHIFT
}

GtkCssChange
_gtk_css_change_for_child (GtkCssChange match)
{
#define BASE_STATES ( GTK_CSS_CHANGE_CLASS \
                    | GTK_CSS_CHANGE_NAME \
                    | GTK_CSS_CHANGE_ID \
                    | GTK_CSS_CHANGE_FIRST_CHILD \
                    | GTK_CSS_CHANGE_LAST_CHILD \
                    | GTK_CSS_CHANGE_NTH_CHILD \
                    | GTK_CSS_CHANGE_NTH_LAST_CHILD \
                    | GTK_CSS_CHANGE_STATE \
                    | GTK_CSS_CHANGE_SIBLING_CLASS \
                    | GTK_CSS_CHANGE_SIBLING_NAME \
                    | GTK_CSS_CHANGE_SIBLING_ID \
                    | GTK_CSS_CHANGE_SIBLING_FIRST_CHILD \
                    | GTK_CSS_CHANGE_SIBLING_LAST_CHILD \
                    | GTK_CSS_CHANGE_SIBLING_NTH_CHILD \
                    | GTK_CSS_CHANGE_SIBLING_NTH_LAST_CHILD \
                    | GTK_CSS_CHANGE_SIBLING_STATE )

#define PARENT_SHIFT 16

  return (match & ~(BASE_STATES|GTK_CSS_CHANGE_SOURCE|GTK_CSS_CHANGE_PARENT_STYLE)) | ((match & BASE_STATES) << PARENT_SHIFT);

#undef BASE_STATES
#undef PARENT_SHIFT
}

void
gtk_css_change_print (GtkCssChange  change,
                      GString      *string)
{
  const struct {
    GtkCssChange flags;
    const char *name;
  } names[] = {
    { GTK_CSS_CHANGE_CLASS, "class" },
    { GTK_CSS_CHANGE_NAME, "name" },
    { GTK_CSS_CHANGE_ID, "id" },
    { GTK_CSS_CHANGE_FIRST_CHILD, "first-child" },
    { GTK_CSS_CHANGE_LAST_CHILD, "last-child" },
    { GTK_CSS_CHANGE_NTH_CHILD, "nth-child" },
    { GTK_CSS_CHANGE_NTH_LAST_CHILD, "nth-last-child" },
    { GTK_CSS_CHANGE_STATE, "state" },
    { GTK_CSS_CHANGE_SIBLING_CLASS, "sibling-class" },
    { GTK_CSS_CHANGE_SIBLING_NAME, "sibling-name" },
    { GTK_CSS_CHANGE_SIBLING_ID, "sibling-id" },
    { GTK_CSS_CHANGE_SIBLING_FIRST_CHILD, "sibling-first-child" },
    { GTK_CSS_CHANGE_SIBLING_LAST_CHILD, "sibling-last-child" },
    { GTK_CSS_CHANGE_SIBLING_NTH_CHILD, "sibling-nth-child" },
    { GTK_CSS_CHANGE_SIBLING_NTH_LAST_CHILD, "sibling-nth-last-child" },
    { GTK_CSS_CHANGE_SIBLING_STATE, "sibling-state" },
    { GTK_CSS_CHANGE_PARENT_CLASS, "parent-class" },
    { GTK_CSS_CHANGE_PARENT_NAME, "parent-name" },
    { GTK_CSS_CHANGE_PARENT_ID, "parent-id" },
    { GTK_CSS_CHANGE_PARENT_FIRST_CHILD, "parent-first-child" },
    { GTK_CSS_CHANGE_PARENT_LAST_CHILD, "parent-last-child" },
    { GTK_CSS_CHANGE_PARENT_NTH_CHILD, "parent-nth-child" },
    { GTK_CSS_CHANGE_PARENT_NTH_LAST_CHILD, "parent-nth-last-child" },
    { GTK_CSS_CHANGE_PARENT_STATE, "parent-state" },
    { GTK_CSS_CHANGE_PARENT_SIBLING_CLASS, "parent-sibling-" },
    { GTK_CSS_CHANGE_PARENT_SIBLING_NAME, "parent-sibling-name" },
    { GTK_CSS_CHANGE_PARENT_SIBLING_ID, "parent-sibling-id" },
    { GTK_CSS_CHANGE_PARENT_SIBLING_FIRST_CHILD, "parent-sibling-first-child" },
    { GTK_CSS_CHANGE_PARENT_SIBLING_LAST_CHILD, "parent-sibling-last-child" },
    { GTK_CSS_CHANGE_PARENT_SIBLING_NTH_CHILD, "parent-sibling-nth-child" },
    { GTK_CSS_CHANGE_PARENT_SIBLING_NTH_LAST_CHILD, "parent-sibling-nth-last-child" },
    { GTK_CSS_CHANGE_PARENT_SIBLING_STATE, "parent-sibling-state" },
    { GTK_CSS_CHANGE_SOURCE, "source" },
    { GTK_CSS_CHANGE_PARENT_STYLE, "parent-style" },
    { GTK_CSS_CHANGE_TIMESTAMP, "timestamp" },
    { GTK_CSS_CHANGE_ANIMATIONS, "animations" },
  };
  guint i;
  gboolean first;

  first = TRUE;

  for (i = 0; i < G_N_ELEMENTS (names); i++)
    {
      if (change & names[i].flags)
        {
          if (first)
            first = FALSE;
          else
            g_string_append (string, "|");
          g_string_append (string, names[i].name);
        }
    }
}

char *
gtk_css_change_to_string (GtkCssChange change)
{
  GString *string = g_string_new (NULL);

  gtk_css_change_print (change, string);

  return g_string_free (string, FALSE);
}

