
#include "gdkwaylandcolor-private.h"
#include "gdksurface-wayland-private.h"
#include <gdk/wayland/color-management-v1-client-protocol.h>
#include <gdk/wayland/color-representation-v1-client-protocol.h>

typedef struct _ImageDescription ImageDescription;

static const uint primaries_map[] = {
  [WP_COLOR_MANAGER_V1_PRIMARIES_SRGB] = 1,
  [WP_COLOR_MANAGER_V1_PRIMARIES_PAL_M] = 4,
  [WP_COLOR_MANAGER_V1_PRIMARIES_PAL] = 5,
  [WP_COLOR_MANAGER_V1_PRIMARIES_NTSC] = 6,
  [WP_COLOR_MANAGER_V1_PRIMARIES_GENERIC_FILM] = 8,
  [WP_COLOR_MANAGER_V1_PRIMARIES_BT2020] = 9,
  [WP_COLOR_MANAGER_V1_PRIMARIES_CIE1931_XYZ] = 10,
  [WP_COLOR_MANAGER_V1_PRIMARIES_DCI_P3] = 11,
  [WP_COLOR_MANAGER_V1_PRIMARIES_DISPLAY_P3] = 12,
  [WP_COLOR_MANAGER_V1_PRIMARIES_ADOBE_RGB] = 0,
};

static uint
wl_to_cicp_primaries (enum wp_color_manager_v1_primaries cp)
{
  return primaries_map[cp];
}

static enum wp_color_manager_v1_primaries
cicp_to_wl_primaries (uint cp)
{
  for (uint32_t i = WP_COLOR_MANAGER_V1_PRIMARIES_SRGB; i < G_N_ELEMENTS (primaries_map); i++)
    if (primaries_map[i] == cp)
       return (enum wp_color_manager_v1_primaries)i;

  return 0;
}

static const uint primaries_primaries[][8] = {
  [WP_COLOR_MANAGER_V1_PRIMARIES_SRGB] =         { 640000, 330000, 300000, 600000, 150000,  60000, 312700, 329000 },
  [WP_COLOR_MANAGER_V1_PRIMARIES_PAL_M] =        { 670000, 330000, 210000, 710000, 140000,  80000, 310000, 316000 },
  [WP_COLOR_MANAGER_V1_PRIMARIES_PAL] =          { 640000, 330000, 290000, 600000, 150000,  60000, 312700, 329000 },
  [WP_COLOR_MANAGER_V1_PRIMARIES_NTSC] =         { 630000, 340000, 310000, 595000, 155000,  70000, 312700, 329000 },
  [WP_COLOR_MANAGER_V1_PRIMARIES_GENERIC_FILM] = { 243000, 692000, 145000,  49000, 681000, 319000, 310000, 316000 },
  [WP_COLOR_MANAGER_V1_PRIMARIES_BT2020] =       { 708000, 292000, 170000, 797000, 131000,  46000, 312700, 329000 },
  [WP_COLOR_MANAGER_V1_PRIMARIES_CIE1931_XYZ] =  {1000000,      0,      0,1000000,      0,      0, 333333, 333333 },
  [WP_COLOR_MANAGER_V1_PRIMARIES_DCI_P3] =       { 680000, 320000, 265000, 690000, 150000,  60000, 314000, 351000 },
  [WP_COLOR_MANAGER_V1_PRIMARIES_DISPLAY_P3] =   { 680000, 320000, 265000, 690000, 150000,  60000, 312700, 329000 },
  [WP_COLOR_MANAGER_V1_PRIMARIES_ADOBE_RGB] =    { 640000, 330000, 210000, 710000, 150000,  60000, 312700, 329000 },
};

static const uint *
wl_primaries_to_primaries (enum wp_color_manager_v1_primaries primaries)
{
  return primaries_primaries[primaries];
}

static gboolean
primaries_to_wl_primaries (const uint primaries[8],
                           enum wp_color_manager_v1_primaries *out_primaries)
{
  for (uint32_t i = WP_COLOR_MANAGER_V1_PRIMARIES_SRGB; i < G_N_ELEMENTS (primaries_primaries); i++)
    {
      uint32_t j;

      for (j = 0; j < 8; j++)
        {
          if (primaries[j] != primaries_primaries[i][j])
            break;
        }
      if (j == 8)
        {
          *out_primaries = i;
          return TRUE;
        }
    }
  return FALSE;
}

static const uint transfer_map[] = {
  [WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_BT1886] = 1,
  [WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_GAMMA22] = 4,
  [WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_GAMMA28] = 5,
  [WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_ST240] = 7,
  [WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_EXT_LINEAR] = 8,
  [WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_LOG_100] = 9,
  [WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_LOG_316] = 10,
  [WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_XVYCC] = 11,
  [WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_SRGB] = 13,
  [WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_EXT_SRGB] = 13,
  [WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_ST2084_PQ] = 16,
  [WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_ST428] = 17,
  [WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_HLG] = 18,
};

static uint
wl_to_cicp_transfer (enum wp_color_manager_v1_transfer_function tf)
{
  return transfer_map[tf];
}

static enum wp_color_manager_v1_transfer_function
cicp_to_wl_transfer (uint tf)
{
  for (uint32_t i = WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_BT1886; i < G_N_ELEMENTS (transfer_map); i++)
    if (transfer_map[i] == tf)
       return (enum wp_color_manager_v1_transfer_function)i;

  return 0;
}

struct {
  enum wp_color_representation_surface_v1_coefficients wp;
  uint cicp;
  const char *name;
} coefficients_map[] = {
 { .wp = WP_COLOR_REPRESENTATION_SURFACE_V1_COEFFICIENTS_IDENTITY, .cicp = 0, .name = "identity" },
 { .wp = WP_COLOR_REPRESENTATION_SURFACE_V1_COEFFICIENTS_BT709, .cicp = 1, .name = "bt709" },
 { .wp = WP_COLOR_REPRESENTATION_SURFACE_V1_COEFFICIENTS_FCC, .cicp = 4, .name = "fcc" },
 { .wp = WP_COLOR_REPRESENTATION_SURFACE_V1_COEFFICIENTS_BT601, .cicp = 5, .name = "bt601" },
 { .wp = WP_COLOR_REPRESENTATION_SURFACE_V1_COEFFICIENTS_SMPTE240, .cicp = 7, .name = "smpte240" },
 { .wp = WP_COLOR_REPRESENTATION_SURFACE_V1_COEFFICIENTS_BT2020, .cicp = 9, .name = "bt2020" },
 { .wp = WP_COLOR_REPRESENTATION_SURFACE_V1_COEFFICIENTS_BT2020_CL, .cicp = 10, .name = "bt2020-cl" },
 { .wp = WP_COLOR_REPRESENTATION_SURFACE_V1_COEFFICIENTS_ICTCP, .cicp = 14, .name = "ictcp" },
};

