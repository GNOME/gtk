#include "config.h"

#include "gskgpuutilsprivate.h"

#include "gdk/gdkcolorstateprivate.h"

GskGpuConversion
gsk_gpu_color_state_get_conversion (GdkColorState *color_state)
{
  const GdkCicp *cicp;

  if (gdk_color_state_get_no_srgb_tf (color_state))
    return GSK_GPU_CONVERSION_SRGB;

  cicp = gdk_color_state_get_cicp (color_state);
  if (cicp != NULL)
    {
      switch (cicp->matrix_coefficients)
        {
        case 1:
          return cicp->range == GDK_CICP_RANGE_NARROW ? GSK_GPU_CONVERSION_BT709_NARROW
                                                      : GSK_GPU_CONVERSION_BT709;
        case 5:
        case 6:
          return cicp->range == GDK_CICP_RANGE_NARROW ? GSK_GPU_CONVERSION_BT601_NARROW
                                                      : GSK_GPU_CONVERSION_BT601;
        case 9:
          return cicp->range == GDK_CICP_RANGE_NARROW ? GSK_GPU_CONVERSION_BT2020_NARROW
                                                      : GSK_GPU_CONVERSION_BT2020;
        default:
          break;
        }
    }

  return GSK_GPU_CONVERSION_NONE;
}

static gboolean
gsk_gpu_cicp_apply_conversion (const GdkCicp    *cicp,
                               GskGpuConversion  conversion,
                               GdkCicp          *result)
{
  switch (conversion)
    {
    case GSK_GPU_CONVERSION_NONE:
    case GSK_GPU_CONVERSION_SRGB:
      return FALSE;

    case GSK_GPU_CONVERSION_BT601:
      if ((cicp->matrix_coefficients != 5 &&
           cicp->matrix_coefficients != 6) ||
          cicp->range != GDK_CICP_RANGE_FULL)
        return FALSE;
      *result = *cicp;
      result->matrix_coefficients = 0;
      return TRUE;

    case GSK_GPU_CONVERSION_BT601_NARROW:
      if ((cicp->matrix_coefficients != 5 &&
           cicp->matrix_coefficients != 6) ||
          cicp->range != GDK_CICP_RANGE_NARROW)
        return FALSE;
      *result = *cicp;
      result->matrix_coefficients = 0;
      result->range = GDK_CICP_RANGE_FULL;
      return TRUE;

    case GSK_GPU_CONVERSION_BT709:
      if (cicp->matrix_coefficients != 1 ||
          cicp->range != GDK_CICP_RANGE_FULL)
        return FALSE;
      *result = *cicp;
      result->matrix_coefficients = 0;
      return TRUE;

    case GSK_GPU_CONVERSION_BT709_NARROW:
      if (cicp->matrix_coefficients != 1 ||
          cicp->range != GDK_CICP_RANGE_NARROW)
        return FALSE;
      *result = *cicp;
      result->matrix_coefficients = 0;
      result->range = GDK_CICP_RANGE_FULL;
      return TRUE;

    case GSK_GPU_CONVERSION_BT2020:
      if (cicp->matrix_coefficients != 9 ||
          cicp->range != GDK_CICP_RANGE_FULL)
        return FALSE;
      *result = *cicp;
      result->matrix_coefficients = 0;
      return TRUE;

    case GSK_GPU_CONVERSION_BT2020_NARROW:
      if (cicp->matrix_coefficients != 9 ||
          cicp->range != GDK_CICP_RANGE_NARROW)
        return FALSE;
      *result = *cicp;
      result->matrix_coefficients = 0;
      result->range = GDK_CICP_RANGE_FULL;
      return TRUE;

    default:
      g_assert_not_reached ();
      return FALSE;
  }
}

/**
 * gsk_gpu_color_state_apply_conversion:
 * @color_state: a color state
 * @conversion: the conversion to apply to the color state
 *
 * Applies the conversion to the color state and returns the result.
 *
 * If the conversion is not possible with the given color state, then NULL is
 * returned.
 *
 * Returns: (transfer full) (nullable): The new color state after the conversion
 **/
GdkColorState *
gsk_gpu_color_state_apply_conversion (GdkColorState    *color_state,
                                      GskGpuConversion  conversion)
{
  GdkColorState *result;

  switch (conversion)
    {
    case GSK_GPU_CONVERSION_NONE:
      return gdk_color_state_ref (color_state);

    case GSK_GPU_CONVERSION_SRGB:
      result = gdk_color_state_get_no_srgb_tf (color_state);
      if (result == NULL)
        return NULL;
      return gdk_color_state_ref (result);

    case GSK_GPU_CONVERSION_BT601:
    case GSK_GPU_CONVERSION_BT601_NARROW:
    case GSK_GPU_CONVERSION_BT709:
    case GSK_GPU_CONVERSION_BT709_NARROW:
    case GSK_GPU_CONVERSION_BT2020:
    case GSK_GPU_CONVERSION_BT2020_NARROW:
      {
        const GdkCicp *cicp;
        GdkCicp converted;

        cicp = gdk_color_state_get_cicp (color_state);
        if (cicp == NULL)
          return NULL;

        if (!gsk_gpu_cicp_apply_conversion (cicp, conversion, &converted))
          return NULL;

        return gdk_color_state_new_for_cicp (&converted, NULL);
      }

    default:
      g_assert_not_reached ();
      return NULL;
  }
}
