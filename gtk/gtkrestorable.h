/*
 * Copyright © Red Hat
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
 */

#pragma once

#include <gdk/gdk.h>
#include <gtk/gtkenums.h>

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

G_BEGIN_DECLS

typedef struct _GtkSaveContext GtkSaveContext;

#define GTK_TYPE_SAVE_CONTEXT (gtk_save_context_get_type ())

GDK_AVAILABLE_IN_4_24
GType gtk_save_context_get_type (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_4_24
GtkSaveContext *gtk_save_context_ref (GtkSaveContext *self);

GDK_AVAILABLE_IN_4_24
void gtk_save_context_unref (GtkSaveContext *self);

GDK_AVAILABLE_IN_4_24
GtkSaveContext *gtk_save_context_child (GtkSaveContext *self,
                                        const char     *key);

GDK_AVAILABLE_IN_4_24
GFile *gtk_save_context_get_bulk_state_dir (GtkSaveContext *self);

GDK_AVAILABLE_IN_4_24
void gtk_save_context_insert (GtkSaveContext *self,
                              const char     *key,
                              const char     *format_string,
                              ...);

GDK_AVAILABLE_IN_4_24
void gtk_save_context_insert_value (GtkSaveContext *self,
                                    const char     *key,
                                    GVariant       *value);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GtkSaveContext, gtk_save_context_unref)

typedef struct _GtkRestoreContext GtkRestoreContext;

#define GTK_TYPE_RESTORE_CONTEXT (gtk_restore_context_get_type ())

GDK_AVAILABLE_IN_4_24
GType gtk_restore_context_get_type (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_4_24
GtkRestoreContext *gtk_restore_context_ref (GtkRestoreContext *self);

GDK_AVAILABLE_IN_4_24
void gtk_restore_context_unref (GtkRestoreContext *self);

GDK_AVAILABLE_IN_4_24
GtkRestoreContext *gtk_restore_context_child (GtkRestoreContext *self,
                                              const char        *key);

GDK_AVAILABLE_IN_4_24
GVariantDict *gtk_restore_context_get_state (GtkRestoreContext *self);

GDK_AVAILABLE_IN_4_24
GtkRestoreReason gtk_restore_context_get_reason (GtkRestoreContext *self);

GDK_AVAILABLE_IN_4_24
gboolean gtk_restore_context_contains (GtkRestoreContext *self,
                                       const char        *key);

GDK_AVAILABLE_IN_4_24
gboolean gtk_restore_context_lookup (GtkRestoreContext *self,
                                     const char        *key,
                                     const char        *format_string,
                                     ...);

GDK_AVAILABLE_IN_4_24
GVariant *gtk_restore_context_lookup_value (GtkRestoreContext  *self,
                                            const char         *key,
                                            const GVariantType *expected_type);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GtkRestoreContext, gtk_restore_context_unref)

#define GTK_TYPE_RESTORABLE (gtk_restorable_get_type ())

GDK_AVAILABLE_IN_4_24
G_DECLARE_INTERFACE (GtkRestorable, gtk_restorable, GTK, RESTORABLE, GObject)

struct _GtkRestorableInterface
{
  GTypeInterface parent_iface;

  void           (*save_state)             (GtkRestorable  *self,
                                            GtkSaveContext *context);
  void           (*save_state_async)       (GtkRestorable       *self,
                                            GtkSaveContext      *context,
                                            GCancellable        *cancellable,
                                            GAsyncReadyCallback  callback,
                                            gpointer             user_data);
  gboolean       (*save_state_finish)      (GtkRestorable  *self,
                                            GAsyncResult   *result,
                                            GError        **error);

  void           (*restore_state)          (GtkRestorable     *self,
                                            GtkRestoreContext *context);
  void           (*restore_state_async)    (GtkRestorable       *self,
                                            GtkRestoreContext   *context,
                                            GCancellable        *cancellable,
                                            GAsyncReadyCallback  callback,
                                            gpointer             user_data);
  gboolean       (*restore_state_finish)   (GtkRestorable  *self,
                                            GAsyncResult   *result,
                                            GError        **error);

  gboolean       (*check_state_dirty)      (GtkRestorable *self);
};

GDK_AVAILABLE_IN_4_24
void gtk_restorable_save_state (GtkRestorable       *self,
                                GtkSaveContext      *context,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data);

GDK_AVAILABLE_IN_4_24
gboolean gtk_restorable_save_state_finish (GtkRestorable  *self,
                                           GAsyncResult   *result,
                                           GError        **error);

GDK_AVAILABLE_IN_4_24
void gtk_restorable_restore_state (GtkRestorable       *self,
                                   GtkRestoreContext   *context,
                                   GCancellable        *cancellable,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data);

GDK_AVAILABLE_IN_4_24
gboolean gtk_restorable_restore_state_finish (GtkRestorable  *self,
                                              GAsyncResult   *result,
                                              GError        **error);

G_END_DECLS
