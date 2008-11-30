/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/* GdkPixbuf library - Simple frame-based animations
 *
 * Copyright (C) Dom Lachowicz
 *
 * Authors: Dom Lachowicz <cinamod@hotmail.com>
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
 *
 * Based on code originally by:
 *          Jonathan Blandford <jrb@redhat.com>
 *          Havoc Pennington <hp@redhat.com>
 */

#include <glib.h>

#include "gdk-pixbuf.h"
#include "gdk-pixbuf-io.h"
#include "gdk-pixbuf-simple-anim.h"
#include "gdk-pixbuf-alias.h"

struct _GdkPixbufSimpleAnimClass
{
        GdkPixbufAnimationClass parent_class;
};

/* Private part of the GdkPixbufSimpleAnim structure */
struct _GdkPixbufSimpleAnim
{
        GdkPixbufAnimation parent_instance;
        
        gint n_frames;
        
        gfloat rate;
        gint total_time;
        
        GList *frames;
        
        gint width;
        gint height;
        
        gboolean loop;
};


typedef struct _GdkPixbufSimpleAnimIter GdkPixbufSimpleAnimIter;
typedef struct _GdkPixbufSimpleAnimIterClass GdkPixbufSimpleAnimIterClass;

#define GDK_TYPE_PIXBUF_SIMPLE_ANIM_ITER              (gdk_pixbuf_simple_anim_iter_get_type ())
#define GDK_PIXBUF_SIMPLE_ANIM_ITER(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_PIXBUF_SIMPLE_ANIM_ITER, GdkPixbufSimpleAnimIter))
#define GDK_IS_PIXBUF_SIMPLE_ANIM_ITER(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_PIXBUF_SIMPLE_ANIM_ITER))

#define GDK_PIXBUF_SIMPLE_ANIM_ITER_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_PIXBUF_SIMPLE_ANIM_ITER, GdkPixbufSimpleAnimIterClass))
#define GDK_IS_PIXBUF_SIMPLE_ANIM_ITER_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_PIXBUF_SIMPLE_ANIM_ITER))
#define GDK_PIXBUF_SIMPLE_ANIM_ITER_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_PIXBUF_SIMPLE_ANIM_ITER, GdkPixbufSimpleAnimIterClass))

GType gdk_pixbuf_simple_anim_iter_get_type (void) G_GNUC_CONST;


struct _GdkPixbufSimpleAnimIterClass
{
        GdkPixbufAnimationIterClass parent_class;
};

struct _GdkPixbufSimpleAnimIter
{
        GdkPixbufAnimationIter parent_instance;
        
        GdkPixbufSimpleAnim *simple_anim;
        
        GTimeVal start_time;
        GTimeVal current_time;
        
        gint position;
        
        GList *current_frame;
};

typedef struct _GdkPixbufFrame GdkPixbufFrame;
struct _GdkPixbufFrame
{
        GdkPixbuf *pixbuf;
        gint delay_time;
        gint elapsed;
};

static void gdk_pixbuf_simple_anim_finalize (GObject *object);

static gboolean   is_static_image  (GdkPixbufAnimation *animation);
static GdkPixbuf *get_static_image (GdkPixbufAnimation *animation);

static void       get_size         (GdkPixbufAnimation *anim,
                                    gint               *width, 
                                    gint               *height);
static GdkPixbufAnimationIter *get_iter (GdkPixbufAnimation *anim,
                                         const GTimeVal     *start_time);


G_DEFINE_TYPE (GdkPixbufSimpleAnim, gdk_pixbuf_simple_anim, GDK_TYPE_PIXBUF_ANIMATION)

static void
gdk_pixbuf_simple_anim_init (GdkPixbufSimpleAnim *anim)
{
}

static void
gdk_pixbuf_simple_anim_class_init (GdkPixbufSimpleAnimClass *klass)
{
        GObjectClass *object_class;
        GdkPixbufAnimationClass *anim_class;

        object_class = G_OBJECT_CLASS (klass);
        anim_class = GDK_PIXBUF_ANIMATION_CLASS (klass);
        
        object_class->finalize = gdk_pixbuf_simple_anim_finalize;
        
        anim_class->is_static_image = is_static_image;
        anim_class->get_static_image = get_static_image;
        anim_class->get_size = get_size;
        anim_class->get_iter = get_iter;
}

static void
gdk_pixbuf_simple_anim_finalize (GObject *object)
{
        GdkPixbufSimpleAnim *anim;
        GList *l;
        GdkPixbufFrame *frame;
        
        anim = GDK_PIXBUF_SIMPLE_ANIM (object);        
        
        for (l = anim->frames; l; l = l->next) {
                frame = l->data;
                g_object_unref (frame->pixbuf);
                g_free (frame);
        }
        
        g_list_free (anim->frames);
        
        G_OBJECT_CLASS (gdk_pixbuf_simple_anim_parent_class)->finalize (object);
}

