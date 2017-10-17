/* GTK - The GIMP Toolkit
 *
 * Copyright © 2017 Benjamin Otte <otte@gnome.org>
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

#ifndef __GSK_SL_ENVIRONMENT_PRIVATE_H__
#define __GSK_SL_ENVIRONMENT_PRIVATE_H__

#include <glib.h>

#include "gsksltypesprivate.h"

G_BEGIN_DECLS

GskSlEnvironment *      gsk_sl_environment_new                  (GskSlShaderStage      stage,
                                                                 GskSlProfile          profile,
                                                                 guint                 version);
GskSlEnvironment *      gsk_sl_environment_new_similar          (GskSlEnvironment     *environment,
                                                                 GskSlProfile          profile,
                                                                 guint                 version,
                                                                 GError              **error);

GskSlEnvironment *      gsk_sl_environment_ref                  (GskSlEnvironment     *environment);
void                    gsk_sl_environment_unref                (GskSlEnvironment     *environment);

GskSlShaderStage        gsk_sl_environment_get_stage            (GskSlEnvironment     *environment);
GskSlProfile            gsk_sl_environment_get_profile          (GskSlEnvironment     *environment);
guint                   gsk_sl_environment_get_version          (GskSlEnvironment     *environment);

GskSlScope *            gsk_sl_environment_create_scope         (GskSlEnvironment     *environment);

G_END_DECLS

#endif /* __GSK_SL_ENVIRONMENT_PRIVATE_H__ */
