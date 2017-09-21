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

G_BEGIN_DECLS

#define GSK_SPV_MAGIC_NUMBER 0x07230203
#define GSK_SPV_VERSION_MAJOR 1
#define GSK_SPV_VERSION_MINOR 0
#define GSK_SPV_GENERATOR 0

typedef enum {
  GSK_SPV_OP_NOP = 0,
  GSK_SPV_OP_UNDEF = 1,
  GSK_SPV_OP_SOURCE_CONTINUED = 2,
  GSK_SPV_OP_SOURCE = 3,
  GSK_SPV_OP_SOURCE_EXTENSION = 4,
  GSK_SPV_OP_NAME = 5,
  GSK_SPV_OP_MEMBER_NAME = 6,
  GSK_SPV_OP_STRING = 7,
  GSK_SPV_OP_LINE = 8,
  GSK_SPV_OP_EXTENSION = 10,
  GSK_SPV_OP_EXT_INST_IMPORT = 11,
  GSK_SPV_OP_EXT_INST = 12,
  GSK_SPV_OP_MEMORY_MODEL = 14,
  GSK_SPV_OP_ENTRY_POINT = 15,
  GSK_SPV_OP_EXECUTION_MODE = 16,
  GSK_SPV_OP_CAPABILITY = 17,
  GSK_SPV_OP_TYPE_VOID = 19,
  GSK_SPV_OP_TYPE_BOOL = 20,
  GSK_SPV_OP_TYPE_INT = 21,
  GSK_SPV_OP_TYPE_FLOAT = 22,
  GSK_SPV_OP_TYPE_VECTOR = 23,
  GSK_SPV_OP_TYPE_MATRIX = 24,
  GSK_SPV_OP_TYPE_IMAGE = 25,
  GSK_SPV_OP_TYPE_SAMPLER = 26,
  GSK_SPV_OP_TYPE_SAMPLED_IMAGE = 27,
  GSK_SPV_OP_TYPE_ARRAY = 28,
  GSK_SPV_OP_TYPE_RUNTIME_ARRAY = 29,
  GSK_SPV_OP_TYPE_STRUCT = 30,
  GSK_SPV_OP_TYPE_OPAQUE = 31,
  GSK_SPV_OP_TYPE_POINTER = 32,
  GSK_SPV_OP_TYPE_FUNCTION = 33,
  GSK_SPV_OP_TYPE_EVENT = 34,
  GSK_SPV_OP_TYPE_DEVICE_EVENT = 35,
  GSK_SPV_OP_TYPE_RESERVE_ID = 36,
  GSK_SPV_OP_TYPE_QUEUE = 37,
  GSK_SPV_OP_TYPE_PIPE = 38,
  GSK_SPV_OP_TYPE_FORWARD_POINTER = 39,
  GSK_SPV_OP_CONSTANT_TRUE = 41,
  GSK_SPV_OP_CONSTANT_FALSE = 42,
  GSK_SPV_OP_CONSTANT = 43,
  GSK_SPV_OP_CONSTANT_COMPOSITE = 44,
  GSK_SPV_OP_CONSTANT_SAMPLER = 45,
  GSK_SPV_OP_CONSTANT_NULL = 46,
  GSK_SPV_OP_SPEC_CONSTANT_TRUE = 48,
  GSK_SPV_OP_SPEC_CONSTANT_FALSE = 49,
  GSK_SPV_OP_SPEC_CONSTANT = 50,
  GSK_SPV_OP_SPEC_CONSTANT_COMPOSITE = 51,
  GSK_SPV_OP_SPEC_CONSTANT_OP = 52,
  GSK_SPV_OP_FUNCTION = 54,
  GSK_SPV_OP_FUNCTION_PARAMETER = 55,
  GSK_SPV_OP_FUNCTION_END = 56,
  GSK_SPV_OP_FUNCTION_CALL = 57,
  GSK_SPV_OP_VARIABLE = 59,
  GSK_SPV_OP_IMAGE_TEXEL_POINTER = 60,
  GSK_SPV_OP_LOAD = 61,
  GSK_SPV_OP_STORE = 62,
  GSK_SPV_OP_COPY_MEMORY = 63,
  GSK_SPV_OP_COPY_MEMORY_SIZED = 64,
  GSK_SPV_OP_ACCESS_CHAIN = 65,
  GSK_SPV_OP_IN_BOUNDS_ACCESS_CHAIN = 66,
  GSK_SPV_OP_PTR_ACCESS_CHAIN = 67,
  GSK_SPV_OP_ARRAY_LENGTH = 68,
  GSK_SPV_OP_GENERIC_PTR_MEM_SEMANTICS = 69,
  GSK_SPV_OP_IN_BOUNDS_PTR_ACCESS_CHAIN = 70,
  GSK_SPV_OP_PHI = 245,
  GSK_SPV_OP_LOOP_MERGE = 246,
  GSK_SPV_OP_SELECTION_MERGE = 247,
  GSK_SPV_OP_LABEL = 248,
  GSK_SPV_OP_BRANCH = 249,
  GSK_SPV_OP_BRANCH_CONDITIONAL = 250,
  GSK_SPV_OP_SWITCH = 251,
  GSK_SPV_OP_KILL = 252,
  GSK_SPV_OP_RETURN = 253,
  GSK_SPV_OP_RETURN_VALUE = 254,
  GSK_SPV_OP_UNREACHABLE = 255,
  GSK_SPV_OP_LIFETIME_START = 256,
  GSK_SPV_OP_LIFETIME_STOP = 257,
  GSK_SPV_OP_SIZE_OF = 321,
  GSK_SPV_OP_TYPE_PIPE_STORAGE = 322,
  GSK_SPV_OP_TYPE_NAMED_BARRIER = 327
} GskSpvOpcode;

