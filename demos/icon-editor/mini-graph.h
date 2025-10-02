#pragma once

#include <gtk/gtk.h>
#include "path-paintable.h"

#define MINI_GRAPH_TYPE (mini_graph_get_type ())
G_DECLARE_FINAL_TYPE (MiniGraph, mini_graph, MINI, GRAPH, GtkWidget)

GtkWidget * mini_graph_new (void);

void mini_graph_set_params (MiniGraph      *self,
                            EasingFunction  easing,
                            CalcMode        mode,
                            const KeyFrame *frames,
                            unsigned int    n_frames);

void mini_graph_set_easing_function (MiniGraph      *self,
                                     EasingFunction  easing);

void mini_graph_set_calc_mode (MiniGraph *self,
                               CalcMode   mode);