static enum wp_color_representation_surface_v1_coefficients
cicp_to_wl_coefficients (uint m)
{
  if (m == 6)
    m = 5;

  for (uint32_t i = 0; i < G_N_ELEMENTS (coefficients_map); i++)
    if (coefficients_map[i].cicp == m)
       return coefficients_map[i].wp;

  return 0;
}

static const char *
wl_coefficients_name (enum wp_color_representation_surface_v1_coefficients value)
{
  for (uint32_t i = 0; i < G_N_ELEMENTS (coefficients_map); i++)
    if (coefficients_map[i].wp == value)
       return coefficients_map[i].name;

  return "invalid";
}

struct {
  enum wp_color_representation_surface_v1_alpha_mode wp;
  const char *name;
} alpha_modes[] = {
  { .wp = WP_COLOR_REPRESENTATION_SURFACE_V1_ALPHA_MODE_PREMULTIPLIED_ELECTRICAL, .name = "premultiplied-electrical" },
  { .wp = WP_COLOR_REPRESENTATION_SURFACE_V1_ALPHA_MODE_PREMULTIPLIED_OPTICAL, .name = "premultiplied-optical" },
  { .wp = WP_COLOR_REPRESENTATION_SURFACE_V1_ALPHA_MODE_STRAIGHT, .name = "straight" },
};

static const char *
wl_alpha_name (enum wp_color_representation_surface_v1_alpha_mode alpha)
{
  for (uint32_t i = 0; i < G_N_ELEMENTS (alpha_modes); i++)
    if (alpha_modes[i].wp == alpha)
       return alpha_modes[i].name;

  return "invalid";
}

static enum wp_color_representation_surface_v1_alpha_mode
gdk_alpha_to_wl_alpha (GdkMemoryAlpha alpha)
{
  switch (alpha)
    {
    case GDK_MEMORY_ALPHA_PREMULTIPLIED:
    case GDK_MEMORY_ALPHA_OPAQUE:
      return WP_COLOR_REPRESENTATION_SURFACE_V1_ALPHA_MODE_PREMULTIPLIED_ELECTRICAL;
    case GDK_MEMORY_ALPHA_STRAIGHT:
      return WP_COLOR_REPRESENTATION_SURFACE_V1_ALPHA_MODE_STRAIGHT;
    default:
      g_assert_not_reached ();
    }
}

static const char *
wl_range_name (enum wp_color_representation_surface_v1_range range)
{
  switch (range)
    {
    case WP_COLOR_REPRESENTATION_SURFACE_V1_RANGE_FULL:
      return "full";
    case WP_COLOR_REPRESENTATION_SURFACE_V1_RANGE_LIMITED:
      return "limited";
    default:
      g_assert_not_reached ();
    }
}

struct _GdkWaylandColor
{
  GdkWaylandDisplay *display;

  struct wp_color_manager_v1 *color_manager;
  struct {
    unsigned int intents;
    unsigned int features;
    unsigned int transfers;
    unsigned int primaries;
  } color_manager_supported;

  GHashTable *cs_to_desc; /* GdkColorState => wp_image_description_v1 or NULL */
  GHashTable *id_to_cs; /* uint32 identifier => GdkColorState */

  struct wp_color_representation_manager_v1 *color_representation_manager;
  struct {
    unsigned int alpha_modes;
    unsigned int coefficients_limited;
    unsigned int coefficients_full;
  } color_representation_supported;
};

static uint32_t
color_state_hash (gconstpointer data)
{
  GdkColorState *cs = (GdkColorState *) data;
  const GdkCicp *cicp;

  cicp = gdk_color_state_get_cicp (cs);
  if (cicp)
    {
      GdkCicp norm;
      gdk_cicp_normalize (cicp, &norm);
      return norm.color_primaries << 24 | norm.transfer_function;
    }

  return 0;
}

static gboolean
color_state_equal (gconstpointer a,
                   gconstpointer b)
{
  GdkColorState *csa = (GdkColorState *) a;
  GdkColorState *csb = (GdkColorState *) b;
  const GdkCicp *cicpa, *cicpb;
  GdkCicp norma, normb;

  cicpa = gdk_color_state_get_cicp (csa);
  cicpb = gdk_color_state_get_cicp (csb);
  if (cicpa == NULL || cicpb == NULL)
    return FALSE;

  gdk_cicp_normalize (cicpa, &norma);
  gdk_cicp_normalize (cicpb, &normb);

  return norma.color_primaries == normb.color_primaries &&
         norma.transfer_function == normb.transfer_function;
}

static void
wp_color_manager_v1_supported_intent (void                       *data,
                                      struct wp_color_manager_v1 *wp_color_manager_v1,
                                      uint32_t                    render_intent)
{
  GdkWaylandColor *color = data;

  color->color_manager_supported.intents |= (1 << render_intent);
}

static void
wp_color_manager_v1_supported_feature (void                       *data,
                                       struct wp_color_manager_v1 *wp_color_manager_v1,
                                       uint32_t                    feature)
{
  GdkWaylandColor *color = data;

  color->color_manager_supported.features |= (1 << feature);
}

static void
wp_color_manager_v1_supported_tf_named (void                       *data,
                                        struct wp_color_manager_v1 *wp_color_manager_v1,
                                        uint32_t                    tf)
{
  GdkWaylandColor *color = data;

  color->color_manager_supported.transfers |= (1 << tf);
}

static void
wp_color_manager_v1_supported_primaries_named (void                       *data,
                                               struct wp_color_manager_v1 *wp_color_manager_v1,
                                               uint32_t                    primaries)
{
  GdkWaylandColor *color = data;

  color->color_manager_supported.primaries |= (1 << primaries);
}

