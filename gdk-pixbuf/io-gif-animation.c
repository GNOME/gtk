/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/* GdkPixbuf library - animated gif support
 *
 * Copyright (C) 1999 The Free Software Foundation
 *
 * Authors: Jonathan Blandford <jrb@redhat.com>
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

#include <config.h>
#include <errno.h>
#include "gdk-pixbuf-io.h"
#include "gdk-pixbuf-private.h"
#include "io-gif-animation.h"

static void gdk_pixbuf_gif_anim_class_init (GdkPixbufGifAnimClass *klass);
static void gdk_pixbuf_gif_anim_finalize   (GObject        *object);

static gboolean                gdk_pixbuf_gif_anim_is_static_image  (GdkPixbufAnimation *animation);
static GdkPixbuf*              gdk_pixbuf_gif_anim_get_static_image (GdkPixbufAnimation *animation);

static void                    gdk_pixbuf_gif_anim_get_size (GdkPixbufAnimation *anim,
                                                             int                *width,
                                                             int                *height);
static GdkPixbufAnimationIter* gdk_pixbuf_gif_anim_get_iter (GdkPixbufAnimation *anim,
                                                             const GTimeVal     *start_time);




static gpointer parent_class;

GType
gdk_pixbuf_gif_anim_get_type (void)
{
        static GType object_type = 0;

        if (!object_type) {
                static const GTypeInfo object_info = {
                        sizeof (GdkPixbufGifAnimClass),
                        (GBaseInitFunc) NULL,
                        (GBaseFinalizeFunc) NULL,
                        (GClassInitFunc) gdk_pixbuf_gif_anim_class_init,
                        NULL,           /* class_finalize */
                        NULL,           /* class_data */
                        sizeof (GdkPixbufGifAnim),
                        0,              /* n_preallocs */
                        (GInstanceInitFunc) NULL,
                };
                
                object_type = g_type_register_static (GDK_TYPE_PIXBUF_ANIMATION,
                                                      "GdkPixbufGifAnim",
                                                      &object_info, 0);
        }
        
        return object_type;
}

static void
gdk_pixbuf_gif_anim_class_init (GdkPixbufGifAnimClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GdkPixbufAnimationClass *anim_class = GDK_PIXBUF_ANIMATION_CLASS (klass);
        
        parent_class = g_type_class_peek_parent (klass);
        
        object_class->finalize = gdk_pixbuf_gif_anim_finalize;

        anim_class->is_static_image = gdk_pixbuf_gif_anim_is_static_image;
        anim_class->get_static_image = gdk_pixbuf_gif_anim_get_static_image;
        anim_class->get_size = gdk_pixbuf_gif_anim_get_size;
        anim_class->get_iter = gdk_pixbuf_gif_anim_get_iter;
}

