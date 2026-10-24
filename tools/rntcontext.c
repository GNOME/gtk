/*  Copyright 2026 Benjamin Otte
 *
 * GTK is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * GTK is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with GTK; see the file COPYING.  If not,
 * see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "rntcontext.h"

#include <glib/gi18n-lib.h>

struct _RntContext
{
  RntContextPrintFunc print_func;
  RntContextPrintFunc printerr_func;
  gpointer user_data;
  GDestroyNotify user_destroy;

  gboolean verbose;

  GPtrArray *nodes;
  GskRenderNode *null_node;

  gboolean failed;
};

static void
default_print (const char *string,
               gpointer    unused)
{
  g_print ("%s", string);
}

static void
default_printerr (const char *string,
                  gpointer    unused)
{
  g_printerr ("%s", string);
}

RntContext *
rnt_context_new (RntContextPrintFunc print_func,
                 RntContextPrintFunc printerr_func,
                 gpointer            user_data,
                 GDestroyNotify      user_destroy)
{
  RntContext *self;

  self = g_new0 (RntContext, 1);

  if (print_func)
    self->print_func = print_func;
  else
    self->print_func = default_print;
  if (printerr_func)
    self->printerr_func = printerr_func;
  else
    self->printerr_func = default_printerr;
  self->user_data = user_data;
  self->user_destroy = user_destroy;

  self->nodes = g_ptr_array_new ();
  g_ptr_array_set_free_func (self->nodes, (GDestroyNotify) gsk_render_node_unref);

  self->null_node = gsk_container_node_new (NULL, 0);

  return self;
}

void
rnt_context_free (RntContext *self)
{
  if (self->user_destroy)
    self->user_destroy (self->user_data);

  g_ptr_array_free (self->nodes, TRUE);
  g_clear_pointer (&self->null_node, gsk_render_node_unref);

  g_free (self);
}

/**
 * rnt_context_get_node:
 * @self: the context
 * @nth: index of item to return
 *
 * Gets the nth node from the node stack or `NULL` if the node
 * stack doesn't contain that many nodes.
 *
 * Returns: (transfer none) (nullable): the nth node
 *   in the node stack
 **/
GskRenderNode *
rnt_context_get_node (RntContext *self,
                      gsize       nth)
{
  if (nth >= self->nodes->len)
    return NULL;

  return g_ptr_array_index (self->nodes, self->nodes->len - nth - 1);
}

/**
 * rnt_context_get_n_nodes:
 * @self: the context
 *
 * Gets the number of nodes on the node stack.
 *
 * Returns: the size of the node stack
 */
size_t
rnt_context_get_n_nodes (RntContext *self)
{
  return self->nodes->len;
}

/**
 * rnt_context_push_node:
 * @self: the context
 * @node: (transfer full) the rendernode to push
 *   to the top of the stack
 *
 * Pushes a new rendrenode to the top of the node stack.
 **/
void
rnt_context_push_node (RntContext    *self,
                       GskRenderNode *node)
{
  g_return_if_fail (node != NULL);

  g_ptr_array_add (self->nodes, node);
}

/**
 * rnt_context_pop_node:
 * @self: the context
 *
 * Pops the topmost node from the node stack and returns it.
 *
 * If the stack was empty, null is returned.
 *
 * Result: (transfrer full) (nullable): The topmost node from
 *   the stack
 **/
GskRenderNode *
rnt_context_pop_node (RntContext *self)
{
  if (self->nodes->len == 0)
    return NULL;

  return g_ptr_array_steal_index (self->nodes,  self->nodes->len - 1);
}

/**
 * rnt_context_clear_nodes:
 * @self: the context
 *
 * Clears the node stack.
 *
 * Some filters consume all nodes, for example the merge filter
 * combines them all into one node.
 **/
void
rnt_context_clear_nodes (RntContext *self)
{
  g_ptr_array_set_size (self->nodes, 0);
}

/**
 * rnt_context_get_null_node:
 * @ctx: the context
 *
 * Get the special null node.
 *
 * The null node is used in places where NULL is not a valid value,
 * for example when a filter produces no output or when rendernode-tool
 * is invoked without files.
 *
 * Some filters may want to ignore the null node, in such cases they
 * shuld check for equality with this node.
 *
 * Returns: (transfer none): the null node
 **/
GskRenderNode *
rnt_context_get_null_node (RntContext *ctx)
{
  return ctx->null_node;
}