static void
wp_color_manager_v1_done (void                       *data,
                          struct wp_color_manager_v1 *wp_color_manager_v1)
{
}

static struct wp_color_manager_v1_listener color_manager_listener = {
  wp_color_manager_v1_supported_intent,
  wp_color_manager_v1_supported_feature,
  wp_color_manager_v1_supported_tf_named,
  wp_color_manager_v1_supported_primaries_named,
  wp_color_manager_v1_done,
};

GdkWaylandColor *
gdk_wayland_color_new (GdkWaylandDisplay *display)
{
  GdkWaylandColor *color;

  color = g_new0 (GdkWaylandColor, 1);

  color->display = display;

  color->cs_to_desc = g_hash_table_new_full (color_state_hash,
                                             color_state_equal,
                                             (GDestroyNotify) gdk_color_state_unref,
                                             (GDestroyNotify) wp_image_description_v1_destroy);
  color->id_to_cs = g_hash_table_new_full (g_direct_hash,
                                           g_direct_equal,
                                           NULL,
                                           (GDestroyNotify) gdk_color_state_unref);

  return color;
}

void
gdk_wayland_color_set_color_manager (GdkWaylandColor    *color,
                                     struct wl_registry *registry,
                                     uint32_t            id,
                                     uint32_t            version)
{
  color->color_manager = wl_registry_bind (registry,
                                           id,
                                           &wp_color_manager_v1_interface,
                                           MIN (version, 1));

  wp_color_manager_v1_add_listener (color->color_manager,
                                    &color_manager_listener,
                                    color);
}

void
gdk_wayland_color_free (GdkWaylandColor *color)
{
  g_clear_pointer (&color->color_manager, wp_color_manager_v1_destroy);

  g_hash_table_unref (color->cs_to_desc);
  g_hash_table_unref (color->id_to_cs);

  g_clear_pointer (&color->color_representation_manager, wp_color_representation_manager_v1_destroy);

  g_free (color);
}

struct wl_proxy *
gdk_wayland_color_get_color_manager (GdkWaylandColor *color)
{
  return (struct wl_proxy *) color->color_manager;
}

struct wl_proxy *
gdk_wayland_color_get_color_representation_manager (GdkWaylandColor *color)
{
  return (struct wl_proxy *) color->color_representation_manager;
}

typedef struct _CsImageDescListenerData {
  GdkWaylandColor *color;
  GdkColorState   *color_state;
  gboolean         sync;
  gboolean         done;
} CsImageDescListenerData;

static void
wp_color_representation_manager_v1_supported_alpha_mode (void *data,
                                                         struct wp_color_representation_manager_v1 *wp_color_representation_manager_v1,
                                                         uint32_t alpha_mode)
{
  GdkWaylandColor *color = data;

  color->color_representation_supported.alpha_modes |= 1 << alpha_mode;
}

static void
wp_color_representation_manager_v1_supported_coefficients_and_ranges (void *data,
                                                                      struct wp_color_representation_manager_v1 *wp_color_representation_manager_v1,
                                                                      uint32_t coefficients,
                                                                      uint32_t range)
{
  GdkWaylandColor *color = data;

  switch (range)
    {
    case WP_COLOR_REPRESENTATION_SURFACE_V1_RANGE_FULL:
      color->color_representation_supported.coefficients_full |= 1 << coefficients;
      break;

    case WP_COLOR_REPRESENTATION_SURFACE_V1_RANGE_LIMITED:
      color->color_representation_supported.coefficients_limited |= 1 << coefficients;
      break;

    default:
      g_assert_not_reached ();
    }
}

static void
wp_color_representation_manager_v1_done (void *data,
                                         struct wp_color_representation_manager_v1 *wp_color_representation_manager_v1)
{
}

static struct wp_color_representation_manager_v1_listener color_representation_manager_listener = {
  wp_color_representation_manager_v1_supported_alpha_mode,
  wp_color_representation_manager_v1_supported_coefficients_and_ranges,
  wp_color_representation_manager_v1_done,
};

void
gdk_wayland_color_set_color_representation (GdkWaylandColor        *color,
                                            struct wl_registry     *registry,
                                            uint32_t                id,
                                            uint32_t                version)
{
  g_assert (color->color_representation_manager == NULL);

  color->color_representation_manager = wl_registry_bind (registry,
                                                         id,
                                                         &wp_color_representation_manager_v1_interface,
                                                         MIN (version, 1));

  wp_color_representation_manager_v1_add_listener (color->color_representation_manager,
                                                   &color_representation_manager_listener,
                                                   color);
}

static void
cs_image_listener_data_free (CsImageDescListenerData *csi)
{
  csi->done = TRUE;

  if (csi->sync)
    return;

  g_free (csi);
}

static void
cs_image_desc_failed (void                           *data,
                      struct wp_image_description_v1 *desc,
                      uint32_t                        cause,
                      const char                     *msg)
{
  CsImageDescListenerData *csi = data;

  g_warning ("Failed to get one of the standard image descriptions: %s", msg);
  wp_image_description_v1_destroy (desc);

  g_hash_table_insert (csi->color->cs_to_desc,
                       gdk_color_state_ref (csi->color_state),
                       NULL);

  cs_image_listener_data_free (csi);
}

static void
cs_image_desc_ready (void                           *data,
                     struct wp_image_description_v1 *desc,
                     uint32_t                        identity)
{
  CsImageDescListenerData *csi = data;

  g_hash_table_insert (csi->color->cs_to_desc,
                       gdk_color_state_ref (csi->color_state),
                       desc);
  g_hash_table_insert (csi->color->id_to_cs,
                       GUINT_TO_POINTER (identity),
                       gdk_color_state_ref (csi->color_state));
 
  cs_image_listener_data_free (csi);
}

static struct wp_image_description_v1_listener cs_image_desc_listener = {
  cs_image_desc_failed,
  cs_image_desc_ready,
};

