/* gtktestatcontext.h: Test AT context
 *
 * Copyright 2020  GNOME Foundation
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
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
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtkatcontext.h>

G_BEGIN_DECLS

#define gtk_test_accessible_assert_cmprole(accessible,role)             G_STMT_START { \
                                                                          GtkAccessible *__a = GTK_ACCESSIBLE (accessible); \
                                                                          GtkAccessibleRole __r = (role); \
                                                                          if (gtk_test_accessible_has_role (__a, __r)) ; else \
                                                                            gtk_test_accessible_assertion_message_cmprole (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, \
                                                                                                                           #accessible " == " #role, \
                                                                                                                           __a, __r); \
                                                                        } G_STMT_END
#define gtk_test_accessible_assert_cmpproperty(accessible,property)     G_STMT_START { \
                                                                          GtkAccessible *__a = GTK_ACCESSIBLE (accessible); \
                                                                          GtkAccessibleProperty __p = (property); \
                                                                          if (gtk_test_accessible_has_property (__a, __p)) ; else \
                                                                            gtk_test_accessible_assertion_message_cmpproperty (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, \
                                                                                                                               #accessible " == " #property, \
                                                                                                                               __a, __p); \
                                                                        } G_STMT_END

GDK_AVAILABLE_IN_ALL
gboolean        gtk_test_accessible_has_role            (GtkAccessible         *accessible,
                                                         GtkAccessibleRole      role);
GDK_AVAILABLE_IN_ALL
gboolean        gtk_test_accessible_has_property        (GtkAccessible         *accessible,
                                                         GtkAccessibleProperty  property);

GDK_AVAILABLE_IN_ALL
void    gtk_test_accessible_assertion_message_cmprole           (const char        *domain,
                                                                 const char        *file,
                                                                 int                line,
                                                                 const char        *func,
                                                                 const char        *expr,
                                                                 GtkAccessible     *accessible,
                                                                 GtkAccessibleRole  role) G_GNUC_NORETURN;
GDK_AVAILABLE_IN_ALL
void    gtk_test_accessible_assertion_message_cmpproperty       (const char            *domain,
                                                                 const char            *file,
                                                                 int                    line,
                                                                 const char            *func,
                                                                 const char            *expr,
                                                                 GtkAccessible         *accessible,
                                                                 GtkAccessibleProperty  property) G_GNUC_NORETURN;


G_END_DECLS
