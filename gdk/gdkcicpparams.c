/* gdkcicpparams.c
 *
 * Copyright 2024 Matthias Clasen
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gdkcicpparamsprivate.h"
#include "gdkcolorstateprivate.h"
#include "gdkenumtypes.h"

/**
 * GdkCicpParams:
 *
 * The `GdkCicpParams` struct contains the parameters that define
 * a colorstate according to the ITU-T H.273
 * [specification](https://www.itu.int/rec/T-REC-H.273/en).
 *
 * See the documentation of individual properties for supported values.
 *
 * The 'unspecified' value (2) is not treated in any special way, and
 * must be replaced by a different value before creating a color state.
 *
 * `GdkCicpParams` can be used as a builder object to construct a color
 * state from Cicp data with [method@Gdk.CicpParams.build_color_state].
 * The function will return an error if the given parameters are not
 * supported.
 *
 * You can obtain a `GdkCicpParams` object from a color state with
 * [method@Gdk.ColorState.create_cicp_params]. This can be used to
 * create a variant of a color state, by changing just one of the cicp
 * parameters, or just to obtain information about the color state.
 *
 * Since: 4.16
 */

/* {{{ GObject boilerplate */

struct _GdkCicpParams
{
  GObject parent_instance;

  GdkCicp cicp;
};

struct _GdkCicpParamsClass
{
  GObjectClass parent_class;
};

G_DEFINE_TYPE (GdkCicpParams, gdk_cicp_params, G_TYPE_OBJECT)

enum
{
  PROP_COLOR_PRIMARIES = 1,
  PROP_TRANSFER_FUNCTION,
  PROP_MATRIX_COEFFICIENTS,
  PROP_RANGE,

