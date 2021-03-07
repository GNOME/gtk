#ifndef __GSK_ROUNDED_RECT_PRIVATE_H__
#define __GSK_ROUNDED_RECT_PRIVATE_H__

#include "gskroundedrect.h"

#include <cairo.h>

G_BEGIN_DECLS

#define GSK_ROUNDED_RECT_INIT_FROM_RECT(_r)   \
  (GskRoundedRect) { .bounds = _r, \
                     .corner = { \
                        GRAPHENE_SIZE_INIT(0, 0),\
                        GRAPHENE_SIZE_INIT(0, 0),\
                        GRAPHENE_SIZE_INIT(0, 0),\
                        GRAPHENE_SIZE_INIT(0, 0),\
                     }}


void                     gsk_rounded_rect_scale_affine          (GskRoundedRect       *dest,
                                                                 const GskRoundedRect *src,
                                                                 float                 scale_x,
                                                                 float                 scale_y,
                                                                 float                 dx,
                                                                 float                 dy);

gboolean                 gsk_rounded_rect_is_circular           (const GskRoundedRect     *self);

void                     gsk_rounded_rect_path                  (const GskRoundedRect     *self,
                                                                 cairo_t                  *cr);
void                     gsk_rounded_rect_to_float              (const GskRoundedRect     *self,
                                                                 float                     rect[12]);

gboolean                 gsk_rounded_rect_equal                 (gconstpointer             rect1,
                                                                 gconstpointer             rect2) G_GNUC_PURE;
char *                   gsk_rounded_rect_to_string             (const GskRoundedRect     *self);


G_END_DECLS

#endif /* __GSK_ROUNDED_RECT_PRIVATE_H__ */
