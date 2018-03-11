/* gtkcupssecretsutils.h: Helper to use a secrets service for printer passwords
 * Copyright (C) 2014 Intevation GmbH
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
#ifndef __GTK_SECRETS_UTILS_H__
#define __GTK_SECRETS_UTILS_H__

#include <glib.h>

#include "gtkcupsutils.h"

G_BEGIN_DECLS

void  gtk_cups_secrets_service_query_task (gpointer                   source_object,
                                           GCancellable              *cancellable,
                                           GAsyncReadyCallback        callback,
                                           gpointer                   user_data,
                                           const gchar               *printer_uri,
                                           gchar                    **auth_info_required);
guint gtk_cups_secrets_service_watch      (GBusNameAppearedCallback   appeared,
                                           GBusNameVanishedCallback   vanished,
                                           gpointer                   user_data);
void  gtk_cups_secrets_service_store      (gchar                    **auth_info,
                                           gchar                    **auth_info_labels,
                                           const gchar               *printer_uri);

G_END_DECLS

#endif /* __GTK_SECRETS_UTILS_H__ */
