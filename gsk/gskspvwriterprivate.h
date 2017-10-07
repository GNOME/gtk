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

#ifndef __GSK_SPV_WRITER_PRIVATE_H__
#define __GSK_SPV_WRITER_PRIVATE_H__

#include <glib.h>

#include "gsksltypesprivate.h"
#include "gskspvenumsprivate.h"

G_BEGIN_DECLS

#define GSK_SPV_MAGIC_NUMBER 0x07230203
#define GSK_SPV_VERSION_MAJOR 1
#define GSK_SPV_VERSION_MINOR 0
#define GSK_SPV_GENERATOR 0

typedef struct _GskSpvCodeBlock GskSpvCodeBlock;

typedef enum {
  GSK_SPV_WRITER_SECTION_HEADER,
  GSK_SPV_WRITER_SECTION_DEBUG,
  GSK_SPV_WRITER_SECTION_DECORATE,
  GSK_SPV_WRITER_SECTION_DEFINE,
  GSK_SPV_WRITER_SECTION_DECLARE,
  GSK_SPV_WRITER_SECTION_CODE,
  /* add more */
  GSK_SPV_WRITER_N_SECTIONS
} GskSpvWriterSection;

#define GSK_SPV_WRITER_SECTION_BLOCK_FIRST GSK_SPV_WRITER_SECTION_DECLARE
#define GSK_SPV_WRITER_N_GLOBAL_SECTIONS GSK_SPV_WRITER_SECTION_BLOCK_FIRST
#define GSK_SPV_WRITER_N_BLOCK_SECTIONS (GSK_SPV_WRITER_N_SECTIONS - GSK_SPV_WRITER_N_GLOBAL_SECTIONS)

GskSpvWriter *          gsk_spv_writer_new                      (void);

GskSpvWriter *          gsk_spv_writer_ref                      (GskSpvWriter           *writer);
void                    gsk_spv_writer_unref                    (GskSpvWriter           *writer);

GBytes *                gsk_spv_writer_write                    (GskSpvWriter           *writer,
                                                                 GskSlFunction          *entry_point,
                                                                 GskSpvWriterFunc        initializer,
                                                                 gpointer                initializer_data);

guint32                 gsk_spv_writer_get_id_for_type          (GskSpvWriter           *writer,
                                                                 GskSlType              *type);
guint32                 gsk_spv_writer_get_id_for_pointer_type  (GskSpvWriter           *writer,
                                                                 GskSlPointerType       *type);
guint32                 gsk_spv_writer_get_id_for_value         (GskSpvWriter           *writer,
                                                                 GskSlValue             *value);
guint32                 gsk_spv_writer_get_id_for_zero          (GskSpvWriter           *writer,
                                                                 GskSlScalarType         scalar);
guint32                 gsk_spv_writer_get_id_for_one           (GskSpvWriter           *writer,
                                                                 GskSlScalarType         scalar);
guint32                 gsk_spv_writer_get_id_for_variable      (GskSpvWriter           *writer,
                                                                 GskSlVariable          *variable);
guint32                 gsk_spv_writer_get_id_for_function      (GskSpvWriter           *writer,
                                                                 GskSlFunction          *function);

guint32                 gsk_spv_writer_make_id                  (GskSpvWriter           *writer);
GArray *                gsk_spv_writer_get_bytes                (GskSpvWriter           *writer,
                                                                 GskSpvWriterSection     section);

guint32                 gsk_spv_writer_convert                  (GskSpvWriter           *writer,
                                                                 guint32                 id,
                                                                 GskSlType              *type,
                                                                 GskSlType              *new_type);

guint32                 gsk_spv_writer_push_new_code_block      (GskSpvWriter           *writer);
void                    gsk_spv_writer_push_code_block          (GskSpvWriter           *writer,
                                                                 GskSpvCodeBlock        *block);
GskSpvCodeBlock *       gsk_spv_writer_pop_code_block           (GskSpvWriter           *writer);
void                    gsk_spv_writer_commit_code_block        (GskSpvWriter           *writer);

guint32                 gsk_spv_code_block_get_label            (GskSpvCodeBlock        *block);

#include "gskspvwritergeneratedprivate.h"

G_END_DECLS

#endif /* __GSK_SPV_WRITER_PRIVATE_H__ */
