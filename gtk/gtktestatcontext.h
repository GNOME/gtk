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

/**
 * gtk_test_accessible_assert_role:
 * @accessible: a #GtkAccessible
 * @role: a #GtkAccessibleRole
 *
 * Checks whether a #GtkAccessible implementation has the given @role,
 * and raises an assertion if the condition is failed.
 */
#define gtk_test_accessible_assert_role(accessible,role)                G_STMT_START { \
                                                                          GtkAccessible *__a = GTK_ACCESSIBLE (accessible); \
                                                                          GtkAccessibleRole __r = (role); \
                                                                          if (gtk_test_accessible_has_role (__a, __r)) ; else \
                                                                            gtk_test_accessible_assertion_message_role (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, \
                                                                                                                        #accessible " == " #role, \
                                                                                                                        __a, __r); \
                                                                        } G_STMT_END
/**
 * gtk_test_accessible_assert_property:
 * @accessible: a #GtkAccessible
 * @property: a #GtkAccessibleProperty
 *
 * Checks whether a #GtkAccessible implementation contains the
 * given @property, and raises an assertion if the condition is
 * failed.
 */
#define gtk_test_accessible_assert_property(accessible,property)        G_STMT_START { \
                                                                          GtkAccessible *__a = GTK_ACCESSIBLE (accessible); \
                                                                          GtkAccessibleProperty __p = (property); \
                                                                          if (gtk_test_accessible_has_property (__a, __p)) ; else \
                                                                            gtk_test_accessible_assertion_message_property (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, \
                                                                                                                            #accessible " has " #property, \
                                                                                                                            __a, __p); \
                                                                        } G_STMT_END
/**
 * gtk_test_accessible_assert_relation:
 * @accessible: a #GtkAccessible
 * @relation: a #GtkAccessibleRelation
 *
 * Checks whether a #GtkAccessible implementation contains the
 * given @relation, and raises an assertion if the condition is
 * failed.
 */
#define gtk_test_accessible_assert_relation(accessible,relation)        G_STMT_START { \
                                                                          GtkAccessible *__a = GTK_ACCESSIBLE (accessible); \
                                                                          GtkAccessibleRelation __r = (relation); \
                                                                          if (gtk_test_accessible_has_relation (__a, __r)) ; else \
                                                                            gtk_test_accessible_assertion_message_relation (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, \
                                                                                                                            #accessible " has " #relation, \
                                                                                                                            __a, __r); \
                                                                        } G_STMT_END
/**
 * gtk_test_accessible_assert_state:
 * @accessible: a #GtkAccessible
 * @state: a #GtkAccessibleState
 *
 * Checks whether a #GtkAccessible implementation contains the
 * given @state, and raises an assertion if the condition is
 * failed.
 */
#define gtk_test_accessible_assert_state(accessible,state)              G_STMT_START { \
                                                                          GtkAccessible *__a = GTK_ACCESSIBLE (accessible); \
                                                                          GtkAccessibleState __s = (state); \
                                                                          if (gtk_test_accessible_has_state (__a, __s)) ; else \
                                                                            gtk_test_accessible_assertion_message_state (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, \
                                                                                                                         #accessible " has " #state, \
                                                                                                                         __a, __s); \
                                                                        } G_STMT_END

GDK_AVAILABLE_IN_ALL
gboolean        gtk_test_accessible_has_role            (GtkAccessible         *accessible,
                                                         GtkAccessibleRole      role);
GDK_AVAILABLE_IN_ALL
gboolean        gtk_test_accessible_has_property        (GtkAccessible         *accessible,
                                                         GtkAccessibleProperty  property);
GDK_AVAILABLE_IN_ALL
gboolean        gtk_test_accessible_has_relation        (GtkAccessible         *accessible,
                                                         GtkAccessibleRelation  relation);
GDK_AVAILABLE_IN_ALL
gboolean        gtk_test_accessible_has_state           (GtkAccessible         *accessible,
                                                         GtkAccessibleState     state);

GDK_AVAILABLE_IN_ALL
void    gtk_test_accessible_assertion_message_role      (const char        *domain,
                                                         const char        *file,
                                                         int                line,
                                                         const char        *func,
                                                         const char        *expr,
                                                         GtkAccessible     *accessible,
                                                         GtkAccessibleRole  role) G_GNUC_NORETURN;
GDK_AVAILABLE_IN_ALL
void    gtk_test_accessible_assertion_message_property  (const char            *domain,
                                                         const char            *file,
                                                         int                    line,
                                                         const char            *func,
                                                         const char            *expr,
                                                         GtkAccessible         *accessible,
                                                         GtkAccessibleProperty  property) G_GNUC_NORETURN;
GDK_AVAILABLE_IN_ALL
void    gtk_test_accessible_assertion_message_relation  (const char            *domain,
                                                         const char            *file,
                                                         int                    line,
                                                         const char            *func,
                                                         const char            *expr,
                                                         GtkAccessible         *accessible,
                                                         GtkAccessibleRelation  relation) G_GNUC_NORETURN;
GDK_AVAILABLE_IN_ALL
void    gtk_test_accessible_assertion_message_state     (const char            *domain,
                                                         const char            *file,
                                                         int                    line,
                                                         const char            *func,
                                                         const char            *expr,
                                                         GtkAccessible         *accessible,
                                                         GtkAccessibleState     state) G_GNUC_NORETURN;

G_END_DECLS
