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

#include "config.h"

#include "gtkpendingoperationprivate.h"

#include <gio/gio.h>

/**
 * GtkPendingOperation:
 *
 * Handle to an in-progress operation. This object is created by GTK and then
 * given to the application via a signal handler. The application can then call
 * [method@Gtk.PendingOperation.defer] to delay the completion of the operation.
 *
 * Since: 4.22
 */

typedef struct {
  guint defer_count;
} GtkPendingOperationPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (GtkPendingOperation,
                                     gtk_pending_operation,
                                     G_TYPE_OBJECT)

static void
gtk_pending_operation_init (GtkPendingOperation *operation)
{
}

static void
gtk_pending_operation_real_fire (GtkPendingOperation *operation)
{
  g_critical ("GtkPendingOperation::fire not implemented for subclass '%s'",
              G_OBJECT_TYPE_NAME (operation));
}

static void
gtk_pending_operation_class_init (GtkPendingOperationClass *class)
{
  class->fire = gtk_pending_operation_real_fire;
}

/**
 * gtk_pending_operation_defer:
 * @save: a [class@Gtk.PendingOperation]
 *
 * Increases the defer count of the handle. This indicates that an application is
 * still doing asynchronous work related to this operation, and so the operation
 * is not yet complete.
 *
 * Once the asynchronous work is done, the application should call
 * [method@Gtk.PendingOperation.complete] to notify GTK of the completion. Each
 * call to [method@Gtk.PendingOperation.defer] must correspond to a call to
 * [method@Gtk.PendingOperation.complete].
 *
 * Since: 4.22
 */
void
gtk_pending_operation_defer (GtkPendingOperation *operation)
{
  GtkPendingOperationPrivate *priv = gtk_pending_operation_get_instance_private (self);

  g_return_if_fail (GTK_IS_PENDING_OPERATION (operation));
  g_return_if_fail (priv->defer_count != -1);

  priv->defer_count++;
}

/**
 * gtk_pending_operation_complete:
 * @save: a [class@Gtk.PendingOperation]
 *
 * Decreases the defer count of the handle. This indicates that an asynchronous
 * operation was completed.
 *
 * Each call to [method@Gtk.PendingOperation.complete] must correspond to a
 * previous call to [method@Gtk.PendingOperation.defer].
 *
 * Once the count reaches zero, the application is done with this operation and
 * GTK is able to complete it. After this, it is not permissible to use this
 * handle except for reference counting operations.
 *
 * Since: 4.22
 */
void
gtk_pending_operation_complete (GtkPendingOperation *operation)
{
  GtkPendingOperationPrivate *priv = gtk_pending_operation_get_instance_private (self);
  GtkPendingOperationClass *class = GTK_PENDING_OPERATION_GET_CLASS (operation);

  g_return_if_fail (GTK_IS_PENDING_OPERATION (operation));
  g_return_if_fail (priv->defer_count != -1);
  g_return_if_fail (priv->defer_count > 0);

  priv->defer_count--;

  if (priv->defer_count == 0)
    {
      class->fire (operation);
      priv->defer_count = -1; /* Invalidate the object */
    }
}

#define GTK_TYPE_SIMPLE_PENDING_OPERATION (gtk_simple_pending_operation_get_type ())

G_DECLARE_FINAL_TYPE (GtkSimplePendingOperation,
                      gtk_simple_pending_operation,
                      GTK, SIMPLE_PENDING_OPERATION,
                      GtkPendingOperation)

struct _GtkSimplePendingOperation
{
  GtkPendingOperation parent_instance;

  GtkSimplePendingOperationCallback callback;
  gpointer user_data;
}

G_DEFINE_FINAL_TYPE (GtkSimplePendingOperation,
                     gtk_simple_pending_operation,
                     GTK_TYPE_PENDING_OPERATION)

static void
gtk_simple_pending_operation_init (GtkSimplePendingOperation *operation)
{
}

static void
gtk_simple_pending_operation_fire (GtkPendingOperation *operation)
{
  GtkSimplePendingOperation *self = GTK_SIMPLE_PENDING_OPERATION (operation);
  if (self->callback)
    self->callback(operation, self->user_data);
}

static void
gtk_simple_pending_operation_class_init (GtkSimplePendingOperationClass *class)
{
  GtkPendingOperationClass *pending_class = GTK_PENDING_OPERATION_CLASS (class);
  pending_class->fire = gtk_simple_pending_operation_fire;
}

GtkPendingOperation *
gtk_simple_pending_operation_new (GtkSimplePendingOperationCallback callback,
                                  gpointer                          data)
{
  GtkSimplePendingOperation *operation = g_object_new (GTK_TYPE_SIMPLE_PENDING_OPERATION,
                                                       NULL);
  operation->callback = callback;
  operation->user_data = data;
  return GTK_PENDING_OPERATION (operation);
}

GtkPendingOperation *
gtk_noop_pending_operation_new (void)
{
  return gtk_simple_pending_operation_new (NULL, NULL);
}
