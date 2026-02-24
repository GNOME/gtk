#include "config.h"

#include "gskatlasallocatorprivate.h"

typedef struct _GskAtlasSlot GskAtlasSlot;

typedef enum {
  /* memory not in use */
  GSK_ATLAS_FREE,
  /* memory not in use but still enqueued in emptylist */
  GSK_ATLAS_FREELIST,
  /* area free for use, enqueued in emptylist */
  GSK_ATLAS_EMPTY,
  /* manages a bunch of children */
  GSK_ATLAS_CONTAINER,
  /* alive and used by a cached item */
  GSK_ATLAS_USED,
} GskAtlasSlotType;

/**
 * lifetime of a slot:
 *
 *                     new slot added to array
 *                                ║
 *                                ║
 *         ╔═══════════════════  FREE
 *         ║                      ║
 *    when removed         used to represent
 *   from epmptylist       an area after split
 *         ║                      ║
 *      FREELIST                  ╠═══════════════════════════════════════════╦════════════╗
 *         ║                      ║                                    (plus a new slot)   ║
 *         ║                      ║                                           ║            ║
 *       merged  ══════════════ EMPTY ═══════════════ split ════════ in same direction     ║
 *                                ║                     ║              as siblings         ║
 *                                ║             in opposite direction                      ║
 *                            allocated                 ║                                  ║
 *                                ║                 CONTAINER                              ║
 *                                ║              (with 2 children)                         ║
 *                               USED                   ║                                  ║
 *                                ║                     ║                                  ║
 *                                ║          once all children are merged                  ║
 *                           deallocated                ║                                  ║
 *                                ╚═════════════════════╩══════════════════════════════════╝
 *
 */
struct _GskAtlasSlot
{
  GskAtlasSlotType type;
  gsize prev; /* or parent if first item */
  gsize next;
  union {
    /* FREE and EMPTY */
    gsize emptylist_next;
    /* USED */
    gpointer user_data;
    /* CONTAINER */
    gsize first_child;
  };
  cairo_rectangle_int_t area;
};

#define GDK_ARRAY_NAME gsk_atlas_slots
#define GDK_ARRAY_TYPE_NAME GskAtlasSlots
#define GDK_ARRAY_ELEMENT_TYPE GskAtlasSlot
#define GDK_ARRAY_BY_VALUE 1
#define GDK_ARRAY_PREALLOC 32
#include "gdk/gdkarrayimpl.c"

#define N_EMPTYLISTS 8

struct _GskAtlasAllocator
{
  GskAtlasSlots slots;
  gsize root;
  gsize first_free_slot;
  gsize emptylists[N_EMPTYLISTS];
};

void
gsk_atlas_allocator_free (GskAtlasAllocator *self)
{
  gsk_atlas_slots_clear (&self->slots);

  g_free (self);
}

static gboolean
is_first_child (GskAtlasAllocator *self,
                gsize              pos)
{
  GskAtlasSlot *slot, *prev;

  slot = gsk_atlas_slots_get (&self->slots, pos);
  if (slot->prev == G_MAXSIZE)
    return TRUE;

  prev = gsk_atlas_slots_get (&self->slots, slot->prev);
  if (prev->type != GSK_ATLAS_CONTAINER)
    return FALSE;

  return prev->first_child == pos;
}

static void
gsk_atlas_allocator_dump (GskAtlasAllocator *self,
                          const char        *format,
                          ...) G_GNUC_PRINTF (2, 3);
#if 0
static const char *type_names[] = {
  "FREE",
  "FREELIST",
  "EMPTY",
  "CONTAINER",
  "USED",
};

