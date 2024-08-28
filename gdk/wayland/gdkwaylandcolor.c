
#include "gdkwaylandcolor-private.h"
#include "gdksurface-wayland-private.h"
#include <gdk/wayland/xx-color-management-v4-client-protocol.h>

typedef struct _ImageDescription ImageDescription;

static const uint primaries_map[] = {
  [XX_COLOR_MANAGER_V4_PRIMARIES_SRGB] = 1,
  [XX_COLOR_MANAGER_V4_PRIMARIES_PAL_M] = 4,
  [XX_COLOR_MANAGER_V4_PRIMARIES_PAL] = 5,
  [XX_COLOR_MANAGER_V4_PRIMARIES_NTSC] = 6,
  [XX_COLOR_MANAGER_V4_PRIMARIES_GENERIC_FILM] = 8,
  [XX_COLOR_MANAGER_V4_PRIMARIES_BT2020] = 9,
  [XX_COLOR_MANAGER_V4_PRIMARIES_CIE1931_XYZ] = 10,
  [XX_COLOR_MANAGER_V4_PRIMARIES_DCI_P3] = 11,
  [XX_COLOR_MANAGER_V4_PRIMARIES_DISPLAY_P3] = 12,
  [XX_COLOR_MANAGER_V4_PRIMARIES_ADOBE_RGB] = 0,
};

static uint
wl_to_cicp_primaries (enum xx_color_manager_v4_primaries cp)
{
  return primaries_map[cp];
}

static enum xx_color_manager_v4_primaries
cicp_to_wl_primaries (uint cp)
{
  for (guint i = 0; i < G_N_ELEMENTS (primaries_map); i++)
    if (primaries_map[i] == cp)
       return (enum xx_color_manager_v4_primaries)i;

  return 0;
}

static const uint primaries_primaries[][8] = {
  [XX_COLOR_MANAGER_V4_PRIMARIES_SRGB] =         { 6400, 3300, 3000, 6000, 1500,  600, 3127, 3290 },
  [XX_COLOR_MANAGER_V4_PRIMARIES_PAL_M] =        { 6700, 3300, 2100, 7100, 1400,  800, 3100, 3160 },
  [XX_COLOR_MANAGER_V4_PRIMARIES_PAL] =          { 6400, 3300, 2900, 6000, 1500,  600, 3127, 3290 },
  [XX_COLOR_MANAGER_V4_PRIMARIES_NTSC] =         { 6300, 3400, 3100, 5950, 1550,  700, 3127, 3290 },
  [XX_COLOR_MANAGER_V4_PRIMARIES_GENERIC_FILM] = { 2430, 6920, 1450,  490, 6810, 3190, 3100, 3160 },
  [XX_COLOR_MANAGER_V4_PRIMARIES_BT2020] =       { 7080, 2920, 1700, 7970, 1310,  460, 3127, 3290 },
  [XX_COLOR_MANAGER_V4_PRIMARIES_CIE1931_XYZ] =  {10000,    0,    0,10000,    0,    0, 3333, 3333 }, 
  [XX_COLOR_MANAGER_V4_PRIMARIES_DCI_P3] =       { 6800, 3200, 2650, 6900, 1500,  600, 3140, 3510 },
  [XX_COLOR_MANAGER_V4_PRIMARIES_DISPLAY_P3] =   { 6800, 3200, 2650, 6900, 1500,  600, 3127, 3290 },
  [XX_COLOR_MANAGER_V4_PRIMARIES_ADOBE_RGB] =    { 6400, 3300, 2100, 7100, 1500,  600, 3127, 3290 },
};

static const uint *
wl_primaries_to_primaries (enum xx_color_manager_v4_primaries primaries)
{
  return primaries_primaries[primaries];
}

static gboolean
primaries_to_wl_primaries (const uint primaries[8],
                           enum xx_color_manager_v4_primaries *out_primaries)
{
  guint i, j;

