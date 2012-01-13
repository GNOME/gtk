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

#include "config.h"

#include "gtkcsstypesprivate.h"
#include "gtkstylecontextprivate.h"

#define DEFINE_BOXED_TYPE_WITH_COPY_FUNC(TypeName, type_name) \
\
static TypeName * \
type_name ## _copy (const TypeName *foo) \
{ \
  return g_memdup (foo, sizeof (TypeName)); \
} \
\
G_DEFINE_BOXED_TYPE (TypeName, type_name, type_name ## _copy, g_free)

DEFINE_BOXED_TYPE_WITH_COPY_FUNC (GtkCssBorderCornerRadius, _gtk_css_border_corner_radius)
DEFINE_BOXED_TYPE_WITH_COPY_FUNC (GtkCssBorderImageRepeat, _gtk_css_border_image_repeat)
DEFINE_BOXED_TYPE_WITH_COPY_FUNC (GtkCssNumber, _gtk_css_number)

void
_gtk_css_number_init (GtkCssNumber *number,
                      double        value,
                      GtkCssUnit    unit)
{
  number->value = value;
  number->unit = unit;
}

void
_gtk_css_number_compute (GtkCssNumber       *dest,
                         const GtkCssNumber *src,
                         GtkStyleContext    *context)
{
  switch (src->unit)
    {
    default:
      g_assert_not_reached();
      /* fall through */
    case GTK_CSS_PERCENT:
    case GTK_CSS_NUMBER:
    case GTK_CSS_PX:
      dest->value = src->value;
      dest->unit = src->unit;
      break;
    case GTK_CSS_PT:
      dest->value = src->value * 96.0 / 72.0;
      dest->unit = GTK_CSS_PX;
      break;
    case GTK_CSS_PC:
      dest->value = src->value * 96.0 / 72.0 * 12.0;
      dest->unit = GTK_CSS_PX;
      break;
    case GTK_CSS_IN:
      dest->value = src->value * 96.0;
      dest->unit = GTK_CSS_PX;
      break;
    case GTK_CSS_CM:
      dest->value = src->value * 96.0 * 0.39370078740157477;
      dest->unit = GTK_CSS_PX;
      break;
    case GTK_CSS_MM:
      dest->value = src->value * 96.0 * 0.039370078740157477;
      dest->unit = GTK_CSS_PX;
      break;
    case GTK_CSS_EM:
      dest->value = src->value * g_value_get_double (_gtk_style_context_peek_property (context, "font-size"));
      dest->unit = GTK_CSS_PX;
      break;
    case GTK_CSS_EX:
      /* for now we pretend ex is half of em */
      dest->value = src->value * g_value_get_double (_gtk_style_context_peek_property (context, "font-size"));
      dest->unit = GTK_CSS_PX;
      break;
    }
}

void
_gtk_css_number_print (const GtkCssNumber *number,
                       GString            *string)
{
  char buf[G_ASCII_DTOSTR_BUF_SIZE];

  g_return_if_fail (number != NULL);
  g_return_if_fail (string != NULL);

  const char *names[] = {
    /* [GTK_CSS_NUMBER] = */ "",
    /* [GTK_CSS_PERCENT] = */ "%",
    /* [GTK_CSS_PX] = */ "px",
    /* [GTK_CSS_PT] = */ "pt",
    /* [GTK_CSS_EM] = */ "em",
    /* [GTK_CSS_EX] = */ "ex",
    /* [GTK_CSS_PC] = */ "pc",
    /* [GTK_CSS_IN] = */ "in",
    /* [GTK_CSS_CM] = */ "cm",
    /* [GTK_CSS_MM] = */ "mm"
  };

  g_ascii_dtostr (buf, sizeof (buf), number->value);
  g_string_append (string, buf);
  if (number->value != 0.0)
    g_string_append (string, names[number->unit]);
}