static void
create_image_desc (GdkWaylandColor *color,
                   GdkColorState   *cs,
                   gboolean         sync)
{
  CsImageDescListenerData data;
  struct wp_image_description_creator_params_v1 *creator;
  struct wp_image_description_v1 *desc;
  const GdkCicp *cicp;
  GdkCicp norm;
  uint32_t primaries, tf;

  cicp = gdk_color_state_get_cicp (cs);
  if (!cicp)
    {
      GDK_DISPLAY_DEBUG (GDK_DISPLAY (color->display), MISC,
                         "Unsupported color state %s: Not a CICP colorstate",
                         gdk_color_state_get_name (cs));
      g_hash_table_insert (color->cs_to_desc, gdk_color_state_ref (cs), NULL);
      return;
    }

  gdk_cicp_normalize (cicp, &norm);
  primaries = cicp_to_wl_primaries (norm.color_primaries);
  tf = cicp_to_wl_transfer (norm.transfer_function);

  if (((color->color_manager_supported.primaries & (1 << primaries)) == 0 &&
       (color->color_manager_supported.features & (1 << WP_COLOR_MANAGER_V1_FEATURE_SET_PRIMARIES)) == 0) ||
      (color->color_manager_supported.transfers & (1 << tf)) == 0)
    {
      GDK_DISPLAY_DEBUG (GDK_DISPLAY (color->display), MISC,
                         "Unsupported color state %s: Primaries or transfer function unsupported",
                         gdk_color_state_get_name (cs));
      g_hash_table_insert (color->cs_to_desc, gdk_color_state_ref (cs), NULL);
      return;
    }

  g_assert (color->color_manager != NULL);

  data.color = color;
  data.color_state = cs;
  data.sync = sync;
  data.done = FALSE;

  creator = wp_color_manager_v1_create_parametric_creator (color->color_manager);

  if (color->color_manager_supported.primaries & (1 << primaries))
    {
      wp_image_description_creator_params_v1_set_primaries_named (creator, primaries);
    }
  else
    {
      const uint *p = wl_primaries_to_primaries (primaries);
      wp_image_description_creator_params_v1_set_primaries (creator,
                                                            p[0], p[1],
                                                            p[2], p[3],
                                                            p[4], p[5],
                                                            p[6], p[7]);
    }
  wp_image_description_creator_params_v1_set_tf_named (creator, tf);

  desc = wp_image_description_creator_params_v1_create (creator);

  if (sync)
    {
      struct wl_event_queue *event_queue;

      event_queue = wl_display_create_queue (color->display->wl_display);
      wl_proxy_set_queue ((struct wl_proxy *) desc, event_queue);
      wp_image_description_v1_add_listener (desc, &cs_image_desc_listener, &data);
      while (!data.done)
        gdk_wayland_display_dispatch_queue (GDK_DISPLAY (color->display), event_queue);

      wl_event_queue_destroy (event_queue);
    }
  else
    {
      wp_image_description_v1_add_listener (desc, &cs_image_desc_listener, g_memdup2 (&data, sizeof data));
    }
}

