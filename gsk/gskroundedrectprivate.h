#ifndef __GSK_ROUNDED_RECT_PRIVATE_H__
#define __GSK_ROUNDED_RECT_PRIVATE_H__

#include "gskroundedrect.h"

#include <cairo.h>

G_BEGIN_DECLS

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
