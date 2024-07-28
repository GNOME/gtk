#include <gdk/gdk.h>
#include "gdkcolorstateprivate.h"
#include <math.h>

#include "gdkcolordefs.h"

typedef float (* TransferFunc) (float v);

typedef struct
{
  const char *name;
  TransferFunc oetf;
  TransferFunc eotf;
} TransferTest;

TransferTest transfers[] = {
  { "srgb", srgb_oetf, srgb_eotf },
  { "pq", pq_oetf, pq_eotf },
  { "bt709", bt709_oetf, bt709_eotf },
  { "hlg", hlg_oetf, hlg_eotf },
  { "gamma22", gamma22_oetf, gamma22_eotf },
  { "gamma28", gamma28_oetf, gamma28_eotf },
};

static void
test_transfer (gconstpointer data)
{
  TransferTest *transfer = (TransferTest *) data;

  for (int i = 0; i < 1000; i++)
    {
      float v = i / 1000.0;
      float v2 = transfer->oetf (transfer->eotf (v));
      g_assert_cmpfloat_with_epsilon (v, v2, 0.05);
    }
}

typedef struct
{
  const char *name;
  const float *to_xyz;
  const float *from_xyz;
} MatrixTest;
static MatrixTest matrices[] = {
  { "srgb", srgb_to_xyz, xyz_to_srgb },
  { "rec2020", rec2020_to_xyz, xyz_to_rec2020 },
  { "pal", pal_to_xyz, xyz_to_pal },
  { "ntsc", ntsc_to_xyz, xyz_to_ntsc },
  { "p3", p3_to_xyz, xyz_to_p3 },
  { "srgb<>rec2020", rec2020_to_srgb, srgb_to_rec2020 },
};

#define IDX(i,j) 3*i+j
static inline void
multiply (float       res[9],
          const float m1[9],
          const float m2[9])
{
  for (int i = 0; i < 3; i++)
    for (int j = 0; j < 3; j++)
      res[IDX(i,j)] =   m1[IDX(i,0)] * m2[IDX(0,j)]
                      + m1[IDX(i,1)] * m2[IDX(1,j)]
                      + m1[IDX(i,2)] * m2[IDX(2,j)];
}

static inline void
difference (float       res[9],
            const float m1[9],
            const float m2[9])
{
  for (int i = 0; i < 9; i++)
    res[i] = m1[i] - m2[i];
}

static float
norm (const float m[9])
{
  float sum = 0;

  for (int i = 0; i < 9; i++)
    sum += m[i] * m[i];

  return sqrtf (sum);
}

static void
print_matrix (const float m[9])
{
  g_print ("%f %f %f\n%f %f %f\n%f %f %f\n",
           m[0], m[1], m[2],
           m[3], m[4], m[5],
           m[6], m[7], m[8]);
}

static void
test_matrix (gconstpointer data)
{
  MatrixTest *matrix = (MatrixTest *) data;
  float res[9];
  float res2[9];

  multiply (res, matrix->to_xyz, matrix->from_xyz);

  if (g_test_verbose ())
    print_matrix (res);

  difference (res2, res, identity);

  if (g_test_verbose ())
    g_print ("distance: %f\n", norm (res2));

  g_assert_cmpfloat_with_epsilon (norm (res2), 0, 0.001);
}

static void
test_srgb_to_rec2020 (void)
{
  float m[9], res[9];

  multiply (m, xyz_to_rec2020, srgb_to_xyz);
  difference (res, m, srgb_to_rec2020);

  g_assert_cmpfloat_with_epsilon (norm (res), 0, 0.001);
}

static void
test_rec2020_to_srgb (void)
{
  float m[9], res[9];

  multiply (m, xyz_to_srgb, rec2020_to_xyz);
  difference (res, m, rec2020_to_srgb);

  g_assert_cmpfloat_with_epsilon (norm (res), 0, 0.001);
}

int
main (int argc, char *argv[])
{
  (g_test_init) (&argc, &argv, NULL);

  for (guint i = 0; i < G_N_ELEMENTS (transfers); i++)
    {
      TransferTest *test = &transfers[i];
      char *path = g_strdup_printf ("/colorstate/transfer/%s", test->name);
      g_test_add_data_func (path, test, test_transfer);
      g_free (path);
    }

  for (guint i = 0; i < G_N_ELEMENTS (matrices); i++)
    {
      MatrixTest *test = &matrices[i];
      char *path = g_strdup_printf ("/colorstate/matrix/%s", test->name);
      g_test_add_data_func (path, test, test_matrix);
      g_free (path);
    }

  g_test_add_func ("/colorstate/matrix/srgb_to_rec2020", test_srgb_to_rec2020);
  g_test_add_func ("/colorstate/matrix/rec2020_to_srgb", test_rec2020_to_srgb);

  return g_test_run ();
}
