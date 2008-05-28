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

#ifndef __GAIL_IMAGE_H__
#define __GAIL_IMAGE_H__

#include <gail/gailwidget.h>

G_BEGIN_DECLS

#define GAIL_TYPE_IMAGE                      (gail_image_get_type ())
#define GAIL_IMAGE(obj)                      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GAIL_TYPE_IMAGE, GailImage))
#define GAIL_IMAGE_CLASS(klass)              (G_TYPE_CHECK_CLASS_CAST ((klass), GAIL_TYPE_IMAGE, GailImageClass))
#define GAIL_IS_IMAGE(obj)                   (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GAIL_TYPE_IMAGE))
#define GAIL_IS_IMAGE_CLASS(klass)           (G_TYPE_CHECK_CLASS_TYPE ((klass), GAIL_TYPE_IMAGE))
#define GAIL_IMAGE_GET_CLASS(obj)            (G_TYPE_INSTANCE_GET_CLASS ((obj), GAIL_TYPE_IMAGE, GailImageClass))

typedef struct _GailImage              GailImage;
typedef struct _GailImageClass         GailImageClass;

struct _GailImage
{
  GailWidget parent;

  gchar*     image_description;
  gchar*     stock_name;
};

GType gail_image_get_type (void);

struct _GailImageClass
{
  GailWidgetClass parent_class;
};

G_END_DECLS

#endif /* __GAIL_IMAGE_H__ */
