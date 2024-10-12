#pragma once

#include "gskrendernode.h"
#include <cairo.h>

#include "gdk/gdkmemoryformatprivate.h"
#include "gdk/gdkcolorprivate.h"

G_BEGIN_DECLS

typedef struct _GskRenderNodeClass GskRenderNodeClass;

/* Keep this in sync with the GskRenderNodeType enumeration.
 *
 * We don't add an "n-types" value to avoid having to handle
 * it in every single switch.
 */
#define GSK_RENDER_NODE_TYPE_N_TYPES    (GSK_SUBSURFACE_NODE + 1)

extern GType gsk_render_node_types[];

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
  guint offscreen_for_opacity : 1;
  guint fully_opaque : 1;
  guint is_hdr : 1;
};

typedef struct
{
  cairo_region_t *region;
  GdkSurface *surface;
} GskDiffData;

struct _GskRenderNodeClass
{
  GTypeClass parent_class;

  GskRenderNodeType node_type;

  void          (* finalize)                            (GskRenderNode               *node);
  void          (* draw)                                (GskRenderNode               *node,
                                                         cairo_t                     *cr,
                                                         GdkColorState               *ccs);
  gboolean      (* can_diff)                            (const GskRenderNode         *node1,
                                                         const GskRenderNode         *node2);
  void          (* diff)                                (GskRenderNode               *node1,
                                                         GskRenderNode               *node2,
                                                         GskDiffData                 *data);
  gboolean      (* get_opaque_rect)                     (GskRenderNode               *node,
                                                         graphene_rect_t             *out_opaque);
};

void            gsk_render_node_init_types              (void);

GType           gsk_render_node_type_register_static    (const char                  *node_name,
                                                         gsize                        instance_size,
                                                         GClassInitFunc               class_init);

gpointer        gsk_render_node_alloc                   (GskRenderNodeType            node_type);

void            _gsk_render_node_unref                  (GskRenderNode               *node);

gboolean        gsk_render_node_can_diff                (const GskRenderNode         *node1,
                                                         const GskRenderNode         *node2) G_GNUC_PURE;
void            gsk_render_node_diff                    (GskRenderNode               *node1,
                                                         GskRenderNode               *node2,
                                                         GskDiffData                 *data);
void            gsk_render_node_diff_impossible         (GskRenderNode               *node1,
                                                         GskRenderNode               *node2,
                                                         GskDiffData                 *data);
void            gsk_container_node_diff_with            (GskRenderNode               *container,
                                                         GskRenderNode               *other,
                                                         GskDiffData                 *data);
void            gsk_render_node_draw_ccs                (GskRenderNode               *node,
                                                         cairo_t                     *cr,
                                                         GdkColorState               *ccs);
void            gsk_render_node_draw_with_color_state   (GskRenderNode               *node,
                                                         cairo_t                     *cr,
                                                         GdkColorState               *color_state);
void            gsk_render_node_draw_fallback           (GskRenderNode               *node,
                                                         cairo_t                     *cr);

bool            gsk_border_node_get_uniform             (const GskRenderNode         *self) G_GNUC_PURE;
bool            gsk_border_node_get_uniform_color       (const GskRenderNode         *self) G_GNUC_PURE;

void            gsk_text_node_serialize_glyphs          (GskRenderNode               *self,
                                                         GString                     *str);

cairo_hint_style_t
                gsk_text_node_get_font_hint_style       (const GskRenderNode         *self) G_GNUC_PURE;

GskRenderNode ** gsk_container_node_get_children        (const GskRenderNode         *node,
                                                         guint                       *n_children);

void            gsk_transform_node_get_translate        (const GskRenderNode         *node,
                                                         float                       *dx,
                                                         float                       *dy);
GdkMemoryDepth  gsk_render_node_get_preferred_depth     (const GskRenderNode         *node) G_GNUC_PURE;
gboolean        gsk_render_node_is_hdr                  (const GskRenderNode         *node) G_GNUC_PURE;

gboolean        gsk_container_node_is_disjoint          (const GskRenderNode         *node) G_GNUC_PURE;

gboolean        gsk_render_node_use_offscreen_for_opacity (const GskRenderNode       *node) G_GNUC_PURE;

#define gsk_render_node_ref(node)   _gsk_render_node_ref(node)
#define gsk_render_node_unref(node) _gsk_render_node_unref(node)

static inline GskRenderNode *
_gsk_render_node_ref (GskRenderNode *node)
{
  g_atomic_ref_count_inc (&node->ref_count);
  return node;
}

GskRenderNode *         gsk_color_node_new2                     (const GdkColor         *color,
                                                                 const graphene_rect_t  *bounds);
const GdkColor *        gsk_color_node_get_color2               (const GskRenderNode    *node);

GskRenderNode *         gsk_border_node_new2                    (const GskRoundedRect   *outline,
                                                                 const float             border_width[4],
                                                                 const GdkColor          border_color[4]);
const GdkColor *        gsk_border_node_get_colors2             (const GskRenderNode    *node);

GskRenderNode *         gsk_inset_shadow_node_new2              (const GskRoundedRect   *outline,
                                                                 const GdkColor         *color,
                                                                 const graphene_point_t *offset,
                                                                 float                   spread,
                                                                 float                   blur_radius);
