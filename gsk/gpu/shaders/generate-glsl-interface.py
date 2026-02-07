#!/usr/bin/env python3

from dataclasses import dataclass
from enum import Enum
import argparse
import re
import sys
import os

class Premultiplied(Enum):
    PREMULTIPLIED = 0,
    STRAIGHT = 1,
    ARGUMENT = 2

    def from_string(val):
        if val.lower() in [ 'true', 'premultiplied' ]:
            return Premultiplied.PREMULTIPLIED;
        elif val.lower() in [ 'false', 'straight' ]:
            return Premultiplied.STRAIGHT;
        elif val.lower() in [ 'argument' ]:
            return Premultiplied.ARGUMENT;
        else:
            raise Exception ('Expected "premultiplied" or "straight" or "argument"')

    def to_c_code(self, argument_name):
        if self == Premultiplied.PREMULTIPLIED:
            return 'TRUE'
        elif self == Premultiplied.STRAIGHT:
            return 'FALSE'
        elif self == Premultiplied.ARGUMENT:
            return argument_name
        else:
            raise Exception('FIXME: Figure out assert_not_reached() with Python')

@dataclass
class VarType:
    c_name: str
    glsl_name: str
    glsl_prefix: str
    vk_type: str
    gl_attrib_pointer_str: str

    def get_gl_type (self, size):
        if size == 1:
            return self.glsl_name
        elif size == 2:
            return self.glsl_prefix + 'vec2'
        elif size == 3:
            return self.glsl_prefix + 'vec3'
        elif size == 4:
            return self.glsl_prefix + 'vec4'
        elif size == 8:
            return self.glsl_prefix + 'mat2x4'
        elif size == 12:
            return self.glsl_prefix + 'mat3x4'
        elif size == 16:
            return self.glsl_prefix + 'mat4'
        else:
            raise Exception(f'''Invalid size {size} for {self}''')

    def gl_attrib_pointer (self, indent, location, size, n_attrs, offset):
        return self.gl_attrib_pointer_str.format (indent, location, size, n_attrs, offset)

VarType.FLOAT = VarType (
        c_name = 'float',
        glsl_name = 'float',
        glsl_prefix ='',
        vk_type = 'SFLOAT',
        gl_attrib_pointer_str = '{0}glVertexAttribPointer ({1},\n'
                                '{0}                       {2},\n'
                                '{0}                       GL_FLOAT,\n'
                                '{0}                       GL_FALSE,\n'
                                '{0}                       {3},\n'
                                '{0}                       GSIZE_TO_POINTER (offset + {4}));'
    )
VarType.INT = VarType (
        c_name = 'gint32',
        glsl_name ='int',
        glsl_prefix = 'i',
        vk_type = 'SINT',
        gl_attrib_pointer_str = '{0}glVertexAttribIPointer ({1},\n'
                                '{0}                        {2},\n'
                                '{0}                        GL_INT,\n'
                                '{0}                        {3},\n'
                                '{0}                        GSIZE_TO_POINTER (offset + {4}));'
    )
VarType.UINT = VarType (
        c_name = 'guint32',
        glsl_name = 'uint',
        glsl_prefix = 'u',
        vk_type = 'UINT',
        gl_attrib_pointer_str = '{0}glVertexAttribIPointer ({1},\n'
                                '{0}                        {2},\n'
                                '{0}                        GL_UNSIGNED_INT,\n'
                                '{0}                        {3},\n'
                                '{0}                        GSIZE_TO_POINTER (offset + {4}));'
    )

@dataclass
class Type:
    type: str
    pointer: bool
    var_type: VarType
    size: int
    struct_init: str

    def struct_initializer (self, indent, instance_name, var_name, struct_member, offset):
        return self.struct_init.format (indent, instance_name, var_name, struct_member, offset);

@dataclass
class VariationType:
    type: str
    bits: int
    glsl_string: str
    c_flag: str

    def to_glsl (self, offset_bits):
        return self.glsl_string.format (offset_bits)

    def to_c_flag (self, name, offset_bits):
        return self.c_flag.format (name, offset_bits)

