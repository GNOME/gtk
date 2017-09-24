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

#ifndef __GSK_SL_DEFINE_PRIVATE_H__
#define __GSK_SL_DEFINE_PRIVATE_H__

#include <glib.h>

#include "gsksltokenizerprivate.h"
#include "gsksltypesprivate.h"

G_BEGIN_DECLS

typedef struct _GskSlDefine GskSlDefine;

GskSlDefine *           gsk_sl_define_new                         (const char            *name,
                                                                   GFile                 *source_file);

GskSlDefine *           gsk_sl_define_ref                         (GskSlDefine           *define);
void                    gsk_sl_define_unref                       (GskSlDefine           *define);

const char *            gsk_sl_define_get_name                    (GskSlDefine           *define);
GFile *                 gsk_sl_define_get_source_file             (GskSlDefine           *define);
guint                   gsk_sl_define_get_n_tokens                (GskSlDefine           *define);
void                    gsk_sl_define_get_token                   (GskSlDefine           *define,
                                                                   guint                  i,
                                                                   GskCodeLocation       *location,
                                                                   GskSlToken            *token);

void                    gsk_sl_define_add_token                   (GskSlDefine           *define,
                                                                   const GskCodeLocation *location,
                                                                   const GskSlToken      *token);

G_END_DECLS

#endif /* __GSK_SL_DEFINE_PRIVATE_H__ */