  for (i = 0; i < G_N_ELEMENTS (primaries_primaries); i++)
    {
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
  [XX_COLOR_MANAGER_V4_TRANSFER_FUNCTION_BT709] = 1,
  [XX_COLOR_MANAGER_V4_TRANSFER_FUNCTION_GAMMA22] = 4,
  [XX_COLOR_MANAGER_V4_TRANSFER_FUNCTION_GAMMA28] = 5,
  [XX_COLOR_MANAGER_V4_TRANSFER_FUNCTION_ST240] = 7,
  [XX_COLOR_MANAGER_V4_TRANSFER_FUNCTION_LINEAR] = 8,
  [XX_COLOR_MANAGER_V4_TRANSFER_FUNCTION_LOG_100] = 9,
  [XX_COLOR_MANAGER_V4_TRANSFER_FUNCTION_LOG_316] = 10,
  [XX_COLOR_MANAGER_V4_TRANSFER_FUNCTION_XVYCC] = 11,
  [XX_COLOR_MANAGER_V4_TRANSFER_FUNCTION_BT1361] = 12,
  [XX_COLOR_MANAGER_V4_TRANSFER_FUNCTION_SRGB] = 13,
  [XX_COLOR_MANAGER_V4_TRANSFER_FUNCTION_EXT_SRGB] = 13,
  [XX_COLOR_MANAGER_V4_TRANSFER_FUNCTION_ST2084_PQ] = 16,
  [XX_COLOR_MANAGER_V4_TRANSFER_FUNCTION_ST428] = 17,
  [XX_COLOR_MANAGER_V4_TRANSFER_FUNCTION_HLG] = 18,
};

static uint
wl_to_cicp_transfer (enum xx_color_manager_v4_transfer_function tf)
{
  return transfer_map[tf];
}

static enum xx_color_manager_v4_transfer_function
cicp_to_wl_transfer (uint tf)
{
  for (guint i = 0; i < G_N_ELEMENTS (transfer_map); i++)
    if (transfer_map[i] == tf)
       return (enum xx_color_manager_v4_transfer_function)i;

  return 0;
}

struct _GdkWaylandColor
{
  GdkWaylandDisplay *display;

  struct xx_color_manager_v4 *color_manager;
  struct {
    unsigned int intents;
    unsigned int features;
    unsigned int transfers;
    unsigned int primaries;
  } color_manager_supported;

  GHashTable *cs_to_desc; /* GdkColorState => xx_image_description_v4 or NULL */
  GHashTable *id_to_cs; /* uint32 identifier => GdkColorState */
};

static guint
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
xx_color_manager_v4_supported_intent (void                       *data,
                                      struct xx_color_manager_v4 *xx_color_manager_v4,
                                      uint32_t                    render_intent)
{
  GdkWaylandColor *color = data;

  color->color_manager_supported.intents |= (1 << render_intent);
}

static void
xx_color_manager_v4_supported_feature (void                       *data,
                                       struct xx_color_manager_v4 *xx_color_manager_v4,
                                       uint32_t                    feature)
{
  GdkWaylandColor *color = data;

  color->color_manager_supported.features |= (1 << feature);
}

static void
xx_color_manager_v4_supported_tf_named (void                       *data,
                                        struct xx_color_manager_v4 *xx_color_manager_v4,
                                        uint32_t                    tf)
{
  GdkWaylandColor *color = data;

  color->color_manager_supported.transfers |= (1 << tf);
}

static void
xx_color_manager_v4_supported_primaries_named (void                       *data,
                                               struct xx_color_manager_v4 *xx_color_manager_v4,
                                               uint32_t                    primaries)
{
  GdkWaylandColor *color = data;

  color->color_manager_supported.primaries |= (1 << primaries);
}

static struct xx_color_manager_v4_listener color_manager_listener = {
  xx_color_manager_v4_supported_intent,
  xx_color_manager_v4_supported_feature,
  xx_color_manager_v4_supported_tf_named,
  xx_color_manager_v4_supported_primaries_named,
};

GdkWaylandColor *
gdk_wayland_color_new (GdkWaylandDisplay  *display,
                       struct wl_registry *registry,
                       uint32_t            id,
                       uint32_t            version)
{
  GdkWaylandColor *color;

  color = g_new0 (GdkWaylandColor, 1);

  color->display = display;
  color->cs_to_desc = g_hash_table_new_full (color_state_hash,
                                             color_state_equal,
                                             (GDestroyNotify) gdk_color_state_unref,
                                             (GDestroyNotify) xx_image_description_v4_destroy);
  color->id_to_cs = g_hash_table_new_full (g_direct_hash,
                                           g_direct_equal,
                                           NULL,
                                           (GDestroyNotify) gdk_color_state_unref);

  color->color_manager = wl_registry_bind (registry,
                                           id,
                                           &xx_color_manager_v4_interface,
                                           MIN (version, 2));

  xx_color_manager_v4_add_listener (color->color_manager,
                                    &color_manager_listener,
                                    color);

  return color;
}

void
gdk_wayland_color_free (GdkWaylandColor *color)
{
  g_clear_pointer (&color->color_manager, xx_color_manager_v4_destroy);

  g_hash_table_unref (color->cs_to_desc);
  g_hash_table_unref (color->id_to_cs);

  g_free (color);
}

struct wl_proxy *
gdk_wayland_color_get_color_manager (GdkWaylandColor *color)
{
  return (struct wl_proxy *) color->color_manager;
}

typedef struct _CsImageDescListenerData {
  GdkWaylandColor *color;
  GdkColorState   *color_state;
  gboolean         sync;
  gboolean         done;
} CsImageDescListenerData;

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
                      struct xx_image_description_v4 *desc,
                      uint32_t                        cause,
                      const char                     *msg)
{
  CsImageDescListenerData *csi = data;

  g_warning ("Failed to get one of the standard image descriptions: %s", msg);
  xx_image_description_v4_destroy (desc);

  g_hash_table_insert (csi->color->cs_to_desc,
                       gdk_color_state_ref (csi->color_state),
                       NULL);

  cs_image_listener_data_free (csi);
}

