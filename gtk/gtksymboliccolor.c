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

#include "config.h"
#include "gtksymboliccolor.h"
#include "gtkstyleset.h"
#include "gtkintl.h"

/* Symbolic colors */
typedef enum {
  COLOR_TYPE_LITERAL,
  COLOR_TYPE_NAME,
  COLOR_TYPE_SHADE,
  COLOR_TYPE_MIX
} ColorType;

struct GtkSymbolicColor
{
  ColorType type;
  guint ref_count;

  union
  {
    GdkColor color;
    gchar *name;

    struct
    {
      GtkSymbolicColor *color;
      gdouble factor;
    } shade;

    struct
    {
      GtkSymbolicColor *color1;
      GtkSymbolicColor *color2;
      gdouble factor;
    } mix;
  };
};

GtkSymbolicColor *
gtk_symbolic_color_new_literal (GdkColor *color)
{
  GtkSymbolicColor *symbolic_color;

  g_return_val_if_fail (color != NULL, NULL);

  symbolic_color = g_slice_new0 (GtkSymbolicColor);
  symbolic_color->type = COLOR_TYPE_LITERAL;
  symbolic_color->color = *color;

  return symbolic_color;
}

GtkSymbolicColor *
gtk_symbolic_color_new_name (const gchar *name)
{
  GtkSymbolicColor *symbolic_color;

  g_return_val_if_fail (name != NULL, NULL);

  symbolic_color = g_slice_new0 (GtkSymbolicColor);
  symbolic_color->type = COLOR_TYPE_NAME;
  symbolic_color->name = g_strdup (name);

  return symbolic_color;
}

GtkSymbolicColor *
gtk_symbolic_color_new_shade (GtkSymbolicColor *color,
                              gdouble           factor)
{
  GtkSymbolicColor *symbolic_color;

  g_return_val_if_fail (color != NULL, NULL);

  symbolic_color = g_slice_new0 (GtkSymbolicColor);
  symbolic_color->type = COLOR_TYPE_SHADE;
  symbolic_color->shade.color = gtk_symbolic_color_ref (color);
  symbolic_color->shade.factor = CLAMP (factor, 0, 1);

  return symbolic_color;
}

GtkSymbolicColor *
gtk_symbolic_color_new_mix (GtkSymbolicColor *color1,
                            GtkSymbolicColor *color2,
                            gdouble           factor)
{
  GtkSymbolicColor *symbolic_color;

  g_return_val_if_fail (color1 != NULL, NULL);
  g_return_val_if_fail (color1 != NULL, NULL);

  symbolic_color = g_slice_new0 (GtkSymbolicColor);
  symbolic_color->type = COLOR_TYPE_MIX;
  symbolic_color->mix.color1 = gtk_symbolic_color_ref (color1);
  symbolic_color->mix.color2 = gtk_symbolic_color_ref (color2);
  symbolic_color->mix.factor = CLAMP (factor, 0, 1);

  return symbolic_color;
}

GtkSymbolicColor *
gtk_symbolic_color_ref (GtkSymbolicColor *color)
{
  g_return_val_if_fail (color != NULL, NULL);

  color->ref_count++;

  return color;
}

void
gtk_symbolic_color_unref (GtkSymbolicColor *color)
{
  g_return_if_fail (color != NULL);

  color->ref_count--;
}

GType
gtk_symbolic_color_get_type (void)
{
  static GType type = 0;

  if (G_UNLIKELY (!type))
    type = g_boxed_type_register_static (I_("GtkSymbolicColor"),
					 (GBoxedCopyFunc) gtk_symbolic_color_ref,
					 (GBoxedFreeFunc) gtk_symbolic_color_unref);

  return type;
}

#define __GTK_SYMBOLIC_COLOR_C__
#include "gtkaliasdef.c"
