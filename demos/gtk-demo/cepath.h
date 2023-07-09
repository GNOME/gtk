#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define CE_TYPE_PATH (ce_path_get_type ())
G_DECLARE_FINAL_TYPE (CEPath, ce_path, CE, PATH, GObject)

typedef struct _CEPathCurve CEPathCurve;

typedef enum
{
  CE_PATH_CUSP,
  CE_PATH_SMOOTH,
  CE_PATH_SYMMETRIC,
  CE_PATH_AUTOMATIC
} CEPathConstraint;

CEPath *        ce_path_new                  (void);

int             ce_path_get_n_curves         (CEPath                 *self);

CEPathCurve *   ce_path_get_curve            (CEPath                 *self,
                                              int                     idx);

CEPathCurve *   ce_path_previous_curve       (CEPath                 *self,
                                              CEPathCurve            *seg);

CEPathCurve *   ce_path_next_curve           (CEPath                 *self,
                                              CEPathCurve            *seg);

void            ce_path_set_gsk_path         (CEPath                 *self,
                                              GskPath                *path);

GskPath *       ce_path_get_gsk_path         (CEPath                 *self);

void            ce_path_split_curve          (CEPath                 *self,
                                              CEPathCurve            *seg,
                                              float                   pos);

void            ce_path_remove_curve         (CEPath                 *self,
                                              CEPathCurve            *seg);

void            ce_path_drag_curve           (CEPath                 *self,
                                              CEPathCurve            *seg,
                                              const graphene_point_t *pos);

CEPathCurve *   ce_path_find_closest_curve   (CEPath                 *self,
                                              graphene_point_t       *point,
                                              float                   threshold,
                                              graphene_point_t       *p,
                                              float                  *pos);

void            ce_path_get_point            (CEPath                 *self,
                                              CEPathCurve            *seg,
                                              int                     c,
                                              graphene_point_t       *point);

void            ce_path_set_point            (CEPath                 *self,
                                              CEPathCurve            *seg,
                                              int                     point,
                                              const graphene_point_t *pos);

GskPathOperation
                ce_path_get_operation        (CEPath                 *self,
                                              CEPathCurve            *seg);

void            ce_path_set_operation        (CEPath                 *self,
                                              CEPathCurve            *seg,
                                              GskPathOperation        op);

float           ce_path_get_weight           (CEPath                 *self,
                                              CEPathCurve            *seg);

void            ce_path_set_weight           (CEPath                 *self,
                                              CEPathCurve            *seg,
                                              float                   weight);

CEPathConstraint
                ce_path_get_constraint       (CEPath                 *self,
                                              CEPathCurve            *seg);

void            ce_path_set_constraint       (CEPath                 *self,
                                              CEPathCurve            *seg,
                                              CEPathConstraint        constraint);



G_END_DECLS
