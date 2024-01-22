#include "config.h"
#include "gskscaleprivate.h"
#include "gsktransform.h"

GskScale
gsk_scale_extract_from_transform (GskTransform *transform)
{
  switch (gsk_transform_get_category (transform))
    {
    default:
      g_assert_not_reached ();
      G_GNUC_FALLTHROUGH;

    case GSK_TRANSFORM_CATEGORY_IDENTITY:
    case GSK_TRANSFORM_CATEGORY_2D_TRANSLATE:
      return gsk_scale_init (1, 1);

    case GSK_TRANSFORM_CATEGORY_2D_AFFINE:
      {
        float scale_x, scale_y, dx, dy;
        gsk_transform_to_affine (transform, &scale_x, &scale_y, &dx, &dy);
        return gsk_scale_init (fabsf (scale_x), fabsf (scale_y));
      }

    case GSK_TRANSFORM_CATEGORY_2D:
      {
        float skew_x, skew_y, scale_x, scale_y, angle, dx, dy;
        gsk_transform_to_2d_components (transform,
                                        &skew_x, &skew_y,
                                        &scale_x, &scale_y,
                                        &angle,
                                        &dx, &dy);
        return gsk_scale_init (fabsf (scale_x), fabsf (scale_y));
      }

    case GSK_TRANSFORM_CATEGORY_UNKNOWN:
    case GSK_TRANSFORM_CATEGORY_ANY:
    case GSK_TRANSFORM_CATEGORY_3D:
      {
        graphene_quaternion_t rotation;
        graphene_matrix_t matrix;
        graphene_vec4_t perspective;
        graphene_vec3_t translation;
        graphene_vec3_t matrix_scale;
        graphene_vec3_t shear;

        gsk_transform_to_matrix (transform, &matrix);
        graphene_matrix_decompose (&matrix,
                                   &translation,
                                   &matrix_scale,
                                   &rotation,
                                   &shear,
                                   &perspective);

        return gsk_scale_init (fabsf (graphene_vec3_get_x (&matrix_scale)),
                               fabsf (graphene_vec3_get_y (&matrix_scale)));
      }
    }
}
