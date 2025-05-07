#pragma once

#include "gdktypes.h"

G_BEGIN_DECLS

/* Number of damage items we track.
   This should ideally be identical to the number of images we put into swapchains */

#define GDK_N_DAMAGE_TRACKED 4

typedef struct _GdkDamageTracker GdkDamageTracker;

struct _GdkDamageTracker
{
  GDestroyNotify free_func;

  struct {
    gpointer item;
    cairo_region_t *damage_to_next;
  } items[GDK_N_DAMAGE_TRACKED];
};

void                    gdk_damage_tracker_init                         (GdkDamageTracker       *self,
                                                                         GDestroyNotify          free_func);
void                    gdk_damage_tracker_finish                       (GdkDamageTracker       *self);

void                    gdk_damage_tracker_reset                        (GdkDamageTracker       *self);

gboolean                gdk_damage_tracker_add                          (GdkDamageTracker       *self,
                                                                         gpointer                item,
                                                                         cairo_region_t         *damage,
                                                                         cairo_region_t         *out_redraw) G_GNUC_WARN_UNUSED_RESULT;



G_END_DECLS