gboolean
gdk_wayland_color_prepare (GdkWaylandColor *color)
{
  if (color->color_manager)
    {
      if (GDK_DISPLAY_DEBUG_CHECK (GDK_DISPLAY (color->display), MISC))
        {
          struct {
            const char *name;
            enum wp_color_manager_v1_render_intent value;
          } intents[] = {
            { "perceptual", WP_COLOR_MANAGER_V1_RENDER_INTENT_PERCEPTUAL },
            { "relative", WP_COLOR_MANAGER_V1_RENDER_INTENT_RELATIVE },
            { "saturation", WP_COLOR_MANAGER_V1_RENDER_INTENT_SATURATION },
            { "absolute", WP_COLOR_MANAGER_V1_RENDER_INTENT_ABSOLUTE },
            { "relative-bpc", WP_COLOR_MANAGER_V1_RENDER_INTENT_RELATIVE_BPC },
          };
          struct {
            const char *name;
            enum wp_color_manager_v1_feature value;
          } features[] = {
            { "icc-v2-v4", WP_COLOR_MANAGER_V1_FEATURE_ICC_V2_V4 },
            { "parametric", WP_COLOR_MANAGER_V1_FEATURE_PARAMETRIC },
            { "set-primaries", WP_COLOR_MANAGER_V1_FEATURE_SET_PRIMARIES },
            { "set-tf-power", WP_COLOR_MANAGER_V1_FEATURE_SET_TF_POWER },
            { "set-luminances", WP_COLOR_MANAGER_V1_FEATURE_SET_LUMINANCES },
            { "set-mastering-display-primaries", WP_COLOR_MANAGER_V1_FEATURE_SET_MASTERING_DISPLAY_PRIMARIES },
            { "extended-target-volume", WP_COLOR_MANAGER_V1_FEATURE_EXTENDED_TARGET_VOLUME },
            { "windows-scrgb", WP_COLOR_MANAGER_V1_FEATURE_WINDOWS_SCRGB },
          };
          struct {
            const char *name;
            enum wp_color_manager_v1_primaries value;
          } primaries[] = {
            { "srgb", WP_COLOR_MANAGER_V1_PRIMARIES_SRGB },
            { "pal-m", WP_COLOR_MANAGER_V1_PRIMARIES_PAL_M },
            { "pal", WP_COLOR_MANAGER_V1_PRIMARIES_PAL },
            { "ntsc", WP_COLOR_MANAGER_V1_PRIMARIES_NTSC },
            { "generic-film", WP_COLOR_MANAGER_V1_PRIMARIES_GENERIC_FILM },
            { "bt2020", WP_COLOR_MANAGER_V1_PRIMARIES_BT2020 },
            { "cie1931-xyz", WP_COLOR_MANAGER_V1_PRIMARIES_CIE1931_XYZ },
            { "dci-p3", WP_COLOR_MANAGER_V1_PRIMARIES_DCI_P3 },
            { "display-p3", WP_COLOR_MANAGER_V1_PRIMARIES_DISPLAY_P3 },
            { "adobe-rgb", WP_COLOR_MANAGER_V1_PRIMARIES_ADOBE_RGB },
          };
          struct {
            const char *name;
            enum wp_color_manager_v1_transfer_function value;
          } tf[] = {
            { "bt1886", WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_BT1886 },
            { "gamma22", WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_GAMMA22 },
            { "gamma28", WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_GAMMA28 },
            { "st240", WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_ST240 },
            { "ext-linear", WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_EXT_LINEAR },
            { "log100", WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_LOG_100 },
            { "log316", WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_LOG_316 },
            { "xvycc", WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_XVYCC },
            { "srgb", WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_SRGB },
            { "ext-srgb", WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_EXT_SRGB },
            { "st2084-pq", WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_ST2084_PQ },
            { "st428", WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_ST428 },
            { "hlg", WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_HLG },
          };
          unsigned int len;

         len = 0;
         for (int i = 0; i < G_N_ELEMENTS (intents); i++)
           len = MAX (len, strlen (intents[i].name));

         for (int i = 0; i < G_N_ELEMENTS (intents); i++)
            gdk_debug_message ("Rendering intent %2u (%s): %*s%s",
                               intents[i].value,
                               intents[i].name,
                               (int) (len - strlen (intents[i].name)), "",
                               color->color_manager_supported.intents & (1 << intents[i].value) ? "✓" : "✗");

         len = 0;
         for (int i = 0; i < G_N_ELEMENTS (features); i++)
           len = MAX (len, strlen (features[i].name));

          for (int i = 0; i < G_N_ELEMENTS (features); i++)
            gdk_debug_message ("Feature %2u (%s): %*s%s",
                               features[i].value,
                               features[i].name,
                               (int) (len - strlen (features[i].name)), "",
                               color->color_manager_supported.features & (1 << features[i].value) ? "✓" : "✗");

         len = 0;
         for (int i = 0; i < G_N_ELEMENTS (primaries); i++)
           len = MAX (len, strlen (primaries[i].name));

          for (int i = 0; i < G_N_ELEMENTS (primaries); i++)
            gdk_debug_message ("Primaries %2u (%s): %*s%s",
                               primaries[i].value,
                               primaries[i].name,
                               (int) (len - strlen (primaries[i].name)), "",
                               color->color_manager_supported.primaries & (1 << primaries[i].value) ? "✓" : "✗");

         len = 0;
         for (int i = 0; i < G_N_ELEMENTS (tf); i++)
           len = MAX (len, strlen (tf[i].name));

          for (int i = 0; i < G_N_ELEMENTS (tf); i++)
            gdk_debug_message ("Transfer function %2u (%s): %*s%s",
                               tf[i].value,
                               tf[i].name,
                               (int) (len - strlen (tf[i].name)), "",
                               color->color_manager_supported.transfers & (1 << tf[i].value) ? "✓" : "✗");
        }
    }

  if (color->color_manager &&
      !(color->color_manager_supported.intents & (1 << WP_COLOR_MANAGER_V1_RENDER_INTENT_PERCEPTUAL)))
    {
      GDK_DISPLAY_DEBUG (GDK_DISPLAY (color->display), MISC, "Not using color management: Missing perceptual render intent");
      g_clear_pointer (&color->color_manager, wp_color_manager_v1_destroy);
    }

  if (color->color_manager &&
      (!(color->color_manager_supported.features & (1 << WP_COLOR_MANAGER_V1_FEATURE_PARAMETRIC)) ||
       !(color->color_manager_supported.transfers & (1 << WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_SRGB)) ||
       !((color->color_manager_supported.primaries & (1 << WP_COLOR_MANAGER_V1_PRIMARIES_SRGB)) ||
         (color->color_manager_supported.features & (1 << WP_COLOR_MANAGER_V1_FEATURE_SET_PRIMARIES)))))

    {
      GDK_DISPLAY_DEBUG (GDK_DISPLAY (color->display), MISC, "Not using color management: Can't create srgb image description");
      g_clear_pointer (&color->color_manager, wp_color_manager_v1_destroy);
    }

  if (color->color_manager)
    {
      create_image_desc (color, GDK_COLOR_STATE_SRGB, FALSE);

      if (color->color_manager_supported.transfers & (1 << WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_EXT_LINEAR))
        create_image_desc (color, GDK_COLOR_STATE_SRGB_LINEAR, FALSE);

      if ((color->color_manager_supported.primaries & (1 << WP_COLOR_MANAGER_V1_PRIMARIES_BT2020) ||
          (color->color_manager_supported.features & (1 << WP_COLOR_MANAGER_V1_FEATURE_SET_PRIMARIES))))
        {
          if (color->color_manager_supported.transfers & (1 << WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_ST2084_PQ))
            create_image_desc (color, GDK_COLOR_STATE_REC2100_PQ, FALSE);

          if (color->color_manager_supported.transfers & (1 << WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_EXT_LINEAR))
            create_image_desc (color, GDK_COLOR_STATE_REC2100_LINEAR, FALSE);
        }
    }

  if (color->color_representation_manager)
    {
      if (GDK_DISPLAY_DEBUG_CHECK (GDK_DISPLAY (color->display), MISC))
        {
         unsigned int len;

         len = 0;
         for (int i = 0; i < G_N_ELEMENTS (alpha_modes); i++)
           len = MAX (len, strlen (alpha_modes[i].name));

         for (int i = 0; i < G_N_ELEMENTS (alpha_modes); i++)
           gdk_debug_message ("Alpha mode %2u (%s): %*s%s",
                              alpha_modes[i].wp,
                              alpha_modes[i].name,
                              (int) (len - strlen (alpha_modes[i].name)), "",
                              color->color_representation_supported.alpha_modes & (1 << alpha_modes[i].wp) ? "✓" : "✗");

         len = 0;
         for (int i = 0; i < G_N_ELEMENTS (coefficients_map); i++)
           len = MAX (len, strlen (coefficients_map[i].name));

         for (int i = 0; i < G_N_ELEMENTS (coefficients_map); i++)
           gdk_debug_message ("Coefficients (full range) %2u (%s): %*s%s",
                              coefficients_map[i].wp,
                              coefficients_map[i].name,
                              (int) (len - strlen (coefficients_map[i].name)), "",
                              color->color_representation_supported.coefficients_full & (1 << coefficients_map[i].wp) ? "✓" : "✗");

         for (int i = 0; i < G_N_ELEMENTS (coefficients_map); i++)
           gdk_debug_message ("Coefficients (limited range) %2u (%s): %*s%s",
                              coefficients_map[i].wp,
                              coefficients_map[i].name,
                              (int) (len - strlen (coefficients_map[i].name)), "",
                              color->color_representation_supported.coefficients_limited & (1 << coefficients_map[i].wp) ? "✓" : "✗");
        }
    }

  if (color->color_representation_manager &&
      !(color->color_representation_supported.coefficients_full & (1 << WP_COLOR_REPRESENTATION_SURFACE_V1_COEFFICIENTS_IDENTITY)))
    {
      GDK_DISPLAY_DEBUG (GDK_DISPLAY (color->display), MISC, "Not using color representation: Can't create identity transform");
      g_clear_pointer (&color->color_representation_manager, wp_color_representation_manager_v1_destroy);
    }

  return color->color_manager != NULL || color->color_representation_manager != NULL;
}

