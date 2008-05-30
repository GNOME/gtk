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

#ifndef MSW_STYLE_H
#define MSW_STYLE_H

#include "gtk/gtk.h"

typedef struct _MswStyle MswStyle;
typedef struct _MswStyleClass MswStyleClass;

extern GType msw_type_style;

#define MSW_TYPE_STYLE              msw_type_style
#define MSW_STYLE(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), MSW_TYPE_STYLE, MswStyle))
#define MSW_STYLE_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), MSW_TYPE_STYLE, MswStyleClass))
#define MSW_IS_STYLE(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), MSW_TYPE_STYLE))
#define MSW_IS_STYLE_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), MSW_TYPE_STYLE))
#define MSW_STYLE_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), MSW_TYPE_STYLE, MswStyleClass))

struct _MswStyle
{
  GtkStyle parent_instance;
};

struct _MswStyleClass
{
  GtkStyleClass parent_class;
};

void msw_style_register_type (GTypeModule *module);
void msw_style_init (void);
void msw_style_finalize (void);
void msw_style_setup_system_settings (void);

#endif /* MSW_TYPE_STYLE */
