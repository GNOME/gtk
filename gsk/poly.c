#include "poly.h"
#include <math.h>
#include <glib.h>

struct Poly
{
  GArray *c; /* c_0 + c_1*t + ... + c_n*t^n */
};

/* {{{ Debugging */

static void
poly_print (const Poly *p, GString *s)
{
  gboolean empty = TRUE;

  for (int i = 0; i < p->c->len; i++)
    {
      double c = g_array_index (p->c, double, i);
      if (c == 0)
        continue;
      if (!empty || c < 0)
        g_string_append_printf (s, " %s ", c < 0 ? "-" : (i > 0 ? "+" : ""));
      c = fabs (c);
      if (i > 1)
        g_string_append_printf (s, "%g*t^%d", c, i);
      else if (i == 1)
        g_string_append_printf (s, "%g*t", c);
      else
        g_string_append_printf (s, "%g", c);
      empty = FALSE;
    }
}

char *
poly_to_string (const Poly *p)
{
  GString *s = g_string_new ("");
  poly_print (p, s);
  return g_string_free (s, FALSE);
}

/* }}} */
/* {{{ Basics */

static gboolean
poly_equal (const Poly *p1, const Poly *p2)
{
  if (p1->c->len != p2->c->len)
    return FALSE;

  for (int i = 0; i < p1->c->len; i++)
    {
      double v1 = g_array_index (p1->c, double, i);
      double v2 = g_array_index (p2->c, double, i);
      if (v1 != v2)
        return FALSE;
    }

  return TRUE;
}

double
poly_eval (const Poly *p, double t)
{
  double val = 0;

  for (int i = p->c->len - 1; i >= 0; i--)
    {
      double c = g_array_index (p->c, double, i);
      val = val * t + c;
    }

  return val;
}

static void
clear_func (gpointer data)
{
  double *f = data;
  *f = 0.f;
}

static Poly *
poly_alloc (void)
{
  Poly *p = g_new (Poly, 1);
  p->c = g_array_new (FALSE, TRUE, sizeof (double));
  g_array_set_clear_func (p->c, clear_func);
  return p;
}

void
poly_free (Poly *p)
{
  g_array_unref (p->c);
  g_free (p);
}

static void
poly_clear (Poly *p)
{
  g_array_set_size (p->c, 0);
}

static void
poly_reduce (Poly *p)
{
  for (int i = p->c->len - 1; i >= 0; i--)
    {
      double v = g_array_index (p->c, double, i);
      if (v == 0)
        g_array_set_size (p->c, p->c->len - 1);
      else
        break;
    }
}

static void
poly_set (Poly *p, int n, double *c)
{
  poly_clear (p);
  g_array_append_vals (p->c, c, n);
  poly_reduce (p);
}

static void
poly_copy (const Poly *p, Poly *q)
{
  poly_set (q, p->c->len, (double *)p->c->data);
}

/* c*t^d */
static void
poly_set_monomial (Poly *p, int d, double c)
{
  g_array_set_size (p->c, d + 1);
  memset (p->c->data, 0, sizeof (double) * p->c->len);
  g_array_index (p->c, double, d) = c;
}

static int
poly_degree (const Poly *p)
{
  return p->c->len - 1;
}

Poly *
poly_new (int n, double *c)
{
  Poly *p = poly_alloc ();
  poly_set (p, n, c);
  return p;
}

static void
poly_add_to (Poly *p1, const Poly *p2)
{
  g_array_set_size (p1->c, MAX (p1->c->len, p2->c->len));

  for (int i = 0; i < p2->c->len; i++)
    {
      double *v1 = &g_array_index (p1->c, double, i);
      double v2 = g_array_index (p2->c, double, i);
      *v1 += v2;
    }

  poly_reduce (p1);
}

/* res may be the same as p1 or p2 */
static Poly *
poly_add (const Poly *p1, const Poly *p2, Poly *res)
{
  if (!res)
    res = poly_alloc ();

  if (res != p1)
    poly_copy (p1, res);

  poly_add_to (res, p2);

  return res;
}

static void
poly_subtract_from (Poly *p1, const Poly *p2)
{
  g_array_set_size (p1->c, MAX (p1->c->len, p2->c->len));

  for (int i = 0; i < p2->c->len; i++)
    {
      double *v1 = &g_array_index (p1->c, double, i);
      double v2 = g_array_index (p2->c, double, i);
      *v1 -= v2;
    }

  poly_reduce (p1);
}

