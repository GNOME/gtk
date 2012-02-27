/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* GTK - The GIMP Toolkit
 * Copyright (C) Javier Jardón <jjardon@gnome.org>
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

#ifndef __GTK_ACCEL_GROUP_PRIVATE_H__
#define __GTK_ACCEL_GROUP_PRIVATE_H__


struct _GtkAccelGroupPrivate
{
  guint               lock_count;
  GdkModifierType     modifier_mask;
  GSList             *acceleratables;
  guint               n_accels;
  GtkAccelGroupEntry *priv_accels;
};

#endif  /* __GTK_ACCEL_GROUP_PRIVATE_H__ */
