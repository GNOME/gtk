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

#ifndef WIMP_STYLE_H
#define WIMP_STYLE_H

#include <gtk/gtkstyle.h>

typedef struct _WimpStyle WimpStyle;
typedef struct _WimpStyleClass WimpStyleClass;

extern GType wimp_type_style;

#define WIMP_TYPE_STYLE              wimp_type_style
#define WIMP_STYLE(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), WIMP_TYPE_STYLE, WimpStyle))
#define WIMP_STYLE_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), WIMP_TYPE_STYLE, WimpStyleClass))
#define WIMP_IS_STYLE(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), WIMP_TYPE_STYLE))
#define WIMP_IS_STYLE_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), WIMP_TYPE_STYLE))
#define WIMP_STYLE_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), WIMP_TYPE_STYLE, WimpStyleClass))

struct _WimpStyle
{
  GtkStyle parent_instance;
};

struct _WimpStyleClass
{
  GtkStyleClass parent_class;
};

void wimp_style_register_type (GTypeModule *module);
void wimp_style_init (void);
void wimp_style_setup_system_settings (void);

#endif /* WIMP_TYPE_STYLE */
