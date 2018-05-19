/* sp-capture-writer.h
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

#ifndef SP_CAPTURE_WRITER_H
#define SP_CAPTURE_WRITER_H

#include "capture/sp-capture-types.h"

G_BEGIN_DECLS

typedef struct _SpCaptureWriter SpCaptureWriter;

typedef struct
{
  /*
   * The number of frames indexed by SpCaptureFrameType
   */
  gsize frame_count[16];

  /*
   * Padding for future expansion.
   */
  gsize padding[48];
} SpCaptureStat;

SpCaptureWriter    *sp_capture_writer_new             (const gchar             *filename,
                                                       gsize                    buffer_size);
SpCaptureWriter    *sp_capture_writer_new_from_fd     (int                      fd,
                                                       gsize                    buffer_size);
SpCaptureWriter    *sp_capture_writer_ref             (SpCaptureWriter         *self);
void                sp_capture_writer_unref           (SpCaptureWriter         *self);
void                sp_capture_writer_stat            (SpCaptureWriter         *self,
                                                       SpCaptureStat           *stat);
gboolean            sp_capture_writer_add_map         (SpCaptureWriter         *self,
                                                       gint64                   time,
                                                       gint                     cpu,
                                                       GPid                     pid,
                                                       guint64                  start,
                                                       guint64                  end,
                                                       guint64                  offset,
                                                       guint64                  inode,
                                                       const gchar             *filename);
gboolean            sp_capture_writer_add_mark        (SpCaptureWriter         *self,
                                                       gint64                   time,
                                                       gint                     cpu,
                                                       GPid                     pid,
                                                       guint64                  duration,
                                                       const gchar             *group,
                                                       const gchar             *name,
                                                       const gchar             *message);
guint64             sp_capture_writer_add_jitmap      (SpCaptureWriter         *self,
                                                       const gchar             *name);
gboolean            sp_capture_writer_add_process     (SpCaptureWriter         *self,
                                                       gint64                   time,
                                                       gint                     cpu,
                                                       GPid                     pid,
                                                       const gchar             *cmdline);
gboolean            sp_capture_writer_add_sample      (SpCaptureWriter         *self,
                                                       gint64                   time,
                                                       gint                     cpu,
                                                       GPid                     pid,
                                                       const SpCaptureAddress  *addrs,
                                                       guint                    n_addrs);
gboolean            sp_capture_writer_add_fork        (SpCaptureWriter         *self,
                                                       gint64                   time,
                                                       gint                     cpu,
                                                       GPid                     pid,
                                                       GPid                     child_pid);
gboolean            sp_capture_writer_add_exit        (SpCaptureWriter         *self,
                                                       gint64                   time,
                                                       gint                     cpu,
                                                       GPid                     pid);
gboolean            sp_capture_writer_add_timestamp   (SpCaptureWriter         *self,
                                                       gint64                   time,
                                                       gint                     cpu,
                                                       GPid                     pid);
gboolean            sp_capture_writer_define_counters (SpCaptureWriter         *self,
                                                       gint64                   time,
                                                       gint                     cpu,
                                                       GPid                     pid,
                                                       const SpCaptureCounter  *counters,
                                                       guint                    n_counters);
gboolean            sp_capture_writer_set_counters    (SpCaptureWriter         *self,
                                                       gint64                   time,
                                                       gint                     cpu,
                                                       GPid                     pid,
                                                       const guint             *counters_ids,
                                                       const SpCaptureCounterValue *values,
                                                       guint                    n_counters);
gboolean            sp_capture_writer_flush           (SpCaptureWriter         *self);
gboolean            sp_capture_writer_save_as         (SpCaptureWriter         *self,
                                                       const gchar             *filename,
                                                       GError                 **error);
gint                sp_capture_writer_request_counter (SpCaptureWriter         *self,
                                                       guint                    n_counters);
SpCaptureReader    *sp_capture_writer_create_reader   (SpCaptureWriter         *self,
                                                       GError                 **error);
gboolean            sp_capture_writer_splice          (SpCaptureWriter         *self,
                                                       SpCaptureWriter         *dest,
                                                       GError                 **error);
gboolean            _sp_capture_writer_splice_from_fd (SpCaptureWriter         *self,
                                                       int                      fd,
                                                       GError                 **error) G_GNUC_INTERNAL;

#ifndef SP_DISABLE_GOBJECT
# define SP_TYPE_CAPTURE_WRITER (sp_capture_writer_get_type())
  GType sp_capture_writer_get_type (void);
#endif

#if GLIB_CHECK_VERSION(2, 44, 0)
  G_DEFINE_AUTOPTR_CLEANUP_FUNC (SpCaptureWriter, sp_capture_writer_unref)
#endif

G_END_DECLS

#endif /* SP_CAPTURE_WRITER_H */

