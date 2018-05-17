/* sp-capture-writer.c
 *
 * Copyright © 2016 Christian Hergert <chergert@redhat.com>
 *
 * This file is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This file is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif

#include <errno.h>
#include <fcntl.h>
#include <glib/gstdio.h>
#include <string.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "capture/sp-capture-writer.h"

#define DEFAULT_BUFFER_SIZE (getpagesize() * 64L)
#define INVALID_ADDRESS     (G_GUINT64_CONSTANT(0))

typedef struct
{
  /* A pinter into the string buffer */
  const gchar *str;

  /* The unique address for the string */
  guint64 addr;
} SpCaptureJitmapBucket;

struct _SpCaptureWriter
{
  /*
   * This is our buffer location for incoming strings. This is used
   * similarly to GStringChunk except there is only one-page, and after
   * it fills, we flush to disk.
   *
   * This is paired with a closed hash table for deduplication.
   */
  gchar addr_buf[4096*4];

  /* Our hashtable for deduplication. */
  SpCaptureJitmapBucket addr_hash[512];

  /* We keep the large fields above so that our allocation will be page
   * alinged for the write buffer. This improves the performance of large
   * writes to the target file-descriptor.
   */
  volatile gint ref_count;

  /*
   * Our address sequence counter. The value that comes from
   * monotonically increasing this is OR'd with JITMAP_MARK to denote
   * the address name should come from the JIT map.
   */
  gsize addr_seq;

  /* Our position in addr_buf. */
  gsize addr_buf_pos;

  /*
   * The number of hash table items in @addr_hash. This is an
   * optimization so that we can avoid calculating the number of strings
   * when flushing out the jitmap.
   */
  guint addr_hash_size;

  /* Capture file handle */
  int fd;

  /* Our write buffer for fd */
  guint8 *buf;
  gsize pos;
  gsize len;

  /* counter id sequence */
  gint next_counter_id;

  /* Statistics while recording */
  SpCaptureStat stat;
};

#ifndef SP_DISABLE_GOBJECT
G_DEFINE_BOXED_TYPE (SpCaptureWriter, sp_capture_writer,
                     sp_capture_writer_ref, sp_capture_writer_unref)
#endif

static inline void
sp_capture_writer_frame_init (SpCaptureFrame     *frame_,
                              gint                len,
                              gint                cpu,
                              GPid                pid,
                              gint64              time_,
                              SpCaptureFrameType  type)
{
  g_assert (frame_ != NULL);

  frame_->len = len;
  frame_->cpu = cpu;
  frame_->pid = pid;
  frame_->time = time_;
  frame_->type = type;
  frame_->padding = 0;
}

static void
sp_capture_writer_finalize (SpCaptureWriter *self)
{
  if (self != NULL)
    {
      sp_capture_writer_flush (self);
      close (self->fd);
      g_free (self->buf);
      g_free (self);
    }
}

SpCaptureWriter *
sp_capture_writer_ref (SpCaptureWriter *self)
{
  g_assert (self != NULL);
  g_assert (self->ref_count > 0);

  g_atomic_int_inc (&self->ref_count);

  return self;
}

void
sp_capture_writer_unref (SpCaptureWriter *self)
{
  g_assert (self != NULL);
  g_assert (self->ref_count > 0);

  if (g_atomic_int_dec_and_test (&self->ref_count))
    sp_capture_writer_finalize (self);
}

static gboolean
sp_capture_writer_flush_data (SpCaptureWriter *self)
{
  const guint8 *buf;
  gssize written;
  gsize to_write;

  g_assert (self != NULL);
  g_assert (self->pos <= self->len);
  g_assert ((self->pos % SP_CAPTURE_ALIGN) == 0);

  buf = self->buf;
  to_write = self->pos;

  while (to_write > 0)
    {
      written = write (self->fd, buf, to_write);
      if (written < 0)
        return FALSE;

      if (written == 0 && errno != EAGAIN)
        return FALSE;

      g_assert (written <= (gssize)to_write);

      buf += written;
      to_write -= written;
    }

  self->pos = 0;

  return TRUE;
}

