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

#ifndef __GSK_SL_QUALIFIER_PRIVATE_H__
#define __GSK_SL_QUALIFIER_PRIVATE_H__

#include <glib.h>

#include "gsksltypesprivate.h"
#include "gskslqualifierprivate.h"
#include "gskspvwriterprivate.h"

G_BEGIN_DECLS

typedef enum {
  GSK_SL_QUALIFIER_GLOBAL,
  GSK_SL_QUALIFIER_PARAMETER,
  GSK_SL_QUALIFIER_LOCAL
} GskSlQualifierLocation;

typedef enum {
  GSK_SL_INTERPOLATE_DEFAULT,
  GSK_SL_INTERPOLATE_SMOOTH,
  GSK_SL_INTERPOLATE_FLAT,
  GSK_SL_INTERPOLATE_NO_PERSPECTIVE
} GskSlInterpolation;

struct _GskSlQualifier
{
  GskSlStorage storage;
  GskSlInterpolation interpolation;

  struct {
    gint set;
    gint binding;
    gint location;
    gint component;
    guint push_constant :1;
  } layout;

  guint invariant :1;
  guint volatile_ :1;
  guint restrict_ :1;
  guint coherent :1;
  guint readonly :1;
  guint writeonly :1;
};

void                    gsk_sl_qualifier_init                           (GskSlQualifier             *qualifier);
void                    gsk_sl_qualifier_parse                          (GskSlQualifier             *qualifier,
                                                                         GskSlScope                 *scope,
                                                                         GskSlPreprocessor          *stream,
                                                                         GskSlQualifierLocation      location);

gboolean                gsk_sl_qualifier_print                          (const GskSlQualifier       *qualifier,
                                                                         GskSlPrinter               *printer);

gboolean                gsk_sl_qualifier_is_constant                    (const GskSlQualifier       *qualifier);
GskSlQualifierLocation  gsk_sl_qualifier_get_location                   (const GskSlQualifier       *qualifier);
GskSpvStorageClass      gsk_sl_qualifier_get_storage_class              (const GskSlQualifier       *qualifier);

gboolean                gsk_sl_qualifier_check_type                     (const GskSlQualifier       *qualifier,
                                                                         GskSlPreprocessor          *preproc,
                                                                         GskSlType                  *type);
void                    gsk_sl_qualifier_write_spv_decorations          (const GskSlQualifier       *qualifier,
                                                                         GskSpvWriter               *writer,
                                                                         guint32                     value_id);

G_END_DECLS

#endif /* __GSK_SL_QUALIFIER_PRIVATE_H__ */