struct _GdkWaylandColorSurface
{
  GdkWaylandColor *color;
  struct wl_surface *wl_surface;
  struct wp_color_management_surface_v1 *mgmt_surface;
  struct wp_color_management_surface_feedback_v1 *mgmt_feedback;
  ImageDescription *current_desc;
  GdkColorStateChanged callback;
  gpointer data;

  struct wp_color_representation_surface_v1 *repr_surface;
};

struct _ImageDescription
{
  GdkWaylandColorSurface *surface;

  struct wp_image_description_v1 *image_desc;
  struct wp_image_description_info_v1 *info;

  uint32_t identity;

  int32_t icc;
  uint32_t icc_size;
  int32_t r_x, r_y, g_x, g_y, b_x, b_y, w_x, w_y;
  uint32_t primaries;
  uint32_t tf_power;
  uint32_t tf_named;
  uint32_t min_lum, max_lum, ref_lum;
  int32_t target_r_x, target_r_y, target_g_x, target_g_y, target_b_x, target_b_y, target_w_x, target_w_y;
  uint32_t target_min_lum, target_max_lum;
  uint32_t target_max_cll, target_max_fall;

  unsigned int has_icc : 1;
  unsigned int has_primaries : 1;
  unsigned int has_primaries_named : 1;
  unsigned int has_tf_power : 1;
  unsigned int has_tf_named : 1;
  unsigned int has_luminances : 1;
  unsigned int has_target_primaries : 1;
  unsigned int has_target_luminance : 1;
  unsigned int has_target_max_cll : 1;
  unsigned int has_target_max_fall : 1;
};

static GdkColorState *
gdk_color_state_from_image_description_bits (ImageDescription *desc)
{
  if (desc->has_primaries_named && desc->has_tf_named)
    {
      GdkCicp cicp;

      cicp.color_primaries = wl_to_cicp_primaries (desc->primaries);
      cicp.transfer_function = wl_to_cicp_transfer (desc->tf_named);
      cicp.matrix_coefficients = 0;
      cicp.range = GDK_CICP_RANGE_FULL;

      return gdk_color_state_new_for_cicp (&cicp, NULL);
    }
  else
    return NULL;
}

static void
gdk_wayland_color_surface_clear_image_desc (GdkWaylandColorSurface *self)
{
  ImageDescription *desc = self->current_desc;

  if (desc == NULL)
    return;

  g_clear_pointer (&desc->image_desc, wp_image_description_v1_destroy);
  g_clear_pointer (&desc->info, wp_image_description_info_v1_destroy);
  g_free (desc);

  self->current_desc = NULL;
}

static void
image_desc_info_done (void *data,
                      struct wp_image_description_info_v1 *info)
{
  ImageDescription *desc = data;
  GdkWaylandColorSurface *self = desc->surface;
  GdkColorState *cs;

  g_assert (self->current_desc == desc);

  cs = gdk_color_state_from_image_description_bits (desc);
  if (cs)
    {
      g_hash_table_insert (self->color->id_to_cs,
                           GUINT_TO_POINTER (desc->identity),
                           gdk_color_state_ref (cs));
    }
  else
    {
      cs = GDK_COLOR_STATE_SRGB;
      g_clear_pointer (&desc->image_desc, wp_image_description_v1_destroy);
    }

  if (self->callback)
    self->callback (desc->surface, cs, self->data);

  gdk_color_state_unref (cs);

  gdk_wayland_color_surface_clear_image_desc (self);
}

static void
image_desc_info_icc_file (void *data,
                          struct wp_image_description_info_v1 *info,
                          int32_t icc,
                          uint32_t icc_size)
{
  ImageDescription *desc = data;

  desc->icc = icc;
  desc->icc_size = icc_size;
  desc->has_icc = 1;
}

static void
image_desc_info_primaries (void *data,
                           struct wp_image_description_info_v1 *info,
                           int32_t r_x, int32_t r_y,
                           int32_t g_x, int32_t g_y,
                           int32_t b_x, int32_t b_y,
                           int32_t w_x, int32_t w_y)
{
  ImageDescription *desc = data;

  desc->r_x = r_x; desc->r_y = r_y;
  desc->g_x = g_x; desc->r_y = g_y;
  desc->b_x = b_x; desc->r_y = b_y;
  desc->w_x = w_x; desc->r_y = w_y;
  desc->has_primaries = 1;
  if (primaries_to_wl_primaries ((uint[]) { r_x, r_y, g_x, g_y, b_x, b_y, w_x, w_y },
                                 &desc->primaries))
    desc->has_primaries_named = 1;
}

static void
image_desc_info_primaries_named (void *data,
                                 struct wp_image_description_info_v1 *info,
                                 uint32_t primaries)
{
  ImageDescription *desc = data;
  const uint *p;

  desc->primaries = primaries;
  desc->has_primaries_named = 1;
  desc->has_primaries = 1;
  p = wl_primaries_to_primaries (primaries);
  desc->r_x = p[0]; desc->r_y = p[1];
  desc->g_x = p[2]; desc->r_y = p[3];
  desc->b_x = p[4]; desc->r_y = p[5];
  desc->w_x = p[6]; desc->r_y = p[7];
}

static void
image_desc_info_tf_power (void *data,
                          struct wp_image_description_info_v1 *info,
                          uint32_t tf_power)
{
  ImageDescription *desc = data;

  desc->tf_power = tf_power;
  desc->has_tf_power = 1;
}

