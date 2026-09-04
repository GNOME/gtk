/*
 * Copyright © 2003 Davide Libenzi
 *             2018 Benjamin Otte
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Davide Libenzi <davidel@xmailserver.org>
 *          Benjamin Otte <otte@gnome.org>
 */

#include "config.h"

#include <string.h>

#include "gskdiffprivate.h"


#define XDL_LINE_MAX G_MAXSSIZE
#define MAXCOST 20
/* A successful bidirectional search has at most 2 * MAXCOST edits. Keep a
 * generously sized inline buffer for common scripts; unusually long sequences
 * of distinct-but-equal elements spill to the heap.
 */
#define MAX_DIFF_OPERATIONS (4 * MAXCOST + 1)

struct _GskDiffSettings {
  GCompareDataFunc        compare_func;
  GskKeepFunc             keep_func;
  GskDeleteFunc           delete_func;
  GskInsertFunc           insert_func;

  guint allow_abort : 1;
  guint defer_callbacks : 1;
};

typedef struct _SplitResult {
  gssize i1, i2;
  gboolean min_lo, min_hi;
} SplitResult;

typedef enum
{
  DIFF_OPERATION_KEEP,
  DIFF_OPERATION_DELETE,
  DIFF_OPERATION_INSERT,
} DiffOperationKind;

typedef struct
{
  DiffOperationKind kind;
  union {
    struct {
      gssize index1;
      gssize index2;
    } keep;
    struct {
      gssize index;
    } insert;
    struct {
      gssize index;
    } delete;
  };
} DiffOperation;

typedef struct
{
  DiffOperation operations[MAX_DIFF_OPERATIONS];
  DiffOperation *overflow;
  gsize n_operations;
  gsize n_allocated;
} DiffScript;

G_STATIC_ASSERT (sizeof (DiffOperation) <= 3 * sizeof (gssize));

GskDiffSettings *
gsk_diff_settings_new (GCompareDataFunc compare_func,
                       GskKeepFunc      keep_func,
                       GskDeleteFunc    delete_func,
                       GskInsertFunc    insert_func)
{
  GskDiffSettings *settings;

  settings = g_new0 (GskDiffSettings, 1);

  settings->compare_func = compare_func;
  settings->keep_func = keep_func;
  settings->delete_func = delete_func;
  settings->insert_func = insert_func;

  return settings;
}

void
gsk_diff_settings_set_allow_abort (GskDiffSettings *settings,
                                   gboolean         allow_abort)
{
  settings->allow_abort = allow_abort;
}

void
gsk_diff_settings_set_defer_callbacks (GskDiffSettings *settings,
                                       gboolean         defer_callbacks)
{
  settings->defer_callbacks = defer_callbacks;
}

void
gsk_diff_settings_free (GskDiffSettings *settings)
{
  g_free (settings);
}

static gboolean
diff_elements_equal (gconstpointer          elem1,
                     gconstpointer          elem2,
                     const GskDiffSettings *settings,
                     gpointer               data)
{
  if (elem1 == elem2)
    return TRUE;

  return settings->compare_func (elem1, elem2, data) == 0;
}

static GskDiffResult
diff_script_append (DiffScript        *script,
                    DiffOperationKind  kind,
                    gssize             index1,
                    gssize             index2)
{
  DiffOperation *operation;

  if (script->n_operations == script->n_allocated)
    {
      script->n_allocated *= 2;
      if (script->overflow == NULL)
        {
          script->overflow = g_new (DiffOperation, script->n_allocated);
          memcpy (script->overflow, script->operations, sizeof script->operations);
        }
      else
        script->overflow = g_renew (DiffOperation,
                                    script->overflow,
                                    script->n_allocated);
    }

  operation = script->overflow != NULL ? script->overflow : script->operations;
  operation += script->n_operations++;
  operation->kind = kind;

  switch (kind)
    {
    case DIFF_OPERATION_KEEP:
      operation->keep.index1 = index1;
      operation->keep.index2 = index2;
      break;

    case DIFF_OPERATION_DELETE:
      operation->delete.index = index1;
      break;

    case DIFF_OPERATION_INSERT:
      operation->insert.index = index2;
      break;

    default:
      g_assert_not_reached ();
    }

  return GSK_DIFF_OK;
}

