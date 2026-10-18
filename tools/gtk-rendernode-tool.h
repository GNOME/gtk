#pragma once

#include <gsk/gsk.h>

typedef struct _RntContext RntContext;

void do_convert     (int *argc, const char ***argv);
void do_extract     (int *argc, const char ***argv);
int  do_filter      (int *argc, const char ***argv);
void do_match       (int *argc, const char ***argv);
void do_render      (int *argc, const char ***argv);

GskRenderNode *filter_background_blur   (GskRenderNode *node, RntContext *ctx, int argc, const char **argv);
GskRenderNode *filter_benchmark         (GskRenderNode *node, RntContext *ctx, int argc, const char **argv);
GskRenderNode *filter_compare           (GskRenderNode *node, RntContext *ctx, int argc, const char **argv);
GskRenderNode *filter_copypaste         (GskRenderNode *node, RntContext *ctx, int argc, const char **argv);
GskRenderNode *filter_cut               (GskRenderNode *node, RntContext *ctx, int argc, const char **argv);
GskRenderNode *filter_diff              (GskRenderNode *node, RntContext *ctx, int argc, const char **argv);
GskRenderNode *filter_info              (GskRenderNode *node, RntContext *ctx, int argc, const char **argv);
GskRenderNode *filter_opaque            (GskRenderNode *node, RntContext *ctx, int argc, const char **argv);
GskRenderNode *filter_show              (GskRenderNode *node, RntContext *ctx, int argc, const char **argv);
GskRenderNode *filter_simplify          (GskRenderNode *node, RntContext *ctx, int argc, const char **argv);
GskRenderNode *filter_strip             (GskRenderNode *node, RntContext *ctx, int argc, const char **argv);
GskRenderNode *filter_texture           (GskRenderNode *node, RntContext *ctx, int argc, const char **argv);
