/* GAIL - The GNOME Accessibility Implementation Library
 * Copyright 2003 Sun Microsystems Inc.
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

#ifndef __GAIL_EXPANDER_H__
#define __GAIL_EXPANDER_H__

#include <gail/gailcontainer.h>
#include <libgail-util/gailtextutil.h>

G_BEGIN_DECLS

#define GAIL_TYPE_EXPANDER              (gail_expander_get_type ())
#define GAIL_EXPANDER(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), GAIL_TYPE_EXPANDER, GailExpander))
#define GAIL_EXPANDER_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GAIL_TYPE_EXPANDER, GailExpanderClass))
#define GAIL_IS_EXPANDER(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GAIL_TYPE_EXPANDER))
#define GAIL_IS_EXPANDER_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GAIL_TYPE_EXPANDER))
#define GAIL_EXPANDER_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GAIL_TYPE_EXPANDER, GailExpanderClass))

typedef struct _GailExpander              GailExpander;
typedef struct _GailExpanderClass         GailExpanderClass;

struct _GailExpander
{
  GailContainer parent;

  gchar         *activate_description;
  gchar         *activate_keybinding;
  guint         action_idle_handler;

  GailTextUtil   *textutil;
};

GType gail_expander_get_type (void);

struct _GailExpanderClass
{
  GailContainerClass parent_class;
};

G_END_DECLS

#endif /* __GAIL_EXPANDER_H__ */
