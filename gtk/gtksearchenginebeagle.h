/*
 * Copyright (C) 2005 Novell, Inc.
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 *
 * Author: Anders Carlsson <andersca@imendio.com>
 *
 * Based on nautilus-search-engine-beagle.h
 */

#ifndef __GTK_SEARCH_ENGINE_BEAGLE_H__
#define __GTK_SEARCH_ENGINE_BEAGLE_H__

#include "gtksearchengine.h"

G_BEGIN_DECLS

#define GTK_TYPE_SEARCH_ENGINE_BEAGLE		(_gtk_search_engine_beagle_get_type ())
#define GTK_SEARCH_ENGINE_BEAGLE(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_SEARCH_ENGINE_BEAGLE, GtkSearchEngineBeagle))
#define GTK_SEARCH_ENGINE_BEAGLE_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_SEARCH_ENGINE_BEAGLE, GtkSearchEngineBeagleClass))
#define GTK_IS_SEARCH_ENGINE_BEAGLE(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_SEARCH_ENGINE_BEAGLE))
#define GTK_IS_SEARCH_ENGINE_BEAGLE_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_SEARCH_ENGINE_BEAGLE))
#define GTK_SEARCH_ENGINE_BEAGLE_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_SEARCH_ENGINE_BEAGLE, GtkSearchEngineBeagleClass))

typedef struct _GtkSearchEngineBeagle GtkSearchEngineBeagle;
typedef struct _GtkSearchEngineBeagleClass GtkSearchEngineBeagleClass;
typedef struct _GtkSearchEngineBeaglePrivate GtkSearchEngineBeaglePrivate;

struct _GtkSearchEngineBeagle 
{
  GtkSearchEngine parent;

  GtkSearchEngineBeaglePrivate *priv;
};

struct _GtkSearchEngineBeagleClass 
{
  GtkSearchEngineClass parent_class;
};

GType            _gtk_search_engine_beagle_get_type (void);

GtkSearchEngine* _gtk_search_engine_beagle_new      (void);

G_END_DECLS

#endif /* __GTK_SEARCH_ENGINE_BEAGLE_H__ */