typedef enum {
  GSK_SPV_CAPABILITY_MATRIX = 0,
  GSK_SPV_CAPABILITY_SHADER = 1
} GskSpvCapability;

typedef enum {
  GSK_SPV_ADDRESSING_LOGICAL = 0,
  GSK_SPV_ADDRESSING_PHYSICAL32 = 1,
  GSK_SPV_ADDRESSING_PHYSICAL64 = 2,
} GskSpvAddressingModel;

typedef enum {
  GSK_SPV_MEMORY_SIMPLE = 0,
  GSK_SPV_MEMORY_GLSL450 = 1,
  GSK_SPV_MEMORY_OPEN_CL = 2
} GskSpvMemoryModel;

typedef enum {
  GSK_SPV_EXECUTION_MODEL_VERTEX = 0,
  GSK_SPV_EXECUTION_MODEL_TESSELATION_CONTROL = 1,
  GSK_SPV_EXECUTION_MODEL_TESSELATION_EVALUATION = 2,
  GSK_SPV_EXECUTION_MODEL_GEOMETRY = 3,
  GSK_SPV_EXECUTION_MODEL_FRAGMENT = 4,
  GSK_SPV_EXECUTION_MODEL_GL_COMPUTE = 5,
  GSK_SPV_EXECUTION_MODEL_KERNEL = 6,
} GskSpvExecutionModel;

typedef enum {
  GSK_SPV_EXECUTION_MODE_ORIGIN_UPPER_LEFT = 7
} GskSpvExecutionMode;

typedef enum {
  GSK_SPV_STORAGE_CLASS_UNIFORM_CONSTANT = 0,
  GSK_SPV_STORAGE_CLASS_INPUT = 1,
  GSK_SPV_STORAGE_CLASS_UNIFORM = 2,
  GSK_SPV_STORAGE_CLASS_OUTPUT = 3,
  GSK_SPV_STORAGE_CLASS_WORKGROUP = 4,
  GSK_SPV_STORAGE_CLASS_CROSS_WORKGROUP = 5,
  GSK_SPV_STORAGE_CLASS_PRIVATE = 6,
  GSK_SPV_STORAGE_CLASS_FUNCTION = 7,
  GSK_SPV_STORAGE_CLASS_GENERIC = 8,
  GSK_SPV_STORAGE_CLASS_PUSH_CONSTANT = 9,
  GSK_SPV_STORAGE_CLASS_ATOMIC_COUNTER = 10,
  GSK_SPV_STORAGE_CLASS_IMAGE = 11,
  GSK_SPV_STORAGE_CLASS_STORAGE_BUFFER = 12
} GskSpvStorageClass;

typedef enum {
  GSK_SPV_WRITER_SECTION_HEADER,
  GSK_SPV_WRITER_SECTION_DEBUG,
  GSK_SPV_WRITER_SECTION_DECLARE,
  GSK_SPV_WRITER_SECTION_CODE,
  /* add more */
  GSK_SPV_WRITER_N_SECTIONS
} GskSpvWriterSection;

GskSpvWriter *          gsk_spv_writer_new                      (void);

GskSpvWriter *          gsk_spv_writer_ref                      (GskSpvWriter           *writer);
void                    gsk_spv_writer_unref                    (GskSpvWriter           *writer);

GBytes *                gsk_spv_writer_write                    (GskSpvWriter           *writer);
void                    gsk_spv_writer_set_entry_point          (GskSpvWriter           *writer,
                                                                 guint32                 entry_point);

guint32                 gsk_spv_writer_get_id_for_type          (GskSpvWriter           *writer,
                                                                 GskSlType              *type);
guint32                 gsk_spv_writer_get_id_for_pointer_type  (GskSpvWriter           *writer,
                                                                 GskSlPointerType       *type);
guint32                 gsk_spv_writer_get_id_for_declaration   (GskSpvWriter           *writer,
                                                                 GskSlNode              *node);
void                    gsk_spv_writer_set_id_for_declaration   (GskSpvWriter           *writer,
                                                                 GskSlNode              *node,
                                                                 guint32                 id);

guint32                 gsk_spv_writer_next_id                  (GskSpvWriter           *writer);
void                    gsk_spv_writer_add                      (GskSpvWriter           *writer,
                                                                 GskSpvWriterSection     section,
                                                                 guint16                 word_count,
                                                                 guint16                 opcode,
                                                                 guint32                *words);

G_END_DECLS

#endif /* __GSK_SPV_WRITER_PRIVATE_H__ */
