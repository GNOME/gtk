/* gdkmacosvulkancontext.h
 *
 * gdkmacosvulkancontext.h: macOS specific Vulkan wrappers
 *
 * Copyright (C) 2017 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
 * Copyright (C) 2025 Arjan Molenaar <gaphor@gmail.com>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "gdkconfig.h"

#ifdef GDK_RENDERING_VULKAN

#define VK_USE_PLATFORM_METAL_EXT

#include "gdkvulkancontextprivate.h"


G_BEGIN_DECLS

#define GDK_TYPE_MACOS_VULKAN_CONTEXT		(gdk_macos_vulkan_context_get_type ())
#define GDK_MACOS_VULKAN_CONTEXT(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), GDK_TYPE_MACOS_VULKAN_CONTEXT, GdkMacosVulkanContext))
#define GDK_IS_MACOS_VULKAN_CONTEXT(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GDK_TYPE_MACOS_VULKAN_CONTEXT))

typedef struct _GdkMacosVulkanContext GdkMacosVulkanContext;
typedef struct _GdkMacosVulkanContextClass GdkMacosVulkanContextClass;

GDK_AVAILABLE_IN_ALL
GType gdk_macos_vulkan_context_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* !GDK_RENDERING_VULKAN */

