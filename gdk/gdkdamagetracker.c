#include "config.h"

#include "gdkdamagetrackerprivate.h"

/*<private>
 * gdk_damage_tracker_init:
 * @self: the tracker to init
 * @free_func: (nullable): the function to use to free items
 * 
 * Initializes a damage tracker
 */
void
gdk_damage_tracker_init (GdkDamageTracker *self,
                         GDestroyNotify    free_func)
{
  memset (self, 0, sizeof (GdkDamageTracker));

  self->free_func = free_func;
}

static void
gdk_damage_tracker_free_item (GdkDamageTracker *self,
                              gpointer          item)
{
  if (item == NULL)
    return;

  if (self->free_func)
    self->free_func (item);
}

/*<private>
 * gdk_damage_tracker_reset:
 * @self: the tracker to reset
 * 
 * Clears all tracked items in the damage tracker.
 * 
 * This function should be called when the swapchain
 * tracked via the tracker gets resized.
 */
void
gdk_damage_tracker_reset (GdkDamageTracker *self)
{
  gsize i;

  for (i = 0; i < G_N_ELEMENTS (self->items); i++)
    {
      gdk_damage_tracker_free_item (self, self->items[i].item);
      self->items[i].item = NULL;
      g_clear_pointer (&self->items[i].damage_to_next, cairo_region_destroy);
    }
}

/*<private>
 * gdk_damage_tracker_finish:
 * @self: the tracker to finish
 * 
 * Clears the tracker and releases all memory held.
 */
void
gdk_damage_tracker_finish (GdkDamageTracker *self)
{
  gdk_damage_tracker_reset (self);
}

/*<private>
 * gdk_damage_tracker_add:
 * @self: the tracker to add the item to
 * @item: (transfer full): the item to add. Note that if this
 *   item has already been added, this function will take a second
 *   reference to that item
 * @damage: damage since last item
 * @out_redraw: (out) (caller-allocates): region that
 *   the total damage that needs to be redrawn will be added
 *   to on success.
 * 
 * Adds an item to the list of tracked items and the damage
 * since the last item was recorded.
 * 
 * `damage` and `out_redraw` can be the same region.
 * 
 * And yes, I'm sorry this function has obscure arguments, but I
 * promise it's really convenient to call from inside draw contexts.
 * 
 * Returns: TRUE if item was found in the tracker and
 *   @out_redraw was set, FALSE otherwise and the whole
 *   item should be considered damaged.
 */
gboolean
gdk_damage_tracker_add (GdkDamageTracker *self,
                        gpointer          item,
                        cairo_region_t   *damage,
                        cairo_region_t   *out_redraw)
{
  gsize i;
  cairo_region_t *damage_copy;
  gboolean result = TRUE;

  for (i = 0; i < G_N_ELEMENTS (self->items); i++)
    {
      if (self->items[i].item == item ||
          self->items[i].item == NULL)
        break;
    }

  damage_copy = cairo_region_copy (damage);

  if (i == G_N_ELEMENTS (self->items))
    {
      result = FALSE;

      i--;
    }
  else
    {
      if (self->items[i].damage_to_next != NULL && i > 0)
        {
          cairo_region_union (self->items[i - 1].damage_to_next,
                              self->items[i].damage_to_next);
        }
      if (self->items[i].item != item)
        result = FALSE;
  }

  gdk_damage_tracker_free_item (self, self->items[i].item);
  g_clear_pointer (&self->items[i].damage_to_next, cairo_region_destroy);

  for (; i > 0; i--)
    {
      self->items[i].item = self->items[i - 1].item;
      self->items[i].damage_to_next = self->items[i - 1].damage_to_next;
      if (result)
        cairo_region_union (out_redraw, self->items[i].damage_to_next);
    }

  self->items[0].item = item;
  self->items[0].damage_to_next = damage_copy;  
  if (result && damage != out_redraw)
    cairo_region_union (out_redraw, self->items[0].damage_to_next);

  {
    cairo_rectangle_int_t ext;
    cairo_region_get_extents (out_redraw, &ext);
  }

  return result;
}
