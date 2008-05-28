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

#ifndef __GAIL_BUTTON_H__
#define __GAIL_BUTTON_H__

#include <gail/gailcontainer.h>
#include <libgail-util/gailtextutil.h>

G_BEGIN_DECLS

#define GAIL_TYPE_BUTTON                     (gail_button_get_type ())
#define GAIL_BUTTON(obj)                     (G_TYPE_CHECK_INSTANCE_CAST ((obj), GAIL_TYPE_BUTTON, GailButton))
#define GAIL_BUTTON_CLASS(klass)             (G_TYPE_CHECK_CLASS_CAST ((klass), GAIL_TYPE_BUTTON, GailButtonClass))
#define GAIL_IS_BUTTON(obj)                  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GAIL_TYPE_BUTTON))
#define GAIL_IS_BUTTON_CLASS(klass)          (G_TYPE_CHECK_CLASS_TYPE ((klass), GAIL_TYPE_BUTTON))
#define GAIL_BUTTON_GET_CLASS(obj)           (G_TYPE_INSTANCE_GET_CLASS ((obj), GAIL_TYPE_BUTTON, GailButtonClass))

typedef struct _GailButton                   GailButton;
typedef struct _GailButtonClass              GailButtonClass;

struct _GailButton
{
  GailContainer parent;

  /*
   * Cache the widget state so we know the previous state when it changed
   */
  gint8         state;

  gchar         *click_description;
  gchar         *press_description;
  gchar         *release_description;
  gchar         *click_keybinding;
  guint         action_idle_handler;
  GQueue        *action_queue;

  GailTextUtil	 *textutil;

  gboolean     default_is_press;
};

GType gail_button_get_type (void);

struct _GailButtonClass
{
  GailContainerClass parent_class;
};

G_END_DECLS

#endif /* __GAIL_BUTTON_H__ */
