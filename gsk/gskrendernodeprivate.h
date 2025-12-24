#pragma once

#include "gskrendernode.h"
#include <cairo.h>

#include "gdk/gdkmemoryformatprivate.h"
#include "gdk/gdkcolorprivate.h"
#include "gskgradientprivate.h"

G_BEGIN_DECLS

typedef enum {
  /* No pixels are copied */
  GSK_COPY_NONE = 0,
  /* Pixels that are copied will be pasted to the same pixel */
  GSK_COPY_SAME_PIXEL,
  /* Pixels are copied and and will be pasted whereever */
  GSK_COPY_ANY
} GskCopyMode;

#define GSK_COPY_MODE_BITS 2

typedef struct _GskRenderNodeClass GskRenderNodeClass;

#define GSK_IS_RENDER_NODE_TYPE(node,type) \
  (G_TYPE_INSTANCE_GET_CLASS ((node), GSK_TYPE_RENDER_NODE, GskRenderNodeClass)->node_type == (type))

#define GSK_RENDER_NODE_TYPE(node) \
  (G_TYPE_INSTANCE_GET_CLASS ((node), GSK_TYPE_RENDER_NODE, GskRenderNodeClass)->node_type)

struct _GskRenderNode
{
  GTypeInstance parent_instance;

  gatomicrefcount ref_count;

  graphene_rect_t bounds;

  guint preferred_depth : GDK_MEMORY_DEPTH_BITS;
  guint copy_mode : GSK_COPY_MODE_BITS;
  guint fully_opaque : 1;
  guint is_hdr : 1;
  guint clears_background : 1; /* mostly relevant for tracking opacity */
  guint contains_subsurface_node : 1; /* contains a subsurface node */
  guint contains_paste_node : 1; /* contains a paste node that has no matching copy node */
};

typedef struct
{
  cairo_region_t *region;
  GSList *copies;
  GdkSurface *surface;
} GskDiffData;

typedef struct
{
  graphene_rect_t opaque;
  GSList *copies;
} GskOpacityData;

#define GSK_OPACITY_DATA_INIT_EMPTY(copies) { GRAPHENE_RECT_INIT(0, 0, 0, 0), (copies) }
#define GSK_OPACITY_DATA_INIT_COPY(data) (*(data))

typedef struct
{
  GdkColorState *ccs;
} GskCairoData;

struct _GskRenderNodeClass
{
  GTypeClass parent_class;

  GskRenderNodeType node_type;

  void          (* finalize)                            (GskRenderNode               *node);
  void          (* draw)                                (GskRenderNode               *node,
                                                         cairo_t                     *cr,
                                                         GskCairoData                *data);
  gboolean      (* can_diff)                            (const GskRenderNode         *node1,
                                                         const GskRenderNode         *node2);
  void          (* diff)                                (GskRenderNode               *node1,
                                                         GskRenderNode               *node2,
                                                         GskDiffData                 *data);
  GskRenderNode**(* get_children)                       (GskRenderNode               *node,
                                                         gsize                       *n_children);
  GskRenderNode*(* replay)                              (GskRenderNode               *node,
                                                         GskRenderReplay             *replay);
  void          (* render_opacity)                      (GskRenderNode               *node,
                                                         GskOpacityData              *data);
};

void            gsk_render_node_init_types              (void);

