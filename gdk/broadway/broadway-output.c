#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <cairo.h>

#include "broadway-output.h"

//#define DEBUG_NODE_SENDING

/************************************************************************
 *                Basic I/O primitives                                  *
 ************************************************************************/

struct BroadwayOutput {
  GOutputStream *out;
  GString *buf;
  int error;
  guint32 serial;
};

static void
broadway_output_send_cmd (BroadwayOutput *output,
                          gboolean fin, BroadwayWSOpCode code,
                          const void *buf, gsize count)
{
  gboolean mask = FALSE;
  guchar header[16];
  size_t p;

  gboolean mid_header = count > 125 && count <= 65535;
  gboolean long_header = count > 65535;

  /* NB. big-endian spec => bit 0 == MSB */
  header[0] = ( (fin ? 0x80 : 0) | (code & 0x0f) );
  header[1] = ( (mask ? 0x80 : 0) |
                (mid_header ? 126 : long_header ? 127 : count) );
  p = 2;
  if (mid_header)
    {
      *(guint16 *)(header + p) = GUINT16_TO_BE( (guint16)count );
      p += 2;
    }
  else if (long_header)
    {
      *(guint64 *)(header + p) = GUINT64_TO_BE( count );
      p += 8;
    }
  // FIXME: if we are paranoid we should 'mask' the data
  // FIXME: we should really emit these as a single write
  g_output_stream_write_all (output->out, header, p, NULL, NULL, NULL);
  g_output_stream_write_all (output->out, buf, count, NULL, NULL, NULL);
}

void broadway_output_pong (BroadwayOutput *output)
{
  broadway_output_send_cmd (output, TRUE, BROADWAY_WS_CNX_PONG, NULL, 0);
}

int
broadway_output_flush (BroadwayOutput *output)
{
  if (output->buf->len == 0)
    return TRUE;

  broadway_output_send_cmd (output, TRUE, BROADWAY_WS_BINARY,
                            output->buf->str, output->buf->len);

  g_string_set_size (output->buf, 0);

  return !output->error;

}

BroadwayOutput *
broadway_output_new (GOutputStream *out, guint32 serial)
{
  BroadwayOutput *output;

  output = g_new0 (BroadwayOutput, 1);

  output->out = g_object_ref (out);
  output->buf = g_string_new ("");
  output->serial = serial;

  return output;
}

void
broadway_output_free (BroadwayOutput *output)
{
  g_object_unref (output->out);
  free (output);
}

guint32
broadway_output_get_next_serial (BroadwayOutput *output)
{
  return output->serial;
}

void
broadway_output_set_next_serial (BroadwayOutput *output,
                                 guint32 serial)
{
  output->serial = serial;
}


/************************************************************************
 *                     Core rendering operations                        *
 ************************************************************************/

static void
append_char (BroadwayOutput *output, char c)
{
  g_string_append_c (output->buf, c);
}

static void
append_bool (BroadwayOutput *output, gboolean val)
{
  g_string_append_c (output->buf, val ? 1: 0);
}

static void
append_flags (BroadwayOutput *output, guint32 val)
{
  g_string_append_c (output->buf, val);
}

static void
append_uint16 (BroadwayOutput *output, guint32 v)
{
  gsize old_len = output->buf->len;
  guint8 *buf;

  g_string_set_size (output->buf, old_len + 2);
  buf = (guint8 *)output->buf->str + old_len;
  buf[0] = (v >> 0) & 0xff;
  buf[1] = (v >> 8) & 0xff;
}

static void
append_uint32 (BroadwayOutput *output, guint32 v)
{
  gsize old_len = output->buf->len;
  guint8 *buf;

  g_string_set_size (output->buf, old_len + 4);
  buf = (guint8 *)output->buf->str + old_len;
  buf[0] = (v >> 0) & 0xff;
  buf[1] = (v >> 8) & 0xff;
  buf[2] = (v >> 16) & 0xff;
  buf[3] = (v >> 24) & 0xff;
}

