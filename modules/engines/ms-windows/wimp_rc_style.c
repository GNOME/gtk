/* Wimp "Windows Impersonator" Engine
 *
 * Copyright (C) 2003 Raymond Penners <raymond@dotsphinx.com>
 * Includes code adapted from redmond95 by Owen Taylor, and
 * gtk-nativewin by Evan Martin
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "wimp_style.h"
#include "wimp_rc_style.h"

static void      wimp_rc_style_init         (WimpRcStyle      *style);
static void      wimp_rc_style_class_init   (WimpRcStyleClass *klass);
static GtkStyle *wimp_rc_style_create_style (GtkRcStyle         *rc_style);

static GtkRcStyleClass *parent_class;

GType wimp_type_rc_style = 0;

void
wimp_rc_style_register_type (GTypeModule *module)
{
  static const GTypeInfo object_info =
  {
    sizeof (WimpRcStyleClass),
    (GBaseInitFunc) NULL,
    (GBaseFinalizeFunc) NULL,
    (GClassInitFunc) wimp_rc_style_class_init,
    NULL,           /* class_finalize */
    NULL,           /* class_data */
    sizeof (WimpRcStyle),
    0,              /* n_preallocs */
    (GInstanceInitFunc) wimp_rc_style_init,
  };
  
  wimp_type_rc_style = g_type_module_register_type (module,
						      GTK_TYPE_RC_STYLE,
						      "WimpRcStyle",
						      &object_info, 0);
}

static void
wimp_rc_style_init (WimpRcStyle *style)
{
}

static void
wimp_rc_style_class_init (WimpRcStyleClass *klass)
{
  GtkRcStyleClass *rc_style_class = GTK_RC_STYLE_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  rc_style_class->create_style = wimp_rc_style_create_style;
}

/* Create an empty style suitable to this RC style
 */
static GtkStyle *
wimp_rc_style_create_style (GtkRcStyle *rc_style)
{
  return GTK_STYLE (g_object_new (WIMP_TYPE_STYLE, NULL));
}

