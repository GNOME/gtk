#include <gtk/gtk.h>

#define GDK_ARRAY_ELEMENT_TYPE gpointer
#define GDK_ARRAY_NAME pointer_vector
#define GDK_ARRAY_TYPE_NAME PointerVector
#include "../../gdk/gdkarrayimpl.c"

#define GDK_ARRAY_ELEMENT_TYPE gpointer
#define GDK_ARRAY_NAME prealloc_vector
#define GDK_ARRAY_TYPE_NAME PreallocVector
#define GDK_ARRAY_PREALLOC 1024
#include "../../gdk/gdkarrayimpl.c"

static inline guint
quick_random (guint prev)
{
  prev ^= prev << 13;
  prev ^= prev >> 17;
  prev ^= prev << 5;
  return prev;
}

typedef struct {
  const char *name;
  gsize stack_space;
  gpointer     (* create)       (gpointer space, gsize size);
  void         (* free)         (gpointer array);
  void         (* reserve)      (gpointer array, gsize size);
  gpointer     (* get)          (gpointer array, gsize pos);
  void         (* append)       (gpointer array, gpointer data);
  void         (* insert)       (gpointer array, gsize pos, gpointer data);
} Array;

static gpointer
create_ptrarray (gpointer space,
                 gsize    size)
{
  return g_ptr_array_sized_new (size);
}

static void
free_ptrarray (gpointer array)
{
  g_ptr_array_free (array, TRUE);
}

static void
reserve_ptrarray (gpointer array,
                  gsize    size)
{
  gsize length = ((GPtrArray *) array)->len;

  if (length >= size)
    return;

  g_ptr_array_set_size (array, size);
  g_ptr_array_set_size (array, length);
}

static gpointer
get_ptrarray (gpointer array,
              gsize    pos)
{
  return g_ptr_array_index ((GPtrArray *) array, pos);
}

static void
append_ptrarray (gpointer array,
                 gpointer data)
{
  g_ptr_array_add (array, data);
}

static void
insert_ptrarray (gpointer array,
                 gsize pos,
                 gpointer data)
{
  g_ptr_array_insert (array, pos, data);
}

static gpointer
create_vector (gpointer space,
               gsize    size)
{
  pointer_vector_init (space);

  if (size)
    pointer_vector_reserve ((PointerVector *) space, size);

  return space;
}

static void
free_vector (gpointer array)
{
  pointer_vector_clear (array);
}

static void
reserve_vector (gpointer array,
                gsize    size)
{
  pointer_vector_reserve (array, size);
}

static gpointer
get_vector (gpointer array,
            gsize    pos)
{
  return pointer_vector_get (array, pos);
}

static void
append_vector (gpointer array,
               gpointer data)
{
  pointer_vector_append (array, data);
}

static void
insert_vector (gpointer array,
               gsize pos,
               gpointer data)
{
  pointer_vector_splice (array, pos, 0, &data, 1);
}

static gpointer
create_prealloc (gpointer space,
                 gsize    size)
{
  prealloc_vector_init (space);

  if (size)
    prealloc_vector_reserve ((PreallocVector *) space, size);

  return space;
}

static void
free_prealloc (gpointer array)
{
  prealloc_vector_clear (array);
}

static void
reserve_prealloc (gpointer array,
                  gsize    size)
{
  prealloc_vector_reserve (array, size);
}

static gpointer
get_prealloc (gpointer array,
              gsize    pos)
{
  return prealloc_vector_get (array, pos);
}

static void
append_prealloc (gpointer array,
                 gpointer data)
{
  prealloc_vector_append (array, data);
}

static void
insert_prealloc (gpointer array,
                 gsize pos,
                 gpointer data)
{
  prealloc_vector_splice (array, pos, 0, &data, 1);
}

static void
do_random_access (const Array *klass,
                  guint        random,
                  gsize        size,
                  gsize        max_size)
{
  gpointer stack;
  gpointer array;
  guint i;
  guint position;
  gint64 start, end;
  guint iterations = 10000000;

  size = pow (100 * 100 * 100 * 100, (double) size / max_size);

  if (klass->stack_space)
    stack = g_alloca (klass->stack_space);
  else
    stack = NULL;
  array = klass->create (stack, size);
  for (i = 0; i < size; i++)
    klass->append (array, GSIZE_TO_POINTER (i));

  start = g_get_monotonic_time ();

  for (i = 0; i < iterations; i++)
    {
      position = random % size;
      random = quick_random (random);
      g_assert_cmpint (position, ==, GPOINTER_TO_SIZE (klass->get (array, position)));
    }

  end = g_get_monotonic_time ();

  g_print ("# \"random access\",\"%s\", %zu, %g\n",
           klass->name,
           size,
           ((double)(end - start)) / iterations);

  klass->free (array);
}