static void
poly_add_constant (Poly *p, double c)
{
  double *v;

  g_array_set_size (p->c, MAX (p->c->len, 1));
  v = &g_array_index (p->c, double, 0);
  *v += c;
}

static void
poly_scale (Poly *p, double s)
{
  g_assert (s != 0);

  for (int i = 0; i < p->c->len; i++)
    {
      double *v = &g_array_index (p->c, double, i);
      *v *= s;
    }
}

static Poly *
poly_multiply (const Poly *p1, const Poly *p2, Poly *res)
{
  g_assert (res != p1 && res != p2);

  if (res)
    poly_clear (res);
  else
    res = poly_alloc ();

  g_array_set_size (res->c, p1->c->len + p2->c->len - 1);
  memset (res->c->data, 0, sizeof (double) * res->c->len);

  for (int i = 0; i < p1->c->len; i++)
    {
      double v1 = g_array_index (p1->c, double, i);
      if (v1 == 0)
        continue;
      for (int j = 0; j < p2->c->len; j++)
        {
          double v2 = g_array_index (p2->c, double, j);
          double *v = &g_array_index (res->c, double, i + j);
          *v += v1 * v2;
        }
    }

  poly_reduce (res);

  return res;
}

/* }}} */
/* {{{ Euclidean Algorithm */

/* Euclidean algorithm, for polynomials
 *
 * Afterwards, n = d*q + r
 */
static void
poly_divide (const Poly *n, const Poly *d, Poly *q, Poly *r)
{
  Poly *t = poly_alloc ();
  Poly *t2 = poly_alloc ();

  g_assert (n != q && n != r);
  g_assert (d != q && d != r);
  g_assert (q != r);

  poly_clear (q);
  poly_copy (n, r);

  g_array_set_size (q->c, (n->c->len - 1) - (d->c->len - 1) + 1);

  while (poly_degree (r) >= poly_degree (d))
    {
      double v1 = g_array_index (r->c, double, poly_degree (r));
      double v2 = g_array_index (d->c, double, poly_degree (d));
      poly_set_monomial (t, poly_degree (r) - poly_degree (d), v1 / v2);
      poly_add_to (q, t);
      poly_multiply (t, d, t2);
      poly_subtract_from (r, t2);
    }

  poly_free (t);
  poly_free (t2);
}

/* }}} */
/* {{{ Derivative */

static Poly *
poly_derive (const Poly *p)
{
  Poly *p1 = poly_alloc ();

  for (int i = 0; i + 1 < p->c->len; i++)
    {
      double v1 = g_array_index (p->c, double, i + 1);
      double v = v1 * (i + 1);
      g_array_append_val (p1->c, v);
    }

  return p1;
}

/* }}} */
/* {{{ Finding roots */
 /* {{{ Finding a root: Newton's Method */

static gboolean
find_root_newton (Poly *p, Poly *d, double min, double max, double *res)
{
  const int iterations = 1000;
  const double epsilon = 0.001;
  const double tolerance = 0.0001;

  double x = (max + min) / 2;
  gboolean found;

  found = FALSE;

  for (int i = 0; i < iterations; i++)
    {
      double y, y2, x1;

      y = poly_eval (p, x);
      y2 = poly_eval (d, x);

      if (fabs (y2) < epsilon)
        break;

      x1 = x - y / y2;

      if (x1 < min || x1 > max)
        return FALSE;

      if (fabs (x1 - x) < tolerance)
        {
          found = TRUE;
          *res = x1;
          break;
        }

      x = x1;
    }

  return found;
}

/* }}} */
/* {{{ Finding a root: Bisection */

static double
find_root_bisection (Poly *p, double min, double max, double ymin, double ymax)
{
  const double tolerance = 0.0001;
  const int iterations = 1000;
  double mid, y;

  for (int i = 0; i < iterations; i++)
    {
      mid = (max + min) / 2;
      y = poly_eval (p, mid);

      if (max - min < tolerance)
        return mid;

      if ((y < 0) == (ymin < 0))
        {
          min = mid;
          ymin = y;
        }
      else
        {
          max = mid;
          ymax = y;
        }
    }

  return mid;
}