static inline void
sp_capture_writer_realign (gsize *pos)
{
  *pos = (*pos + SP_CAPTURE_ALIGN - 1) & ~(SP_CAPTURE_ALIGN - 1);
}

static inline gboolean
sp_capture_writer_ensure_space_for (SpCaptureWriter *self,
                                    gsize            len)
{
  /* Check for max frame size */
  if (len > G_MAXUSHORT)
    return FALSE;

  if ((self->len - self->pos) < len)
    {
      if (!sp_capture_writer_flush_data (self))
        return FALSE;
    }

  return TRUE;
}

static inline gpointer
sp_capture_writer_allocate (SpCaptureWriter *self,
                            gsize           *len)
{
  gpointer p;

  g_assert (self != NULL);
  g_assert (len != NULL);
  g_assert ((self->pos % SP_CAPTURE_ALIGN) == 0);

  sp_capture_writer_realign (len);

  if (!sp_capture_writer_ensure_space_for (self, *len))
    return NULL;

  p = (gpointer)&self->buf[self->pos];

  self->pos += *len;

  g_assert ((self->pos % SP_CAPTURE_ALIGN) == 0);

  return p;
}

static gboolean
sp_capture_writer_flush_jitmap (SpCaptureWriter *self)
{
  SpCaptureJitmap jitmap;
  gssize r;
  gsize len;

  g_assert (self != NULL);

  if (self->addr_hash_size == 0)
    return TRUE;

  g_assert (self->addr_buf_pos > 0);

  len = sizeof jitmap + self->addr_buf_pos;

  sp_capture_writer_realign (&len);

  sp_capture_writer_frame_init (&jitmap.frame,
                                len,
                                -1,
                                getpid (),
                                SP_CAPTURE_CURRENT_TIME,
                                SP_CAPTURE_FRAME_JITMAP);
  jitmap.n_jitmaps = self->addr_hash_size;

  if (sizeof jitmap != write (self->fd, &jitmap, sizeof jitmap))
    return FALSE;

  r = write (self->fd, self->addr_buf, len - sizeof jitmap);
  if (r < 0 || (gsize)r != (len - sizeof jitmap))
    return FALSE;

  self->addr_buf_pos = 0;
  self->addr_hash_size = 0;
  memset (self->addr_hash, 0, sizeof self->addr_hash);

  self->stat.frame_count[SP_CAPTURE_FRAME_JITMAP]++;

  return TRUE;
}

static gboolean
sp_capture_writer_lookup_jitmap (SpCaptureWriter  *self,
                                 const gchar      *name,
                                 SpCaptureAddress *addr)
{
  guint hash;
  guint i;

  g_assert (self != NULL);
  g_assert (name != NULL);
  g_assert (addr != NULL);

  hash = g_str_hash (name) % G_N_ELEMENTS (self->addr_hash);

  for (i = hash; i < G_N_ELEMENTS (self->addr_hash); i++)
    {
      SpCaptureJitmapBucket *bucket = &self->addr_hash[i];

      if (bucket->str == NULL)
        return FALSE;

      if (strcmp (bucket->str, name) == 0)
        {
          *addr = bucket->addr;
          return TRUE;
        }
    }

  for (i = 0; i < hash; i++)
    {
      SpCaptureJitmapBucket *bucket = &self->addr_hash[i];

      if (bucket->str == NULL)
        return FALSE;

      if (strcmp (bucket->str, name) == 0)
        {
          *addr = bucket->addr;
          return TRUE;
        }
    }

  return FALSE;
}