static void
cs_image_desc_ready (void                           *data,
                     struct xx_image_description_v4 *desc,
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

static struct xx_image_description_v4_listener cs_image_desc_listener = {
  cs_image_desc_failed,
  cs_image_desc_ready,
};

static void
create_image_desc (GdkWaylandColor *color,
                   GdkColorState   *cs,
                   gboolean         sync)
{
  CsImageDescListenerData data;
  struct xx_image_description_creator_params_v4 *creator;
  struct xx_image_description_v4 *desc;
  const GdkCicp *cicp;
  GdkCicp norm;
  uint32_t primaries, tf;

  cicp = gdk_color_state_get_cicp (cs);
  if (!cicp)
    {
      GDK_DEBUG (MISC, "Unsupported color state %s: Not a CICP colorstate",
                 gdk_color_state_get_name (cs));
      g_hash_table_insert (color->cs_to_desc, gdk_color_state_ref (cs), NULL);
      return;
    }
  
  gdk_cicp_normalize (cicp, &norm);
  primaries = cicp_to_wl_primaries (norm.color_primaries);
  tf = cicp_to_wl_transfer (norm.transfer_function);

  if (((color->color_manager_supported.primaries & (1 << primaries)) == 0 &&
       (color->color_manager_supported.features & (1 << XX_COLOR_MANAGER_V4_FEATURE_SET_PRIMARIES)) == 0) ||
      (color->color_manager_supported.transfers & (1 << tf)) == 0)
    {
      GDK_DEBUG (MISC, "Unsupported color state %s: Primaries or transfer function unsupported",
                 gdk_color_state_get_name (cs));
      g_hash_table_insert (color->cs_to_desc, gdk_color_state_ref (cs), NULL);
      return;
    }

  data.color = color;
  data.color_state = cs;
  data.sync = sync;
  data.done = FALSE;

  creator = xx_color_manager_v4_new_parametric_creator (color->color_manager);

  if (color->color_manager_supported.primaries & (1 << primaries))
    {
      xx_image_description_creator_params_v4_set_primaries_named (creator, primaries);
    }
  else
    {
      const uint *p = wl_primaries_to_primaries (primaries);
      xx_image_description_creator_params_v4_set_primaries (creator,
                                                            p[0], p[1],
                                                            p[2], p[3],
                                                            p[4], p[5],
                                                            p[6], p[7]);
    }
  xx_image_description_creator_params_v4_set_tf_named (creator, tf);

  desc = xx_image_description_creator_params_v4_create (creator);

  if (sync)
    {
      struct wl_event_queue *event_queue;
      
      event_queue = wl_display_create_queue (color->display->wl_display);
      wl_proxy_set_queue ((struct wl_proxy *) desc, event_queue);
      xx_image_description_v4_add_listener (desc, &cs_image_desc_listener, &data);
      while (!data.done)
        gdk_wayland_display_dispatch_queue (GDK_DISPLAY (color->display), event_queue);

      wl_event_queue_destroy (event_queue);
    }
  else
    {
      xx_image_description_v4_add_listener (desc, &cs_image_desc_listener, g_memdup (&data, sizeof data));
    }
}

gboolean
gdk_wayland_color_prepare (GdkWaylandColor *color)
{
  if (color->color_manager)
    {
      const char *intents[] = {
        "perceptual", "relative", "saturation", "absolute", "relative-bpc"
      };
      const char *features[] = {
        "icc-v4-v4", "parametric", "set-primaries", "set-tf-power",
        "set-mastering-display-primaries", "extended-target-volume"
      };
      const char *tf[] = {
        "bt709", "gamma22", "gamma28", "st240", "linear", "log100",
        "log316", "xvycc", "bt1361", "srgb", "ext-srgb", "pq", "st428", "hlg"
      };
      const char *primaries[] = {
        "srgb", "pal-m", "pal", "ntsc", "generic-film", "bt2020", "xyz",
        "dci-p3", "display-p3", "adobe-rgb",
      };

     for (int i = 0; i < G_N_ELEMENTS (intents); i++)
        {
          GDK_DEBUG (MISC, "Rendering intent %d (%s): %s",
                   i, intents[i], color->color_manager_supported.intents & (1 << i) ? "✓" : "✗");
        }

      for (int i = 0; i < G_N_ELEMENTS (features); i++)
        {
          GDK_DEBUG (MISC, "Feature %d (%s): %s",
                   i, features[i], color->color_manager_supported.features & (1 << i) ? "✓" : "✗");
        }

      for (int i = 0; i < G_N_ELEMENTS (tf); i++)
        {
          GDK_DEBUG (MISC, "Transfer function %d (%s): %s",
                   i, tf[i], color->color_manager_supported.transfers & (1 << i) ? "✓" : "✗");
        }

      for (int i = 0; i < G_N_ELEMENTS (primaries); i++)
        {
          GDK_DEBUG (MISC, "Primaries %d (%s): %s",
                   i, primaries[i], color->color_manager_supported.primaries& (1 << i) ? "✓" : "✗");
        }
    }

  if (color->color_manager &&
      !(color->color_manager_supported.intents & (1 << XX_COLOR_MANAGER_V4_RENDER_INTENT_PERCEPTUAL)))
    {
      GDK_DEBUG (MISC, "Not using color management: Missing perceptual render intent");
      g_clear_pointer (&color->color_manager, xx_color_manager_v4_destroy);
    }

  if (color->color_manager &&
      (!(color->color_manager_supported.features & (1 << XX_COLOR_MANAGER_V4_FEATURE_PARAMETRIC)) ||
       !(color->color_manager_supported.transfers & (1 << XX_COLOR_MANAGER_V4_TRANSFER_FUNCTION_SRGB)) ||
       !((color->color_manager_supported.primaries & (1 << XX_COLOR_MANAGER_V4_PRIMARIES_SRGB)) ||
         (color->color_manager_supported.features & (1 << XX_COLOR_MANAGER_V4_FEATURE_SET_PRIMARIES)))))

    {
      GDK_DEBUG (MISC, "Not using color management: Can't create srgb image description");
      g_clear_pointer (&color->color_manager, xx_color_manager_v4_destroy);
    }

  if (color->color_manager)
    {
      create_image_desc (color, GDK_COLOR_STATE_SRGB, FALSE);

      if (color->color_manager_supported.transfers & (1 << XX_COLOR_MANAGER_V4_TRANSFER_FUNCTION_LINEAR))
        create_image_desc (color, GDK_COLOR_STATE_SRGB_LINEAR, FALSE);

      if ((color->color_manager_supported.primaries & (1 << XX_COLOR_MANAGER_V4_PRIMARIES_BT2020) ||
          (color->color_manager_supported.features & (1 << XX_COLOR_MANAGER_V4_FEATURE_SET_PRIMARIES))))
        {
          if (color->color_manager_supported.transfers & (1 << XX_COLOR_MANAGER_V4_TRANSFER_FUNCTION_ST2084_PQ))
            create_image_desc (color, GDK_COLOR_STATE_REC2100_PQ, FALSE);

          if (color->color_manager_supported.transfers & (1 << XX_COLOR_MANAGER_V4_TRANSFER_FUNCTION_LINEAR))
            create_image_desc (color, GDK_COLOR_STATE_REC2100_LINEAR, FALSE);
        }
    }

  return color->color_manager != NULL;
}

struct _GdkWaylandColorSurface
{
  GdkWaylandColor *color;
  struct xx_color_management_surface_v4 *surface;
  struct xx_color_management_feedback_surface_v4 *feedback;
  ImageDescription *current_desc;
  GdkColorStateChanged callback;
  gpointer data;
};

struct _ImageDescription
{
  GdkWaylandColorSurface *surface;

  struct xx_image_description_v4 *image_desc;
  struct xx_image_description_info_v4 *info;

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

  g_clear_pointer (&desc->image_desc, xx_image_description_v4_destroy);
  g_clear_pointer (&desc->info, xx_image_description_info_v4_destroy);
  g_free (desc);

  self->current_desc = NULL;
}

static void
image_desc_info_done (void *data,
                      struct xx_image_description_info_v4 *info)
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
      g_clear_pointer (&desc->image_desc, xx_image_description_v4_destroy);
    }

  if (self->callback)
    self->callback (desc->surface, cs, self->data);

  gdk_color_state_unref (cs);

  gdk_wayland_color_surface_clear_image_desc (self);
}

