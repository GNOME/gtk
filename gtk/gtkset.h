/*
 * Copyright © 2019 Red Hat, Inc.
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Matthias Clasen <mclasen@redhat.com>
 */

#ifndef __GTK_SET_H__
#define __GTK_SET_H__

#include <glib.h>

typedef struct _GtkSet GtkSet;
typedef struct _GtkSetIter GtkSetIter;

struct _GtkSetIter
{
  gpointer dummy1;
  gpointer dummy2;
  int      dummy3;
  int      dummy4;
};

GtkSet   *gtk_set_new          (void);
void      gtk_set_free         (GtkSet   *set);

gboolean  gtk_set_contains     (GtkSet   *set,
                                guint     item);

void      gtk_set_remove_all   (GtkSet   *set);
void      gtk_set_add_item     (GtkSet   *set,
                                guint     item);
void      gtk_set_remove_item  (GtkSet   *set,
                                guint     item);
void      gtk_set_add_range    (GtkSet   *set,
                                guint     first,
                                guint     n);
void      gtk_set_remove_range (GtkSet   *set,
                                guint     first,
                                guint     n);

void      gtk_set_shift        (GtkSet   *set,
                                guint     first,
                                int       shift);

void      gtk_set_iter_init    (GtkSetIter *iter,
                                GtkSet     *set);
gboolean  gtk_set_iter_next    (GtkSetIter *iter,
                                guint      *item);

#endif  /* __GTK_SET_H__ */
