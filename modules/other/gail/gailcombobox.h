/* GAIL - The GNOME Accessibility Implementation Library
 * Copyright 2004 Sun Microsystems Inc.
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

#ifndef __GAIL_COMBO_BOX_H__
#define __GAIL_COMBO_BOX_H__

#include <gail/gailcontainer.h>

G_BEGIN_DECLS

#define GAIL_TYPE_COMBO_BOX                      (gail_combo_box_get_type ())
#define GAIL_COMBO_BOX(obj)                      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GAIL_TYPE_COMBO_BOX, GailComboBox))
#define GAIL_COMBO_BOX_CLASS(klass)              (G_TYPE_CHECK_CLASS_CAST ((klass), GAIL_TYPE_COMBO_BOX, GailComboBoxClass))
#define GAIL_IS_COMBO_BOX(obj)                   (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GAIL_TYPE_COMBO_BOX))
#define GAIL_IS_COMBO_BOX_CLASS(klass)           (G_TYPE_CHECK_CLASS_TYPE ((klass), GAIL_TYPE_COMBO_BOX))
#define GAIL_COMBO_BOX_GET_CLASS(obj)            (G_TYPE_INSTANCE_GET_CLASS ((obj), GAIL_TYPE_COMBO_BOX, GailComboBoxClass))

typedef struct _GailComboBox              GailComboBox;
typedef struct _GailComboBoxClass         GailComboBoxClass;

struct _GailComboBox
{
  GailContainer parent;

  gchar         *press_keybinding;
  gchar         *press_description;
  guint         action_idle_handler;

  gchar         *name;
  gint          old_selection;
  gboolean      popup_set;
};

GType gail_combo_box_get_type (void);

struct _GailComboBoxClass
{
  GailContainerClass parent_class;
};

G_END_DECLS

#endif /* __GAIL_COMBO_BOX_H__ */
