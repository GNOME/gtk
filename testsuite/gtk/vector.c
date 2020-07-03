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

#include <locale.h>

#include <gtk/gtk.h>

static void
int_free_func (int data)
{
}

#define GTK_VECTOR_ELEMENT_TYPE int
#define GTK_VECTOR_NAME int_vector
#define GTK_VECTOR_TYPE_NAME IntVector
#include "vectorimpl.c"

#define GTK_VECTOR_ELEMENT_TYPE int
#define GTK_VECTOR_NAME pre_int_vector
#define GTK_VECTOR_TYPE_NAME PreIntVector
#define GTK_VECTOR_PREALLOC 100
#include "vectorimpl.c"

#define GTK_VECTOR_ELEMENT_TYPE int
#define GTK_VECTOR_NAME free_int_vector
#define GTK_VECTOR_TYPE_NAME FreeIntVector
#define GTK_VECTOR_FREE_FUNC int_free_func
#include "vectorimpl.c"

#define GTK_VECTOR_ELEMENT_TYPE int
#define GTK_VECTOR_NAME pre_free_int_vector
#define GTK_VECTOR_TYPE_NAME PreFreeIntVector
#define GTK_VECTOR_PREALLOC 100
#define GTK_VECTOR_FREE_FUNC int_free_func
#include "vectorimpl.c"

#define GTK_VECTOR_ELEMENT_TYPE int
#define GTK_VECTOR_NAME null_int_vector
#define GTK_VECTOR_TYPE_NAME NullIntVector
#define GTK_VECTOR_NULL_TERMINATED 1
#include "vectorimpl.c"

#define GTK_VECTOR_ELEMENT_TYPE int
#define GTK_VECTOR_NAME null_pre_int_vector
#define GTK_VECTOR_TYPE_NAME NullPreIntVector
#define GTK_VECTOR_PREALLOC 100
#define GTK_VECTOR_NULL_TERMINATED 1
#include "vectorimpl.c"

#define GTK_VECTOR_ELEMENT_TYPE int
#define GTK_VECTOR_NAME null_free_int_vector
#define GTK_VECTOR_TYPE_NAME NullFreeIntVector
#define GTK_VECTOR_FREE_FUNC int_free_func
#define GTK_VECTOR_NULL_TERMINATED 1
#include "vectorimpl.c"

#define GTK_VECTOR_ELEMENT_TYPE int
#define GTK_VECTOR_NAME null_pre_free_int_vector
#define GTK_VECTOR_TYPE_NAME NullPreFreeIntVector
#define GTK_VECTOR_PREALLOC 100
#define GTK_VECTOR_FREE_FUNC int_free_func
#define GTK_VECTOR_NULL_TERMINATED 1
#include "vectorimpl.c"

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);
  setlocale (LC_ALL, "C");

  g_test_add_func ("/intvector/simple", int_vector_test_simple);
  g_test_add_func ("/intvector/prealloc/simple", pre_int_vector_test_simple);
  g_test_add_func ("/intvector/freefunc/simple", free_int_vector_test_simple);
  g_test_add_func ("/intvector/prealloc/freefunc/simple", pre_free_int_vector_test_simple);
  g_test_add_func ("/intvector/null/simple", null_int_vector_test_simple);
  g_test_add_func ("/intvector/null/prealloc/simple", null_pre_int_vector_test_simple);
  g_test_add_func ("/intvector/null/freefunc/simple", null_free_int_vector_test_simple);
  g_test_add_func ("/intvector/null/prealloc/freefunc/simple", null_pre_free_int_vector_test_simple);
  g_test_add_func ("/intvector/splice", int_vector_test_splice);
  g_test_add_func ("/intvector/prealloc/splice", pre_int_vector_test_splice);
  g_test_add_func ("/intvector/freefunc/splice", free_int_vector_test_splice);
  g_test_add_func ("/intvector/prealloc/freefunc/splice", pre_free_int_vector_test_splice);
  g_test_add_func ("/intvector/null/splice", null_int_vector_test_splice);
  g_test_add_func ("/intvector/null/prealloc/splice", null_pre_int_vector_test_splice);
  g_test_add_func ("/intvector/null/freefunc/splice", null_free_int_vector_test_splice);
  g_test_add_func ("/intvector/null/prealloc/freefunc/splice", null_pre_free_int_vector_test_splice);

  return g_test_run ();
}