static void
gsk_atlas_allocator_dump_depth (GskAtlasAllocator *self,
                                gsize              pos,
                                gsize              depth)
{
  gsize prev;
  GskAtlasSlot *slot;
  cairo_region_t *region;

  slot = gsk_atlas_slots_get (&self->slots, pos);
  if (slot->prev == G_MAXSIZE)
    {
      g_assert (depth == 0);
      region = cairo_region_create_rectangle (&slot->area);
    }
  else
    {
      GskAtlasSlot *prev_slot = gsk_atlas_slots_get (&self->slots, slot->prev);
      g_assert (is_first_child (self, pos));
      region = cairo_region_create_rectangle (&prev_slot->area);
    }
  prev = slot->prev;

  while (pos != G_MAXSIZE)
    {
      slot = gsk_atlas_slots_get (&self->slots, pos);
      g_assert_cmpuint (slot->prev, ==, prev);
      g_assert (slot->type != GSK_ATLAS_FREE && slot->type != GSK_ATLAS_FREELIST);
      g_assert_cmpint (cairo_region_intersect_rectangle (region, &slot->area), ==, CAIRO_REGION_OVERLAP_IN);
      cairo_region_subtract_rectangle (region, &slot->area);
      g_assert_cmpint (cairo_region_num_rectangles (region), <=, 1);
      g_print ("%*s%s (%zu) %d %d %d %d\n",
               2 * (int) depth, "",
               type_names[slot->type],
               pos,
               slot->area.x, slot->area.y,
               slot->area.width, slot->area.height);
      if (slot->type == GSK_ATLAS_CONTAINER)
        gsk_atlas_allocator_dump_depth (self, slot->first_child, depth + 1);

      prev = pos;
      pos = slot->next;
    }

  g_assert_true (cairo_region_is_empty (region));
  cairo_region_destroy (region);
}

static void
gsk_atlas_allocator_dump (GskAtlasAllocator *self,
                          const char        *format,
                          ...)
{
  gsize i, j;
  va_list args;
  char *s;

  va_start (args, format);
  s = g_strdup_vprintf (format, args);
  g_print ("%s\n", s);
  va_end (args);

  gsk_atlas_allocator_dump_depth (self, self->root, 0);

  for (i = 0; i < G_N_ELEMENTS (self->emptylists); i++)
    {
      g_print ("emptylist %zu: ", i);
      for (j = self->emptylists[i]; j != G_MAXSIZE; )
        {
          GskAtlasSlot *slot;
      
          slot = gsk_atlas_slots_get (&self->slots, j);
          g_assert (slot->type == GSK_ATLAS_EMPTY || slot->type == GSK_ATLAS_FREELIST);
          if (j != self->emptylists[i])
            g_print (", ");
          g_print ("%zu%s", j, slot->type == GSK_ATLAS_FREELIST ? "*" : "");
          j = slot->emptylist_next;
        }
      g_print ("\n");
    }
}
#else
static void
gsk_atlas_allocator_dump (GskAtlasAllocator *self,
                          const char        *format,
                          ...)
{
}
#endif

static gsize
get_emptylist_for_size (gsize width,
                        gsize height)
{
  return MIN (g_bit_storage (MIN (width, height)), N_EMPTYLISTS) - 1;
}

GskAtlasAllocator *
gsk_atlas_allocator_new (gsize width,
                         gsize height)
{
  GskAtlasAllocator *self;
  gsize i;

  self = g_new0 (GskAtlasAllocator, 1);
  gsk_atlas_slots_init (&self->slots);

  gsk_atlas_slots_append (&self->slots,
                          &(GskAtlasSlot) {
                              .type = GSK_ATLAS_EMPTY,
                              .prev = G_MAXSIZE,
                              .next = G_MAXSIZE,
                              .emptylist_next = G_MAXSIZE,
                              .area = (cairo_rectangle_int_t) { 0, 0, width, height }
                          });
  self->root = 0;
  for (i = 0; i < G_N_ELEMENTS (self->emptylists); i++)
    self->emptylists[i] = G_MAXSIZE;
  self->emptylists[get_emptylist_for_size (width, height)] = 0;

  return self;
}

