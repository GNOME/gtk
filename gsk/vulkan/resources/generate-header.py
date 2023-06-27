#!/usr/bin/env python3

import sys
import re
import os

name = os.path.splitext(os.path.basename(sys.argv[1]))[0]
var_name = "gsk_vulkan_" + name.replace('-', '_')
struct_name = "GskVulkan" + name.title().replace('-', '') + "Instance"

lines = open (sys.argv[1]).readlines()
matches = []

for line in lines:
    match = re.search("^layout\(location = ([0-9]+)\) in ([a-z0-9]+) ([a-zA-Z0-9]+);$", line)
    if not match:
        if re.search("layout.*\sin\s.*", line):
            raise Exception("Failed to parse file")
        continue;
    if not match.group(3).startswith('in'):
        raise Exception("Variable doesn't start with 'in'")
    matches.append({ 'name': ''.join('_' + char.lower() if char.isupper() else char for char in match.group(3))[3:],
                     'location': int(match.group(1)),
                     'type': match.group(2) })

print(
f'''#pragma once

typedef struct _{struct_name} {struct_name};

struct _{struct_name} {{''')

expected = 0;
for match in matches:
    if expected != int(match['location']):
        raise Exception(f"Should be layout location {expected} but is {match['location']}")

    match match['type']:
        case "float":
            print(f"  float {match['name']};")
            expected += 1
        case "int":
            print(f"  gint32 {match['name']};")
            expected += 1
        case "uint":
            print(f"  guint32 {match['name']};")
            expected += 1
        case "uvec2":
            print(f"  guint32 {match['name']}[2];")
            expected += 1
        case "vec2":
            print(f"  float {match['name']}[2];")
            expected += 1
        case "vec4":
            print(f"  float {match['name']}[4];")
            expected += 1
        case "mat3x4":
            print(f"  float {match['name']}[12];")
            expected += 3
        case "mat4":
            print(f"  float {match['name']}[16];")
            expected += 4
        case _:
            raise Exception(f"Don't know what a {match['type']} is")

print(
'''};
''')

print(
f'''static const VkPipelineVertexInputStateCreateInfo {var_name}_info = {{
  .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
  .vertexBindingDescriptionCount = 1,
  .pVertexBindingDescriptions = (VkVertexInputBindingDescription[1]) {{
      {{
          .binding = 0,
          .stride = sizeof ({struct_name}),
          .inputRate = VK_VERTEX_INPUT_RATE_INSTANCE
      }}
  }},
  .vertexAttributeDescriptionCount = {expected},
  .pVertexAttributeDescriptions = (VkVertexInputAttributeDescription[{expected}]) {{''')

for match in matches:
    match match['type']:
        case "float":
            print(
f'''      {{
        .location = {match['location']},
        .binding = 0,
        .format = VK_FORMAT_R32_SFLOAT,
        .offset = G_STRUCT_OFFSET({struct_name}, {match['name']}),
      }},''')

        case "int":
            print(
f'''      {{
        .location = {match['location']},
        .binding = 0,
        .format = VK_FORMAT_R32_SINT,
        .offset = G_STRUCT_OFFSET({struct_name}, {match['name']}),
      }},''')

        case "uint":
            print(
f'''      {{
        .location = {match['location']},
        .binding = 0,
        .format = VK_FORMAT_R32_UINT,
        .offset = G_STRUCT_OFFSET({struct_name}, {match['name']}),
      }},''')

        case "uvec2":
            print(
f'''      {{
        .location = {match['location']},
        .binding = 0,
        .format = VK_FORMAT_R32G32_UINT,
        .offset = G_STRUCT_OFFSET({struct_name}, {match['name']}),
      }},''')

        case "vec2":
            print(
f'''      {{
        .location = {match['location']},
        .binding = 0,
        .format = VK_FORMAT_R32G32_SFLOAT,
        .offset = G_STRUCT_OFFSET({struct_name}, {match['name']}),
      }},''')

        case "vec4":
            print(
f'''      {{
        .location = {match['location']},
        .binding = 0,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .offset = G_STRUCT_OFFSET({struct_name}, {match['name']}),
      }},''')

        case "mat3x4":
            print(
f'''      {{
        .location = {match['location']},
        .binding = 0,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .offset = G_STRUCT_OFFSET({struct_name}, {match['name']}),
      }},
      {{
        .location = {int(match['location']) + 1},
        .binding = 0,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .offset = G_STRUCT_OFFSET({struct_name}, {match['name']}) + sizeof (float) * 4,
      }},
      {{
        .location = {int(match['location']) + 2},
        .binding = 0,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .offset = G_STRUCT_OFFSET({struct_name}, {match['name']}) + sizeof (float) * 8,
      }},''')

        case "mat4":
            print(
f'''      {{
        .location = {match['location']},
        .binding = 0,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .offset = G_STRUCT_OFFSET({struct_name}, {match['name']}),
      }},
      {{
        .location = {int(match['location']) + 1},
        .binding = 0,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .offset = G_STRUCT_OFFSET({struct_name}, {match['name']}) + sizeof (float) * 4,
      }},
      {{
        .location = {int(match['location']) + 2},
        .binding = 0,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .offset = G_STRUCT_OFFSET({struct_name}, {match['name']}) + sizeof (float) * 8,
      }},
      {{
        .location = {int(match['location']) + 3},
        .binding = 0,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .offset = G_STRUCT_OFFSET({struct_name}, {match['name']}) + sizeof (float) * 12,
      }},''')

        case _:
            raise Exception(f"Don't know what a {match['type']} is")

print("  },")
print("};")
