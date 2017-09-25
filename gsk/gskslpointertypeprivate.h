/* GTK - The GIMP Toolkit
 *
 * Copyright © 2017 Benjamin Otte <otte@gnome.org>
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GSK_SL_POINTER_TYPE_PRIVATE_H__
#define __GSK_SL_POINTER_TYPE_PRIVATE_H__

#include <glib.h>

#include "gsksltypesprivate.h"
#include "gskspvwriterprivate.h"

G_BEGIN_DECLS

typedef enum {
  GSK_SL_DECORATION_CONST,
  GSK_SL_DECORATION_CALLER_ACCESS,
  GSK_SL_DECORATION_INVARIANT,
  GSK_SL_DECORATION_COHERENT,
  GSK_SL_DECORATION_VOLATILE,
  GSK_SL_DECORATION_RESTRICT,
  GSK_SL_DECORATION_ACCESS,
  GSK_SL_DECORATION_LAYOUT_LOCATION,
  GSK_SL_DECORATION_LAYOUT_COMPONENT,
  GSK_SL_DECORATION_LAYOUT_SET,
  GSK_SL_DECORATION_LAYOUT_BINDING,
  /* add */
  GSK_SL_N_DECORATIONS
} GskSlDecoration;

typedef enum {
  GSK_SL_DECORATION_ACCESS_READ = (1 << 0),
  GSK_SL_DECORATION_ACCESS_WRITE = (2 << 0),
  GSK_SL_DECORATION_ACCESS_READWRITE = GSK_SL_DECORATION_ACCESS_READ | GSK_SL_DECORATION_ACCESS_WRITE,
} GskSlDecorationAccess;

typedef struct _GskSlDecorations GskSlDecorations;

struct _GskSlDecorations
{
  struct {
    gboolean set;
    gint value;
  } values[GSK_SL_N_DECORATIONS];
};

gboolean                gsk_sl_decoration_list_parse                    (GskSlPreprocessor          *stream,
                                                                         GskSlDecorations           *list);

GskSlPointerType *      gsk_sl_pointer_type_new                         (GskSlType                  *type,
                                                                         gboolean                    local,
                                                                         GskSlDecorationAccess       access);

GskSlPointerType *      gsk_sl_pointer_type_ref                         (GskSlPointerType           *type);
void                    gsk_sl_pointer_type_unref                       (GskSlPointerType           *type);

void                    gsk_sl_pointer_type_print                       (const GskSlPointerType     *type,
                                                                         GString                    *string);

GskSlType *             gsk_sl_pointer_type_get_type                    (const GskSlPointerType     *type);

gboolean                gsk_sl_pointer_type_equal                       (gconstpointer               a,
                                                                         gconstpointer               b);
guint                   gsk_sl_pointer_type_hash                        (gconstpointer               type);

GskSpvStorageClass      gsk_sl_pointer_type_get_storage_class           (const GskSlPointerType     *type);
guint32                 gsk_sl_pointer_type_write_spv                   (const GskSlPointerType     *type,
                                                                         GskSpvWriter               *writer);

G_END_DECLS

#endif /* __GSK_SL_POINTER_TYPE_PRIVATE_H__ */
