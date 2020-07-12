/* Lots of code for an adaptive, stable, natural mergesort.  There are many
 * pieces to this algorithm; read listsort.txt for overviews and details.
 */

#include "config.h"

#include "gtktimsortprivate.h"

/*
 * This is the minimum sized sequence that will be merged.  Shorter
 * sequences will be lengthened by calling binarySort.  If the entire
 * array is less than this length, no merges will be performed.
 *
 * This constant should be a power of two.  It was 64 in Tim Peter's C
 * implementation, but 32 was empirically determined to work better in
 * [Android's Java] implementation.  In the unlikely event that you set
 * this constant to be a number that's not a power of two, you'll need
 * to change the compute_min_run() computation.
 *
 * If you decrease this constant, you must change the
 * GTK_TIM_SORT_MAX_PENDING value, or you risk running out of space.
 * See Python's listsort.txt for a discussion of the minimum stack
 * length required as a function of the length of the array being sorted and
 * the minimum merge sequence length.
 */
#define MIN_MERGE 32


/*
 * When we get into galloping mode, we stay there until both runs win less
 * often than MIN_GALLOP consecutive times.
 */
#define MIN_GALLOP 7

/*
 * Returns the minimum acceptable run length for an array of the specified
 * length. Natural runs shorter than this will be extended with binary sort.
 *
 * Roughly speaking, the computation is:
 *
 *  If n < MIN_MERGE, return n (it's too small to bother with fancy stuff).
 *  Else if n is an exact power of 2, return MIN_MERGE/2.
 *  Else return an int k, MIN_MERGE/2 <= k <= MIN_MERGE, such that n/k
 *   is close to, but strictly less than, an exact power of 2.
 *
 * For the rationale, see listsort.txt.
 *
 * @param n the length of the array to be sorted
 * @return the length of the minimum run to be merged
 */
static gsize
compute_min_run (gsize n)
{
  gsize r = 0; // Becomes 1 if any 1 bits are shifted off

  while (n >= MIN_MERGE) {
    r |= (n & 1);
    n >>= 1;
  }
  return n + r;
}

void
gtk_tim_sort_init (GtkTimSort       *self,
                   gpointer          base,
                   gsize             size,
                   gsize             element_size,
                   GCompareDataFunc  compare_func,
                   gpointer          data)
{
  self->element_size = element_size;
  self->base = base;
  self->size = size;
  self->compare_func = compare_func;
  self->data = data;

  self->min_gallop = MIN_GALLOP;
  self->min_run = compute_min_run (size);

  self->tmp = NULL;
  self->tmp_length = 0;
  self->pending_runs = 0;
}

void
gtk_tim_sort_finish (GtkTimSort *self)
{
  g_free (self->tmp);
}

void
gtk_tim_sort (gpointer         base,
              gsize            size,
              gsize            element_size,
              GCompareDataFunc compare_func,
              gpointer         user_data)
{
  GtkTimSort self;

  gtk_tim_sort_init (&self, base, size, element_size, compare_func, user_data);

  while (gtk_tim_sort_step (&self));

  gtk_tim_sort_finish (&self);
}

static inline int
gtk_tim_sort_compare (GtkTimSort *self,
                      gpointer    a,
                      gpointer    b)
{
  return self->compare_func (a, b, self->data);
}


/**
 * Pushes the specified run onto the pending-run stack.
 *
 * @param runBase index of the first element in the run
 * @param runLen  the number of elements in the run
 */
static void
gtk_tim_sort_push_run (GtkTimSort *self,
                       void       *base,
                       gsize       len)
{
  g_assert (self->pending_runs < GTK_TIM_SORT_MAX_PENDING);

  self->run[self->pending_runs].base = base;
  self->run[self->pending_runs].len = len;
  self->pending_runs++;

  /* Advance to find next run */
  self->base = ((char *) self->base) + len * self->element_size;
  self->size -= len;
}

/**
 * Ensures that the external array tmp has at least the specified
 * number of elements, increasing its size if necessary.  The size
 * increases exponentially to ensure amortized linear time complexity.
 *
 * @param min_capacity the minimum required capacity of the tmp array
 * @return tmp, whether or not it grew
 */
static gpointer
gtk_tim_sort_ensure_capacity (GtkTimSort *self,
                              gsize       min_capacity)
{
  if (self->tmp_length < min_capacity)
    {
      /* Compute smallest power of 2 > min_capacity */
      gsize new_size = min_capacity;
      new_size |= new_size >> 1;
      new_size |= new_size >> 2;
      new_size |= new_size >> 4;
      new_size |= new_size >> 8;
      new_size |= new_size >> 16;
      if (sizeof(new_size) > 4)
        new_size |= new_size >> 32;

      new_size++;
      if (new_size == 0) /* (overflow) Not bloody likely! */
        new_size = min_capacity;

      g_free (self->tmp);
      self->tmp_length = new_size;
      self->tmp = g_malloc (self->tmp_length * self->element_size);
  }

  return self->tmp;
}

void
gtk_tim_sort_set_already_sorted (GtkTimSort *self,
                                 gsize       already_sorted)
{
  g_assert (self);
  g_assert (self->pending_runs == 0);
  g_assert (already_sorted <= self->size);

  if (already_sorted > 1)
    gtk_tim_sort_push_run (self, self->base, already_sorted);
}

#if 1
#define WIDTH 4
#include "gtktimsort-impl.c"

#define WIDTH 8
#include "gtktimsort-impl.c"

#define WIDTH 16
#include "gtktimsort-impl.c"
#endif

#define NAME default
#define WIDTH (self->element_size)
#include "gtktimsort-impl.c"

gboolean
gtk_tim_sort_step (GtkTimSort *self)
{
  g_assert (self);

  switch (self->element_size)
  {
#if 1
    case 4:
      return gtk_tim_sort_step_4 (self);
    case 8:
      return gtk_tim_sort_step_8 (self);
    case 16:
      return gtk_tim_sort_step_16 (self);
#endif
    default:
      return gtk_tim_sort_step_default(self);
  }
}

