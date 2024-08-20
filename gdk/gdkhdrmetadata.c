#include "config.h"

#include <math.h>
#include "gdkhdrmetadataprivate.h"
#include "gdkcolordefs.h"
#include "gdkcolorprivate.h"

#include <glib.h>
#include <graphene.h>

/**
 * GdkHdrMetadata:
 *
 * The `GdkHdrMetadata` struct contains information about the subset
 * of a color state that is used in a texture, or displayable by an
 * output device.
 *
 * The subset is specified in the same way that a color state's gamut
 * is defined: with chromaticity coordinates for r, g, b primaries and
 * a white point.
 *
 * In addition, `GdkHdrMetadata provides information about the minimum
 * and maximum luminance, as well as average light levels.
 *
 * In the context of video mastering, this data is commonly known as
 * 'HDR metadata' or as 'mastering display color volume'. The relevant
 * specifications for this are SMPTE ST 2086 and CEA-861.3, Appendix A.
 *
 * The information in this struct is used in gamut or tone mapping.
 *
 * Since: 4.16
 */

G_DEFINE_BOXED_TYPE (GdkHdrMetadata, gdk_hdr_metadata,
                     gdk_hdr_metadata_ref, gdk_hdr_metadata_unref);

/**
 * gdk_hdr_metadata_new:
 * @rx: Chromaticity coordinates for the red primary
 * @ry: Chromaticity coordinates for the red primary
 * @gx: Chromaticity coordinates for the green primary
 * @gy: Chromaticity coordinates for the green primary
 * @bx: Chromaticity coordinates for the blue primary
 * @by: Chromaticity coordinates for the blue primary
 * @wx: Chromaticity coordinates for the white point
 * @wy: Chromaticity coordinates for the white point
 * @min_lum: minimum luminance, in cd/m²
 * @max_lum: maximum luminance, in cd/m²
 * @max_cll: maximum content light level, in cd/m²
 * @max_fall: maximum average frame light level, in cd/m²
 *
 * Creates a new `GdkHdrMetadata` struct with the given data.
 *
 * Returns: a newly allocated `GdkHdrMetadata` struct
 *
 * Since: 4.16
 */
GdkHdrMetadata *
gdk_hdr_metadata_new (float rx, float ry,
                      float gx, float gy,
                      float bx, float by,
                      float wx, float wy,
                      float min_lum, float max_lum,
                      float max_cll, float max_fall)
{
  GdkHdrMetadata *self;

  self = g_new (GdkHdrMetadata, 1);

  self->rx = rx; self->ry = ry;
  self->gx = gx; self->gy = gy;
  self->bx = bx; self->by = by;
  self->wx = wx; self->wy = wy;
  self->min_lum = min_lum;
  self->max_lum = max_lum;
  self->max_cll = max_cll;
  self->max_fall = max_fall;

  return self;
}

/**
 * gdk_hdr_metadata_ref:
 * @self: a `GdkHdrMetadata`
 *
 * Increase the reference count of @self.
 *
 * Returns: @self
 *
 * Since: 4.16
 */
GdkHdrMetadata *
gdk_hdr_metadata_ref (GdkHdrMetadata *self)
{
  g_atomic_ref_count_inc (&self->ref_count);

  return self;
}

/**
 * gdk_hdr_metadata_unref:
 * @self: a `GdkHdrMetadata`
 *
 * Decreate the reference count of @self, and
 * free it if the reference count drops to zero.
 *
 * Since: 4.16
 */
void
gdk_hdr_metadata_unref (GdkHdrMetadata *self)
{
  if (g_atomic_ref_count_dec (&self->ref_count))
    g_free (self);
}

/**
 * gdk_hdr_metadata_equal:
 * @v1: a `GdkHdrMetadata`
 * @v2: another `GdkHdrMetadata`
 *
 * Returns whether @v1 and @v2 contain the same data.
 *
 * Returns: `TRUE` if @v1 and @v2 are equal
 *
 * Since: 4.16
 */
gboolean
gdk_hdr_metadata_equal (const GdkHdrMetadata *v1,
                        const GdkHdrMetadata *v2)
{
  if (v1 == v2)
    return TRUE;

  if (v1 == NULL || v2 == NULL)
    return FALSE;

  return v1->rx == v2->rx && v1->ry == v2->ry &&
         v1->gx == v2->gx && v1->gy == v2->gy &&
         v1->bx == v2->bx && v1->by == v2->by &&
         v1->wx == v2->wx && v1->wy == v2->wy &&
         v1->min_lum == v2->min_lum &&
         v1->max_lum == v2->max_lum &&
         v1->max_cll == v2->max_cll &&
         v1->max_fall == v2->max_fall;
}

