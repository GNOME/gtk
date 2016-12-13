#ifndef __GSK_ROUNDED_RECT_PRIVATE_H__
#define __GSK_ROUNDED_RECT_PRIVATE_H__

#include "gskroundedrect.h"

#include <cairo.h>

G_BEGIN_DECLS

void                     gsk_rounded_rect_path                  (const GskRoundedRect     *self,
                                                                 cairo_t                  *cr);

G_END_DECLS

#endif /* __GSK_ROUNDED_RECT_PRIVATE_H__ */