static gboolean
is_static_image (GdkPixbufAnimation *animation)
{
        GdkPixbufSimpleAnim *anim;
        
        anim = GDK_PIXBUF_SIMPLE_ANIM (animation);

        return (anim->frames != NULL && anim->frames->next == NULL);
}

static GdkPixbuf *
get_static_image (GdkPixbufAnimation *animation)
{
        GdkPixbufSimpleAnim *anim;
        
        anim = GDK_PIXBUF_SIMPLE_ANIM (animation);
        
        if (anim->frames == NULL)
                return NULL;
        else
                return ((GdkPixbufFrame *)anim->frames->data)->pixbuf;
}

static void
get_size (GdkPixbufAnimation *animation,
          gint               *width, 
          gint               *height)
{
        GdkPixbufSimpleAnim *anim;

        anim = GDK_PIXBUF_SIMPLE_ANIM (animation);
        
        if (width)
                *width = anim->width;
        
        if (height)
                *height = anim->height;
}

static void
iter_clear (GdkPixbufSimpleAnimIter *iter)
{
        iter->current_frame = NULL;
}

static void
iter_restart (GdkPixbufSimpleAnimIter *iter)
{
        iter_clear (iter);
        
        iter->current_frame = iter->simple_anim->frames;
}

static GdkPixbufAnimationIter *
get_iter (GdkPixbufAnimation *anim,
          const GTimeVal    *start_time)
{
        GdkPixbufSimpleAnimIter *iter;
        
        iter = g_object_new (GDK_TYPE_PIXBUF_SIMPLE_ANIM_ITER, NULL);

        iter->simple_anim = GDK_PIXBUF_SIMPLE_ANIM (anim);

        g_object_ref (iter->simple_anim);
        
        iter_restart (iter);
        
        iter->start_time = *start_time;
        iter->current_time = *start_time;
        
        return GDK_PIXBUF_ANIMATION_ITER (iter);
}

static void gdk_pixbuf_simple_anim_iter_finalize (GObject *object);

static gint       get_delay_time             (GdkPixbufAnimationIter *iter);
static GdkPixbuf *get_pixbuf                 (GdkPixbufAnimationIter *iter);
static gboolean   on_currently_loading_frame (GdkPixbufAnimationIter *iter);
static gboolean   advance                    (GdkPixbufAnimationIter *iter,
                                              const GTimeVal         *current_time);

G_DEFINE_TYPE (GdkPixbufSimpleAnimIter, gdk_pixbuf_simple_anim_iter, GDK_TYPE_PIXBUF_ANIMATION_ITER)

static void
gdk_pixbuf_simple_anim_iter_init (GdkPixbufSimpleAnimIter *iter)
{
}

static void
gdk_pixbuf_simple_anim_iter_class_init (GdkPixbufSimpleAnimIterClass *klass)
{
        GObjectClass *object_class;
        GdkPixbufAnimationIterClass *anim_iter_class;

        object_class = G_OBJECT_CLASS (klass);
        anim_iter_class = GDK_PIXBUF_ANIMATION_ITER_CLASS (klass);
        
        object_class->finalize = gdk_pixbuf_simple_anim_iter_finalize;
        
        anim_iter_class->get_delay_time = get_delay_time;
        anim_iter_class->get_pixbuf = get_pixbuf;
        anim_iter_class->on_currently_loading_frame = on_currently_loading_frame;
        anim_iter_class->advance = advance;
}

static void
gdk_pixbuf_simple_anim_iter_finalize (GObject *object)
{
        GdkPixbufSimpleAnimIter *iter;
        
        iter = GDK_PIXBUF_SIMPLE_ANIM_ITER (object);
        iter_clear (iter);
        
        g_object_unref (iter->simple_anim);
        
        G_OBJECT_CLASS (gdk_pixbuf_simple_anim_iter_parent_class)->finalize (object);
}

