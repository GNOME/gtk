
#pragma once

void do_info        (int *argc, const char ***argv);
void do_save        (int *argc, const char ***argv);
void do_show        (int *argc, const char ***argv);

GdkTexture * load_image_file (const char *filename);
