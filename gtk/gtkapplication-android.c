/*
 * Copyright (c) 2024 Florian "sp1rit" <sp1rit@disroot.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the licence, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Florian "sp1rit" <sp1rit@disroot.org>
 */

#include "config.h"

#include "gtkapplicationprivate.h"

typedef GtkApplicationImplClass GtkApplicationImplAndroidClass;

typedef struct {
  GtkApplicationImpl impl;
} GtkApplicationImplAndroid;

G_DEFINE_TYPE (GtkApplicationImplAndroid, gtk_application_impl_android, GTK_TYPE_APPLICATION_IMPL)

static guint
gtk_application_impl_android_inhibit (GtkApplicationImpl         *impl,
                                      GtkWindow                  *window,
                                      GtkApplicationInhibitFlags  flags,
                                      const char                 *reason)
{
  // if (flags & GTK_APPLICATION_INHIBIT_SUSPEND)
    // TODO: iterate over all active surfaces and call toplevel_inhibit_suspend for all toplevels
    // potentionally for GTK_APPLICATION_INHIBIT_SWITCH lockTask mode?
  return 0;
}

static void
gtk_application_impl_android_uninhibit (GtkApplicationImpl *impl, guint cookie)
{}

static void
gtk_application_impl_android_init (GtkApplicationImplAndroid *self)
{}

static void
gtk_application_impl_android_class_init (GtkApplicationImplClass *klass)
{
  klass->inhibit = gtk_application_impl_android_inhibit;
  klass->uninhibit = gtk_application_impl_android_uninhibit;
}