/* }}} */
/* {{{ Isolating roots: Descartes' rule of signs */

typedef struct
{
  double min, max;
} Interval;

static void
compute_pi (const Poly *p, Interval *interval, GPtrArray *xis, Poly *pi)
{
  Poly *l, *li, *tmp;

  poly_clear (pi);

  /* Compute P_I = (x+1)^n P((ax+b)/(x+1))
   *
   * If P is c_0 + c_1x + ... c_nx^n,
   * then P_I = sum_0^n c_i (x+1)^(n-i) (ax+b)^i
   *          = sum     c_i xi(n-i)     li(i)
   *
   * We precompute the xi, and we build the li as we go.
   */

  l = poly_new (2, (double []) {interval->max, interval->min }); /* at + b */
  li = poly_new (1, (double[]){1});

  tmp = poly_alloc ();

  for (int i = 0; i < p->c->len; i++)
    {
      double c = g_array_index (p->c, double, i);

      if (c != 0)
        {
          Poly *xi;

          xi = g_ptr_array_index (xis, p->c->len - 1 - i);
          poly_multiply (xi, li, tmp);
          poly_scale (tmp, c);
          poly_add_to (pi, tmp);
        }

      poly_multiply (li, l, tmp);
      poly_copy (tmp, li);
    }

  poly_free (tmp);

  poly_reduce (pi);

  poly_free (li);
  poly_free (l);
}

static unsigned int
count_sign_changes (Poly *p)
{
  unsigned int changes;
  int sign;

  sign = 0;
  changes = 0;
  for (int i = p->c->len - 1; i >= 0; i--)
    {
      double c = g_array_index (p->c, double, i);
      if (c > 0)
        {
          if (sign == -1)
            changes++;
          sign = 1;
        }
      else if (c < 0)
        {
          if (sign == 1)
            changes++;
          sign = -1;
        }
    }

  return changes;
}

static GPtrArray *
compute_monomials_for_p (Poly *p)
{
  GPtrArray *xi;
  Poly *m0, *m;

  xi = g_ptr_array_new ();
  g_ptr_array_set_free_func (xi, (GDestroyNotify) poly_free);

  m0 = poly_new (2, (double[]) {1, 1});
  g_ptr_array_add (xi, poly_new (1, (double[]) {1}));
  g_ptr_array_add (xi, m0);
  for (int i = 2; i < p->c->len; i++)
    {
      Poly *mi = g_ptr_array_index (xi, i - 1);
      m = poly_multiply (mi, m0, poly_alloc ());
      g_ptr_array_add (xi, m);
    }

  return xi;
}

static Interval *
isolate_roots (Poly *p, Interval *initial, gsize *n_intervals)
{
  GArray *active;
  GArray *isolating;
  GPtrArray *monomials;
  Poly *pi;

  active = g_array_new (FALSE, FALSE, sizeof (Interval));
  isolating = g_array_new (FALSE, FALSE, sizeof (Interval));

  monomials = compute_monomials_for_p (p);
  pi = poly_alloc ();

  g_array_append_val (active, *initial);
  while (active->len > 0)
    {
      Interval current;
      unsigned int vi;

      current = g_array_index (active, Interval, 0);

      g_array_remove_index_fast (active, 0);

      compute_pi (p, &current, monomials, pi);

      vi = count_sign_changes (pi);

      if (vi == 0)
        {
          /* no real roots */
          continue;
        }
      else if (vi == 1)
        {
          /* one real root */
          g_array_append_val (isolating, current);
          continue;
        }
      else
        {
          double mid = (current.min + current.max) / 2;

          if (poly_eval (p, mid) == 0)
            g_array_append_val (isolating, ((Interval) { mid, mid }));

          g_array_append_val (active, ((Interval) { current.min, mid }));
          g_array_append_val (active, ((Interval) { mid, current.max }));
        }
    }

  g_ptr_array_free (monomials, TRUE);
  poly_free (pi);

  g_array_free (active, TRUE);

  *n_intervals = isolating->len;
  return (Interval *) g_array_free (isolating, FALSE);
}

/* }}} */