static SpCaptureAddress
sp_capture_writer_insert_jitmap (SpCaptureWriter *self,
                                 const gchar     *str)
{
  SpCaptureAddress addr;
  gchar *dst;
  gsize len;
  guint hash;
  guint i;

  g_assert (self != NULL);
  g_assert (str != NULL);
  g_assert ((self->pos % SP_CAPTURE_ALIGN) == 0);

  len = sizeof addr + strlen (str) + 1;

  if ((self->addr_hash_size == G_N_ELEMENTS (self->addr_hash)) ||
      ((sizeof self->addr_buf - self->addr_buf_pos) < len))
    {
      if (!sp_capture_writer_flush_jitmap (self))
        return INVALID_ADDRESS;

      g_assert (self->addr_hash_size == 0);
      g_assert (self->addr_buf_pos == 0);
    }

  g_assert (self->addr_hash_size < G_N_ELEMENTS (self->addr_hash));
  g_assert (len > sizeof addr);

  /* Allocate the next unique address */
  addr = SP_CAPTURE_JITMAP_MARK | ++self->addr_seq;

  /* Copy the address into the buffer */
  dst = (gchar *)&self->addr_buf[self->addr_buf_pos];
  memcpy (dst, &addr, sizeof addr);

  /*
   * Copy the string into the buffer, keeping dst around for
   * when we insert into the hashtable.
   */
  dst += sizeof addr;
  memcpy (dst, str, len - sizeof addr);

  /* Advance our string cache position */
  self->addr_buf_pos += len;
  g_assert (self->addr_buf_pos <= sizeof self->addr_buf);

  /* Now place the address into the hashtable */
  hash = g_str_hash (str) % G_N_ELEMENTS (self->addr_hash);

  /* Start from the current hash bucket and go forward */
  for (i = hash; i < G_N_ELEMENTS (self->addr_hash); i++)
    {
      SpCaptureJitmapBucket *bucket = &self->addr_hash[i];

      if (G_LIKELY (bucket->str == NULL))
        {
          bucket->str = dst;
          bucket->addr = addr;
          self->addr_hash_size++;
          return addr;
        }
    }

  /* Wrap around to the beginning */
  for (i = 0; i < hash; i++)
    {
      SpCaptureJitmapBucket *bucket = &self->addr_hash[i];

      if (G_LIKELY (bucket->str == NULL))
        {
          bucket->str = dst;
          bucket->addr = addr;
          self->addr_hash_size++;
          return addr;
        }
    }

  g_assert_not_reached ();

  return INVALID_ADDRESS;
}

SpCaptureWriter *
sp_capture_writer_new_from_fd (int   fd,
                               gsize buffer_size)
{
  g_autofree gchar *nowstr = NULL;
  SpCaptureWriter *self;
  SpCaptureFileHeader *header;
  GTimeVal tv;
  gsize header_len = sizeof(*header);

  if (buffer_size == 0)
    buffer_size = DEFAULT_BUFFER_SIZE;

  g_assert (fd != -1);
  g_assert (buffer_size % getpagesize() == 0);

  if (ftruncate (fd, 0) != 0)
    return NULL;

  self = g_new0 (SpCaptureWriter, 1);
  self->ref_count = 1;
  self->fd = fd;
  self->buf = (guint8 *)g_malloc0 (buffer_size);
  self->len = buffer_size;
  self->next_counter_id = 1;

  g_get_current_time (&tv);
  nowstr = g_time_val_to_iso8601 (&tv);

  header = sp_capture_writer_allocate (self, &header_len);

  if (header == NULL)
    {
      sp_capture_writer_finalize (self);
      return NULL;
    }

  header->magic = SP_CAPTURE_MAGIC;
  header->version = 1;
#ifdef G_LITTLE_ENDIAN
  header->little_endian = TRUE;
#else
  header->little_endian = FALSE;
#endif
  header->padding = 0;
  g_strlcpy (header->capture_time, nowstr, sizeof header->capture_time);
  header->time = SP_CAPTURE_CURRENT_TIME;
  header->end_time = 0;
  memset (header->suffix, 0, sizeof header->suffix);

  if (!sp_capture_writer_flush_data (self))
    {
      sp_capture_writer_finalize (self);
      return NULL;
    }

  g_assert (self->pos == 0);
  g_assert (self->len > 0);
  g_assert (self->len % getpagesize() == 0);
  g_assert (self->buf != NULL);
  g_assert (self->addr_hash_size == 0);
  g_assert (self->fd != -1);

  return self;
}

SpCaptureWriter *
sp_capture_writer_new (const gchar *filename,
                       gsize        buffer_size)
{
  SpCaptureWriter *self;
  int fd;

  g_assert (filename != NULL);
  g_assert (buffer_size % getpagesize() == 0);

  if ((-1 == (fd = open (filename, O_CREAT | O_RDWR, 0640))) ||
      (-1 == ftruncate (fd, 0L)))
    return NULL;

  self = sp_capture_writer_new_from_fd (fd, buffer_size);

  if (self == NULL)
    close (fd);

  return self;
}

