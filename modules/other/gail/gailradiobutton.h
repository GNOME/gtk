/* GAIL - The GNOME Accessibility Implementation Library
 * Copyright 2001 Sun Microsystems Inc.
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

#ifndef __GAIL_RADIO_BUTTON_H__
#define __GAIL_RADIO_BUTTON_H__

#include <gail/gailtogglebutton.h>

G_BEGIN_DECLS

#define GAIL_TYPE_RADIO_BUTTON               (gail_radio_button_get_type ())
#define GAIL_RADIO_BUTTON(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), GAIL_TYPE_RADIO_BUTTON, GailRadioButton))
#define GAIL_RADIO_BUTTON_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), GAIL_TYPE_RADIO_BUTTON, GailRadioButtonClass))
#define GAIL_IS_RADIO_BUTTON(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GAIL_TYPE_RADIO_BUTTON))
#define GAIL_IS_RADIO_BUTTON_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), GAIL_TYPE_RADIO_BUTTON))
#define GAIL_RADIO_BUTTON_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), GAIL_TYPE_RADIO_BUTTON, GailRadioButtonClass))

typedef struct _GailRadioButton              GailRadioButton;
typedef struct _GailRadioButtonClass         GailRadioButtonClass;

struct _GailRadioButton
{
  GailToggleButton parent;

  GSList *old_group;
};

GType gail_radio_button_get_type (void);

struct _GailRadioButtonClass
{
  GailToggleButtonClass parent_class;
};

G_END_DECLS

#endif /* __GAIL_RADIO_BUTTON_H__ */
