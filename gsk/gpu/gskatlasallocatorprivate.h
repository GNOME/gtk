#pragma once

#include <glib.h>
#include <cairo.h>

G_BEGIN_DECLS

typedef struct _GskAtlasAllocator GskAtlasAllocator;
typedef gpointer GskAtlasAllocatorIter;

GskAtlasAllocator *     gsk_atlas_allocator_new                         (gsize                   width,
                                                                         gsize                   height);
void                    gsk_atlas_allocator_free                        (GskAtlasAllocator      *self);

gsize                   gsk_atlas_allocator_allocate                    (GskAtlasAllocator      *self,
                                                                         gsize                   width,
                                                                         gsize                   height);
void                    gsk_atlas_allocator_deallocate                  (GskAtlasAllocator      *self,
                                                                         gsize                   pos);

const cairo_rectangle_int_t *
                        gsk_atlas_allocator_get_area                    (GskAtlasAllocator      *self,
                                                                         gsize                   pos);
void                    gsk_atlas_allocator_set_user_data               (GskAtlasAllocator      *self,
                                                                         gsize                   pos,
                                                                         gpointer                user_data);
gpointer                gsk_atlas_allocator_get_user_data               (GskAtlasAllocator      *self,
                                                                         gsize                   pos);


void                    gsk_atlas_allocator_iter_init                   (GskAtlasAllocator      *self,
                                                                         GskAtlasAllocatorIter  *iter);
gsize                   gsk_atlas_allocator_iter_next                   (GskAtlasAllocator      *self,
                                                                         GskAtlasAllocatorIter  *iter);
G_END_DECLS
