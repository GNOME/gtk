#pragma once

#include <gtk/gtk.h>

typedef struct _RntContext RntContext;
typedef struct _RntFilter RntFilter;

typedef void (* RntContextPrintFunc) (const char *message, void *user_data);

RntContext *            rnt_context_new                         (RntContextPrintFunc     print_func,
                                                                 RntContextPrintFunc     printerr_func,
                                                                 gpointer                user_data,
                                                                 GDestroyNotify          user_destroy);
void                    rnt_context_free                        (RntContext             *self);

GOptionGroup *          rnt_context_create_option_group         (RntContext             *self);

void                    rnt_context_push_node                   (RntContext             *self,
                                                                 GskRenderNode          *node);
GskRenderNode *         rnt_context_pop_node                    (RntContext             *self);
void                    rnt_context_clear_nodes                 (RntContext             *self);
GskRenderNode *         rnt_context_get_node                    (RntContext             *self,
                                                                 gsize                   id);
size_t                  rnt_context_get_n_nodes                 (RntContext             *self);

GskRenderNode *         rnt_context_get_null_node               (RntContext             *ctx);

gboolean                rnt_context_get_failed                  (RntContext             *ctx);
void                    rnt_context_set_failed                  (RntContext             *ctx,
                                                                 gboolean                failed);
gboolean                rnt_context_get_verbose                 (RntContext             *self);
void                    rnt_context_set_verbose                 (RntContext             *ctx,
                                                                 gboolean                verbose);

void                    rnt_context_print                       (RntContext             *self,
                                                                 const char             *format,
                                                                 ...) G_GNUC_PRINTF(2, 3);
void                    rnt_context_verbose                     (RntContext             *self,
                                                                 const char             *format,
                                                                 ...) G_GNUC_PRINTF(2, 3);
void                    rnt_context_warn                        (RntContext             *self,
                                                                 const char             *format,
                                                                 ...) G_GNUC_PRINTF(2, 3);
void                    rnt_context_error                       (RntContext             *self,
                                                                 const char             *format,
                                                                 ...) G_GNUC_PRINTF(2, 3) G_GNUC_NORETURN;