static GskDiffResult
diff_emit_operation (DiffScript            *script,
                     DiffOperationKind      kind,
                     gconstpointer         *elem1,
                     gssize                 index1,
                     gconstpointer         *elem2,
                     gssize                 index2,
                     const GskDiffSettings *settings,
                     gpointer               data)
{
  if (script != NULL)
    return diff_script_append (script, kind, index1, index2);

  switch (kind)
    {
    case DIFF_OPERATION_KEEP:
      return settings->keep_func (elem1[index1], elem2[index2], data);

    case DIFF_OPERATION_DELETE:
      return settings->delete_func (elem1[index1], index1, data);

    case DIFF_OPERATION_INSERT:
      return settings->insert_func (elem2[index2], index2, data);

    default:
      g_assert_not_reached ();
    }
}

static GskDiffResult
diff_script_apply (const DiffScript      *script,
                   gconstpointer         *elem1,
                   gconstpointer         *elem2,
                   const GskDiffSettings *settings,
                   gpointer               data)
{
  const DiffOperation *operations;
  gsize i;

  operations = script->overflow != NULL ? script->overflow : script->operations;

  for (i = 0; i < script->n_operations; i++)
    {
      const DiffOperation *operation;
      GskDiffResult result;

      operation = &operations[i];

      switch (operation->kind)
        {
        case DIFF_OPERATION_KEEP:
          result = settings->keep_func (elem1[operation->keep.index1],
                                        elem2[operation->keep.index2],
                                        data);
          break;

        case DIFF_OPERATION_DELETE:
          result = settings->delete_func (elem1[operation->delete.index],
                                          operation->delete.index,
                                          data);
          break;

        case DIFF_OPERATION_INSERT:
          result = settings->insert_func (elem2[operation->insert.index],
                                          operation->insert.index,
                                          data);
          break;

        default:
          g_assert_not_reached ();
        }

      if (result != GSK_DIFF_OK)
        return result;
    }

  return GSK_DIFF_OK;
}

/*
 * See "An O(ND) Difference Algorithm and its Variations", by Eugene Myers.
 * Basically considers a "box" (off1, off2, lim1, lim2) and scan from both
 * the forward diagonal starting from (off1, off2) and the backward diagonal
 * starting from (lim1, lim2). If the K values on the same diagonal crosses
 * returns the furthest point of reach. Searches that exceed MAXCOST either
 * abort or choose a suboptimal split, depending on the settings.
 *
 * Keep the frontier arrays out of compare()'s stack frame. Compilers may
 * otherwise inline split(), keeping that storage alive while compare()
 * recursively invokes callbacks.
 */