static void
image_desc_info_icc_file (void *data,
                          struct xx_image_description_info_v4 *info,
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
                           struct xx_image_description_info_v4 *info,
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
                                 struct xx_image_description_info_v4 *info,
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
                          struct xx_image_description_info_v4 *info,
                          uint32_t tf_power)
{
  ImageDescription *desc = data;

  desc->tf_power = tf_power;
  desc->has_tf_power = 1;
}

static void
image_desc_info_tf_named (void *data,
                          struct xx_image_description_info_v4 *info,
                          uint32_t tf)
{
  ImageDescription *desc = data;

  desc->tf_named = tf;
  desc->has_tf_named = 1;
}

static void
image_desc_info_luminances (void *data,
                            struct xx_image_description_info_v4 *info,
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
                                  struct xx_image_description_info_v4 *info,
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
                                  struct xx_image_description_info_v4 *info,
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
                                struct xx_image_description_info_v4 *info,
                                uint32_t max_cll)
{
  ImageDescription *desc = data;

  desc->target_max_cll = max_cll;
  desc->has_target_max_cll = 1;
}

static void
image_desc_info_target_max_fall (void *data,
                                 struct xx_image_description_info_v4 *info,
                                 uint32_t max_fall)
{
  ImageDescription *desc = data;

  desc->target_max_fall = max_fall;
  desc->has_target_max_fall = 1;
}