static void
gsk_atlas_allocator_free_slot (GskAtlasAllocator *self,
                               gsize              pos)
{
  GskAtlasSlot *slot;
  
  slot = gsk_atlas_slots_get (&self->slots, pos);
  slot->type = GSK_ATLAS_FREE;
  if (pos < self->first_free_slot)
    self->first_free_slot = pos;
}

#if 0
/* This rounds up to the next number that has <= 2 bits set:
 * 1, 2, 3, 4, 6, 8, 12, 16, 24, 32, 48, 64, 96, 128, ...
 * That is roughly sqrt(2), so it should limit waste
 */
static gsize 
round_up_atlas_size (gsize num)
{
  gsize storage = g_bit_storage (num);

  num = num + (((1 << storage) - 1) >> 2);
  num &= (((gsize) 7) << storage) >> 2;

  return num;
}
#endif

static gsize
gsk_atlas_allocator_allocate_pos (GskAtlasAllocator *self,
                                  gsize              width,
                                  gsize              height)
{
  GskAtlasSlot *prev, *best_prev, *best_slot, *slot;
  gsize list, pos, best_list, best_pos, best_size;

  best_prev = NULL;
  best_slot = NULL;
  best_pos = G_MAXSIZE;
  best_size = G_MAXSIZE;
  best_list = G_MAXSIZE;

  for (list = get_emptylist_for_size (width, height); list < N_EMPTYLISTS; list++)
    {
      prev = NULL;

      for (pos = self->emptylists[list]; pos < gsk_atlas_slots_get_size (&self->slots); /* done in loop */)
        {
          slot = gsk_atlas_slots_get (&self->slots, pos);
          if (slot->type == GSK_ATLAS_FREELIST)
            {
              gsize next_pos = slot->emptylist_next;
              if (prev)
                prev->emptylist_next = slot->emptylist_next;
              else
                self->emptylists[list] = slot->emptylist_next;
              gsk_atlas_allocator_free_slot (self, pos);
              pos = next_pos;
              continue;
            }

          if (slot->area.width >= width &&
              slot->area.height >= height &&
              slot->area.width * slot->area.height < best_size)
            {
              best_prev = prev;
              best_list = list;
              best_slot = slot;
              best_pos = pos;
              best_size = slot->area.width * slot->area.height;
            }
          prev = slot;
          pos = slot->emptylist_next;
        }
    }

  if (best_pos >= gsk_atlas_slots_get_size (&self->slots))
    return G_MAXSIZE;

  if (best_prev)
    {
      best_prev->emptylist_next = best_slot->emptylist_next;
      best_slot->emptylist_next = G_MAXSIZE;
    }
  else
    self->emptylists[best_list] = best_slot->emptylist_next;

  return best_pos;
}

static void
gsk_atlas_allocator_enqueue_empty (GskAtlasAllocator *self,
                                   gsize              pos)
{
  GskAtlasSlot *slot;
  gsize list;

  slot = gsk_atlas_slots_get (&self->slots, pos);

  g_assert (slot->type == GSK_ATLAS_EMPTY);

  list = get_emptylist_for_size (slot->area.width, slot->area.height);
  slot->emptylist_next = self->emptylists[list];
  self->emptylists[list] = pos;
}

static void
rectangle_union (const cairo_rectangle_int_t *src1,
		 const cairo_rectangle_int_t *src2,
		 cairo_rectangle_int_t       *dest)
{
  int dest_x, dest_y;
  
  g_return_if_fail (src1 != NULL);
  g_return_if_fail (src2 != NULL);
  g_return_if_fail (dest != NULL);

  dest_x = MIN (src1->x, src2->x);
  dest_y = MIN (src1->y, src2->y);
  dest->width = MAX (src1->x + src1->width, src2->x + src2->width) - dest_x;
  dest->height = MAX (src1->y + src1->height, src2->y + src2->height) - dest_y;
  dest->x = dest_x;
  dest->y = dest_y;
}