G_GNUC_NO_INLINE
static GskDiffResult
split (gconstpointer         *elem1,
       gssize                 off1,
       gssize                 lim1,
       gconstpointer         *elem2,
       gssize                 off2,
       gssize                 lim2,
       gboolean               need_min,
       const GskDiffSettings *settings,
       gpointer               data,
       SplitResult           *spl)
{
  gssize dmin = off1 - lim2, dmax = lim1 - off2;
  gssize fmid = off1 - off2, bmid = lim1 - lim2;
  gboolean odd = (fmid - bmid) & 1;
  gssize fmin = fmid, fmax = fmid;
  gssize bmin = bmid, bmax = bmid;
  gssize kvdf[2 * MAXCOST + 3];
  gssize kvdb[2 * MAXCOST + 3];
  gssize *kvdf_center = kvdf + MAXCOST + 1;
  gssize *kvdb_center = kvdb + MAXCOST + 1;
  gssize ec, d, i1, i2;

#define KVDF(diagonal) kvdf_center[(diagonal) - fmid]
#define KVDB(diagonal) kvdb_center[(diagonal) - bmid]

  /*
   * Set initial diagonal values for both forward and backward path.
   */
  KVDF (fmid) = off1;
  KVDB (bmid) = lim1;

  for (ec = 1;; ec++)
    {
      /* The non-minimal search stops at MAXCOST. A minimal box is
       * always cut from one of those frontiers, so it cannot need a
       * wider diagonal range either.
       */
      g_assert (ec <= MAXCOST);

      /*
       * We need to extent the diagonal "domain" by one. If the next
       * values exits the box boundaries we need to change it in the
       * opposite direction because (max - min) must be a power of two.
       * Also we initialize the external K value to -1 so that we can
       * avoid extra conditions check inside the core loop.
       */
      if (fmin > dmin)
        KVDF (--fmin - 1) = -1;
      else
        ++fmin;
      if (fmax < dmax)
        KVDF (++fmax + 1) = -1;
      else
        --fmax;

      for (d = fmax; d >= fmin; d -= 2)
        {
          if (KVDF (d - 1) >= KVDF (d + 1))
            i1 = KVDF (d - 1) + 1;
          else
            i1 = KVDF (d + 1);
          i2 = i1 - d;
          for (; i1 < lim1 && i2 < lim2; i1++, i2++)
            {
              if (!diff_elements_equal (elem1[i1], elem2[i2], settings, data))
                break;
            }
          KVDF (d) = i1;
          if (odd && bmin <= d && d <= bmax && KVDB (d) <= i1)
            {
              spl->i1 = i1;
              spl->i2 = i2;
              spl->min_lo = spl->min_hi = 1;
              return GSK_DIFF_OK;
            }
        }

      /*
       * We need to extent the diagonal "domain" by one. If the next
       * values exits the box boundaries we need to change it in the
       * opposite direction because (max - min) must be a power of two.
       * Also we initialize the external K value to -1 so that we can
       * avoid extra conditions check inside the core loop.
       */
      if (bmin > dmin)
        KVDB (--bmin - 1) = XDL_LINE_MAX;
      else
        ++bmin;
      if (bmax < dmax)
        KVDB (++bmax + 1) = XDL_LINE_MAX;
      else
        --bmax;

      for (d = bmax; d >= bmin; d -= 2)
        {
          if (KVDB (d - 1) < KVDB (d + 1))
            i1 = KVDB (d - 1);
          else
            i1 = KVDB (d + 1) - 1;
          i2 = i1 - d;
          for (; i1 > off1 && i2 > off2; i1--, i2--)
            {
              if (!diff_elements_equal (elem1[i1 - 1], elem2[i2 - 1], settings, data))
                break;
            }
          KVDB (d) = i1;
          if (!odd && fmin <= d && d <= fmax && i1 <= KVDF (d))
            {
              spl->i1 = i1;
              spl->i2 = i2;
              spl->min_lo = spl->min_hi = 1;
              return GSK_DIFF_OK;
            }
        }

      if (need_min)
        continue;

      /*
       * Enough is enough. We spent too much time here and now we collect
       * the furthest reaching path using the (i1 + i2) measure.
       */
      if (ec >= MAXCOST)
        {
          gssize fbest, fbest1, bbest, bbest1;

          if (settings->allow_abort)
            return GSK_DIFF_ABORTED;

          fbest = fbest1 = -1;
          for (d = fmax; d >= fmin; d -= 2)
            {
              i1 = MIN (KVDF (d), lim1);
              i2 = i1 - d;
              if (lim2 < i2)
                i1 = lim2 + d, i2 = lim2;
              if (fbest < i1 + i2)
                {
                  fbest = i1 + i2;
                  fbest1 = i1;
                }
            }

          bbest = bbest1 = XDL_LINE_MAX;
          for (d = bmax; d >= bmin; d -= 2)
            {
              i1 = MAX (off1, KVDB (d));
              i2 = i1 - d;
              if (i2 < off2)
                i1 = off2 + d, i2 = off2;
              if (i1 + i2 < bbest)
                {
                  bbest = i1 + i2;
                  bbest1 = i1;
                }
            }

          if ((lim1 + lim2) - bbest < fbest - (off1 + off2))
            {
              spl->i1 = fbest1;
              spl->i2 = fbest - fbest1;
              spl->min_lo = 1;
              spl->min_hi = 0;
            }
          else
            {
              spl->i1 = bbest1;
              spl->i2 = bbest - bbest1;
              spl->min_lo = 0;
              spl->min_hi = 1;
            }

          return GSK_DIFF_OK;
        }
    }

#undef KVDF
#undef KVDB
}

/*
 * Rule: "Divide et Impera". Recursively split the box in sub-boxes by calling
 * the box splitting function. Operations are either sent directly to their
 * callbacks or appended to a script for later replay.
 */