/**
 * rnt_context_get_failed:
 * @ctx: the context
 *
 * Checks for reported failure. See rnt_context_set_failed() for
 * details.
 *
 * Returns: true if the context has failed
 **/
gboolean
rnt_context_get_failed (RntContext *ctx)
{
  return ctx->failed;
}

/**
 * rnt_context_set_failed:
 * @ctx: the context
 * @failed: true if failure occured, false to reset any previous
 *   failure
 *
 * Sets if this context is in a failure state.
 *
 * A failure state indicates to `gtk-rendernode-tool` that it should
 * exit with a non-zero exit status. It does not abort processing any
 * ongoing filter chain.
 **/
void
rnt_context_set_failed (RntContext *ctx,
                        gboolean    failed)
{
  ctx->failed = failed;
}

/**
 * rnt_context_create_option_group:
 * @self: a context
 *
 * Creates an option group to allow users to set properties for the context.
 *
 * Returns: an option group to configure the context
 **/
GOptionGroup *
rnt_context_create_option_group (RntContext *self)
{
  const GOptionEntry entries[] = {
    { "verbose", 'v', 0, G_OPTION_ARG_NONE, &self->verbose, N_("Be verbose"), NULL },
    { NULL, }
  };
  GOptionGroup *group;

  group = g_option_group_new ("general",
                              _("General options"),
                              _("Options that apply to all filters"),
                              self,
                              NULL);
  g_option_group_set_translation_domain (group, GETTEXT_PACKAGE);
  g_option_group_add_entries (group, entries);

  return group;
}

/**
 * rnt_context_set_verbose:
 * @self: the context
 * @verbose: if verbose output is desired
 *
 * If set to true, [method@Rnt.Context.verbose] is going
 * to print its output, too.
 *
 * This is usually configured using the --verbose command line
 * argument.
 **/
void
rnt_context_set_verbose (RntContext *self,
                         gboolean    verbose)
{
  self->verbose = verbose;
}

/**
 * rnt_context_get_verbose:
 * @self: the context
 *
 * Checks if verbose mode is enabled.
 *
 * This is usually done via the --verbose command
 * line switch.
 *
 * Returns: true if verbose mode is on
 **/
gboolean
rnt_context_get_verbose (RntContext *self)
{
  return self->verbose;
}

/**
 * rnt_context_verbose:
 * @self: the context
 * @format: the printf-format message
 * @...: arguments
 *
 * Prints a message if verbose mode is on, otherwise
 * discards it.
 **/
void
rnt_context_verbose (RntContext *self,
                     const char *format,
                     ...)
{
  va_list args;
  char *s;

  if (!self->verbose)
    return;

  va_start (args, format);
  s = g_strdup_vprintf (format, args);
  self->print_func (s, self->user_data);
  va_end (args);
  g_free (s);
}

/**
 * rnt_context_print:
 * @self: the context
 * @format: printf-format string for the message
 * @...: message arguments
 *
 * Prints a message. 
 *
 * This should only be used for requested messages from filters
 * designed to print output.
 *
 * For status messages, use [method@Rnt.Context.verbose].
 **/
void
rnt_context_print (RntContext *self,
                   const char *format,
                   ...)
{
  va_list args;
  char *s;

  va_start (args, format);
  s = g_strdup_vprintf (format, args);
  self->print_func (s, self->user_data);
  va_end (args);
  g_free (s);
}

/**
 * rnt_context_warn:
 * @self: the context
 * @format: printf-format string for the message
 * @...: message arguments
 *
 * Prints a warning message using the error function.
 **/
void
rnt_context_warn (RntContext *self,
                  const char *format,
                  ...)
{
  va_list args;
  char *s;

  va_start (args, format);
  s = g_strdup_vprintf (format, args);
  self->printerr_func (s, self->user_data);
  va_end (args);
  g_free (s);
}

/**
 * rnt_context_error:
 * @self: the context
 * @format: printf-format string for the message
 * @...: message arguments
 *
 * Prints an error message using the error function
 * and aborts the program.
 **/
void
rnt_context_error (RntContext *self,
                   const char *format,
                   ...)
{
  va_list args;
  char *s;

  va_start (args, format);
  s = g_strdup_vprintf (format, args);
  self->printerr_func (s, self->user_data);
  va_end (args);
  g_free (s);

  exit (1);
}

