/* gskglrenderer.h
 *
 * Copyright 2020 Christian Hergert <chergert@redhat.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#ifndef __GI_SCANNER__
# define WARNING_MSG "#include <gsk/gsk.h> instead of <gsk/gl/gskglrenderer.h> to avoid this warning"
# ifdef _MSC_VER
#  pragma message("WARNING: " WARNING_MSG)
# else
#  warning WARNING_MSG
# endif
# undef WARNING_MSG
#endif

#include <gsk/gsk.h>
