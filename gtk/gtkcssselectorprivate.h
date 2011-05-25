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

#ifndef __GTK_CSS_SELECTOR_PRIVATE_H__
#define __GTK_CSS_SELECTOR_PRIVATE_H__

#include <gtk/gtkenums.h>
#include <gtk/gtkwidgetpath.h>

G_BEGIN_DECLS

typedef enum {
  GTK_CSS_COMBINE_DESCANDANT,
  GTK_CSS_COMBINE_CHILD
} GtkCssCombinator;

typedef struct _GtkCssSelector GtkCssSelector;

GtkCssSelector *  _gtk_css_selector_new             (GtkCssSelector         *previous,
                                                     GtkCssCombinator        combine,
                                                     const char *            name,
                                                     GQuark *                ids,
                                                     GQuark *                classes,
                                                     GtkRegionFlags          pseudo_classes,
                                                     GtkStateFlags           state);
void              _gtk_css_selector_free            (GtkCssSelector         *selector);

char *            _gtk_css_selector_to_string       (const GtkCssSelector   *selector);
void              _gtk_css_selector_print           (const GtkCssSelector   *selector,
                                                     GString                *str);

GtkStateFlags     _gtk_css_selector_get_state_flags (GtkCssSelector         *selector);

gboolean          _gtk_css_selector_matches         (const GtkCssSelector   *selector,
                                                     const GtkWidgetPath    *path,
                                                     guint                   length);
int               _gtk_css_selector_compare         (const GtkCssSelector   *a,
                                                     const GtkCssSelector   *b);

G_END_DECLS

#endif /* __GTK_CSS_SELECTOR_PRIVATE_H__ */