static void
patch_uint32 (BroadwayOutput *output, guint32 v, gsize offset)
{
  guint8 *buf;

  buf = (guint8 *)output->buf->str + offset;
  buf[0] = (v >> 0) & 0xff;
  buf[1] = (v >> 8) & 0xff;
  buf[2] = (v >> 16) & 0xff;
  buf[3] = (v >> 24) & 0xff;
}


static void
write_header(BroadwayOutput *output, char op)
{
  append_char (output, op);
  append_uint32 (output, output->serial++);
}

void
broadway_output_grab_pointer (BroadwayOutput *output,
                              int id,
                              gboolean owner_event)
{
  write_header (output, BROADWAY_OP_GRAB_POINTER);
  append_uint16 (output, id);
  append_bool (output, owner_event);
}

guint32
broadway_output_ungrab_pointer (BroadwayOutput *output)
{
  guint32 serial;

  serial = output->serial;
  write_header (output, BROADWAY_OP_UNGRAB_POINTER);

  return serial;
}

void
broadway_output_new_surface(BroadwayOutput *output,
                            int id, int x, int y, int w, int h,
                            gboolean is_temp)
{
  write_header (output, BROADWAY_OP_NEW_SURFACE);
  append_uint16 (output, id);
  append_uint16 (output, x);
  append_uint16 (output, y);
  append_uint16 (output, w);
  append_uint16 (output, h);
  append_bool (output, is_temp);
}

void
broadway_output_disconnected (BroadwayOutput *output)
{
  write_header (output, BROADWAY_OP_DISCONNECTED);
}

void
broadway_output_show_surface(BroadwayOutput *output,  int id)
{
  write_header (output, BROADWAY_OP_SHOW_SURFACE);
  append_uint16 (output, id);
}

void
broadway_output_hide_surface(BroadwayOutput *output,  int id)
{
  write_header (output, BROADWAY_OP_HIDE_SURFACE);
  append_uint16 (output, id);
}

void
broadway_output_raise_surface(BroadwayOutput *output,  int id)
{
  write_header (output, BROADWAY_OP_RAISE_SURFACE);
  append_uint16 (output, id);
}

void
broadway_output_lower_surface(BroadwayOutput *output,  int id)
{
  write_header (output, BROADWAY_OP_LOWER_SURFACE);
  append_uint16 (output, id);
}

void
broadway_output_destroy_surface(BroadwayOutput *output,  int id)
{
  write_header (output, BROADWAY_OP_DESTROY_SURFACE);
  append_uint16 (output, id);
}

void
broadway_output_roundtrip (BroadwayOutput *output,
                           int             id,
                           guint32         tag)
{
  write_header (output, BROADWAY_OP_ROUNDTRIP);
  append_uint16 (output, id);
  append_uint32 (output, tag);
}

void
broadway_output_set_show_keyboard (BroadwayOutput *output,
                                   gboolean show)
{
  write_header (output, BROADWAY_OP_SET_SHOW_KEYBOARD);
  append_uint16 (output, show);
}

void
broadway_output_move_resize_surface (BroadwayOutput *output,
                                     int             id,
                                     gboolean        has_pos,
                                     int             x,
                                     int             y,
                                     gboolean        has_size,
                                     int             w,
                                     int             h)
{
  int val;

  if (!has_pos && !has_size)
    return;

  write_header (output, BROADWAY_OP_MOVE_RESIZE);
  val = (!!has_pos) | ((!!has_size) << 1);
  append_uint16 (output, id);
  append_flags (output, val);
  if (has_pos)
    {
      append_uint16 (output, x);
      append_uint16 (output, y);
    }
  if (has_size)
    {
      append_uint16 (output, w);
      append_uint16 (output, h);
    }
}

