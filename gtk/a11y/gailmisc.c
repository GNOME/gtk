/* GAIL - The GNOME Accessibility Implementation Library
 * Copyright 2001, 2002, 2003 Sun Microsystems Inc.
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

#include <gtk/gtk.h>
#include "gailmisc.h"


G_DEFINE_TYPE (GailMisc, _gail_misc, ATK_TYPE_MISC)

static void
gail_misc_threads_enter (AtkMisc *misc)
{
  gdk_threads_enter ();
}

static void
gail_misc_threads_leave (AtkMisc *misc)
{
  gdk_threads_leave ();
}

static void
_gail_misc_class_init (GailMiscClass *klass)
{
  AtkMiscClass *misc_class = ATK_MISC_CLASS (klass);

  misc_class->threads_enter = gail_misc_threads_enter;
  misc_class->threads_leave = gail_misc_threads_leave;
}

static void
_gail_misc_init (GailMisc *misc)
{
}
