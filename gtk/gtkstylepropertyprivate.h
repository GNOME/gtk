/* GTK - The GIMP Toolkit
 * Copyright (C) 2010 Carlos Garnacho <carlosg@gnome.org>
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

#ifndef __GTK_STYLEPROPERTY_PRIVATE_H__
#define __GTK_STYLEPROPERTY_PRIVATE_H__

#include "gtkcssparserprivate.h"

G_BEGIN_DECLS

typedef struct _GtkStyleProperty GtkStyleProperty;
typedef enum {
  GTK_STYLE_PROPERTY_INHERIT = (1 << 0)
} GtkStylePropertyFlags;

typedef GParameter *     (* GtkStyleUnpackFunc)            (const GValue           *value,
                                                            guint                  *n_params);
typedef void             (* GtkStylePackFunc)              (GValue                 *value,
                                                            GtkStyleProperties     *props,
                                                            GtkStateFlags           state);
typedef gboolean         (* GtkStyleParseFunc)             (GtkCssParser           *parser,
                                                            GFile                  *base,
                                                            GValue                 *value);
typedef void             (* GtkStylePrintFunc)             (const GValue           *value,
                                                            GString                *string);
typedef void             (* GtkStyleDefaultValueFunc)      (GtkStyleProperties     *props,
                                                            GtkStateFlags           state,
                                                            GValue                 *value);
typedef void             (* GtkStyleUnsetFunc)             (GtkStyleProperties     *props,
                                                            GtkStateFlags           state);


struct _GtkStyleProperty
{
  GParamSpec               *pspec;
  GtkStylePropertyFlags     flags;

  GtkStylePropertyParser    property_parse_func;
  GtkStyleUnpackFunc        unpack_func;
  GtkStylePackFunc          pack_func;
  GtkStyleParseFunc         parse_func;
  GtkStylePrintFunc         print_func;
  GtkStyleDefaultValueFunc  default_value_func;
  GtkStyleUnsetFunc         unset_func;
};

const GtkStyleProperty * _gtk_style_property_lookup        (const char             *name);

void                     _gtk_style_property_register      (GParamSpec             *pspec,
                                                            GtkStylePropertyFlags   flags,
                                                            GtkStylePropertyParser  property_parse_func,
                                                            GtkStyleUnpackFunc      unpack_func,
                                                            GtkStylePackFunc        pack_func,
                                                            GtkStyleParseFunc       parse_func,
                                                            GtkStylePrintFunc       print_func,
                                                            GtkStyleDefaultValueFunc default_value_func,
                                                            GtkStyleUnsetFunc       unset_func);

gboolean                 _gtk_style_property_is_inherit    (const GtkStyleProperty *property);

void                     _gtk_style_property_default_value (const GtkStyleProperty *property,
                                                            GtkStyleProperties     *properties,
                                                            GtkStateFlags           state,
                                                            GValue                 *value);

void                     _gtk_style_property_resolve       (const GtkStyleProperty *property,
                                                            GtkStyleProperties     *properties,
                                                            GtkStateFlags           state,
                                                            GValue                 *value);

gboolean                 _gtk_style_property_is_shorthand  (const GtkStyleProperty *property);
GParameter *             _gtk_style_property_unpack        (const GtkStyleProperty *property,
                                                            const GValue           *value,
                                                            guint                  *n_params);
void                     _gtk_style_property_pack          (const GtkStyleProperty *property,
                                                            GtkStyleProperties     *props,
                                                            GtkStateFlags           state,
                                                            GValue                 *value);

gboolean                 _gtk_style_property_parse_value   (const GtkStyleProperty *property,
                                                            GValue                 *value,
                                                            GtkCssParser           *parser,
                                                            GFile                  *base);
void                     _gtk_style_property_print_value   (const GtkStyleProperty *property,
                                                            const GValue           *value,
                                                            GString                *string);

G_END_DECLS

#endif /* __GTK_CSS_STYLEPROPERTY_PRIVATE_H__ */