gboolean
sp_capture_writer_add_map (SpCaptureWriter *self,
                           gint64           time,
                           gint             cpu,
                           GPid             pid,
                           guint64          start,
                           guint64          end,
                           guint64          offset,
                           guint64          inode,
                           const gchar     *filename)
{
  SpCaptureMap *ev;
  gsize len;

  if (filename == NULL)
    filename = "";

  g_assert (self != NULL);
  g_assert (filename != NULL);

  len = sizeof *ev + strlen (filename) + 1;

  ev = (SpCaptureMap *)sp_capture_writer_allocate (self, &len);
  if (!ev)
    return FALSE;

  sp_capture_writer_frame_init (&ev->frame,
                                len,
                                cpu,
                                pid,
                                time,
                                SP_CAPTURE_FRAME_MAP);
  ev->start = start;
  ev->end = end;
  ev->offset = offset;
  ev->inode = inode;

  g_strlcpy (ev->filename, filename, len - sizeof *ev);
  ev->filename[len - sizeof *ev - 1] = '\0';

  self->stat.frame_count[SP_CAPTURE_FRAME_MAP]++;

  return TRUE;
}

gboolean
sp_capture_writer_add_mark (SpCaptureWriter *self,
                            gint64           time,
                            gint             cpu,
                            GPid             pid,
                            guint64          duration,
                            const gchar     *group,
                            const gchar     *name,
                            const gchar     *message)
{
  SpCaptureMark *ev;
  gsize message_len;
  gsize len;

  g_assert (self != NULL);
  g_assert (name != NULL);
  g_assert (group != NULL);

  if (message == NULL)
    message = "";
  message_len = strlen (message) + 1;

  len = sizeof *ev + message_len;
  ev = (SpCaptureMark *)sp_capture_writer_allocate (self, &len);
  if (!ev)
    return FALSE;

  sp_capture_writer_frame_init (&ev->frame,
                                len,
                                cpu,
                                pid,
                                time,
                                SP_CAPTURE_FRAME_MARK);

  ev->duration = duration;
  g_strlcpy (ev->group, group, sizeof ev->group);
  g_strlcpy (ev->name, name, sizeof ev->name);
  memcpy (ev->message, message, message_len);

  self->stat.frame_count[SP_CAPTURE_FRAME_MARK]++;

  return TRUE;
}

SpCaptureAddress
sp_capture_writer_add_jitmap (SpCaptureWriter *self,
                              const gchar     *name)
{
  SpCaptureAddress addr = INVALID_ADDRESS;

  if (name == NULL)
    name = "";

  g_assert (self != NULL);
  g_assert (name != NULL);

  if (!sp_capture_writer_lookup_jitmap (self, name, &addr))
    addr = sp_capture_writer_insert_jitmap (self, name);

  return addr;
}

gboolean
sp_capture_writer_add_process (SpCaptureWriter *self,
                               gint64           time,
                               gint             cpu,
                               GPid             pid,
                               const gchar     *cmdline)
{
  SpCaptureProcess *ev;
  gsize len;

  if (cmdline == NULL)
    cmdline = "";

  g_assert (self != NULL);
  g_assert (cmdline != NULL);

  len = sizeof *ev + strlen (cmdline) + 1;

  ev = (SpCaptureProcess *)sp_capture_writer_allocate (self, &len);
  if (!ev)
    return FALSE;

  sp_capture_writer_frame_init (&ev->frame,
                                len,
                                cpu,
                                pid,
                                time,
                                SP_CAPTURE_FRAME_PROCESS);

  g_strlcpy (ev->cmdline, cmdline, len - sizeof *ev);
  ev->cmdline[len - sizeof *ev - 1] = '\0';

  self->stat.frame_count[SP_CAPTURE_FRAME_PROCESS]++;

  return TRUE;
}