@dataclass
class Input:
    name: str
    type: Type

@dataclass
class Attribute:
    name: str
    inputs: Input
    var_type: VarType
    size: int
    location: int

@dataclass
class Variable:
    name: str
    type: Type

@dataclass
class Variation:
    name: str
    offset_bits: int
    type: VariationType

@dataclass
class File:
    filename: str
    name: str
    var_name: str
    struct_name: str
    n_textures: int
    n_instances: int
    ccs_premultiplied: Premultiplied
    acs_premultiplied: Premultiplied
    acs_equals_ccs: bool
    opacity: bool
    dual_blend: bool
    variables: list[Variable]
    variations: list[Variation]

types = [
Type(
    type = 'float',
    pointer = False,
    var_type = VarType.FLOAT,
    size = 1,
    struct_init = '{0}{1}{3}[{4}] = {2};'
),
Type(
    type = 'guint32',
    pointer = False,
    var_type = VarType.UINT,
    size = 1,
    struct_init = '{0}{1}{3}[{4}] = {2};'
),
Type(
    type = 'graphene_point_t',
    pointer = True,
    var_type = VarType.FLOAT,
    size = 2,
    struct_init = '{0}{1}{3}[{4}] = {2}->x + offset->x;\n'
                  '{0}{1}{3}[{4} + 1] = {2}->y + offset->y;'
),
Type(
    type = 'graphene_size_t',
    pointer = True,
    var_type = VarType.FLOAT,
    size = 2,
    struct_init = '{0}{1}{3}[{4}] = {2}->width;\n'
                  '{0}{1}{3}[{4} + 1] = {2}->height;'
),
Type(
    type = 'graphene_vec2_t',
    pointer = True,
    var_type = VarType.FLOAT,
    size = 2,
    struct_init = '{0}graphene_vec2_to_float ({2}, {1}{3});'
),
Type(
    type = 'graphene_vec4_t',
    pointer = True,
    var_type = VarType.FLOAT,
    size = 4,
    struct_init = '{0}graphene_vec4_to_float ({2}, {1}{3});'
),
Type(
    type = 'graphene_rect_t',
    pointer = True,
    var_type = VarType.FLOAT,
    size = 4,
    struct_init = '{0}gsk_gpu_rect_to_float ({2}, offset, {1}{3});'
),
Type(
    type = 'GskRoundedRect',
    pointer = True,
    var_type = VarType.FLOAT,
    size = 12,
    struct_init = '{0}gsk_rounded_rect_to_float ({2}, offset, {1}{3});'
),
Type(
    type = 'graphene_matrix_t',
    pointer = True,
    var_type = VarType.FLOAT,
    size = 16,
    struct_init = '{0}graphene_matrix_to_float ({2}, {1}{3});'
),
Type(
    type = 'GdkColor',
    pointer = True,
    var_type = VarType.FLOAT,
    size = 4,
    struct_init = '{0}gdk_color_to_float ({2}, acs, {1}{3});'
                  '{0}{1}{3}[3] *= opacity;'
),
]

variation_types = [
VariationType(
    type = 'gboolean',
    bits = 1,
    glsl_string = '(((GSK_VARIATION >> {0}u) & 1u) == 1u)',
    c_flag = '(({0} & 1) << {1})'
),
VariationType(
    type = 'GdkBuiltinColorStateId',
    bits = 8,
    glsl_string = '((GSK_VARIATION >> {0}u) & 255u)',
    c_flag = '(({0} & 0xFF) << {1})'
),
VariationType(
    type = 'GskBlendMode',
    bits = 8,
    glsl_string = '((GSK_VARIATION >> {0}u) & 255u)',
    c_flag = '(({0} & 0xFF) << {1})'
),
VariationType(
    type = 'GskMaskMode',
    bits = 8,
    glsl_string = '((GSK_VARIATION >> {0}u) & 255u)',
    c_flag = '(({0} & 0xFF) << {1})'
),
VariationType(
    type = 'GskPorterDuff',
    bits = 8,
    glsl_string = '((GSK_VARIATION >> {0}u) & 255u)',
    c_flag = '(({0} & 0xFF) << {1})'
),
VariationType(
    type = 'GskRepeat',
    bits = 4,
    glsl_string = '((GSK_VARIATION >> {0}u) & 15u)',
    c_flag = '(({0} & 0xF) << {1})'
),
]