static void
gsk_atlas_allocator_make_empty (GskAtlasAllocator *self,
                                gsize              pos)
{
  GskAtlasSlot *slot;
  gsize tmp;

  slot = gsk_atlas_slots_get (&self->slots, pos);

  if (!is_first_child (self, pos))
    {
      GskAtlasSlot *prev = gsk_atlas_slots_get (&self->slots, slot->prev);
      if (prev->type == GSK_ATLAS_EMPTY)
        {
          rectangle_union (&slot->area, &prev->area, &slot->area);
          if (is_first_child (self, slot->prev))
            {
              if (prev->prev != G_MAXSIZE)
                {
                  GskAtlasSlot *parent = gsk_atlas_slots_get (&self->slots, prev->prev);
                  parent->first_child = pos;
                }
              else
                {
                  self->root = pos;
                }
            }
          else
            {
              GskAtlasSlot *prev_prev = gsk_atlas_slots_get (&self->slots, prev->prev);
              prev_prev->next = pos;
            }
          slot->prev = prev->prev;
          prev->type = GSK_ATLAS_FREELIST;
        }
    }
  if (slot->next != G_MAXSIZE)
    {
      GskAtlasSlot *next = gsk_atlas_slots_get (&self->slots, slot->next);
      if (next->type == GSK_ATLAS_EMPTY)
        {
          rectangle_union (&slot->area, &next->area, &slot->area);
          if (next->next != G_MAXSIZE)
            {
              GskAtlasSlot *next_next = gsk_atlas_slots_get (&self->slots, next->next);
              next_next->prev = pos;
            }
          slot->next = next->next;
          next->type = GSK_ATLAS_FREELIST;
        }
    }
  if (is_first_child (self, pos) && slot->prev != G_MAXSIZE && slot->next == G_MAXSIZE)
    {
      tmp = slot->prev;
      /* only item in this level, merge with parent */
      gsk_atlas_allocator_free_slot (self, pos);
      gsk_atlas_allocator_make_empty (self, tmp);
    }
  else
    {
      slot->type = GSK_ATLAS_EMPTY;
      gsk_atlas_allocator_enqueue_empty (self, pos);
    }
}

void
gsk_atlas_allocator_deallocate (GskAtlasAllocator *self,
                                gsize              pos)
{
#ifndef G_DISABLE_ASSERT
  GskAtlasSlot *slot;

  slot = gsk_atlas_slots_get (&self->slots, pos);
  g_assert (slot->type == GSK_ATLAS_USED);
#endif

  gsk_atlas_allocator_make_empty (self, pos);

  gsk_atlas_allocator_dump (self, "DEALLOCATION:");
}

static gsize
gsk_atlas_allocator_allocate_slot (GskAtlasAllocator *self)
{
  gsize pos;

  for (pos = self->first_free_slot; pos < gsk_atlas_slots_get_size (&self->slots); pos++)
    {
      GskAtlasSlot *slot;

      slot = gsk_atlas_slots_get (&self->slots, pos);
      if (slot->type == GSK_ATLAS_FREE)
        {
          self->first_free_slot = pos + 1;
          return pos;
        }
    }
  gsk_atlas_slots_set_size (&self->slots, pos + 1);
  self->first_free_slot = pos + 1;

  return pos;
}

