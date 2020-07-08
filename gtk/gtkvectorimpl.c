/*
 * Copyright © 2020 Benjamin Otte
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
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#include <glib.h>

G_BEGIN_DECLS

#ifndef GTK_VECTOR_TYPE_NAME
#define GTK_VECTOR_TYPE_NAME GtkVector
#endif

#ifndef GTK_VECTOR_NAME
#define GTK_VECTOR_NAME gtk_vector
#endif

#ifndef GTK_VECTOR_ELEMENT_TYPE
#define GTK_VECTOR_ELEMENT_TYPE gpointer
#endif

#ifdef GTK_VECTOR_PREALLOC
#if GTK_VECTOR_PREALLOC == 0
#undef GTK_VECTOR_PREALLOC
#endif
#endif

#ifdef GTK_VECTOR_NULL_TERMINATED
#define GTK_VECTOR_REAL_SIZE(_size) ((_size) + 1)
#else
#define GTK_VECTOR_REAL_SIZE(_size) (_size)
#endif

/* make this readable */
#define _T_ GTK_VECTOR_ELEMENT_TYPE
#define GtkVector GTK_VECTOR_TYPE_NAME
#define gtk_vector_paste_more(GTK_VECTOR_NAME, func_name) GTK_VECTOR_NAME ## _ ## func_name
#define gtk_vector_paste(GTK_VECTOR_NAME, func_name) gtk_vector_paste_more (GTK_VECTOR_NAME, func_name)
#define gtk_vector(func_name) gtk_vector_paste (GTK_VECTOR_NAME, func_name)

typedef struct GtkVector GtkVector;

struct GtkVector
{
  _T_ *start;
  _T_ *end;
  _T_ *end_allocation;
#ifdef GTK_VECTOR_PREALLOC
  _T_ preallocated[GTK_VECTOR_REAL_SIZE(GTK_VECTOR_PREALLOC)];
#endif
};

/* no G_GNUC_UNUSED here, if you don't use an array type, remove it. */
static inline void
gtk_vector(init) (GtkVector *self)
{
#ifdef GTK_VECTOR_PREALLOC
  self->start = self->preallocated;
  self->end = self->start;
  self->end_allocation = self->start + GTK_VECTOR_PREALLOC;
#ifdef GTK_VECTOR_NULL_TERMINATED
  *self->start = *(_T_[1]) {};
#endif
#else
  self->start = NULL;
  self->end = NULL;
  self->end_allocation = NULL;
#endif
}

static inline void
gtk_vector(free_elements) (_T_ *start,
                           _T_ *end)
{
#ifdef GTK_VECTOR_FREE_FUNC
  _T_ *e;
  for (e = start; e < end; e++)
#ifdef GTK_VECTOR_BY_VALUE
    GTK_VECTOR_FREE_FUNC (e);
#else
    GTK_VECTOR_FREE_FUNC (*e);
#endif
#endif
}

/* no G_GNUC_UNUSED here */
static inline void
gtk_vector(clear) (GtkVector *self)
{
  gtk_vector(free_elements) (self->start, self->end);

#ifdef GTK_VECTOR_PREALLOC
  if (self->start != self->preallocated)
#endif
    g_free (self->start);
  gtk_vector(init) (self);
}

G_GNUC_UNUSED static inline _T_ *
gtk_vector(get_data) (const GtkVector *self)
{
  return self->start;
}

G_GNUC_UNUSED static inline _T_ *
gtk_vector(index) (const GtkVector *self,
                   gsize            pos)
{
  return self->start + pos;
}

G_GNUC_UNUSED static inline gsize
gtk_vector(get_capacity) (const GtkVector *self)
{
  return self->end_allocation - self->start;
}

G_GNUC_UNUSED static inline gsize
gtk_vector(get_size) (const GtkVector *self)
{
  return self->end - self->start;
}

G_GNUC_UNUSED static inline gboolean
gtk_vector(is_empty) (const GtkVector *self)
{
  return self->end == self->start;
}