static struct xx_image_description_info_v4_listener info_listener = {
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
                   struct xx_image_description_v4 *image_desc,
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
                  struct xx_image_description_v4 *image_desc,
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

  desc->info = xx_image_description_v4_get_information (image_desc);
  desc->identity = identity;

  xx_image_description_info_v4_add_listener (desc->info, &info_listener, desc);
}

static const struct xx_image_description_v4_listener image_desc_listener = {
  image_desc_failed,
  image_desc_ready
};

static void
preferred_changed (void *data,
                   struct xx_color_management_feedback_surface_v4 *feedback)
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

  desc->image_desc = xx_color_management_feedback_surface_v4_get_preferred (self->feedback);

  xx_image_description_v4_add_listener (desc->image_desc, &image_desc_listener, desc);
}

static const struct xx_color_management_feedback_surface_v4_listener color_listener = {
  preferred_changed,
};

GdkWaylandColorSurface *
gdk_wayland_color_surface_new (GdkWaylandColor          *color,
                               struct wl_surface        *wl_surface,
                               GdkColorStateChanged      callback,
                               gpointer                  data)
{
  GdkWaylandColorSurface *self;

  self = g_new0 (GdkWaylandColorSurface, 1);

  self->color = color;

  self->surface = xx_color_manager_v4_get_surface (color->color_manager, wl_surface);
  self->feedback = xx_color_manager_v4_get_feedback_surface (color->color_manager, wl_surface);

  self->callback = callback;
  self->data = data;

  xx_color_management_feedback_surface_v4_add_listener (self->feedback, &color_listener, self);
  preferred_changed (self, self->feedback);

  return self;
}

void
gdk_wayland_color_surface_free (GdkWaylandColorSurface *self)
{
  gdk_wayland_color_surface_clear_image_desc (self);

  xx_color_management_surface_v4_destroy (self->surface);
  xx_color_management_feedback_surface_v4_destroy (self->feedback);

  g_free (self);
}

static struct xx_image_description_v4 *
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

void
gdk_wayland_color_surface_set_color_state (GdkWaylandColorSurface *self,
                                           GdkColorState          *cs)
{
  struct xx_image_description_v4 *desc;

  desc = gdk_wayland_color_get_image_description (self->color, cs);

  if (desc)
    xx_color_management_surface_v4_set_image_description (self->surface,
                                                          desc,
                                                          XX_COLOR_MANAGER_V4_RENDER_INTENT_PERCEPTUAL);
  else
    xx_color_management_surface_v4_unset_image_description (self->surface);
}

gboolean
gdk_wayland_color_surface_can_set_color_state (GdkWaylandColorSurface *self,
                                               GdkColorState          *cs)
{
  return gdk_wayland_color_get_image_description (self->color, cs) != NULL;
}
