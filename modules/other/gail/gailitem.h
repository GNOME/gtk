/* GAIL - The GNOME Accessibility Implementation Library
 * Copyright 2001, 2002, 2003 Sun Microsystems Inc.
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

#ifndef __GAIL_ITEM_H__
#define __GAIL_ITEM_H__

#include <gail/gailcontainer.h>
#include <libgail-util/gailtextutil.h>

G_BEGIN_DECLS

#define GAIL_TYPE_ITEM                          (gail_item_get_type ())
#define GAIL_ITEM(obj)                          (G_TYPE_CHECK_INSTANCE_CAST ((obj), GAIL_TYPE_ITEM, GailItem))
#define GAIL_ITEM_CLASS(klass)                  (G_TYPE_CHECK_CLASS_CAST ((klass), GAIL_TYPE_ITEM, GailItemClass))
#define GAIL_IS_ITEM(obj)                       (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GAIL_TYPE_ITEM))
#define GAIL_IS_ITEM_CLASS(klass)               (G_TYPE_CHECK_CLASS_TYPE ((klass), GAIL_TYPE_ITEM))
#define GAIL_ITEM_GET_CLASS(obj)                (G_TYPE_INSTANCE_GET_CLASS ((obj), GAIL_TYPE_ITEM, GailItemClass))

typedef struct _GailItem                   GailItem;
typedef struct _GailItemClass              GailItemClass;

struct _GailItem
{
  GailContainer parent;

  GailTextUtil  *textutil;

  gchar *text;
};

GType gail_item_get_type (void);

struct _GailItemClass
{
  GailContainerClass parent_class;
};

G_END_DECLS

#endif /* __GAIL_ITEM_H__ */