static gsize
gsk_atlas_allocator_resize_slot (GskAtlasAllocator *self,
                                 gsize              pos,
                                 gboolean           horizontal,
                                 gboolean           opposite,
                                 gsize              size)
{
  GskAtlasSlot *slot, *split;
  gsize split_pos;

  slot = gsk_atlas_slots_get (&self->slots, pos);
  if ((horizontal && slot->area.width <= size) ||
      (!horizontal && slot->area.height <= size))
    {
      if (horizontal)
        {
          g_assert (slot->area.width == size);
        }
      else
        {
          g_assert (slot->area.height == size);
        }
      return pos;
    }

  if (opposite)
    {
      GskAtlasSlot *child;
      gsize child_pos;

      child_pos = gsk_atlas_allocator_allocate_slot (self);
      slot = gsk_atlas_slots_get (&self->slots, pos);
      child = gsk_atlas_slots_get (&self->slots, child_pos);
      child->type = GSK_ATLAS_EMPTY;
      child->prev = pos;
      child->next = G_MAXSIZE;
      child->area = slot->area;

      slot->type = GSK_ATLAS_CONTAINER;
      slot->first_child = child_pos;

      slot = child;
      pos = child_pos;
    }

  split_pos = gsk_atlas_allocator_allocate_slot (self);
  slot = gsk_atlas_slots_get (&self->slots, pos);
  split = gsk_atlas_slots_get (&self->slots, split_pos);
  *split = *slot;
  split->prev = pos;
  if (slot->next != G_MAXSIZE)
    {
      GskAtlasSlot *next = gsk_atlas_slots_get (&self->slots, slot->next);
      next->prev = split_pos;
    }
  slot->next = split_pos;
  if (horizontal)
    {
      slot->area.width = size;
      split->area.x += size;
      split->area.width -= size;
    }
  else
    {
      slot->area.height = size;
      split->area.y += size;
      split->area.height -= size;
    }

  gsk_atlas_allocator_enqueue_empty (self, split_pos);

  return pos;
}

gsize
gsk_atlas_allocator_allocate (GskAtlasAllocator *self,
                              gsize              width,
                              gsize              height)
{
  GskAtlasSlot *slot, *other;
  gsize pos;

  pos = gsk_atlas_allocator_allocate_pos (self, width, height);
  if (pos == G_MAXSIZE)
    return G_MAXSIZE;

  slot = gsk_atlas_slots_get (&self->slots, pos);
  if (!is_first_child (self, pos))
    other = gsk_atlas_slots_get (&self->slots, slot->prev);
  else if (slot->next < G_MAXSIZE)
    other = gsk_atlas_slots_get (&self->slots, slot->next);
  else
    other = NULL;

  if (other == NULL || other->area.y == slot->area.y)
    {
      pos = gsk_atlas_allocator_resize_slot (self, pos, TRUE, FALSE, width);
      pos = gsk_atlas_allocator_resize_slot (self, pos, FALSE, TRUE, height);
    }
  else
    {
      pos = gsk_atlas_allocator_resize_slot (self, pos, FALSE, FALSE, height);
      pos = gsk_atlas_allocator_resize_slot (self, pos, TRUE, TRUE, width);
    }

  slot = gsk_atlas_slots_get (&self->slots, pos);
  slot->type = GSK_ATLAS_USED;
  slot->user_data = NULL;

  gsk_atlas_allocator_dump (self, "ALLOCATION %zux%zu:", width, height);

  return pos;
}

const cairo_rectangle_int_t *
gsk_atlas_allocator_get_area (GskAtlasAllocator *self,
                              gsize              pos)
{
  GskAtlasSlot *slot;

  g_assert (pos < gsk_atlas_slots_get_size (&self->slots));
  slot = gsk_atlas_slots_get (&self->slots, pos);
  g_assert (slot->type == GSK_ATLAS_USED);

  return &slot->area;
}

void
gsk_atlas_allocator_set_user_data (GskAtlasAllocator *self,
                                   gsize              pos,
                                   gpointer           user_data)
{
  GskAtlasSlot *slot;

  g_assert (pos < gsk_atlas_slots_get_size (&self->slots));
  slot = gsk_atlas_slots_get (&self->slots, pos);
  g_assert (slot->type == GSK_ATLAS_USED);

  slot->user_data = user_data;
}

gpointer
gsk_atlas_allocator_get_user_data (GskAtlasAllocator *self,
                                   gsize              pos)
{
  GskAtlasSlot *slot;

  g_assert (pos < gsk_atlas_slots_get_size (&self->slots));
  slot = gsk_atlas_slots_get (&self->slots, pos);
  g_assert (slot->type == GSK_ATLAS_USED);

  return slot->user_data;
}