static void
multiply (const float m[9],
          const float v[3],
          float       res[3])
{
  for (int i = 0; i < 3; i++)
    res[i] = m[3*i+0]*v[0] + m[3*i+1]*v[1] + m[3*i+2]*v[2];
}

static const float rec2020_to_lms[9] = {
  0.412109, 0.523926, 0.063965,
  0.166748, 0.720459, 0.112793,
  0.024170, 0.075439, 0.900391,
};

static const float lms_to_ictcp[9] = {
  0.500000, 0.500000, 0.000000,
  1.613770, -3.323486, 1.709717,
  4.378174, -4.245605, -0.132568,
};

static const float lms_to_rec2020[9] = {
  3.436607, -2.506452, 0.069845,
  -0.791330, 1.983600, -0.192271,
  -0.025950, -0.098914, 1.124864,
};

static const float ictcp_to_lms[9] = {
  1.000000, 0.008609, 0.111030,
  1.000000, -0.008609, -0.111030,
  1.000000, 0.560031, -0.320627,
};

static void
rec2100_linear_to_ictcp (float  in[4],
                         float out[4])
{
  float lms[3];

  multiply (rec2020_to_lms, in, lms);

  lms[0] = pq_oetf (lms[0]);
  lms[1] = pq_oetf (lms[1]);
  lms[2] = pq_oetf (lms[2]);

  multiply (lms_to_ictcp, lms, out);

  out[3] = in[3];
}

static void
ictcp_to_rec2100_linear (float  in[4],
                         float out[4])
{
  float lms[3];

  multiply (ictcp_to_lms, in, lms);

  lms[0] = pq_eotf (lms[0]);
  lms[1] = pq_eotf (lms[1]);
  lms[2] = pq_eotf (lms[2]);

  multiply (lms_to_rec2020, lms, out);

  out[3] = in[3];
}

/*< private >
 * gdk_color_map:
 * @src: the `GdkColor` to map
 * @src_metadata: (nullable): HDR metadata for @src
 * @target_metadata: HDR metadata to map to
 * @target_color_state: color state to return the result in
 * @out: return location for the result
 *
 * Maps a `GdkColor` to the color volume described
 * by @target_metadata.
 *
 * The resulting color will be in the @target_color_state.
 */
void
gdk_color_map (const GdkColor *src,
               GdkHdrMetadata *src_metadata,
               GdkColorState  *target_color_state,
               GdkHdrMetadata *target_metadata,
               GdkColor       *dest)
{
  float values[4];
  float ictcp[4];
  GdkColor tmp;
  float ref_lum = 203;
  float src_max_lum;
  float target_max_lum;
  float src_lum;
  float needed_range;
  float added_range;
  float new_ref_lum;
  float rel_highlight;
  float low;
  float high;
  float new_lum;

  src_max_lum = src_metadata->max_lum;
  target_max_lum = target_metadata->max_lum;

  if (src_max_lum <= target_max_lum * 1.01)
    {
      /* luminance is in range */
      if (gdk_color_state_equal (src->color_state, target_color_state))
        gdk_color_init_copy (dest, src); /* nothing to do */
      else
        gdk_color_convert (dest, target_color_state, src);
      return;
    }

  needed_range = src_max_lum / ref_lum;
  added_range = MIN (needed_range, 1.5);
  new_ref_lum = ref_lum / added_range;

  gdk_color_to_float (src, GDK_COLOR_STATE_REC2100_LINEAR, values);
  rec2100_linear_to_ictcp (values, ictcp);

  src_lum = pq_eotf (ictcp[0]) * 10000;

  low = MIN (src_lum / added_range, new_ref_lum);
  rel_highlight = CLAMP ((src_lum - new_ref_lum) / (src_max_lum - new_ref_lum), 0, 1);
  high = pow (rel_highlight, 0.5) * (target_max_lum - new_ref_lum);
  new_lum = low + high;

  ictcp[0] = pq_oetf (new_lum / 10000);

  ictcp_to_rec2100_linear (ictcp, values);
  gdk_color_init (&tmp, GDK_COLOR_STATE_REC2100_LINEAR, values);
  gdk_color_convert (dest, target_color_state, &tmp);
  gdk_color_finish (&tmp);
}
