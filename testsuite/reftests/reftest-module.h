/* GTK - The GIMP Toolkit
 * Copyright (C) 2014 Red Hat, Inc.
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

#ifndef __REFTEST_MODULE_H__
#define __REFTEST_MODULE_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef struct _ReftestModule ReftestModule;

ReftestModule * reftest_module_new              (const char     *directory,
                                                 const char     *module_name);
ReftestModule * reftest_module_new_self         (void);

ReftestModule * reftest_module_ref              (ReftestModule  *module);
void            reftest_module_unref            (ReftestModule  *module);

GCallback       reftest_module_lookup           (ReftestModule  *module,
                                                 const char     *function_name);

G_END_DECLS

#endif /* __REFTEST_MODULE_H__ */
