/* MS-Windows Engine (aka GTK-Wimp)
 *
 * Copyright (C) 2003, 2004 Raymond Penners <raymond@dotsphinx.com>
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

#include "msw_style.h"
#include "msw_rc_style.h"

static void msw_rc_style_init (MswRcStyle * style);
static void msw_rc_style_class_init (MswRcStyleClass * klass);
static GtkStyle *msw_rc_style_create_style (GtkRcStyle * rc_style);

static GtkRcStyleClass *parent_class;

GType msw_type_rc_style = 0;

void
msw_rc_style_register_type (GTypeModule *module)
{
  const GTypeInfo object_info = {
    sizeof (MswRcStyleClass),
    (GBaseInitFunc) NULL,
    (GBaseFinalizeFunc) NULL,
    (GClassInitFunc) msw_rc_style_class_init,
    NULL,			/* class_finalize */
    NULL,			/* class_data */
    sizeof (MswRcStyle),
    0,				/* n_preallocs */
    (GInstanceInitFunc) msw_rc_style_init,
  };

  msw_type_rc_style = g_type_module_register_type (module,
						   GTK_TYPE_RC_STYLE,
						   "MswRcStyle",
						   &object_info, 0);
}

static void
msw_rc_style_init (MswRcStyle *style)
{
}

static void
msw_rc_style_class_init (MswRcStyleClass *klass)
{
  GtkRcStyleClass *rc_style_class = GTK_RC_STYLE_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  rc_style_class->create_style = msw_rc_style_create_style;
}

/* Create an empty style suitable to this RC style
 */
static GtkStyle *
msw_rc_style_create_style (GtkRcStyle *rc_style)
{
  return g_object_new (MSW_TYPE_STYLE, NULL);
}
