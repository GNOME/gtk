#include "config.h"

#include "gsklayer.h"

#include <gdk/gdk.h>

#include "gskenumtypes.h"
#include "gskdebugprivate.h"

typedef struct {
  GskLayer *parent;

  GskLayer *first_child;
  GskLayer *last_child;
  GskLayer *prev_sibling;
  GskLayer *next_sibling;

  int n_children;

  gint64 age;

  graphene_rect_t bounds;
  graphene_point_t anchor;

  graphene_matrix_t transform;
  graphene_matrix_t child_transform;

  gboolean transform_set : 1;
  gboolean child_transform_set : 1;
  gboolean is_hidden : 1;
  gboolean is_opaque : 1;
} GskLayerPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GskLayer, gsk_layer, G_TYPE_INITIALLY_UNOWNED)

static void
gsk_layer_class_init (GskLayerClass *klass)
{
}

static void
gsk_layer_init (GskLayer *self)
{
}

/**
 * gsk_layer_new:
 *
 * Creates a new #GskLayer.
 *
 * Returns: (transfer full): the newly created #GskLayer
 *
 * Since: 3.22
 */
GskLayer *
gsk_layer_new (void)
{
  return g_object_new (GSK_TYPE_LAYER, NULL);
}
