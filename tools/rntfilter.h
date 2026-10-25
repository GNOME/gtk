#pragma once

#include <gtk/gtk.h>

#include "rntcontext.h"

gsize                   rnt_filter_n_filters                    (void) G_GNUC_CONST;

const RntFilter *       rnt_filter_get                          (gsize                   i) G_GNUC_CONST;
const RntFilter *       rnt_filter_find                         (const char             *name) G_GNUC_CONST;

const char *            rnt_filter_get_name                     (const RntFilter        *filter);
const char *            rnt_filter_get_description              (const RntFilter        *filter);

GskRenderNode *         rnt_filter_run                          (const RntFilter        *filter,
                                                                 GskRenderNode          *node,
                                                                 RntContext             *ctx,
                                                                 int                     argc,
                                                                 const char            **argv);

char **                 rnt_filter_argv                         (int                     argc,
                                                                 const char            **argv);


GskRenderNode *filter_background_blur   (GskRenderNode *node, RntContext *ctx, int argc, const char **argv);
GskRenderNode *filter_benchmark         (GskRenderNode *node, RntContext *ctx, int argc, const char **argv);
GskRenderNode *filter_compare           (GskRenderNode *node, RntContext *ctx, int argc, const char **argv);
GskRenderNode *filter_copypaste         (GskRenderNode *node, RntContext *ctx, int argc, const char **argv);
GskRenderNode *filter_cut               (GskRenderNode *node, RntContext *ctx, int argc, const char **argv);
GskRenderNode *filter_diff              (GskRenderNode *node, RntContext *ctx, int argc, const char **argv);
GskRenderNode *filter_extract           (GskRenderNode *node, RntContext *ctx, int argc, const char **argv);
GskRenderNode *filter_info              (GskRenderNode *node, RntContext *ctx, int argc, const char **argv);
GskRenderNode *filter_load_icon         (GskRenderNode *node, RntContext *ctx, int argc, const char **argv);
GskRenderNode *filter_load_svg          (GskRenderNode *node, RntContext *ctx, int argc, const char **argv);
GskRenderNode *filter_match             (GskRenderNode *node, RntContext *ctx, int argc, const char **argv);
GskRenderNode *filter_merge             (GskRenderNode *node, RntContext *ctx, int argc, const char **argv);
GskRenderNode *filter_opaque            (GskRenderNode *node, RntContext *ctx, int argc, const char **argv);
GskRenderNode *filter_print             (GskRenderNode *node, RntContext *ctx, int argc, const char **argv);
GskRenderNode *filter_render            (GskRenderNode *node, RntContext *ctx, int argc, const char **argv);
GskRenderNode *filter_save              (GskRenderNode *node, RntContext *ctx, int argc, const char **argv);
GskRenderNode *filter_show              (GskRenderNode *node, RntContext *ctx, int argc, const char **argv);
GskRenderNode *filter_simplify          (GskRenderNode *node, RntContext *ctx, int argc, const char **argv);
GskRenderNode *filter_strip             (GskRenderNode *node, RntContext *ctx, int argc, const char **argv);
GskRenderNode *filter_texture           (GskRenderNode *node, RntContext *ctx, int argc, const char **argv);