  N_PROPERTIES,
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

static void
gdk_cicp_params_init (GdkCicpParams *self)
{
  self->cicp.color_primaries = 2;
  self->cicp.transfer_function = 2;
  self->cicp.matrix_coefficients = 2;
  self->cicp.range = GDK_CICP_RANGE_NARROW;
}

static void
gdk_cicp_params_get_property (GObject    *object,
                              guint       property_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  GdkCicpParams *self = GDK_CICP_PARAMS (object);

  switch (property_id)
    {
    case PROP_COLOR_PRIMARIES:
      g_value_set_uint (value, self->cicp.color_primaries);
      break;

    case PROP_TRANSFER_FUNCTION:
      g_value_set_uint (value, self->cicp.transfer_function);
      break;

    case PROP_MATRIX_COEFFICIENTS:
      g_value_set_uint (value, self->cicp.matrix_coefficients);
      break;

    case PROP_RANGE:
      g_value_set_enum (value, self->cicp.range);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gdk_cicp_params_set_property (GObject      *object,
                              guint         property_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  GdkCicpParams *self = GDK_CICP_PARAMS (object);

  switch (property_id)
    {
    case PROP_COLOR_PRIMARIES:
      gdk_cicp_params_set_color_primaries (self, g_value_get_uint (value));
      break;

    case PROP_TRANSFER_FUNCTION:
      gdk_cicp_params_set_transfer_function (self, g_value_get_uint (value));
      break;

    case PROP_MATRIX_COEFFICIENTS:
      gdk_cicp_params_set_matrix_coefficients (self, g_value_get_uint (value));
      break;

    case PROP_RANGE:
      gdk_cicp_params_set_range (self, g_value_get_enum (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gdk_cicp_params_class_init (GdkCicpParamsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = gdk_cicp_params_get_property;
  object_class->set_property = gdk_cicp_params_set_property;

  /**
   * GdkCicpParams:color-primaries:
   *
   * The color primaries to use.
   *
   * Supported values:
   *
   * - 1: BT.709 / sRGB
   * - 2: unspecified
   * - 5: PAL
   * - 6,7: BT.601 / NTSC
   * - 9: BT.2020
   * - 12: Display P3
   *
   * Since: 4.16
   */
  properties[PROP_COLOR_PRIMARIES] =
    g_param_spec_uint ("color-primaries", NULL, NULL,
                       0, 255, 2,
                       G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GdkCicpParams:transfer-function:
   *
   * The transfer function to use.
   *
   * Supported values:
   *
   * - 1,6,14,15: BT.709, BT.601, BT.2020
   * - 2: unspecified
   * - 4: gamma 2.2
   * - 5: gamma 2.8
   * - 8: linear
   * - 13: sRGB
   * - 16: BT.2100 PQ
   * - 18: BT.2100 HLG
   *
   * Since: 4.16
   */
  properties[PROP_TRANSFER_FUNCTION] =
    g_param_spec_uint ("transfer-function", NULL, NULL,
                       0, 255, 2,
                       G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GdkCicpParams:matrix-coefficients:
   *
   * The matrix coefficients (for YUV to RGB conversion).
   *
   * Supported values:
   *
   * - 0: RGB
   * - 2: unspecified
   *
   * Since: 4.16
   */
  properties[PROP_MATRIX_COEFFICIENTS] =
    g_param_spec_uint ("matrix-coefficients", NULL, NULL,
                       0, 255, 2,
                       G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GdkCicpParams:range:
   *
   * Whether the data is using the full range of values.
   *
   * The range of the data.
   *
   * Since: 4.16
   */
  properties[PROP_RANGE] =
    g_param_spec_enum ("range", NULL, NULL,
                       GDK_TYPE_CICP_RANGE,
                       GDK_CICP_RANGE_NARROW,
                       G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

/* }}} */
/* {{{ Public API */

/**
 * gdk_cicp_params_new:
 *
 * Creates a new `GdkCicpParams` object.
 *
 * The initial values of the properties are the values for "undefined"
 * and need to be set before a color state object can be built.
 *
 * Returns: (transfer full): a new `GdkCicpParams`
 *
 * Since: 4.16
 */
GdkCicpParams  *
gdk_cicp_params_new (void)
{
  return g_object_new (GDK_TYPE_CICP_PARAMS, NULL);
}

/**
 * gdk_cicp_params_get_color_primaries:
 * @self: a `GdkCicpParams`
 *
 * Returns the value of the color-primaries property
 * of @self.
 *
 * Returns: the color-primaries value
 *
 * Since: 4.16
 */
guint
gdk_cicp_params_get_color_primaries (GdkCicpParams *self)
{
  g_return_val_if_fail (GDK_IS_CICP_PARAMS (self), 0);

  return self->cicp.color_primaries;
}

/**
 * gdk_cicp_params_set_color_primaries:
 * @self: a `GdkCicpParams`
 * @color_primaries: the new color primaries value
 *
 * Sets the color-primaries property of @self.
 *
 * Since: 4.16
 */
void
gdk_cicp_params_set_color_primaries (GdkCicpParams *self,
                                     guint          color_primaries)
{
  g_return_if_fail (GDK_IS_CICP_PARAMS (self));

  if (self->cicp.color_primaries == color_primaries)
    return;

  self->cicp.color_primaries = color_primaries;
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_COLOR_PRIMARIES]);
}

/**
 * gdk_cicp_params_get_transfer_function:
 * @self: a `GdkCicpParams`
 *
 * Gets the transfer-function property of @self.
 *
 * Returns: the transfer-function value
 *
 * Since: 4.16
 */
guint
gdk_cicp_params_get_transfer_function (GdkCicpParams *self)
{
  g_return_val_if_fail (GDK_IS_CICP_PARAMS (self), 0);

  return self->cicp.transfer_function;
}

/**
 * gdk_cicp_params_set_transfer_function:
 * @self: a `GdkCicpParams`
 * @transfer_function: the new transfer-function value
 *
 * Sets the transfer-function property of @self.
 *
 * Since: 4.16
 */
void
gdk_cicp_params_set_transfer_function (GdkCicpParams *self,
                                       guint          transfer_function)
{
  g_return_if_fail (GDK_IS_CICP_PARAMS (self));

  if (self->cicp.transfer_function == transfer_function)
    return;

  self->cicp.transfer_function = transfer_function;
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TRANSFER_FUNCTION]);
}

/**
 * gdk_cicp_params_get_matrix_coefficients:
 * @self: a `GdkCicpParams`
 *
 * Gets the matrix-coefficients property of @self.
 *
 * Returns: the matrix-coefficients value
 *
 * Since: 4.16
 */
guint
gdk_cicp_params_get_matrix_coefficients (GdkCicpParams *self)
{
  g_return_val_if_fail (GDK_IS_CICP_PARAMS (self), 0);

  return self->cicp.matrix_coefficients;
}

/**
 * gdk_cicp_params_set_matrix_coefficients:
 * @self a `GdkCicpParams`
 * @matrix_coefficients: the new matrix-coefficients value
 *
 * Sets the matrix-coefficients property of @self.
 *
 * Since: 4.16
 */
void
gdk_cicp_params_set_matrix_coefficients (GdkCicpParams *self,
                                         guint          matrix_coefficients)
{
  g_return_if_fail (GDK_IS_CICP_PARAMS (self));

  if (self->cicp.matrix_coefficients == matrix_coefficients)
    return;

  self->cicp.matrix_coefficients = matrix_coefficients;
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MATRIX_COEFFICIENTS]);
}

/**
 * gdk_cicp_params_get_range:
 * @self: a `GdkCicpParams`
 *
 * Gets the range property of @self.
 *
 * Returns: the range value
 *
 * Since: 4.16
 */
GdkCicpRange
gdk_cicp_params_get_range (GdkCicpParams *self)
{
  g_return_val_if_fail (GDK_IS_CICP_PARAMS (self), GDK_CICP_RANGE_NARROW);

  return self->cicp.range;
}

/**
 * gdk_cicp_params_set_range:
 * @self: a `GdkCipParams`
 * @range: the range value
 *
 * Sets the range property of @self
 *
 * Since: 4.16
 */
void
gdk_cicp_params_set_range (GdkCicpParams *self,
                           GdkCicpRange   range)
{
  g_return_if_fail (GDK_IS_CICP_PARAMS (self));

  if (self->cicp.range == range)
    return;

  self->cicp.range = range;
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_RANGE]);
}

/**
 * gdk_cicp_params_build_color_state:
 * @self: a `GdkCicpParams`
 * @error: return location for errors
 *
 * Creates a new `GdkColorState` object for the cicp parameters in @self.
 *
 * Note that this may fail if the cicp parameters in @self are not
 * supported by GTK. In that case, `NULL` is returned, and @error is set
 * with an error message that can be presented to the user.
 *
 * Returns: (transfer full) (nullable): A newly allocated `GdkColorState`
 *
 * Since: 4.16
 */
GdkColorState *
gdk_cicp_params_build_color_state (GdkCicpParams  *self,
                                   GError        **error)
{
  return gdk_color_state_new_for_cicp (gdk_cicp_params_get_cicp (self), error);
}

/* }}} */
/* {{{ Private API */

/*< private >
 * gdk_cicp_params_new_for_cicp:
 * @cicp: a GdkCicp struct
 *
 * Create a `GdkCicpParams` from the values in @cicp.
 *
 * Returns: (transfer full): a new `GdkCicpParams` object
 */
GdkCicpParams *
gdk_cicp_params_new_for_cicp (const GdkCicp *cicp)
{
  return g_object_new (GDK_TYPE_CICP_PARAMS,
                       "color-primaries", cicp->color_primaries,
                       "transfer-function", cicp->transfer_function,
                       "matrix-coefficients", cicp->matrix_coefficients,
                       "range", cicp->range,
                       NULL);
}

/*< private >
 * gdk_cicp_params_get_cicp:
 * @self: a `GdkCicpParams` object
 *
 * Gets the `GdkCicp` struct of @self.
 *
 * Returns: (transfer none): a `GdkCicp` struct containing
 *     the values of @self
 */
const GdkCicp *
gdk_cicp_params_get_cicp (GdkCicpParams *self)
{
  return &self->cicp;
}

/* }}} */

/* vim:set foldmethod=marker: */