#define GSK_DEFINE_RENDER_NODE_TYPE(TypeName, type_name) \
GType \
type_name##_get_type (void) \
{ \
  static _g_type_once_init_type static_g_define_type_id = 0; \
  if (_g_type_once_init_enter (&static_g_define_type_id)) \
    { \
      GType g_define_type_id = gsk_render_node_type_register_static (g_intern_static_string (#TypeName), \
                                                                     sizeof (TypeName), \
                                                                     type_name ## _class_init); \
      _g_type_once_init_leave (&static_g_define_type_id, g_define_type_id); \
    } \
  return static_g_define_type_id; \
}

GType           gsk_render_node_type_register_static    (const char                  *node_name,
                                                         gsize                        instance_size,
                                                         GClassInitFunc               class_init);

gpointer        gsk_render_node_alloc                   (GType                        node_type);

void            _gsk_render_node_unref                  (GskRenderNode               *node);

gboolean        gsk_render_node_can_diff                (const GskRenderNode         *node1,
                                                         const GskRenderNode         *node2) G_GNUC_PURE;
void            gsk_render_node_diff                    (GskRenderNode               *node1,
                                                         GskRenderNode               *node2,
                                                         GskDiffData                 *data);
void            gsk_render_node_diff_impossible         (GskRenderNode               *node1,
                                                         GskRenderNode               *node2,
                                                         GskDiffData                 *data);
void            gsk_render_node_draw_full               (GskRenderNode               *node,
                                                         cairo_t                     *cr,
                                                         GskCairoData                *data);
void            gsk_render_node_draw_with_color_state   (GskRenderNode               *node,
                                                         cairo_t                     *cr,
                                                         GdkColorState               *color_state);
void            gsk_render_node_draw_fallback           (GskRenderNode               *node,
                                                         cairo_t                     *cr);
void            gsk_render_node_render_opacity          (GskRenderNode               *self,
                                                         GskOpacityData              *data);

GskRenderNode **gsk_render_node_get_children            (GskRenderNode               *node,
                                                         gsize                       *n_children);

bool            gsk_border_node_get_uniform             (const GskRenderNode         *self) G_GNUC_PURE;
bool            gsk_border_node_get_uniform_color       (const GskRenderNode         *self) G_GNUC_PURE;

GdkMemoryDepth  gsk_render_node_get_preferred_depth     (const GskRenderNode         *node) G_GNUC_PURE;
gboolean        gsk_render_node_is_hdr                  (const GskRenderNode         *node) G_GNUC_PURE;
gboolean        gsk_render_node_is_fully_opaque         (const GskRenderNode         *node) G_GNUC_PURE;
gboolean        gsk_render_node_clears_background       (const GskRenderNode         *node) G_GNUC_PURE;
GskCopyMode     gsk_render_node_get_copy_mode           (const GskRenderNode         *node) G_GNUC_PURE;
gboolean        gsk_render_node_contains_subsurface_node(const GskRenderNode         *node) G_GNUC_PURE;
gboolean        gsk_render_node_contains_paste_node     (const GskRenderNode         *node) G_GNUC_PURE;

#define gsk_render_node_ref(node)   _gsk_render_node_ref(node)
#define gsk_render_node_unref(node) _gsk_render_node_unref(node)

static inline GskRenderNode *
_gsk_render_node_ref (GskRenderNode *node)
{
  g_atomic_ref_count_inc (&node->ref_count);
  return node;
}

GskRenderNode *         gsk_border_node_new2                    (const GskRoundedRect   *outline,
                                                                 const float             border_width[4],
                                                                 const GdkColor          border_color[4]);
const GdkColor *        gsk_border_node_get_gdk_colors          (const GskRenderNode    *node);

GskRenderNode *         gsk_inset_shadow_node_new2              (const GskRoundedRect   *outline,
                                                                 const GdkColor         *color,
                                                                 const graphene_point_t *offset,
                                                                 float                   spread,
                                                                 float                   blur_radius);
const GdkColor *        gsk_inset_shadow_node_get_gdk_color     (const GskRenderNode    *node);
const graphene_point_t *gsk_inset_shadow_node_get_offset        (const GskRenderNode    *node);

GskRenderNode *         gsk_outset_shadow_node_new2             (const GskRoundedRect   *outline,
                                                                 const GdkColor         *color,
                                                                 const graphene_point_t *offset,
                                                                 float                   spread,
                                                                 float                   blur_radius);
const GdkColor *        gsk_outset_shadow_node_get_gdk_color    (const GskRenderNode    *node);
const graphene_point_t *gsk_outset_shadow_node_get_offset       (const GskRenderNode    *node);

typedef struct _GskShadowEntry GskShadowEntry;
struct _GskShadowEntry
{
  GdkColor color;
  graphene_point_t offset;
  float radius;
};

GskRenderNode * gsk_shadow_node_new2                    (GskRenderNode        *child,
                                                         const GskShadowEntry *shadows,
                                                         gsize                 n_shadows);

const GskShadowEntry *gsk_shadow_node_get_shadow_entry  (const GskRenderNode  *node,
                                                         gsize                 i);

GskRenderNode * gsk_text_node_new2                      (PangoFont              *font,
                                                         PangoGlyphString       *glyphs,
                                                         const GdkColor         *color,
                                                         const graphene_point_t *offset);
const GdkColor *gsk_text_node_get_gdk_color             (const GskRenderNode    *node);


#define GSK_RENDER_NODE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GSK_TYPE_RENDER_NODE, GskRenderNodeClass))

#define gsk_render_node_get_node_type(node) _gsk_render_node_get_node_type (node)

G_GNUC_PURE static inline
GskRenderNodeType
_gsk_render_node_get_node_type (const GskRenderNode *node)
{
  return GSK_RENDER_NODE_GET_CLASS (node)->node_type;
}

GskRenderNode * gsk_linear_gradient_node_new2           (const graphene_rect_t   *bounds,
                                                         const graphene_point_t  *start,
                                                         const graphene_point_t  *end,
                                                         const GskGradient       *gradient);

GskRenderNode * gsk_radial_gradient_node_new2           (const graphene_rect_t   *bounds,
                                                         const graphene_point_t  *start_center,
                                                         float                    start_radius,
                                                         const graphene_point_t  *end_center,
                                                         float                    end_radius,
                                                         float                    aspect_ratio,
                                                         const GskGradient       *gradient);

GskRenderNode * gsk_conic_gradient_node_new2            (const graphene_rect_t   *bounds,
                                                         const graphene_point_t  *center,
                                                         float                    rotation,
                                                         const GskGradient       *gradient);

const GskGradient * gsk_gradient_node_get_gradient      (const GskRenderNode *node);

const graphene_point_t *gsk_radial_gradient_node_get_start_center     (const GskRenderNode *node) G_GNUC_PURE;
const graphene_point_t *gsk_radial_gradient_node_get_end_center       (const GskRenderNode *node) G_GNUC_PURE;
float                   gsk_radial_gradient_node_get_start_radius     (const GskRenderNode *node) G_GNUC_PURE;
float                   gsk_radial_gradient_node_get_end_radius       (const GskRenderNode *node) G_GNUC_PURE;
float                   gsk_radial_gradient_node_get_aspect_ratio     (const GskRenderNode *node) G_GNUC_PURE;

gboolean gsk_radial_gradient_fills_plane (const graphene_point_t *c1,
                                          float                   r1,
                                          const graphene_point_t *c2,
                                          float                   r2);

G_END_DECLS
