#include "config.h"

#if defined(HAVE_POSIX_MEMALIGN) && !defined(_XOPEN_SOURCE)
# define _XOPEN_SOURCE 600
#endif

#include "gskallocprivate.h"

#if defined(HAVE_MEMALIGN) || defined(HAVE__ALIGNED_MALLOC)
/* Required for _aligned_malloc() and _aligned_free() on Windows */
#include <malloc.h>
#endif

#include <errno.h>
#include <string.h>

#ifdef HAVE__ALIGNED_MALLOC
/* _aligned_malloc() takes parameters of aligned_alloc() in reverse order */
# define aligned_alloc(alignment, size) _aligned_malloc (size, alignment)

/* _aligned_malloc()'ed memory must be freed by _align_free() on Windows */
# define aligned_free(x) _aligned_free (x)
#else
# define aligned_free(x) free (x)
#endif

/*
 * gsk_aligned_alloc:
 * @size: the size of the memory to allocate
 * @number: the multiples of @size to allocate
 * @alignment: the alignment to be enforced, as a power of 2
 *
 * Allocates @number times @size memory, with the given @alignment.
 *
 * If the total requested memory overflows %G_MAXSIZE, this function
 * will abort.
 *
 * If allocation fails, this function will abort, in line with
 * the behaviour of GLib.
 *
 * Returns: (transfer full): the allocated memory
 */
gpointer
gsk_aligned_alloc (gsize size,
                   gsize number,
                   gsize alignment)
{
  gpointer res = NULL;
  gsize real_size;

  if (G_UNLIKELY (number > G_MAXSIZE / size))
    {
#ifndef G_DISABLE_ASSERT
      g_error ("%s: overflow in the allocation of (%"G_GSIZE_FORMAT" x %"G_GSIZE_FORMAT") bytes",
               G_STRLOC, size, number);
#endif
      return NULL;
    }

  real_size = size * number;

#ifndef G_DISABLE_ASSERT
  errno = 0;
#endif

#if defined(HAVE_POSIX_MEMALIGN) && !defined (MALLOC_IS_ALIGNED16)
  errno = posix_memalign (&res, alignment, real_size);
#elif (defined(HAVE_ALIGNED_ALLOC) || defined(HAVE__ALIGNED_MALLOC)) && !defined (MALLOC_IS_ALIGNED16)
  /* real_size must be a multiple of alignment */
  if (real_size % alignment != 0)
    {
      gsize offset = real_size % alignment;
      real_size += (alignment - offset);
    }
    
  res = aligned_alloc (alignment, real_size);
#elif defined(HAVE_MEMALIGN) && !defined (MALLOC_IS_ALIGNED16)
  res = memalign (alignment, real_size);
#else
  res = malloc (real_size);
#endif

#ifndef G_DISABLE_ASSERT
  if (errno != 0 || res == NULL)
    {
      g_error ("%s: error in the allocation of (%"G_GSIZE_FORMAT" x %"G_GSIZE_FORMAT") bytes: %s",
               G_STRLOC, size, number, strerror (errno));
    }
#endif

  return res;
}

/*
 * gsk_aligned_alloc0:
 * @size: the size of the memory to allocate
 * @number: the multiples of @size to allocate
 * @alignment: the alignment to be enforced, as a power of 2
 *
 * Allocates @number times @size memory, with the given @alignment,
 * like gsk_aligned_alloc(), but it also clears the memory.
 *
 * Returns: (transfer full): the allocated, cleared memory
 */
gpointer
gsk_aligned_alloc0 (gsize size,
                    gsize number,
                    gsize alignment)
{
  gpointer mem;

  mem = gsk_aligned_alloc (size, number, alignment);

  if (mem)
    memset (mem, 0, size * number);

  return mem;
}

/*
 * gsk_aligned_free:
 * @mem: the memory to deallocate
 *
 * Frees the memory allocated by gsk_aligned_alloc().
 */
void
gsk_aligned_free (gpointer mem)
{
  aligned_free (mem);
}
