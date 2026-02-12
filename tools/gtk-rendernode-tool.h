#pragma once

#include <gsk/gsk.h>

void do_benchmark   (int *argc, const char ***argv);
void do_compare     (int *argc, const char ***argv);
void do_convert     (int *argc, const char ***argv);
void do_extract     (int *argc, const char ***argv);
void do_filter      (int *argc, const char ***argv);
void do_info        (int *argc, const char ***argv);
void do_show        (int *argc, const char ***argv);
void do_render      (int *argc, const char ***argv);

GskRenderNode *filter_copypaste     (GskRenderNode *node, int argc, const char **argv);
GskRenderNode *filter_strip         (GskRenderNode *node, int argc, const char **argv);
