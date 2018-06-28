/* GTK+ - accessibility implementations
 * Copyright 2001 Sun Microsystems Inc.
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

#include "gtkaccessibility.h"
#include "gtkaccessibilityutil.h"

#ifdef GDK_WINDOWING_X11
#include <atk-bridge.h>
#endif

static int initialized = FALSE;

void
_gtk_accessibility_init (void)
{
  if (initialized)
    return;

  initialized = TRUE;

  _gtk_accessibility_override_atk_util ();

#ifdef GDK_WINDOWING_X11
  atk_bridge_adaptor_init (NULL, NULL);
#endif

}
