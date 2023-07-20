
#pragma once

void do_show    (int *argc, const char ***argv);
void do_render  (int *argc, const char ***argv);
void do_info    (int *argc, const char ***argv);

GskRenderNode *load_node_file (const char *filename);