const GdkColor *        gsk_inset_shadow_node_get_color2        (const GskRenderNode    *node);
const graphene_point_t *gsk_inset_shadow_node_get_offset        (const GskRenderNode    *node);

GskRenderNode *         gsk_outset_shadow_node_new2             (const GskRoundedRect   *outline,
                                                                 const GdkColor         *color,
                                                                 const graphene_point_t *offset,
                                                                 float                   spread,
                                                                 float                   blur_radius);
const GdkColor *        gsk_outset_shadow_node_get_color2       (const GskRenderNode    *node);
const graphene_point_t *gsk_outset_shadow_node_get_offset       (const GskRenderNode    *node);

typedef struct _GskShadow2 GskShadow2;
struct _GskShadow2
{
  GdkColor color;
  graphene_point_t offset;
  float radius;
};

GskRenderNode * gsk_shadow_node_new2                    (GskRenderNode        *child,
                                                         const GskShadow2     *shadows,
                                                         gsize                 n_shadows);

const GskShadow2 *gsk_shadow_node_get_shadow2           (const GskRenderNode  *node,
                                                         gsize                 i);

GskRenderNode * gsk_text_node_new2                      (PangoFont              *font,
                                                         PangoGlyphString       *glyphs,
                                                         const GdkColor         *color,
                                                         const graphene_point_t *offset);
const GdkColor *gsk_text_node_get_color2                (const GskRenderNode    *node);


#define GSK_RENDER_NODE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GSK_TYPE_RENDER_NODE, GskRenderNodeClass))

#define gsk_render_node_get_node_type(node) _gsk_render_node_get_node_type (node)

G_GNUC_PURE static inline
GskRenderNodeType
_gsk_render_node_get_node_type (const GskRenderNode *node)
{
  return GSK_RENDER_NODE_GET_CLASS (node)->node_type;
}

typedef struct _GskColorStop2 GskColorStop2;
struct _GskColorStop2
{
  float offset;
  GdkColor color;
};


typedef enum
{
  GSK_HUE_INTERPOLATION_SHORTER,
  GSK_HUE_INTERPOLATION_LONGER,
  GSK_HUE_INTERPOLATION_INCREASING,
  GSK_HUE_INTERPOLATION_DECREASING,
} GskHueInterpolation;

GskRenderNode * gsk_linear_gradient_node_new2           (const graphene_rect_t   *bounds,
                                                         const graphene_point_t  *start,
                                                         const graphene_point_t  *end,
                                                         GdkColorState           *interpolation,
                                                         GskHueInterpolation      hue_interpolation,
                                                         const GskColorStop2     *color_stops,
                                                         gsize                    n_color_stops);
GskRenderNode * gsk_repeating_linear_gradient_node_new2 (const graphene_rect_t   *bounds,
                                                         const graphene_point_t  *start,
                                                         const graphene_point_t  *end,
                                                         GdkColorState           *interpolation,
                                                         GskHueInterpolation      hue_interpolation,
                                                         const GskColorStop2     *color_stops,
                                                         gsize                    n_color_stops);
const GskColorStop2 *gsk_linear_gradient_node_get_color_stops2
                                                        (const GskRenderNode *node);
GdkColorState * gsk_linear_gradient_node_get_interpolation_color_state
                                                        (const GskRenderNode *node);
GskHueInterpolation gsk_linear_gradient_node_get_hue_interpolation
                                                        (const GskRenderNode *node);

GskRenderNode * gsk_radial_gradient_node_new2           (const graphene_rect_t   *bounds,
                                                         const graphene_point_t  *center,
                                                         float                    hradius,
                                                         float                    vradius,
                                                         float                    start,
                                                         float                    end,
                                                         GdkColorState           *interpolation,
                                                         GskHueInterpolation      hue_interpolation,
                                                         const GskColorStop2     *color_stops,
                                                         gsize                    n_color_stops);
GskRenderNode * gsk_repeating_radial_gradient_node_new2 (const graphene_rect_t   *bounds,
                                                         const graphene_point_t  *center,
                                                         float                    hradius,
                                                         float                    vradius,
                                                         float                    start,
                                                         float                    end,
                                                         GdkColorState           *interpolation,
                                                         GskHueInterpolation      hue_interpolation,
                                                         const GskColorStop2     *color_stops,
                                                         gsize                    n_color_stops);
const GskColorStop2 *gsk_radial_gradient_node_get_color_stops2
                                                        (const GskRenderNode *node);
GdkColorState * gsk_radial_gradient_node_get_interpolation_color_state
                                                        (const GskRenderNode *node);
GskHueInterpolation gsk_radial_gradient_node_get_hue_interpolation
                                                        (const GskRenderNode *node);

GskRenderNode * gsk_conic_gradient_node_new2            (const graphene_rect_t   *bounds,
                                                         const graphene_point_t  *center,
                                                         float                    rotation,
                                                         GdkColorState           *interpolation,
                                                         GskHueInterpolation      hue_interpolation,
                                                         const GskColorStop2     *color_stops,
                                                         gsize                    n_color_stops);
const GskColorStop2 *gsk_conic_gradient_node_get_color_stops2
                                                        (const GskRenderNode *node);
GdkColorState * gsk_conic_gradient_node_get_interpolation_color_state
                                                        (const GskRenderNode *node);
GskHueInterpolation gsk_conic_gradient_node_get_hue_interpolation
                                                        (const GskRenderNode *node);

G_END_DECLS
