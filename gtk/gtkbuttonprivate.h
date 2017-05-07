/* GTK - The GIMP Toolkit
 *
 * Copyright (C) 2010 Javier Jardón
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
 * License along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GTK_BUTTON_PRIVATE_H__
#define __GTK_BUTTON_PRIVATE_H__

#include "gtkactionhelper.h"
#include "gtkgesturesingle.h"

G_BEGIN_DECLS


struct _GtkButtonPrivate
{
  GtkActionHelper       *action_helper;

  GdkDevice             *grab_keyboard;

  GtkGesture            *gesture;

  guint                  activate_timeout;

  guint          button_down           : 1;
  guint          in_button             : 1;
  guint          use_underline         : 1;
  guint          child_type            : 2;
};


G_END_DECLS

#endif /* __GTK_BUTTON_PRIVATE_H__ */