static void
gdk_pixbuf_gif_anim_finalize (GObject *object)
{
        GdkPixbufGifAnim *gif_anim = GDK_PIXBUF_GIF_ANIM (object);

        GList *l;
        GdkPixbufFrame *frame;
        
        for (l = gif_anim->frames; l; l = l->next) {
                frame = l->data;
                g_object_unref (frame->pixbuf);
                if (frame->composited)
                        g_object_unref (frame->composited);
                if (frame->revert)
                        g_object_unref (frame->revert);
                g_free (frame);
        }
        
        g_list_free (gif_anim->frames);
        
        G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gboolean
gdk_pixbuf_gif_anim_is_static_image  (GdkPixbufAnimation *animation)
{
        GdkPixbufGifAnim *gif_anim;

        gif_anim = GDK_PIXBUF_GIF_ANIM (animation);

        return (gif_anim->frames != NULL &&
                gif_anim->frames->next == NULL);
}

static GdkPixbuf*
gdk_pixbuf_gif_anim_get_static_image (GdkPixbufAnimation *animation)
{
        GdkPixbufGifAnim *gif_anim;

        gif_anim = GDK_PIXBUF_GIF_ANIM (animation);

        if (gif_anim->frames == NULL)
                return NULL;
        else
                return GDK_PIXBUF (((GdkPixbufFrame*)gif_anim->frames->data)->pixbuf);        
}

static void
gdk_pixbuf_gif_anim_get_size (GdkPixbufAnimation *anim,
                              int                *width,
                              int                *height)
{
        GdkPixbufGifAnim *gif_anim;

        gif_anim = GDK_PIXBUF_GIF_ANIM (anim);

        if (width)
                *width = gif_anim->width;

        if (height)
                *height = gif_anim->height;
}


static void
iter_clear (GdkPixbufGifAnimIter *iter)
{
        iter->current_frame = NULL;
}

static void
iter_restart (GdkPixbufGifAnimIter *iter)
{
        iter_clear (iter);
  
        iter->current_frame = iter->gif_anim->frames;
}

static GdkPixbufAnimationIter*
gdk_pixbuf_gif_anim_get_iter (GdkPixbufAnimation *anim,
                              const GTimeVal     *start_time)
{
        GdkPixbufGifAnimIter *iter;

        iter = g_object_new (GDK_TYPE_PIXBUF_GIF_ANIM_ITER, NULL);

        iter->gif_anim = GDK_PIXBUF_GIF_ANIM (anim);

        g_object_ref (iter->gif_anim);
        
        iter_restart (iter);

        iter->start_time = *start_time;
        iter->current_time = *start_time;
        
        return GDK_PIXBUF_ANIMATION_ITER (iter);
}



static void gdk_pixbuf_gif_anim_iter_class_init (GdkPixbufGifAnimIterClass *klass);
static void gdk_pixbuf_gif_anim_iter_finalize   (GObject                   *object);

static int        gdk_pixbuf_gif_anim_iter_get_delay_time             (GdkPixbufAnimationIter *iter);
static GdkPixbuf* gdk_pixbuf_gif_anim_iter_get_pixbuf                 (GdkPixbufAnimationIter *iter);
static gboolean   gdk_pixbuf_gif_anim_iter_on_currently_loading_frame (GdkPixbufAnimationIter *iter);
static gboolean   gdk_pixbuf_gif_anim_iter_advance                    (GdkPixbufAnimationIter *iter,
                                                                       const GTimeVal         *current_time);



static gpointer iter_parent_class;

GType
gdk_pixbuf_gif_anim_iter_get_type (void)
{
        static GType object_type = 0;

        if (!object_type) {
                static const GTypeInfo object_info = {
                        sizeof (GdkPixbufGifAnimIterClass),
                        (GBaseInitFunc) NULL,
                        (GBaseFinalizeFunc) NULL,
                        (GClassInitFunc) gdk_pixbuf_gif_anim_iter_class_init,
                        NULL,           /* class_finalize */
                        NULL,           /* class_data */
                        sizeof (GdkPixbufGifAnimIter),
                        0,              /* n_preallocs */
                        (GInstanceInitFunc) NULL,
                };
                
                object_type = g_type_register_static (GDK_TYPE_PIXBUF_ANIMATION_ITER,
                                                      "GdkPixbufGifAnimIter",
                                                      &object_info, 0);
        }
        
        return object_type;
}

static void
gdk_pixbuf_gif_anim_iter_class_init (GdkPixbufGifAnimIterClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GdkPixbufAnimationIterClass *anim_iter_class =
                GDK_PIXBUF_ANIMATION_ITER_CLASS (klass);
        
        iter_parent_class = g_type_class_peek_parent (klass);
        
        object_class->finalize = gdk_pixbuf_gif_anim_iter_finalize;

        anim_iter_class->get_delay_time = gdk_pixbuf_gif_anim_iter_get_delay_time;
        anim_iter_class->get_pixbuf = gdk_pixbuf_gif_anim_iter_get_pixbuf;
        anim_iter_class->on_currently_loading_frame = gdk_pixbuf_gif_anim_iter_on_currently_loading_frame;
        anim_iter_class->advance = gdk_pixbuf_gif_anim_iter_advance;
}

static void
gdk_pixbuf_gif_anim_iter_finalize (GObject *object)
{
        GdkPixbufGifAnimIter *iter = GDK_PIXBUF_GIF_ANIM_ITER (object);

        iter_clear (iter);

        g_object_unref (iter->gif_anim);
        
        G_OBJECT_CLASS (iter_parent_class)->finalize (object);
}

static gboolean
gdk_pixbuf_gif_anim_iter_advance (GdkPixbufAnimationIter *anim_iter,
                                  const GTimeVal         *current_time)
{
        GdkPixbufGifAnimIter *iter;
        gint elapsed;
        GList *tmp;
        GList *old;
        
        iter = GDK_PIXBUF_GIF_ANIM_ITER (anim_iter);
        
        iter->current_time = *current_time;

        /* We use milliseconds for all times */
        elapsed =
          (((iter->current_time.tv_sec - iter->start_time.tv_sec) * G_USEC_PER_SEC +
            iter->current_time.tv_usec - iter->start_time.tv_usec)) / 1000;

        if (elapsed < 0) {
                /* Try to compensate; probably the system clock
                 * was set backwards
                 */
                iter->start_time = iter->current_time;
                elapsed = 0;
        }

        g_assert (iter->gif_anim->total_time > 0);
        
        /* See how many times we've already played the full animation,
         * and subtract time for that.
         */
        elapsed = elapsed % iter->gif_anim->total_time;

        iter->position = elapsed;
        
        /* Now move to the proper frame */
        tmp = iter->gif_anim->frames;
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

int
gdk_pixbuf_gif_anim_iter_get_delay_time (GdkPixbufAnimationIter *anim_iter)
{
        GdkPixbufFrame *frame;
        GdkPixbufGifAnimIter *iter;
  
        iter = GDK_PIXBUF_GIF_ANIM_ITER (anim_iter);

        if (iter->current_frame) {
                frame = iter->current_frame->data;

#if 0
                g_print ("frame start: %d pos: %d frame len: %d frame remaining: %d\n",
                         frame->elapsed,
                         iter->position,
                         frame->delay_time,
                         frame->delay_time - (iter->position - frame->elapsed));
#endif
                
                return frame->delay_time - (iter->position - frame->elapsed);
        } else {
                return -1; /* show last frame forever */
        }
}

void
gdk_pixbuf_gif_anim_frame_composite (GdkPixbufGifAnim *gif_anim,
                                     GdkPixbufFrame   *frame)
{  
        GList *link;
        GList *tmp;
        
        link = g_list_find (gif_anim->frames, frame);
        
        if (frame->need_recomposite || frame->composited == NULL) {
                /* For now, to composite we start with the last
                 * composited frame and composite everything up to
                 * here.
                 */

                /* Rewind to last composited frame. */
                tmp = link;
                while (tmp != NULL) {
                        GdkPixbufFrame *f = tmp->data;
                        
                        if (f->need_recomposite) {
                                if (f->composited) {
                                        g_object_unref (f->composited);
                                        f->composited = NULL;
                                }
                        }

                        if (f->composited != NULL)
                                break;
                        
                        tmp = tmp->prev;
                }

                /* Go forward, compositing all frames up to the current frame */
                if (tmp == NULL)
                        tmp = gif_anim->frames;
                
                while (tmp != NULL) {
                        GdkPixbufFrame *f = tmp->data;

                        if (f->need_recomposite) {
                                if (f->composited) {
                                        g_object_unref (f->composited);
                                        f->composited = NULL;
                                }
                        }
                        
                        if (f->composited != NULL)
                                goto next;

                        if (tmp->prev == NULL) {
                                /* First frame may be smaller than the whole image;
                                 * if so, we make the area outside it full alpha if the
                                 * image has alpha, and background color otherwise.
                                 * GIF spec doesn't actually say what to do about this.
                                 */
                                f->composited = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
                                                                TRUE,
                                                                8, gif_anim->width, gif_anim->height);

                                /* alpha gets dumped if f->composited has no alpha */
                                
                                gdk_pixbuf_fill (f->composited,
                                                 (gif_anim->bg_red << 24) |
                                                 (gif_anim->bg_green << 16) |
                                                 (gif_anim->bg_blue << 8) |
                                                 (f->bg_transparent ? 0 : 255));

                                gdk_pixbuf_composite (f->pixbuf,
                                                      f->composited,
                                                      f->x_offset,
                                                      f->y_offset,
                                                      gdk_pixbuf_get_width (f->pixbuf),
                                                      gdk_pixbuf_get_height (f->pixbuf),
                                                      f->x_offset, f->y_offset,
                                                      1.0, 1.0,
                                                      GDK_INTERP_BILINEAR,
                                                      255);
#if 0                                
                                gdk_pixbuf_copy_area (f->pixbuf,
                                                      0, 0,
                                                      gdk_pixbuf_get_width (f->pixbuf),
                                                      gdk_pixbuf_get_height (f->pixbuf),
                                                      f->composited,
                                                      f->x_offset,
                                                      f->y_offset);
                                
#endif
                                
                                if (f->action == GDK_PIXBUF_FRAME_REVERT)
                                        g_warning ("First frame of GIF has bad dispose mode, GIF loader should not have loaded this image");

                                f->need_recomposite = FALSE;
                        } else {
                                GdkPixbufFrame *prev_frame;
                                
                                prev_frame = tmp->prev->data;

                                /* Init f->composited with what we should have after the previous
                                 * frame
                                 */
                                
                                if (prev_frame->action == GDK_PIXBUF_FRAME_RETAIN) {
                                        f->composited = gdk_pixbuf_copy (prev_frame->composited);
                                        
                                } else if (prev_frame->action == GDK_PIXBUF_FRAME_DISPOSE) {
                                        GdkPixbuf *area;
                                        
                                        f->composited = gdk_pixbuf_copy (prev_frame->composited);

                                        /* Clear area of previous frame to background */
                                        area = gdk_pixbuf_new_subpixbuf (f->composited,
                                                                         prev_frame->x_offset,
                                                                         prev_frame->y_offset,
                                                                         gdk_pixbuf_get_width (prev_frame->pixbuf),
                                                                         gdk_pixbuf_get_height (prev_frame->pixbuf));

                                        gdk_pixbuf_fill (area,
                                                         (gif_anim->bg_red << 24) |
                                                         (gif_anim->bg_green << 16) |
                                                         (gif_anim->bg_blue << 8) |
                                                         prev_frame->bg_transparent ? 0 : 255);

                                        g_object_unref (area);
                                        
                                } else if (prev_frame->action == GDK_PIXBUF_FRAME_REVERT) {
                                        f->composited = gdk_pixbuf_copy (prev_frame->composited);

                                        /* Copy in the revert frame */
                                        gdk_pixbuf_copy_area (prev_frame->revert,
                                                              0, 0,
                                                              gdk_pixbuf_get_width (prev_frame->revert),
                                                              gdk_pixbuf_get_height (prev_frame->revert),
                                                              f->composited,
                                                              prev_frame->x_offset,
                                                              prev_frame->y_offset);
                                } else {
                                        g_warning ("Unknown revert action for GIF frame");
                                }

                                if (f->revert == NULL &&
                                    f->action == GDK_PIXBUF_FRAME_REVERT) {
                                        /* We need to save the contents before compositing */
                                        GdkPixbuf *area;

                                        area = gdk_pixbuf_new_subpixbuf (f->composited,
                                                                         f->x_offset,
                                                                         f->y_offset,
                                                                         gdk_pixbuf_get_width (f->pixbuf),
                                                                         gdk_pixbuf_get_height (f->pixbuf));

                                        f->revert = gdk_pixbuf_copy (area);
                                        
                                        g_object_unref (area);
                                }

                                /* Put current frame onto f->composited */
                                gdk_pixbuf_composite (f->pixbuf,
                                                      f->composited,
                                                      f->x_offset,
                                                      f->y_offset,
                                                      gdk_pixbuf_get_width (f->pixbuf),
                                                      gdk_pixbuf_get_height (f->pixbuf),
                                                      f->x_offset, f->y_offset,
                                                      1.0, 1.0,
                                                      GDK_INTERP_NEAREST,
                                                      255);
                        
                                f->need_recomposite = FALSE;
                        }
                        
                next:
                        if (tmp == link)
                                break;
                        
                        tmp = tmp->next;
                }
        }

        g_assert (frame->composited != NULL);
        g_assert (gdk_pixbuf_get_width (frame->composited) == gif_anim->width);
        g_assert (gdk_pixbuf_get_height (frame->composited) == gif_anim->height);
}

GdkPixbuf*
gdk_pixbuf_gif_anim_iter_get_pixbuf (GdkPixbufAnimationIter *anim_iter)
{
        GdkPixbufGifAnimIter *iter;
        GdkPixbufFrame *frame;
        
        iter = GDK_PIXBUF_GIF_ANIM_ITER (anim_iter);

        frame = iter->current_frame ? iter->current_frame->data : NULL;

#if 0
        if (FALSE && frame)
          g_print ("current frame %d dispose mode %d  %d x %d\n",
                   g_list_index (iter->gif_anim->frames,
                                 frame),
                   frame->action,
                   gdk_pixbuf_get_width (frame->pixbuf),
                   gdk_pixbuf_get_height (frame->pixbuf));
#endif
        
        if (frame == NULL)
                return NULL;

        gdk_pixbuf_gif_anim_frame_composite (iter->gif_anim, frame);
        
        return frame->composited;
}

static gboolean
gdk_pixbuf_gif_anim_iter_on_currently_loading_frame (GdkPixbufAnimationIter *anim_iter)
{
        GdkPixbufGifAnimIter *iter;
  
        iter = GDK_PIXBUF_GIF_ANIM_ITER (anim_iter);

        return iter->current_frame == NULL || iter->current_frame->next == NULL;  
}