G_GNUC_UNUSED static void
gtk_vector(reserve) (GtkVector *self,
                     gsize      n)
{
  gsize new_size, size;

  if (n <= gtk_vector(get_capacity) (self))
    return;

  size = gtk_vector(get_size) (self);
  new_size = 1 << g_bit_storage (MAX (GTK_VECTOR_REAL_SIZE (n), 16) - 1);

#ifdef GTK_VECTOR_PREALLOC
  if (self->start == self->preallocated)
    {
      self->start = g_new (_T_, new_size);
      memcpy (self->start, self->preallocated, sizeof (_T_) * GTK_VECTOR_REAL_SIZE (size));
    }
  else
#endif
#ifdef GTK_VECTOR_NULL_TERMINATED
  if (self->start == NULL)
    {
      self->start = g_new (_T_, new_size);
      *self->start = *(_T_[1]) {};
    }
  else
#endif
    self->start = g_renew (_T_, self->start, new_size);

  self->end = self->start + size;
  self->end_allocation = self->start + new_size;
#ifdef GTK_VECTOR_NULL_TERMINATED
  self->end_allocation--;
#endif
}

G_GNUC_UNUSED static void
gtk_vector(splice) (GtkVector *self,
                    gsize      pos,
                    gsize      removed,
                    _T_       *additions,
                    gsize      added)
{
  gsize size;
  gsize remaining;

  size = gtk_vector(get_size) (self);
  g_assert (pos + removed <= size);
  remaining = size - pos - removed;

  gtk_vector(free_elements) (gtk_vector(index) (self, pos),
                             gtk_vector(index) (self, pos + removed));

  gtk_vector(reserve) (self, size - removed + added);

  if (GTK_VECTOR_REAL_SIZE (remaining) && removed != added)
    memmove (gtk_vector(index) (self, pos + added),
             gtk_vector(index) (self, pos + removed),
             GTK_VECTOR_REAL_SIZE (remaining) * sizeof (_T_));

  if (added)
    {
      if (additions)
        memcpy (gtk_vector(index) (self, pos),
                additions,
                added * sizeof (_T_));
      else
        memset (gtk_vector(index) (self, pos), 0, added * sizeof (_T_));
    }

  /* might overflow, but does the right thing */
  self->end += added - removed;
}

G_GNUC_UNUSED static void
gtk_vector(set_size) (GtkVector *self,
                      gsize      new_size)
{
  gsize old_size = gtk_vector(get_size) (self);
  if (new_size > old_size)
    gtk_vector(splice) (self, old_size, 0, NULL, new_size - old_size);
  else
    gtk_vector(splice) (self, new_size, old_size - new_size, NULL, 0);
}

G_GNUC_UNUSED static void
gtk_vector(append) (GtkVector *self,
#ifdef GTK_VECTOR_BY_VALUE
                    _T_       *value)
#else
                    _T_        value)
#endif
{
  gtk_vector(splice) (self, 
                      gtk_vector(get_size) (self),
                      0,
#ifdef GTK_VECTOR_BY_VALUE
                      value,
#else
                      &value,
#endif
                      1);
}

#ifdef GTK_VECTOR_BY_VALUE
G_GNUC_UNUSED static _T_ *
gtk_vector(get) (const GtkVector *self,
                 gsize            pos)
{
  return gtk_vector(index) (self, pos);
}
#else
G_GNUC_UNUSED static _T_ 
gtk_vector(get) (const GtkVector *self,
                 gsize            pos)
{
  return *gtk_vector(index) (self, pos);
}
#endif


#ifndef GTK_VECTOR_NO_UNDEF

#undef _T_
#undef GtkVector
#undef gtk_vector_paste_more
#undef gtk_vector_paste
#undef gtk_vector
#undef GTK_VECTOR_REAL_SIZE

#undef GTK_VECTOR_BY_VALUE
#undef GTK_VECTOR_ELEMENT_TYPE
#undef GTK_VECTOR_FREE_FUNC
#undef GTK_VECTOR_NAME
#undef GTK_VECTOR_NULL_TERMINATED
#undef GTK_VECTOR_PREALLOC
#undef GTK_VECTOR_TYPE_NAME

#endif