static GskDiffResult
compare (gconstpointer             *elem1,
         gssize                     off1,
         gssize                     lim1,
         gconstpointer             *elem2,
         gssize                     off2,
         gssize                     lim2,
         DiffScript                *script,
         gboolean                   need_min,
         const GskDiffSettings     *settings,
         gpointer                   data)
{
  GskDiffResult res;

  /*
   * Shrink the box by walking through each diagonal snake (SW and NE).
   */
  for (; off1 < lim1 && off2 < lim2; off1++, off2++)
    {
      if (elem1[off1] == elem2[off2])
        continue;

      if (!diff_elements_equal (elem1[off1], elem2[off2], settings, data))
        break;

      res = diff_emit_operation (script, DIFF_OPERATION_KEEP,
                                 elem1, off1, elem2, off2,
                                 settings, data);
      if (res != GSK_DIFF_OK)
        return res;
    }

  for (; off1 < lim1 && off2 < lim2; lim1--, lim2--)
    {
      if (elem1[lim1 - 1] == elem2[lim2 - 1])
        continue;

      if (!diff_elements_equal (elem1[lim1 - 1], elem2[lim2 - 1], settings, data))
        break;

      res = diff_emit_operation (script, DIFF_OPERATION_KEEP,
                                 elem1, lim1 - 1, elem2, lim2 - 1,
                                 settings, data);
      if (res != GSK_DIFF_OK)
        return res;
    }

  /*
   * If one dimension is empty, then all records on the other one must
   * be obviously changed.
   */
  if (off1 == lim1)
    {
      for (; off2 < lim2; off2++)
        {
          res = diff_emit_operation (script, DIFF_OPERATION_INSERT,
                                     elem1, 0, elem2, off2,
                                     settings, data);
          if (res != GSK_DIFF_OK)
            return res;
        }
    }
  else if (off2 == lim2)
    {
      for (; off1 < lim1; off1++)
        {
          res = diff_emit_operation (script, DIFF_OPERATION_DELETE,
                                     elem1, off1, elem2, 0,
                                     settings, data);
          if (res != GSK_DIFF_OK)
            return res;
        }
    }
  else
    {
      SplitResult spl = { 0, };

      /*
       * Divide ...
       */
      res = split (elem1, off1, lim1,
                   elem2, off2, lim2,
                   need_min,
                   settings, data,
                   &spl);
      if (res != GSK_DIFF_OK)
        return res;

      /*
       * ... et Impera.
       */
      res = compare (elem1, off1, spl.i1,
                     elem2, off2, spl.i2,
                     script,
                     spl.min_lo,
                     settings, data);
      if (res != GSK_DIFF_OK)
        return res;
      res = compare (elem1, spl.i1, lim1,
                     elem2, spl.i2, lim2,
                     script,
                     spl.min_hi,
                     settings, data);
      if (res != GSK_DIFF_OK)
        return res;
    }

  return GSK_DIFF_OK;
}

/* Keep the operation buffer out of gsk_diff()'s stack frame when callbacks
 * can be delivered directly without building a script.
 */
G_GNUC_NO_INLINE
static GskDiffResult
compare_deferred (gconstpointer         *elem1,
                  gssize                 off1,
                  gssize                 lim1,
                  gconstpointer         *elem2,
                  gssize                 off2,
                  gssize                 lim2,
                  const GskDiffSettings *settings,
                  gpointer               data)
{
  DiffScript script;
  GskDiffResult result;

  script.overflow = NULL;
  script.n_operations = 0;
  script.n_allocated = G_N_ELEMENTS (script.operations);

  result = compare (elem1, off1, lim1,
                    elem2, off2, lim2,
                    &script,
                    FALSE,
                    settings, data);
  if (result == GSK_DIFF_OK)
    result = diff_script_apply (&script, elem1, elem2, settings, data);

  g_free (script.overflow);

  return result;
}

GskDiffResult
gsk_diff (gconstpointer             *elem1,
          gsize                      n1,
          gconstpointer             *elem2,
          gsize                      n2,
          const GskDiffSettings     *settings,
          gpointer                   data)
{
  gssize off1 = 0;
  gssize off2 = 0;
  gssize lim1 = n1;
  gssize lim2 = n2;

  while (off1 < lim1 && off2 < lim2 && elem1[off1] == elem2[off2])
    off1++, off2++;

  while (off1 < lim1 && off2 < lim2 && elem1[lim1 - 1] == elem2[lim2 - 1])
    lim1--, lim2--;

  if (off1 == lim1 && off2 == lim2)
    return GSK_DIFF_OK;

  /* The remaining lengths bound the edit cost. If their sum fits within
   * MAXCOST, the search cannot structurally abort and callbacks need not
   * be deferred.
   */
  if (!settings->allow_abort ||
      !settings->defer_callbacks ||
      (lim1 - off1 <= MAXCOST &&
       lim2 - off2 <= MAXCOST - (lim1 - off1)))
    return compare (elem1, off1, lim1,
                    elem2, off2, lim2,
                    NULL,
                    FALSE,
                    settings, data);

  return compare_deferred (elem1, off1, lim1,
                           elem2, off2, lim2,
                           settings, data);
}