static void
image_desc_info_tf_named (void *data,
                          struct wp_image_description_info_v1 *info,
                          uint32_t tf)
{
  ImageDescription *desc = data;

  desc->tf_named = tf;
  desc->has_tf_named = 1;
}

static void
image_desc_info_luminances (void *data,
                            struct wp_image_description_info_v1 *info,
                            uint32_t min_lum,
                            uint32_t max_lum,
                            uint32_t ref_lum)
{
  ImageDescription *desc = data;

  desc->min_lum = min_lum;
  desc->max_lum = max_lum;
  desc->ref_lum = ref_lum;
  desc->has_luminances = 1;
}

static void
image_desc_info_target_primaries (void *data,
                                  struct wp_image_description_info_v1 *info,
                                  int32_t r_x, int32_t r_y,
                                  int32_t g_x, int32_t g_y,
                                  int32_t b_x, int32_t b_y,
                                  int32_t w_x, int32_t w_y)
{
  ImageDescription *desc = data;

  desc->target_r_x = r_x; desc->target_r_y = r_y;
  desc->target_g_x = g_x; desc->target_r_y = g_y;
  desc->target_b_x = b_x; desc->target_r_y = b_y;
  desc->target_w_x = w_x; desc->target_r_y = w_y;
  desc->has_target_primaries = 1;
}

static void
image_desc_info_target_luminance (void *data,
                                  struct wp_image_description_info_v1 *info,
                                  uint32_t min_lum,
                                  uint32_t max_lum)
{
  ImageDescription *desc = data;

  desc->target_min_lum = min_lum;
  desc->target_max_lum = max_lum;
  desc->has_target_luminance = 1;
}

static void
image_desc_info_target_max_cll (void *data,
                                struct wp_image_description_info_v1 *info,
                                uint32_t max_cll)
{
  ImageDescription *desc = data;

  desc->target_max_cll = max_cll;
  desc->has_target_max_cll = 1;
}

static void
image_desc_info_target_max_fall (void *data,
                                 struct wp_image_description_info_v1 *info,
                                 uint32_t max_fall)
{
  ImageDescription *desc = data;

  desc->target_max_fall = max_fall;
  desc->has_target_max_fall = 1;
}

static struct wp_image_description_info_v1_listener info_listener = {
  image_desc_info_done,
  image_desc_info_icc_file,
  image_desc_info_primaries,
  image_desc_info_primaries_named,
  image_desc_info_tf_power,
  image_desc_info_tf_named,
  image_desc_info_luminances,
  image_desc_info_target_primaries,
  image_desc_info_target_luminance,
  image_desc_info_target_max_cll,
  image_desc_info_target_max_fall,
};

static void
image_desc_failed (void                           *data,
                   struct wp_image_description_v1 *image_desc,
                   uint32_t                        cause,
                   const char                     *msg)
{
  ImageDescription *desc = data;
  GdkWaylandColorSurface *self = desc->surface;

  g_assert (self->current_desc == desc);

  self->callback (self, GDK_COLOR_STATE_SRGB, self->data);

  gdk_wayland_color_surface_clear_image_desc (self);
}

static void
image_desc_ready (void                           *data,
                  struct wp_image_description_v1 *image_desc,
                  uint32_t                        identity)
{
  ImageDescription *desc = data;
  GdkWaylandColorSurface *self = desc->surface;
  GdkColorState *cs;

  g_assert (self->current_desc == desc);

  cs = g_hash_table_lookup (self->color->id_to_cs, GUINT_TO_POINTER (identity));
  if (cs)
    {
      self->callback (self, cs, self->data);

      gdk_wayland_color_surface_clear_image_desc (self);
      return;
    }

  desc->info = wp_image_description_v1_get_information (image_desc);
  desc->identity = identity;

  wp_image_description_info_v1_add_listener (desc->info, &info_listener, desc);
}

static const struct wp_image_description_v1_listener image_desc_listener = {
  image_desc_failed,
  image_desc_ready
};

static void
preferred_changed (void *data,
                   struct wp_color_management_surface_feedback_v1 *feedback,
                   uint32_t identity)
{
  GdkWaylandColorSurface *self = data;
  ImageDescription *desc;

  if (!self->callback)
    return;

  /* If there's still an ongoing query, cancel it. It's outdated. */
  gdk_wayland_color_surface_clear_image_desc (self);

  desc = g_new0 (ImageDescription, 1);

  desc->surface = self;
  self->current_desc = desc;

  desc->image_desc = wp_color_management_surface_feedback_v1_get_preferred_parametric (self->mgmt_feedback);

  wp_image_description_v1_add_listener (desc->image_desc, &image_desc_listener, desc);
}

static const struct wp_color_management_surface_feedback_v1_listener color_listener = {
  preferred_changed,
};

GdkWaylandColorSurface *
gdk_wayland_color_surface_new (GdkWaylandColor      *color,
                               struct wl_surface    *wl_surface,
                               GdkColorStateChanged  callback,
                               gpointer              data)
{
  GdkWaylandColorSurface *self;

  self = g_new0 (GdkWaylandColorSurface, 1);

  self->color = color;
  self->wl_surface = wl_surface;

  if (color->color_manager)
    {
      self->mgmt_surface = wp_color_manager_v1_get_surface (color->color_manager, wl_surface);
      self->mgmt_feedback = wp_color_manager_v1_get_surface_feedback (color->color_manager, wl_surface);

      self->callback = callback;
      self->data = data;

      wp_color_management_surface_feedback_v1_add_listener (self->mgmt_feedback, &color_listener, self);
      preferred_changed (self, self->mgmt_feedback, 0);
    }

  return self;
}

void
gdk_wayland_color_surface_free (GdkWaylandColorSurface *self)
{
  gdk_wayland_color_surface_clear_image_desc (self);

  g_clear_pointer (&self->mgmt_surface, wp_color_management_surface_v1_destroy);
  g_clear_pointer (&self->mgmt_feedback, wp_color_management_surface_feedback_v1_destroy);
  g_clear_pointer (&self->repr_surface, wp_color_representation_surface_v1_destroy);

  g_free (self);
}