def strtobool (val):
    if val.lower() in [ 'true' ]:
        return True;
    elif val.lower() in [ 'false' ]:
        return False;
    else:
        raise Exception ('Expected "true" or "false"')

def read_file (filename):
    lines = open(filename).readlines()
    variables = []
    variations = []
    variation_bits = 0
    on = False
    n_textures = 0
    n_instances = 6
    ccs_premultiplied = Premultiplied.PREMULTIPLIED
    acs_premultiplied = Premultiplied.STRAIGHT
    acs_equals_ccs = False
    opacity = True
    dual_blend = False
    name = ''
    var_name = ''
    struct_name = ''

    for pos, line in enumerate (lines):
        line = line.strip ()
        try:
            if not on and line == "#ifdef GSK_PREAMBLE":
                on = True
            elif not on:
                pass
            elif line == "#endif /* GSK_PREAMBLE */":
                on = False
            elif line == "":
                pass
            elif match := re.search (r'^variation\s*:\s*(\w+)\s+(\w+)\s*;$', line):
                var_type = next ((x for x in variation_types if x.type == match.group (1)), None)
                if not var_type:
                    raise Exception (f'''Unknown variation type "{match.group (1)}"''')
                if variation_bits + var_type.bits > 32:
                    raise Exception (f'''too many bits taken by variations''')
                variations.append (Variation (name = match.group (2),
                                              offset_bits = variation_bits,
                                              type = var_type))
                variation_bits += var_type.bits
            elif match := re.search (r'(\w+)\s+(\w+)\s*;', line):
                var_type = next ((x for x in types if x.type == match.group (1)), None)
                if not var_type:
                    raise Exception (f'''Unknown type name "{match.group (1)}"''')
                variables.append (Input (name = match.group (2),
                                         type = var_type))
            elif match := re.search (r'^textures\s*=\s*(\d+)\s*;$', line):
                n_textures = int (match.group(1))
                if n_textures < 0 or n_textures > 2:
                    raise Exception (f'''Number of textures must be <= 2''')
            elif match := re.search (r'^instances\s*=\s*(\d+)\s*;$', line):
                n_instances = int (match.group(1))
            elif match := re.search (r'^name\s*=\s*"(\w+)"\s*;$', line):
                name = match.group(1)
            elif match := re.search (r'^var_name\s*=\s*"(\w+)"\s*;$', line):
                var_name = match.group(1)
            elif match := re.search (r'^struct_name\s*=\s*"(\w+)"\s*;$', line):
                struct_name = match.group(1)
            elif match := re.search (r'^ccs_premultiplied\s*=\s*(\w+)\s*;$', line):
                ccs_premultiplied = Premultiplied.from_string (match.group(1))
            elif match := re.search (r'^acs_premultiplied\s*=\s*(\w+)\s*;$', line):
                acs_premultiplied = Premultiplied.from_string (match.group(1))
            elif match := re.search (r'^acs_equals_ccs\s*=\s*(\w+)\s*;$', line):
                acs_equals_ccs = strtobool (match.group(1))
            elif match := re.search (r'^opacity\s*=\s*(\w+)\s*;$', line):
                opacity = strtobool (match.group(1))
            elif match := re.search (r'^dual_blend\s*=\s*(\w+)\s*;$', line):
                dual_blend = strtobool (match.group(1))
            else:
                raise Exception ('Could not parse line')
        except Exception as e:
            raise Exception (f'''{filename}:{pos}: {repr (e)}''')
    
    if not name:
        name = os.path.splitext(os.path.splitext(os.path.basename(filename))[0])[0][6:]
    if not var_name:
        var_name = "gsk_gpu_" + name.replace('-', '_')
    if not struct_name:
        struct_name = "GskGpu" + name.title().replace('-', '')

    return File (filename = filename,
                 name = name,
                 var_name = var_name,
                 struct_name = struct_name,
                 n_textures = n_textures,
                 n_instances = n_instances,
                 ccs_premultiplied = ccs_premultiplied,
                 acs_premultiplied = acs_premultiplied,
                 acs_equals_ccs = acs_equals_ccs,
                 opacity = opacity,
                 dual_blend = dual_blend,
                 variables = variables,
                 variations = variations)


