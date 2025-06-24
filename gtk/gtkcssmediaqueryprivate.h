/* GTK - The GIMP Toolkit
 * Copyright (C) 2025 Arjan Molenaar <amolenaarg@gnome.org>
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

#pragma once

#include <glib.h>
#include "gtk/css/gtkcssparserprivate.h"

G_BEGIN_DECLS

typedef struct
{
  char *name;
  char *value;
} GtkCssDiscreteMediaFeature;


void     gtk_css_media_feature_define_discrete (const char      *feature_name,
                                                gsize            n_feature_values,
                                                const char     **feature_values);

gboolean gtk_css_media_feature_is_valid        (const char *feature_name,
                                                const char *feature_value);


gboolean gtk_css_media_query_parse             (GtkCssParser *parser,
                                                GArray *media_features);


void     gtk_css_media_feature_init            (GtkCssDiscreteMediaFeature *media_feature,
                                                const char *feature_name,
                                                const char *feature_value);

void     gtk_css_media_feature_update          (GtkCssDiscreteMediaFeature *media_feature,
                                                const char *feature_value);

void     gtk_css_media_feature_clear           (GtkCssDiscreteMediaFeature *media_feature);


G_END_DECLS