static struct wp_image_description_v1 *
gdk_wayland_color_get_image_description (GdkWaylandColor *color,
                                         GdkColorState   *cs)
{
  gpointer result;

  if (g_hash_table_lookup_extended (color->cs_to_desc, cs, NULL, &result))
    return result;

  create_image_desc (color, cs, TRUE);

  if (!g_hash_table_lookup_extended (color->cs_to_desc, cs, NULL, &result))
    {
      g_assert_not_reached ();
    }

  return result;
}

static gboolean
gdk_wayland_color_get_color_representation (GdkWaylandColor *color,
                                            GdkColorState   *cs,
                                            GdkMemoryFormat  format,
                                            uint32_t        *coefficients,
                                            uint32_t        *range,
                                            uint32_t        *alpha)
{
  GdkCicpParams *cicp = gdk_color_state_create_cicp_params (cs);
  uint32_t c, a;

  *coefficients = 0;
  *range = 0;
  *alpha = 0;

  c = cicp_to_wl_coefficients (gdk_cicp_params_get_matrix_coefficients (cicp));

  if (gdk_cicp_params_get_range (cicp) == GDK_CICP_RANGE_NARROW)
    {
      *range = WP_COLOR_REPRESENTATION_SURFACE_V1_RANGE_LIMITED;
      if (color->color_representation_supported.coefficients_limited & (1 << c))
        *coefficients = c;
      else
        return FALSE;
    }
  else
    {
      *range = WP_COLOR_REPRESENTATION_SURFACE_V1_RANGE_FULL;
      if (color->color_representation_supported.coefficients_full & (1 << c))
        *coefficients = c;
      else
        return FALSE;
    }

  a = gdk_alpha_to_wl_alpha (gdk_memory_format_alpha (format));
  if (color->color_representation_supported.alpha_modes & (1 << a))
    *alpha = a;
  else
    return FALSE;

  return TRUE;
}

void
gdk_wayland_color_surface_set_color_state (GdkWaylandColorSurface *self,
                                           GdkColorState          *cs,
                                           uint32_t                fourcc,
                                           gboolean                premultiplied)
{
  if (self->mgmt_surface)
    {
      struct wp_image_description_v1 *desc;

      desc = gdk_wayland_color_get_image_description (self->color, cs);

      g_assert (desc);

      GDK_DISPLAY_DEBUG (GDK_DISPLAY (self->color->display), MISC,
                         "Setting color state %s (fourcc %.4s, premul %d) on surface: image desc %p",
                         gdk_color_state_get_name (cs),
                         (char *) &fourcc,
                         premultiplied,
                         desc);

      wp_color_management_surface_v1_set_image_description (self->mgmt_surface,
                                                            desc,
                                                            WP_COLOR_MANAGER_V1_RENDER_INTENT_PERCEPTUAL);
    }

  if (self->color->color_representation_manager)
    {
      uint32_t coefficients, range, alpha;
      gboolean ret G_GNUC_UNUSED;
      GdkMemoryFormat format;
      gboolean is_yuv;

      ret = gdk_memory_format_find_by_dmabuf_fourcc (fourcc, premultiplied, &format, &is_yuv);
      g_assert (ret);

      ret = gdk_wayland_color_get_color_representation (self->color, cs, format, &coefficients, &range, &alpha);
      g_assert (ret);

      GDK_DISPLAY_DEBUG (GDK_DISPLAY (self->color->display), MISC,
                         "Setting color state %s (fourcc %.4s, premul %d)  on surface: coefficients: %u (%s), range: %u (%s), alpha %u (%s)",
                         gdk_color_state_get_name (cs),
                         (char *) &fourcc,
                         premultiplied,
                         coefficients, wl_coefficients_name (coefficients),
                         range, wl_range_name (range),
                         alpha, wl_alpha_name (alpha));

      if (!self->repr_surface)
        self->repr_surface= wp_color_representation_manager_v1_get_surface (self->color->color_representation_manager, self->wl_surface);

      wp_color_representation_surface_v1_set_coefficients_and_range (self->repr_surface, coefficients, range);
      wp_color_representation_surface_v1_set_alpha_mode (self->repr_surface, alpha);
    }
}

void
gdk_wayland_color_surface_unset_color_state (GdkWaylandColorSurface *self)
{
  if (self->mgmt_surface)
    wp_color_management_surface_v1_unset_image_description (self->mgmt_surface);

  g_clear_pointer (&self->repr_surface, wp_color_representation_surface_v1_destroy);
}

gboolean
gdk_wayland_color_surface_can_set_color_state (GdkWaylandColorSurface  *self,
                                               GdkColorState           *cs,
                                               uint32_t                 fourcc,
                                               gboolean                 premultiplied,
                                               GError                 **error)
{
  GdkMemoryFormat format;
  gboolean is_yuv;
  GdkColorState *default_cs;

  if (!gdk_memory_format_find_by_dmabuf_fourcc (fourcc, premultiplied, &format, &is_yuv))
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED, "No memory format for fourcc");
      return FALSE;
    }

  default_cs = is_yuv ? GDK_COLOR_STATE_YUV : GDK_COLOR_STATE_SRGB;

  if (!self->mgmt_surface)
    {
      if (!gdk_color_state_equivalent (cs, default_cs))
        {
          g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED, "No color-management, non-default color state");
          return FALSE;
        }
    }
  else
    {
      if (!gdk_wayland_color_get_image_description (self->color, cs))
        {
          g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED, "No image description");
          return FALSE;
        }
    }

  if (!self->color->color_representation_manager)
    {
      if (!gdk_color_state_equivalent (cs, default_cs))
        {
          g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED, "No color-representation, non-default color state");
          return FALSE;
        }

      if (gdk_memory_format_alpha (format) == GDK_MEMORY_ALPHA_STRAIGHT)
        {
          g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED, "No color-representation, straight alpha");
          return FALSE;
        }
    }
  else
    {
      uint32_t coefficients, range, alpha;

      if (!gdk_wayland_color_get_color_representation (self->color, cs, format, &coefficients, &range, &alpha))
        {
          g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED, coefficients == 0 ? "Coefficients not supported"
                                                                                       : "Alpha not supported");
          return FALSE;
        }
    }

  return TRUE;
}