def generate_attributes(inputs):
    inputs = inputs.copy ()
    result = []
    location = 0
    while inputs:
        input = inputs.pop (0)
        size = input.type.size
        var_type = input.type.var_type
        used = [input]
        while size < 4 and inputs:
            input = next ((i for i in inputs if i.type.size + size <= 4 and i.type.var_type == var_type), None)
            if not input:
                break
            inputs.remove (input)
            size += input.type.size
            used.append (input)

        attr = Attribute (name = '_'.join (x.name for x in used),
                          var_type = var_type,
                          size = size,
                          inputs = used,
                          location = location)
        location += (size + 3) // 4;
        result.append (attr)

    if location > 16:
        raise Exception (f'''MAX_VERTEX_ATTRIBS is 16, this shader uses {location}''')
            
    return location, result


def print_glsl_variations (variations):
    for var in variations:
        print (f'''#define VARIATION_{var.name.upper()} {var.type.to_glsl (var.offset_bits)}''')
    if variations:
        print ()

def print_glsl_attributes (attributes):
    print (f'''#ifdef GSK_VERTEX_SHADER''')
    for attr in attributes:
        print (f'''IN({attr.location}) {attr.var_type.get_gl_type (attr.size)} in_{attr.name};''')
        if len (attr.inputs) > 1:
            size = 0
            for input in attr.inputs:
                print (f'''#define in_{input.name} in_{attr.name}.{'xyzw'[size:size + input.type.size]}''')
                size += input.type.size
    print (f'''#endif''')

def print_c_struct (file, n_attributes, attributes):
    print (f'''typedef struct _{file.struct_name}Instance {file.struct_name}Instance;

struct _{file.struct_name}Instance {{''');

    for attr in attributes:
        print (f'''  {attr.var_type.c_name} {attr.name}[{max (4, attr.size)}];''')
        
    print (f'''}};
''')

@dataclass
class FunctionArg:
    type: str
    pointer: bool
    name: str

def print_c_function (retval, name, args, is_prototype):
    indent = len (name) + 2
    type_len = max (map (lambda arg: len (arg.type), args))
    
    print (retval)
    if args:
        print (f'''{name} ({args[0].type.ljust(type_len)} {'*' if args[0].pointer else ' '}{args[0].name}{',' if len (args) > 1 else (');' if is_prototype else ')')}''')
    else:
        print (f'''{name} (void){';' if is_prototype else ''}''')

    for pos, arg in enumerate (args[1:], 2):
        print (f'''{' '.ljust(indent)}{arg.type.ljust(type_len)} {'*' if arg.pointer else ' '}{arg.name}{',' if pos < len (args) else (');' if is_prototype else ')')}''')

def print_c_print_function (file):
    print_c_function ('static void',
                      file.var_name + '_op_print_instance',
                      [ FunctionArg ('GskGpuShaderOp', True,  'shader'),
                        FunctionArg ('gpointer',       False, 'instance'),
                        FunctionArg ('GString',        True,  'string') ],
                      False)
    print (f'''{{
  /* FIXME: Implement */
}}
''')

def print_c_shader_op_class (file):
    print (f'''static const GskGpuShaderOpClass {file.var_name.upper()}_OP_CLASS = {{
  {{
    GSK_GPU_OP_SIZE (GskGpuShaderOp),
    GSK_GPU_STAGE_SHADER,
    gsk_gpu_shader_op_finish,
    gsk_gpu_shader_op_print,
#ifdef GDK_RENDERING_VULKAN
    gsk_gpu_shader_op_vk_command,
#endif
    gsk_gpu_shader_op_gl_command
  }},
  "gskgpu{file.name}",
  {file.n_textures},
  {file.n_instances},
  sizeof ({file.struct_name}Instance),
#ifdef GDK_RENDERING_VULKAN
  &{file.var_name}_info,
#endif
  {file.var_name}_op_print_instance,
  {file.var_name}_setup_attrib_locations,
  {file.var_name}_setup_vao
}};
''')