gboolean
sp_capture_writer_add_sample (SpCaptureWriter        *self,
                              gint64                  time,
                              gint                    cpu,
                              GPid                    pid,
                              const SpCaptureAddress *addrs,
                              guint                   n_addrs)
{
  SpCaptureSample *ev;
  gsize len;

  g_assert (self != NULL);

  len = sizeof *ev + (n_addrs * sizeof (SpCaptureAddress));

  ev = (SpCaptureSample *)sp_capture_writer_allocate (self, &len);
  if (!ev)
    return FALSE;

  sp_capture_writer_frame_init (&ev->frame,
                                len,
                                cpu,
                                pid,
                                time,
                                SP_CAPTURE_FRAME_SAMPLE);
  ev->n_addrs = n_addrs;

  memcpy (ev->addrs, addrs, (n_addrs * sizeof (SpCaptureAddress)));

  self->stat.frame_count[SP_CAPTURE_FRAME_SAMPLE]++;

  return TRUE;
}

gboolean
sp_capture_writer_add_fork (SpCaptureWriter *self,
                            gint64           time,
                            gint             cpu,
                            GPid             pid,
                            GPid             child_pid)
{
  SpCaptureFork *ev;
  gsize len = sizeof *ev;

  g_assert (self != NULL);

  ev = (SpCaptureFork *)sp_capture_writer_allocate (self, &len);
  if (!ev)
    return FALSE;

  sp_capture_writer_frame_init (&ev->frame,
                                len,
                                cpu,
                                pid,
                                time,
                                SP_CAPTURE_FRAME_FORK);
  ev->child_pid = child_pid;

  self->stat.frame_count[SP_CAPTURE_FRAME_FORK]++;

  return TRUE;
}

gboolean
sp_capture_writer_add_exit (SpCaptureWriter *self,
                            gint64           time,
                            gint             cpu,
                            GPid             pid)
{
  SpCaptureExit *ev;
  gsize len = sizeof *ev;

  g_assert (self != NULL);

  ev = (SpCaptureExit *)sp_capture_writer_allocate (self, &len);
  if (!ev)
    return FALSE;

  sp_capture_writer_frame_init (&ev->frame,
                                len,
                                cpu,
                                pid,
                                time,
                                SP_CAPTURE_FRAME_EXIT);

  self->stat.frame_count[SP_CAPTURE_FRAME_EXIT]++;

  return TRUE;
}

gboolean
sp_capture_writer_add_timestamp (SpCaptureWriter *self,
                                 gint64           time,
                                 gint             cpu,
                                 GPid             pid)
{
  SpCaptureTimestamp *ev;
  gsize len = sizeof *ev;

  g_assert (self != NULL);

  ev = (SpCaptureTimestamp *)sp_capture_writer_allocate (self, &len);
  if (!ev)
    return FALSE;

  sp_capture_writer_frame_init (&ev->frame,
                                len,
                                cpu,
                                pid,
                                time,
                                SP_CAPTURE_FRAME_TIMESTAMP);

    self->stat.frame_count[SP_CAPTURE_FRAME_TIMESTAMP]++;

  return TRUE;
}

static gboolean
sp_capture_writer_flush_end_time (SpCaptureWriter *self)
{
  gint64 end_time = SP_CAPTURE_CURRENT_TIME;
  ssize_t ret;

  g_assert (self != NULL);

  /* This field is opportunistic, so a failure is okay. */

again:
  ret = pwrite (self->fd,
                &end_time,
                sizeof (end_time),
                G_STRUCT_OFFSET (SpCaptureFileHeader, end_time));

  if (ret < 0 && errno == EAGAIN)
    goto again;

  return TRUE;
}

gboolean
sp_capture_writer_flush (SpCaptureWriter *self)
{
  g_assert (self != NULL);

  return (sp_capture_writer_flush_jitmap (self) &&
          sp_capture_writer_flush_data (self) &&
          sp_capture_writer_flush_end_time (self));
}

/**
 * sp_capture_writer_save_as:
 * @self: A #SpCaptureWriter
 * @filename: the file to save the capture as
 * @error: a location for a #GError or %NULL.
 *
 * Saves the captured data as the file @filename.
 *
 * This is primarily useful if the writer was created with a memory-backed
 * file-descriptor such as a memfd or tmpfs file on Linux.
 *
 * Returns: %TRUE if successful, otherwise %FALSE and @error is set.
 */