static gboolean
advance (GdkPixbufAnimationIter *anim_iter,
         const GTimeVal         *current_time)
{
        GdkPixbufSimpleAnimIter *iter;
        gint elapsed;
        gint loop;
        GList *tmp;
        GList *old;
        
        iter = GDK_PIXBUF_SIMPLE_ANIM_ITER (anim_iter);
        
        iter->current_time = *current_time;
        
        /* We use milliseconds for all times */
        elapsed = (((iter->current_time.tv_sec - iter->start_time.tv_sec) * G_USEC_PER_SEC +
                    iter->current_time.tv_usec - iter->start_time.tv_usec)) / 1000;
        
        if (elapsed < 0) {
                /* Try to compensate; probably the system clock
                 * was set backwards
                 */
                iter->start_time = iter->current_time;
                elapsed = 0;
        }
        
        g_assert (iter->simple_anim->total_time > 0);
        
        /* See how many times we've already played the full animation,
         * and subtract time for that.
         */
        loop = elapsed / iter->simple_anim->total_time;
        elapsed = elapsed % iter->simple_anim->total_time;
        
        iter->position = elapsed;
        
        /* Now move to the proper frame */
        if (loop < 1)
                tmp = iter->simple_anim->frames;
        else
                tmp = NULL;
        
        while (tmp != NULL) {
                GdkPixbufFrame *frame = tmp->data;
                
                if (iter->position >= frame->elapsed &&
                    iter->position < (frame->elapsed + frame->delay_time))
                        break;
                
                tmp = tmp->next;
        }
        
        old = iter->current_frame;
        
        iter->current_frame = tmp;
        
        return iter->current_frame != old;
}

static gint
get_delay_time (GdkPixbufAnimationIter *anim_iter)
{
        GdkPixbufFrame *frame;
        GdkPixbufSimpleAnimIter *iter;

        iter = GDK_PIXBUF_SIMPLE_ANIM_ITER (anim_iter);
        
        if (iter->current_frame) {
                frame = iter->current_frame->data;
                return frame->delay_time - (iter->position - frame->elapsed);
        }
        else {
                return -1;		/* show last frame forever */
        }
}

static GdkPixbuf *
get_pixbuf (GdkPixbufAnimationIter *anim_iter)
{
        GdkPixbufSimpleAnimIter *iter;
        GdkPixbufFrame *frame;
        
        iter = GDK_PIXBUF_SIMPLE_ANIM_ITER (anim_iter);
        
        if (iter->current_frame)
                frame = iter->current_frame->data;
        else if (g_list_length (iter->simple_anim->frames) > 0)
                frame = g_list_last (iter->simple_anim->frames)->data;
        else
                frame = NULL;

        if (frame == NULL)
                return NULL;
        
        return frame->pixbuf;
}

static gboolean
on_currently_loading_frame (GdkPixbufAnimationIter *anim_iter)
{
  GdkPixbufSimpleAnimIter *iter;

  iter = GDK_PIXBUF_SIMPLE_ANIM_ITER (anim_iter);

  return iter->current_frame == NULL || iter->current_frame->next == NULL;
}

/**
 * gdk_pixbuf_simple_anim_new:
 * @width: the width of the animation
 * @height: the height of the animation
 * @rate: the speed of the animation, in frames per second
 *
 * Creates a new, empty animation.
 *
 * Returns: a newly allocated #GdkPixbufSimpleAnim 
 *
 * Since: 2.8
 */
GdkPixbufSimpleAnim *
gdk_pixbuf_simple_anim_new (gint   width, 
                            gint   height, 
                            gfloat rate)
{
  GdkPixbufSimpleAnim *anim;

  anim = g_object_new (GDK_TYPE_PIXBUF_SIMPLE_ANIM, NULL);
  anim->width = width;
  anim->height = height;
  anim->rate = rate;

  return anim;
}

/**
 * gdk_pixbuf_simple_anim_add_frame:
 * @animation: a #GdkPixbufSimpleAnim
 * @pixbuf: the pixbuf to add 
 *
 * Adds a new frame to @animation. The @pixbuf must
 * have the dimensions specified when the animation 
 * was constructed.
 *
 * Since: 2.8
 */
void
gdk_pixbuf_simple_anim_add_frame (GdkPixbufSimpleAnim *animation,
                                  GdkPixbuf           *pixbuf)
{
  GdkPixbufFrame *frame;
  int nframe = 0;
  
  g_return_if_fail (GDK_IS_PIXBUF_SIMPLE_ANIM (animation));
  g_return_if_fail (GDK_IS_PIXBUF (pixbuf));
  
  nframe = g_list_length (animation->frames);
  
  frame = g_new0 (GdkPixbufFrame, 1);
  frame->delay_time = (gint) (1000 / animation->rate);
  frame->elapsed = (gint) (frame->delay_time * nframe);
  animation->total_time += frame->delay_time;
  frame->pixbuf = g_object_ref (pixbuf);

  animation->frames = g_list_append (animation->frames, frame);
}


#define __GDK_PIXBUF_SIMPLE_ANIM_C__
#include "gdk-pixbuf-aliasdef.c"