def print_c_invocation (file, n_attributes, attributes, prototype_only):
    args = [ FunctionArg ('GskGpuRenderPass',            True,  'pass'),
             FunctionArg ('GskGpuShaderClip',            False, 'clip'),
             FunctionArg ('GdkColorState',               True,  'ccs') ]
    if file.ccs_premultiplied == Premultiplied.ARGUMENT:
        args.append (FunctionArg ('gboolean',            False,  'ccs_premultiplied'))
    if not file.acs_equals_ccs:
        args.append (FunctionArg ('GdkColorState',       True,  'acs'))
    if file.acs_premultiplied == Premultiplied.ARGUMENT:
        args.append (FunctionArg ('gboolean',            False,  'acs_premultiplied'))
    if file.opacity:
        args.append (FunctionArg ('float',               False, 'opacity'))
    args.append (FunctionArg ('const graphene_point_t',  True,  'offset'))

    for i in range(1, file.n_textures + 1):
        args += [ FunctionArg ('GskGpuImage',             True, 'image' + str (i)),
                  FunctionArg ('GskGpuSampler',           False, 'sampler' + str (i)) ]

    for var in file.variations:
        args.append (FunctionArg (var.type.type, False, 'variation_' + var.name))
    for var in file.variables:
        if var.name == 'opacity':
            continue
        args.append (FunctionArg (('const ' if var.type.pointer else '') + var.type.type, var.type.pointer, var.name))

    print_c_function ('void',
                      f'''{file.var_name}_op''',
                      args,
                      prototype_only)

    if prototype_only:
        return

    print (f'''{{
  {file.struct_name}Instance *instance;

  gsk_gpu_shader_op_alloc (pass->frame,
                           &{file.var_name.upper()}_OP_CLASS,
                           ccs ? gsk_gpu_color_states_create (ccs, {file.ccs_premultiplied.to_c_code('ccs_premultiplied')}, {'ccs' if file.acs_equals_ccs else 'acs'}, {file.acs_premultiplied.to_c_code('acs_premultiplied')})
                               : gsk_gpu_color_states_create_equal ({file.ccs_premultiplied.to_c_code('ccs_premultiplied')}, {file.acs_premultiplied.to_c_code('acs_premultiplied')}),''')
    if file.variations:
        for pos, var in enumerate (file.variations, 1):
            print (f'''                           {var.type.to_c_flag ('variation_' + var.name, var.offset_bits)}{' |' if pos < len (file.variations) else ','}''')

    else:
        print (f'''                           0,''')
    print (f'''                           clip,''')
    if file.n_textures > 0:
        print (f'''                           (GskGpuImage *[{file.n_textures}]) {{ {', '.join (map (lambda x: 'image' + str(x), range(1, file.n_textures + 1)))} }},
                           (GskGpuSampler[{file.n_textures}]) {{ {', '.join (map (lambda x: 'sampler' + str(x), range(1, file.n_textures + 1)))} }},''')
    else:
        print (f'''                           NULL,
                           NULL,''')
    print (f'''                           &instance);''')

    for attr in attributes:
        size = 0
        for var in attr.inputs:
            print (var.type.struct_initializer ('  ', 'instance->', var.name, attr.name, size))
            size += var.type.size

    print (f'''}}
''')


def print_gl_setup_vao (file, n_attributes, attributes):
    print(f'''static inline void
{file.var_name}_setup_vao (gsize offset)
{{''')

    for attr in attributes:
        for offset in range(0, attr.size, 4):
            size = min (attr.size - offset, 4)
            print(f'''  glEnableVertexAttribArray ({attr.location + offset // 4});
  glVertexAttribDivisor ({attr.location + offset // 4}, 1);
{attr.var_type.gl_attrib_pointer ('  ', attr.location + offset // 4, size, n_attributes * 16, attr.location * 16 + offset * 4)}''')

    print(f'''}}
''');