static void
do_linear_access (const Array *klass,
                  guint        random,
                  gsize        size,
                  gsize        max_size)
{
  gpointer stack;
  gpointer array;
  guint i;
  gint64 start, end;
  guint iterations = 1000000;

  size = pow (100 * 100 * 100 * 100, (double) size / max_size);

  if (klass->stack_space)
    stack = g_alloca (klass->stack_space);
  else
    stack = NULL;
  array = klass->create (stack, size);
  for (i = 0; i < size; i++)
    klass->append (array, GSIZE_TO_POINTER (i));

  start = g_get_monotonic_time ();

  for (i = 0; i < iterations; i++)
    {
      g_assert_cmpint (i % size, ==, GPOINTER_TO_SIZE (klass->get (array, i % size)));
    }

  end = g_get_monotonic_time ();

  g_print ("# \"linear access\", \"%s\", %zu, %g\n",
           klass->name,
           size,
           ((double)(end - start)) / iterations);

  klass->free (array);
}

static void
do_append (const Array *klass,
           guint        random,
           gsize        size,
           gsize        max_size)
{
  gpointer stack;
  gpointer array;
  guint i;
  gint64 start, end;
  int iterations = 10000;

  size = pow (100 * 1000 * 1000, (double) size / max_size);

  if (klass->stack_space)
    stack = g_alloca (klass->stack_space);
  else
    stack = NULL;
  array = klass->create (stack, size);
  for (i = 0; i < size; i++)
    klass->append (array, GSIZE_TO_POINTER (i));

  start = g_get_monotonic_time ();

  for (i = size; i < size + iterations; i++)
    {
      klass->append (array, GSIZE_TO_POINTER (i));
    }

  end = g_get_monotonic_time ();

  klass->free (array);

  g_print ("# \"append\", \"%s\", %zu, %g\n",
           klass->name,
           size,
           ((double) (end - start)) / iterations);
}

static void
do_insert (const Array *klass,
           guint        random,
           gsize        size,
           gsize        max_size)
{
  gpointer stack;
  gpointer array;
  guint i;
  gint64 start, end;
  int iterations = 10000;

  size = pow (25 * 25 * 25 * 25, (double) size / max_size);

  if (klass->stack_space)
    stack = g_alloca (klass->stack_space);
  else
    stack = NULL;
  array = klass->create (stack, size);
  for (i = 0; i < size; i++)
    klass->append (array, GSIZE_TO_POINTER (i));

  start = g_get_monotonic_time ();

  for (i = size; i < size + iterations; i++)
    {
      gsize position = random % size;
      random = quick_random (random);

      klass->insert (array, position, GSIZE_TO_POINTER (i));
    }

  end = g_get_monotonic_time ();

  klass->free (array);

  g_print ("# \"insert\", \"%s\", %zu, %g\n",
           klass->name,
           size,
           ((double) (end - start)) / iterations);
}

static void
do_create (const Array *klass,
           guint        random,
           gsize        size,
           gsize        max_size)
{
  gpointer stack;
  gpointer array;
  gsize i, j;
  gint64 start, end;
  gsize iterations = 100000;

  size = pow (4 * 4 * 4 * 4, (double) size / max_size);

  if (klass->stack_space)
    stack = g_alloca (klass->stack_space);
  else
    stack = NULL;

  start = g_get_monotonic_time ();

  for (i = 0; i < iterations; i++)
    {
      gsize position = random % size;
      random = quick_random (random);

      array = klass->create (stack, size);
      for (j = 0; j < size; j++)
        klass->append (array, GSIZE_TO_POINTER (i));

      klass->insert (array, position, GSIZE_TO_POINTER (i));
      klass->free (array);
    }

  end = g_get_monotonic_time ();

  g_print ("# \"create\", \"%s\", %zu, %g\n",
           klass->name,
           size,
           ((double) (end - start)) / iterations);
}

const Array all_arrays[] = {
  {
    "ptrarray",
    0,
    create_ptrarray,
    free_ptrarray,
    reserve_ptrarray,
    get_ptrarray,
    append_ptrarray,
    insert_ptrarray
  },
  {
    "vector",
    sizeof (PointerVector),
    create_vector,
    free_vector,
    reserve_vector,
    get_vector,
    append_vector,
    insert_vector
  },
  {
    "preallocated-vector",
    sizeof (PreallocVector),
    create_prealloc,
    free_prealloc,
    reserve_prealloc,
    get_prealloc,
    append_prealloc,
    insert_prealloc
  }
};

static void
run_test (void (* test_func) (const Array *klass, guint random, gsize size, gsize max_size))
{
  int max_size = 4;
  int size;
  int i;
  guint random = g_random_int ();

  for (i = 0; i < G_N_ELEMENTS (all_arrays); i++)
    {
      for (size = 1; size <= max_size; size++)
        {
          test_func (&all_arrays[i], random, size, max_size);
        }
    }
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv);

  g_print ("# \"test\",\"model\",\"model size\",\"time\"\n");
  run_test (do_random_access);
  run_test (do_linear_access);
  run_test (do_append);
  run_test (do_insert);
  run_test (do_create);

  return g_test_run ();
}
