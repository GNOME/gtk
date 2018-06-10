#ifndef __GSK_ALLOC_PRIVATE_H__
#define __GSK_ALLOC_PRIVATE_H__

#include <glib.h>

G_BEGIN_DECLS

gpointer gsk_aligned_alloc  (gsize    size,
                             gsize    number,
                             gsize    alignment);
gpointer gsk_aligned_alloc0 (gsize    size,
                             gsize    number,
                             gsize    alignment);
void     gsk_aligned_free   (gpointer mem);

G_END_DECLS

#endif /* __GSK_ALLOC_PRIVATE_H__ */