gboolean
sp_capture_writer_save_as (SpCaptureWriter            *self,
                           const gchar                *filename,
                           GError                    **error)
{
  gsize to_write;
  off_t in_off;
  off_t pos;
  int fd = -1;

  g_assert (self != NULL);
  g_assert (self->fd != -1);
  g_assert (filename != NULL);

  if (-1 == (fd = open (filename, O_CREAT | O_RDWR, 0640)))
    goto handle_errno;

  if (!sp_capture_writer_flush (self))
    goto handle_errno;

  if (-1 == (pos = lseek (self->fd, 0L, SEEK_CUR)))
    goto handle_errno;

  to_write = pos;
  in_off = 0;

  while (to_write > 0)
    {
      gssize written;

      written = sendfile (fd, self->fd, &in_off, pos);

      if (written < 0)
        goto handle_errno;

      if (written == 0 && errno != EAGAIN)
        goto handle_errno;

      g_assert (written <= (gssize)to_write);

      to_write -= written;
    }

  close (fd);

  return TRUE;

handle_errno:
  g_set_error (error,
               G_FILE_ERROR,
               g_file_error_from_errno (errno),
               "%s", g_strerror (errno));

  if (fd != -1)
    {
      close (fd);
      g_unlink (filename);
    }

  return FALSE;
}

/**
 * _sp_capture_writer_splice_from_fd:
 * @self: An #SpCaptureWriter
 * @fd: the fd to read from.
 * @error: A location for a #GError, or %NULL.
 *
 * This is internal API for SpCaptureWriter and SpCaptureReader to
 * communicate when splicing a reader into a writer.
 *
 * This should not be used outside of #SpCaptureReader or
 * #SpCaptureWriter.
 *
 * This will not advance the position of @fd.
 *
 * Returns: %TRUE if successful; otherwise %FALSE and @error is set.
 */
gboolean
_sp_capture_writer_splice_from_fd (SpCaptureWriter  *self,
                                   int               fd,
                                   GError          **error)
{
  struct stat stbuf;
  off_t in_off;
  gsize to_write;

  g_assert (self != NULL);
  g_assert (self->fd != -1);

  if (-1 == fstat (fd, &stbuf))
    goto handle_errno;

  if (stbuf.st_size < 256)
    {
      g_set_error (error,
                   G_FILE_ERROR,
                   G_FILE_ERROR_INVAL,
                   "Cannot splice, possibly corrupt file.");
      return FALSE;
    }

  in_off = 256;
  to_write = stbuf.st_size - in_off;

  while (to_write > 0)
    {
      gssize written;

      written = sendfile (self->fd, fd, &in_off, to_write);

      if (written < 0)
        goto handle_errno;

      if (written == 0 && errno != EAGAIN)
        goto handle_errno;

      g_assert (written <= (gssize)to_write);

      to_write -= written;
    }

  return TRUE;

handle_errno:
  g_set_error (error,
               G_FILE_ERROR,
               g_file_error_from_errno (errno),
               "%s", g_strerror (errno));

  return FALSE;
}

/**
 * sp_capture_writer_splice:
 * @self: An #SpCaptureWriter
 * @dest: An #SpCaptureWriter
 * @error: A location for a #GError, or %NULL.
 *
 * This function will copy the capture @self into the capture @dest.  This
 * tries to be semi-efficient by using sendfile() to copy the contents between
 * the captures. @self and @dest will be flushed before the contents are copied
 * into the @dest file-descriptor.
 *
 * Returns: %TRUE if successful, otherwise %FALSE and and @error is set.
 */