static double
bound_for_roots (Poly *p)
{
  double b, cn;

  /* If |x| >= 1 then |x|^n > |x|^n-1 and
   * |a_n||x|^n =  |a_nx^n|
   *            =  |a_0 + a_1x + ... + a_n-1x^n-1|
   *            <= |x|^n-1 (|a_0| + ... + |a_n-1|)
   *
   * Not a great bound, but simple.
   */

  b = 0;
  cn = g_array_index (p->c, double, p->c->len - 1);
  for (int i = 0; i < p->c->len; i++)
    {
      double c = g_array_index (p->c, double, i);
      b += fabs (c/cn);
    }

  return MAX (1, b);
}

static int
poly_find_roots (Poly *p, double *t, int n)
{
  double b;
  double f;
  int n_roots = 0;
  Interval *ivals;
  gsize n_ivals;
  Poly *d;

  b = bound_for_roots (p);

  ivals = isolate_roots (p, &(Interval) { -b, b }, &n_ivals);

  d = poly_derive (p);

  for (int i = 0; i < n_ivals; i++)
    {
      if (ivals[i].min == ivals[i].max)
        {
          t[n_roots++] = ivals[i].min;
        }
      else
        {
          double ymin = poly_eval (p, ivals[i].min);
          double ymax = poly_eval (p, ivals[i].max);

          if ((ymin < 0) != (ymax < 0))
            {
              t[n_roots++] = find_root_bisection (p, ivals[i].min, ivals[i].max, ymin, ymax);
            }
          else if (find_root_newton (p, d, ivals[i].min, ivals[i].max, &f))
            {
              t[n_roots++] = f;
            }
          else
            {
              g_print ("wtf. Found no zero in [%g %g]\n", ivals[i].min, ivals[i].max);
            }
        }
    }

  poly_free (d);

  g_free (ivals);

  return n_roots;
}

/* }}} */
/* {{{ Bézier polynomials */

Poly *
poly_bezier (int n, double *w)
{
  switch (n)
    {
    case 2: /* line */
      return poly_new (2, (double[]) { w[0], w[1] - w[0] });
    case 3: /* quadratic */
      return poly_new (3, (double[]) { w[0], 2 * w[1] - 2 * w[0], w[0] - 2*w[1] + w[2] });
    case 4: /* cubic */
      return poly_new (4, (double[]) { w[0], 3*w[1] - 3*w[0], 3*w[2] - 6*w[1] + 3*w[0], w[3] - 3*w[2] + 3*w[1] - w[0] });
    default:
      g_assert_not_reached ();
    }
}

/* }}} */

/* The goal is to find the minimum of |q(t) - P|,
 * where b(t) is the Bézier curve with controls points
 * (x[i],y[i]), and P is the point (px, py).
 */
double
poly_curve_find_closest_point (Poly *qx, Poly *qy,
                               double px, double py)
{
  Poly *qx2, *qy2;
  Poly *d, *dd;
  double t[10], t2[10];
  int m, m2;

  poly_add_constant (qx, -px);
  poly_add_constant (qy, -py);

  qx2 = poly_multiply (qx, qx, poly_alloc ());
  qy2 = poly_multiply (qy, qy, poly_alloc ());

  poly_add_constant (qx, px);
  poly_add_constant (qy, py);

  /* d is |b(t) - P|^2 */
  d = poly_add (qx2, qy2, poly_alloc ());
  dd = poly_derive (d);

  m = poly_find_roots (dd, t, 10);

  /* We are only interested in the interval [0,1] */
  m2 = 0;
  for (int i = 0; i < m; i++)
    {
      if (t[i] >= 0. && t[i] <= 1.)
        t2[m2++] = t[i];
    }

  /* But we need to check at the edges too */
  t2[m2++] = 0;
  t2[m2++] = 1;

  double dist = INFINITY;
  double tt = -1;
  for (int i = 0; i < m2; i++)
    {
      double xx = poly_eval (qx, t2[i]);
      double yy = poly_eval (qy, t2[i]);
      double d2 = (xx - px) * (xx - px) + (yy - py) * (yy - py);
      if (d2 < dist)
        {
          tt = t2[i];
          dist = d2;
        }
    }

  poly_free (qx2);
  poly_free (qy2);
  poly_free (d);
  poly_free (dd);

  return tt;
}

/* vim:set foldmethod=marker expandtab: */
