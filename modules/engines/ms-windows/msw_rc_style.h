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

#ifndef MSW_RC_STYLE_H
#define MSW_RC_STYLE_H

#include "gtk/gtk.h"

typedef struct _MswRcStyle MswRcStyle;
typedef struct _MswRcStyleClass MswRcStyleClass;

extern GType msw_type_rc_style;

#define MSW_TYPE_RC_STYLE              msw_type_rc_style
#define MSW_RC_STYLE(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), MSW_TYPE_RC_STYLE, MswRcStyle))
#define MSW_RC_STYLE_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), MSW_TYPE_RC_STYLE, MswRcStyleClass))
#define MSW_IS_RC_STYLE(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), MSW_TYPE_RC_STYLE))
#define MSW_IS_RC_STYLE_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), MSW_TYPE_RC_STYLE))
#define MSW_RC_STYLE_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), MSW_TYPE_RC_STYLE, MswRcStyleClass))

struct _MswRcStyle
{
  GtkRcStyle parent_instance;
  
  GList *img_list;
};

struct _MswRcStyleClass
{
  GtkRcStyleClass parent_class;
};

void msw_rc_style_register_type (GTypeModule *module);

#endif /* MSW_TYPE_RC_STYLE */