gboolean
sp_capture_writer_splice (SpCaptureWriter  *self,
                          SpCaptureWriter  *dest,
                          GError          **error)
{
  gboolean ret;
  off_t pos;

  g_assert (self != NULL);
  g_assert (self->fd != -1);
  g_assert (dest != NULL);
  g_assert (dest->fd != -1);

  /* Flush before writing anything to ensure consistency */
  if (!sp_capture_writer_flush (self) || !sp_capture_writer_flush (dest))
    goto handle_errno;

  /* Track our current position so we can reset */
  if ((off_t)-1 == (pos = lseek (self->fd, 0L, SEEK_CUR)))
    goto handle_errno;

  /* Perform the splice */
  ret = _sp_capture_writer_splice_from_fd (dest, self->fd, error);

  /* Now reset or file-descriptor position (it should be the same */
  if (pos != lseek (self->fd, pos, SEEK_SET))
    {
      ret = FALSE;
      goto handle_errno;
    }

  return ret;

handle_errno:
  g_set_error (error,
               G_FILE_ERROR,
               g_file_error_from_errno (errno),
               "%s", g_strerror (errno));

  return FALSE;
}

/**
 * sp_capture_writer_stat:
 * @self: A #SpCaptureWriter
 * @stat: (out): A location for an #SpCaptureStat
 *
 * This function will fill @stat with statistics generated while capturing
 * the profiler session.
 */
void
sp_capture_writer_stat (SpCaptureWriter *self,
                        SpCaptureStat   *stat)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (stat != NULL);

  *stat = self->stat;
}

gboolean
sp_capture_writer_define_counters (SpCaptureWriter        *self,
                                   gint64                  time,
                                   gint                    cpu,
                                   GPid                    pid,
                                   const SpCaptureCounter *counters,
                                   guint                   n_counters)
{
  SpCaptureFrameCounterDefine *def;
  gsize len;
  guint i;

  g_assert (self != NULL);
  g_assert (counters != NULL);

  if (n_counters == 0)
    return TRUE;

  len = sizeof *def + (sizeof *counters * n_counters);

  def = (SpCaptureFrameCounterDefine *)sp_capture_writer_allocate (self, &len);
  if (!def)
    return FALSE;

  sp_capture_writer_frame_init (&def->frame,
                                len,
                                cpu,
                                pid,
                                time,
                                SP_CAPTURE_FRAME_CTRDEF);
  def->padding = 0;
  def->n_counters = n_counters;

  for (i = 0; i < n_counters; i++)
    def->counters[i] = counters[i];

  self->stat.frame_count[SP_CAPTURE_FRAME_CTRDEF]++;

  return TRUE;
}

gboolean
sp_capture_writer_set_counters (SpCaptureWriter             *self,
                                gint64                       time,
                                gint                         cpu,
                                GPid                         pid,
                                const guint                 *counters_ids,
                                const SpCaptureCounterValue *values,
                                guint                        n_counters)
{
  SpCaptureFrameCounterSet *set;
  gsize len;
  guint n_groups;
  guint group;
  guint field;
  guint i;

  g_assert (self != NULL);
  g_assert (counters_ids != NULL);
  g_assert (values != NULL || !n_counters);

  if (n_counters == 0)
    return TRUE;

  /* Determine how many value groups we need */
  n_groups = n_counters / G_N_ELEMENTS (set->values[0].values);
  if ((n_groups * G_N_ELEMENTS (set->values[0].values)) != n_counters)
    n_groups++;

  len = sizeof *set + (n_groups * sizeof (SpCaptureCounterValues));

  set = (SpCaptureFrameCounterSet *)sp_capture_writer_allocate (self, &len);
  if (!set)
    return FALSE;

  memset (set, 0, len);

  sp_capture_writer_frame_init (&set->frame,
                                len,
                                cpu,
                                pid,
                                time,
                                SP_CAPTURE_FRAME_CTRSET);
  set->padding = 0;
  set->n_values = n_groups;

  for (i = 0, group = 0, field = 0; i < n_counters; i++)
    {
      set->values[group].ids[field] = counters_ids[i];
      set->values[group].values[field] = values[i];

      field++;

      if (field == G_N_ELEMENTS (set->values[0].values))
        {
          field = 0;
          group++;
        }
    }

  self->stat.frame_count[SP_CAPTURE_FRAME_CTRSET]++;

  return TRUE;
}

gint
sp_capture_writer_request_counter (SpCaptureWriter *self,
                                   guint            n_counters)
{
  gint ret;

  g_assert (self != NULL);

  ret = self->next_counter_id;
  self->next_counter_id += n_counters;

  return ret;
}

