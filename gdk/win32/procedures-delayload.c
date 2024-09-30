/* GTK - The GIMP Toolkit
 * Copyright (C) 2024 the GTK team
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

#include "config.h"
#include "procedures.h"

#include <windows.h>
#include <apiquery2.h>
#include <appmodel.h>

#include <stdint.h>

/* This file is used to generate entries in the delayload import table for
 * all the optional procedures used by GTK. That's needed for appcontainer
 * environments, where procedures can be obtained dynamically only when
 * present in the delayload import table (even though we don't use delay
 * loading)
 */

/* This file must be compiled with OneCoreUAP_apiset.lib
 * and WINAPI_PARTITION_DESKTOP
 */

# if !WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
#   error "Invalid WINAPI_FAMILY_PARTITION, WINAPI_PARTITION_DESKTOP is required"
# endif

static void
api_entries_dummy_ctor (void)
{
  uintptr_t dummy_value =
# define PROCEDURE(name, api_set_id, module_id, os_version) \
    ((uintptr_t) (void*) name) +
    PROCEDURES
# endif
  0;

  EncodePointer ((void*)dummy_value);
}

G_DEFINE_CONSTRUCTOR (api_entries_dummy_ctor)