void
broadway_output_set_transient_for (BroadwayOutput *output,
                                   int             id,
                                   int             parent_id)
{
  write_header (output, BROADWAY_OP_SET_TRANSIENT_FOR);
  append_uint16 (output, id);
  append_uint16 (output, parent_id);
}

static gint append_node_depth = -1;

static void
append_type (BroadwayOutput *output, guint32 type, BroadwayNode *node)
{
#ifdef DEBUG_NODE_SENDING
  g_print ("%*s%s", append_node_depth*2, "", broadway_node_type_names[type]);
  if (type == BROADWAY_NODE_TEXTURE)
    g_print (" %u", node->data[4]);
  g_print ("\n");
#endif

  append_uint32 (output, type);
}


/***********************************
 * This outputs the tree to the client, while at the same time diffing
 * against the old tree.  This allows us to avoid sending certain
 * parts.
 *
 * Reusing existing dom nodes are problematic because doing so
 * automatically inherits all their children.  There are two cases
 * where we do this:
 *
 * If the entire sub tree is identical we emit a KEEP_ALL node which
 * just reuses the entire old dom subtree.
 *
 * If a the node is unchanged (but some descendant may have changed),
 * and all parents are also unchanged, then we can just avoid
 * changing the dom node at all, and we emit a KEEP_THIS node.
 *
 ***********************************/
static void
append_node (BroadwayOutput *output,
             BroadwayNode   *node,
             BroadwayNode   *old_node,
             gboolean        all_parents_are_kept)
{
  guint32 i;

  append_node_depth++;

  if (old_node != NULL && broadway_node_equal (node, old_node))
    {
      if (broadway_node_deep_equal (node, old_node))
        {
          append_type (output, BROADWAY_NODE_KEEP_ALL, node);
          goto out;
        }

      if (all_parents_are_kept)
        {
          append_type (output, BROADWAY_NODE_KEEP_THIS, node);
          append_uint32 (output, node->n_children);
          for (i = 0; i < node->n_children; i++)
            append_node (output, node->children[i],
                         i < old_node->n_children ? old_node->children[i] : NULL,
                         TRUE);

          goto out;
        }
    }

  append_type (output, node->type, node);
  for (i = 0; i < node->n_data; i++)
    append_uint32 (output, node->data[i]);
  for (i = 0; i < node->n_children; i++)
    append_node (output,
                 node->children[i],
                 (old_node != NULL && i < old_node->n_children) ? old_node->children[i] : NULL,
                 FALSE);

 out:
  append_node_depth--;
}

void
broadway_output_surface_set_nodes (BroadwayOutput *output,
                                   int             id,
                                   BroadwayNode   *root,
                                   BroadwayNode   *old_root)
{
  gsize size_pos, start, end;

  /* Early return if nothing changed */
  if (old_root != NULL &&
      broadway_node_deep_equal (root, old_root))
    return;

  write_header (output, BROADWAY_OP_SET_NODES);

  append_uint16 (output, id);

  size_pos = output->buf->len;
  append_uint32 (output, 0);

  start = output->buf->len;
#ifdef DEBUG_NODE_SENDING
  g_print ("====== node tree for %d =======\n", id);
#endif
  append_node (output, root, old_root, TRUE);
  end = output->buf->len;
  patch_uint32 (output, (end - start) / 4, size_pos);
}

void
broadway_output_upload_texture (BroadwayOutput *output,
                                guint32 id,
                                GBytes *texture)
{
  gsize len = g_bytes_get_size (texture);
  write_header (output, BROADWAY_OP_UPLOAD_TEXTURE);
  append_uint32 (output, id);
  append_uint32 (output, (guint32)len);
  g_string_append_len (output->buf, g_bytes_get_data (texture, NULL), len);
}

void
broadway_output_release_texture (BroadwayOutput *output,
                                 guint32 id)
{
  write_header (output, BROADWAY_OP_RELEASE_TEXTURE);
  append_uint32 (output, id);
}