def print_gl_attrib_locations (file, n_attributes, attributes):
    print(f'''static inline void
{file.var_name}_setup_attrib_locations (GLuint program)
{{''')

    for attr in attributes:
        print(f'''  glBindAttribLocation (program, {attr.location}, "in_{attr.name}");''')

    print(f'''}}
''');


def print_vulkan_info (file, n_attributes, attributes):
    print(f'''#ifdef GDK_RENDERING_VULKAN

static const VkPipelineVertexInputStateCreateInfo {file.var_name}_info = {{
  .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
  .vertexBindingDescriptionCount = 1,
  .pVertexBindingDescriptions = (VkVertexInputBindingDescription[1]) {{
      {{
        .binding = 0,
        .stride = {n_attributes * 16},
        .inputRate = VK_VERTEX_INPUT_RATE_INSTANCE
      }}
  }},
  .vertexAttributeDescriptionCount = {n_attributes},
  .pVertexAttributeDescriptions = (VkVertexInputAttributeDescription[{n_attributes}]) {{''')

    for attr in attributes:
        for offset in range(0, attr.size, 4):
            size = min (attr.size - offset, 4)
            print(f'''      {{
        .location = {attr.location + offset // 4},
        .binding = 0,
        .format = VK_FORMAT_{'R32G32B32A32'[0:size * 3]}_{attr.var_type.vk_type},
        .offset = {attr.location * 16 + offset * 4},
      }},''')

    print(f'''  }},
}};

#endif /* GDK_RENDERING_VULKAN */
''')

def print_glsl_file (file, n_attributes, attributes):
    print (f'''#define GSK_N_TEXTURES {file.n_textures}
{'//#undef' if not file.dual_blend else '#define'} GSK_DUAL_BLEND 1

#include "common.glsl"
''')
    print_glsl_variations (file.variations)
    print_glsl_attributes (attributes)

def print_header_file (file, n_attributes, attributes):
    print (f'''/* This file is auto-generated; changes will not be preserved */
#pragma once

#include "gskgpushaderopprivate.h"
#include "gskgradientprivate.h" /* for GskRepeat */
#include "gsk/gskroundedrectprivate.h"
''')
    print_c_invocation (file, n_attributes, attributes, True)

def print_source_file (file, n_attributes, attributes):
    print (f'''/* This file is auto-generated; changes will not be preserved */

#include "config.h"

#include "gskgpu{file.name}opprivate.h"

#include "gskgpurenderpassprivate.h"
#include "gskgpushaderopprivate.h"
#include "gskrectprivate.h"
#include <graphene.h>

''')
    print_c_struct (file, n_attributes, attributes)
    print_c_print_function (file)
    print_gl_setup_vao (file, n_attributes, attributes)
    print_gl_attrib_locations (file, n_attributes, attributes)
    print_vulkan_info (file, n_attributes, attributes)
    print_c_shader_op_class (file)
    print_c_invocation (file, n_attributes, attributes, False)


parser = argparse.ArgumentParser()
parser.add_argument ('--generate-glsl', action='store_true', help='Generate GLSL includes')
parser.add_argument ('--generate-old-header', action='store_true', help='Generate old version of C header')
parser.add_argument ('--generate-header', action='store_true', help='Generate C header')
parser.add_argument ('--generate-source', action='store_true', help='Generate C source')
parser.add_argument ('FILES', nargs='*', help='Input files')
args = parser.parse_args()

for path in args.FILES:
    file = read_file (path)
    n_attributes, attributes = generate_attributes (file.variables)

    if args.generate_glsl:
        print_glsl_file (file, n_attributes,attributes)
    if args.generate_header:
        print_header_file (file, n_attributes, attributes)
    if args.generate_source:
        print_source_file (file, n_attributes, attributes)

