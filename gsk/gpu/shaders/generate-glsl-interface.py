#!/usr/bin/env python3

from dataclasses import dataclass
from enum import Enum
import argparse
import re
import sys
import os

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

    def struct_initializer (self, indent, var_name, struct_member, offset):
        return self.struct_init.format (indent, var_name, struct_member, offset);

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
class File:
    filename: str
    name: str
    var_name: str
    struct_name: str
    n_textures: int
    n_instances: int
    variables: list[Variable]

types = [
Type(
    type = 'float',
    pointer = False,
    var_type = VarType.FLOAT,
    size = 1,
    struct_init = '{0}instance->{2}[{3}] = {1};'
),
Type(
    type = 'guint32',
    pointer = False,
    var_type = VarType.UINT,
    size = 1,
    struct_init = '{0}instance->{2}[{3}] = {1};'
),
Type(
    type = 'graphene_point_t',
    pointer = True,
    var_type = VarType.FLOAT,
    size = 2,
    struct_init = '{0}gsk_gpu_point_to_float ({1}, offset, &instance->{2}[{3}]);'
),
Type(
    type = 'graphene_size_t',
    pointer = True,
    var_type = VarType.FLOAT,
    size = 2,
    struct_init = '{0}instance->{2}[{3}] = {1}->width;\n'
                  '{0}instance->{2}[{3} + 1] = {1}->height;'
),
Type(
    type = 'graphene_vec2_t',
    pointer = True,
    var_type = VarType.FLOAT,
    size = 2,
    struct_init = '{0}graphene_vec2_to_float ({1}, instance->{2});'
),
Type(
    type = 'graphene_vec4_t',
    pointer = True,
    var_type = VarType.FLOAT,
    size = 4,
    struct_init = '{0}graphene_vec4_to_float ({1}, instance->{2});'
),
Type(
    type = 'graphene_rect_t',
    pointer = True,
    var_type = VarType.FLOAT,
    size = 4,
    struct_init = '{0}gsk_gpu_rect_to_float ({1}, offset, instance->{2});'
),
Type(
    type = 'GskRoundedRect',
    pointer = True,
    var_type = VarType.FLOAT,
    size = 12,
    struct_init = '{0}gsk_rounded_rect_to_float ({1}, offset, instance->{2});'
),
Type(
    type = 'graphene_matrix_t',
    pointer = True,
    var_type = VarType.FLOAT,
    size = 16,
    struct_init = '{0}graphene_matrix_to_float ({1}, instance->{2});'
),
Type(
    type = 'GdkColor',
    pointer = True,
    var_type = VarType.FLOAT,
    size = 4,
    struct_init = '{0}gsk_gpu_color_to_float ({1}, color_space, opacity, instance->{2});'
),
]

def read_file (filename):
    lines = open(filename).readlines()
    variables = []
    on = False
    n_textures = 0
    n_instances = 6
    name = ''
    var_name = ''
    struct_name = ''

    for pos, line in enumerate (lines):
        line = line.strip ()
        if not on and line == "#ifdef GSK_PREAMBLE":
            on = True
        elif not on:
            pass
        elif line == "#endif":
            on = False
        elif line == "":
            pass
        elif match := re.search (r'(\w+)\s+(\w+)\s*;', line):
            var_type = next ((x for x in types if x.type == match.group (1)), None)
            if not var_type:
                raise Exception (f'''{filename}:{pos}: Unknown type name "{match.group (1)}"''')
            variables.append (Input (name = match.group (2),
                                     type = var_type))
        elif match := re.search (r'^textures\s*=\s*(\d+)\s*;$', line):
            n_textures = int (match.group(1))
            if n_textures < 0 or n_textures > 2:
                raise Excepthin (f'''{filename}:{pos}: Number of textures must be <= 2''')
        elif match := re.search (r'^instances\s*=\s*(\d+)\s*;$', line):
            n_instances = int (match.group(1))
        elif match := re.search (r'^name\s*=\s*"(\w+)"\s*;$', line):
            name = match.group(1)
        elif match := re.search (r'^var_name\s*=\s*"(\w+)"\s*;$', line):
            var_name = match.group(1)
        elif match := re.search (r'^struct_name\s*=\s*"(\w+)"\s*;$', line):
            struct_name = match.group(1)
        else:
            raise Exception (f'''{filename}:{pos}: Could not parse line''')
    
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
                 variables = variables)


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
            
    return location, result


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

def print_c_struct_initializer (file, n_attributes, attributes):
    indent = len (f'''{file.var_name}_instance_init (''')
    type_len = max (map (lambda var: len ((('const ' if var.type.pointer else '') + var.type.type)), file.variables))
    type_len = max (type_len, len (file.struct_name + 'Instance'))
    print (f'''static inline void
{file.var_name}_instance_init ({(file.struct_name + 'Instance').ljust(type_len)} *instance,
{''.ljust(indent)}{'GdkColorState'.ljust(type_len)} *color_space,
{''.ljust(indent)}{'const graphene_point_t'.ljust(type_len)} *offset,
{''.ljust(indent)}{'float'.ljust(type_len)}  opacity,''')

    variables = [var for var in file.variables if var.name != 'opacity']
    for pos, var in enumerate (variables):
        if var.name == 'opacity':
            continue
        print (f'''{''.ljust(indent)}{(('const ' if var.type.pointer else '') + var.type.type).ljust (type_len)} {'*' if var.type.pointer else ' '}{var.name}{',' if (pos + 1 < len (variables)) else ')'}''')

    print (f'''{{''')
    for attr in attributes:
        size = 0
        for var in attr.inputs:
            print (var.type.struct_initializer ('  ', var.name, attr.name, size))
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

#include "common.glsl"
''')
    print_glsl_attributes (attributes)

def print_header_file (file, n_attributes, attributes):
    print (f'''/* This file is auto-generated; changes will not be preserved */
#pragma once

#define {file.var_name}_n_textures {file.n_textures}
#define {file.var_name}_n_instances {file.n_instances}
''')
    print_c_struct (file, n_attributes, attributes)
    print_c_struct_initializer (file, n_attributes, attributes)
    print_gl_setup_vao (file, n_attributes, attributes)
    print_gl_attrib_locations (file, n_attributes, attributes)
    print_vulkan_info (file, n_attributes, attributes)

parser = argparse.ArgumentParser()
parser.add_argument ('--generate-glsl', action='store_true', help='Generate GLSL includes')
parser.add_argument ('--generate-header', action='store_true', help='Generate C header')
parser.add_argument ('FILES', nargs='*', help='Input files')
args = parser.parse_args()

for path in args.FILES:
    file = read_file (path)
    n_attributes, attributes = generate_attributes (file.variables)

    if args.generate_glsl:
        print_glsl_file (file, n_attributes,attributes)
    if args.generate_header:
        print_header_file (file, n_attributes, attributes)
