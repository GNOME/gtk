/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/* GdkPixbuf library - Private declarations
 *
 * Copyright (C) 1999 The Free Software Foundation
 *
 * Authors: Mark Crichton <crichton@gimp.org>
 *          Miguel de Icaza <miguel@gnu.org>
 *          Federico Mena-Quintero <federico@gimp.org>
 *          Havoc Pennington <hp@redhat.com>
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
 * Boston, MA 02111-1307, USA.
 */

#ifndef GDK_PIXBUF_ANIMATION_H
#define GDK_PIXBUF_ANIMATION_H

#include "gdk-pixbuf/gdk-pixbuf.h"

G_BEGIN_DECLS

#ifdef GDK_PIXBUF_ENABLE_BACKEND



typedef struct _GdkPixbufAnimationClass GdkPixbufAnimationClass;

#define GDK_PIXBUF_ANIMATION_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_PIXBUF_ANIMATION, GdkPixbufAnimationClass))
#define GDK_IS_PIXBUF_ANIMATION_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_PIXBUF_ANIMATION))
#define GDK_PIXBUF_ANIMATION_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_PIXBUF_ANIMATION, GdkPixbufAnimationClass))

/* Private part of the GdkPixbufAnimation structure */
struct _GdkPixbufAnimation {
        GObject parent_instance;

};

struct _GdkPixbufAnimationClass {
        GObjectClass parent_class;

        /*< public >*/

        gboolean                (*is_static_image)  (GdkPixbufAnimation *anim);

        GdkPixbuf*              (*get_static_image) (GdkPixbufAnimation *anim);
        
        void                    (*get_size) (GdkPixbufAnimation *anim,
                                             int                 *width,
                                             int                 *height);
        
        GdkPixbufAnimationIter* (*get_iter) (GdkPixbufAnimation *anim,
                                             const GTimeVal     *start_time);

};



typedef struct _GdkPixbufAnimationIterClass GdkPixbufAnimationIterClass;

#define GDK_PIXBUF_ANIMATION_ITER_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_PIXBUF_ANIMATION_ITER, GdkPixbufAnimationIterClass))
#define GDK_IS_PIXBUF_ANIMATION_ITER_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_PIXBUF_ANIMATION_ITER))
#define GDK_PIXBUF_ANIMATION_ITER_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_PIXBUF_ANIMATION_ITER, GdkPixbufAnimationIterClass))

struct _GdkPixbufAnimationIter {
        GObject parent_instance;

};

struct _GdkPixbufAnimationIterClass {
        GObjectClass parent_class;

        /*< public >*/

        int        (*get_delay_time)   (GdkPixbufAnimationIter *iter);

        GdkPixbuf* (*get_pixbuf)       (GdkPixbufAnimationIter *iter);

        gboolean (*on_currently_loading_frame) (GdkPixbufAnimationIter *iter);

        gboolean (*advance)            (GdkPixbufAnimationIter *iter,
                                        const GTimeVal         *current_time);
};
      

GdkPixbufAnimation* gdk_pixbuf_non_anim_new (GdkPixbuf *pixbuf);

#endif /* GDK_PIXBUF_ENABLE_BACKEND */

G_END_DECLS

#endif /* GDK_PIXBUF_ANIMATION_H */
