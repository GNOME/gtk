/* GTK+ - accessibility implementations
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
#include "gtkaccessibilitymisc.h"

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

G_DEFINE_TYPE (GtkMiscImpl, _gtk_misc_impl, ATK_TYPE_MISC)

static void
gtk_misc_impl_threads_enter (AtkMisc *misc)
{
  gdk_threads_enter ();
}

static void
gtk_misc_impl_threads_leave (AtkMisc *misc)
{
  gdk_threads_leave ();
}

static void
_gtk_misc_impl_class_init (GtkMiscImplClass *klass)
{
  AtkMiscClass *misc_class = ATK_MISC_CLASS (klass);

  misc_class->threads_enter = gtk_misc_impl_threads_enter;
  misc_class->threads_leave = gtk_misc_impl_threads_leave;
}

static void
_gtk_misc_impl_init (GtkMiscImpl *misc)
{
}

G_GNUC_END_IGNORE_DEPRECATIONS
