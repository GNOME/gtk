/* GAIL - The GNOME Accessibility Enabling Library
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

#ifndef __GAIL_RENDERER_CELL_FACTORY_H__
#define __GAIL_RENDERER_CELL_FACTORY_H__

#include <atk/atkobjectfactory.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GAIL_TYPE_RENDERER_CELL_FACTORY                 (gail_renderer_cell_factory_get_type ())
#define GAIL_RENDERER_CELL_FACTORY(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), GAIL_TYPE_RENDERER_CELL_FACTORY, GailRendererCellFactory))
#define GAIL_RENDERER_CELL_FACTORY_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), AGTK_TYPE_RENDERER_CELL_FACTORY, GailRendererCellFactoryClass))
#define GAIL_IS_RENDERER_CELL_FACTORY(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GAIL_TYPE_RENDERER_CELL_FACTORY))
#define GAIL_IS_RENDERER_CELL_FACTORY_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), GAIL_TYPE_RENDERER_CELL_FACTORY))
#define GAIL_RENDERER_CELL_FACTORY_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), GAIL_TYPE_RENDERER_CELL_FACTORY, GailRendererCellFactoryClass))

typedef struct _GailRendererCellFactory             GailRendererCellFactory;
typedef struct _GailRendererCellFactoryClass        GailRendererCellFactoryClass;

struct _GailRendererCellFactory
{
  AtkObjectFactory parent;
};

struct _GailRendererCellFactoryClass
{
  AtkObjectFactoryClass parent_class;
};

GType gail_renderer_cell_factory_get_type (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GAIL_RENDERER_CELL_FACTORY_H__ */
