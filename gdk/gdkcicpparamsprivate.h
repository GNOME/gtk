#pragma once

#include "gdkcicpparams.h"

typedef struct _GdkCicp GdkCicp;

struct _GdkCicp
{
  guint color_primaries;
  guint transfer_function;
  guint matrix_coefficients;
  GdkCicpRange range;
};

/*< private >
 * gdk_cicp_equal:
 * @p1: a `GdkCicp`
 * @p2: another `GdkCicp`
 *
 * Compare two cicp tuples for equality.
 *
 * Note that several cicp values are 'functionally equivalent'.
 * If you are interested in that notion, use gdk_cicp_equivalent().
 *
 * Returns: whether @p1 and @p2 are equal
 */
static inline gboolean
gdk_cicp_equal (const GdkCicp *p1,
                const GdkCicp *p2)
{
  return p1->color_primaries == p2->color_primaries &&
         p1->transfer_function == p2->transfer_function &&
         p1->matrix_coefficients == p2->matrix_coefficients &&
         p1->range == p2->range;
}

static inline void
gdk_cicp_normalize (const GdkCicp *orig,
                    GdkCicp       *out)
{
  memcpy (out, orig, sizeof (GdkCicp));

  /* ntsc */
  if (out->color_primaries == 6)
    out->color_primaries = 5;

  /* bt709 */
  if (out->transfer_function == 6 ||
      out->transfer_function == 14 ||
      out->transfer_function == 15)
    out->transfer_function = 1;

  /* bt601 */
  if (out->matrix_coefficients == 6)
    out->matrix_coefficients = 5;
}

/*< private >
 * gdk_cicp_equivalent:
 * @p1: a `GdkCicp`
 * @p2: another `GdkCicp`
 *
 * Determine whether two cicp tuples are functionally equivalent.
 *
 * Returns: whether @p1 and @p2 are functionally equivalent
 */
static inline gboolean
gdk_cicp_equivalent (const GdkCicp *p1,
                     const GdkCicp *p2)
{
  GdkCicp n1, n2;

  if (gdk_cicp_equal (p1, p2))
    return TRUE;

  gdk_cicp_normalize (p1, &n1);
  gdk_cicp_normalize (p2, &n2);

  return gdk_cicp_equal (&n1, &n2);
}

const GdkCicp * gdk_cicp_params_get_cicp (GdkCicpParams *params);

GdkCicpParams * gdk_cicp_params_new_for_cicp (const GdkCicp  *cicp);
