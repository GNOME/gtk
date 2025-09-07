#pragma once

#include <gtk/gtk.h>

G_DECLARE_FINAL_TYPE (MiniGraph, mini_graph, MINI, GRAPH, GtkWidget)

GtkWidget * mini_graph_new (void);

void        mini_graph_set_identity (MiniGraph *self);
void        mini_graph_set_levels   (MiniGraph *self,
                                     guint      levels);
void        mini_graph_set_linear   (MiniGraph *self,
                                     float      m,
                                     float      b);
void        mini_graph_set_gamma    (MiniGraph *self,
                                     float      amp,
                                     float      exp,
                                     float      ofs);
void        mini_graph_set_discrete (MiniGraph *self,
                                     guint      n,
                                     float     *values);
void        mini_graph_set_table    (MiniGraph *self,
                                     guint      n,
                                     float     *values);
