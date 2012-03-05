/* gdkeventsequence.h - tracking sequences of events
 * Copyright (C) 2012 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.▸ See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include "gdkeventsequenceprivate.h"

#include "gdkdeviceprivate.h"

/* XXX: Does not return a reference - should we change that? */
GdkEventSequence *
gdk_event_sequence_new (GdkDevice *device,
                        guint      sequence_id)
{
  GdkEventSequence *sequence = g_slice_new0 (GdkEventSequence);

  /* device owns sequence, so cannot ref device here */
  sequence->ref_count = 1;
  sequence->device = device;
  sequence->sequence_id = sequence_id;
  sequence->axes = g_new0 (gdouble, gdk_device_get_n_axes (device));

  device->sequences = g_slist_prepend (device->sequences, sequence);

  return sequence;
}

GdkEventSequence *
gdk_event_sequence_lookup (GdkDevice *device,
                           guint      sequence_id)
{
  GSList *list;

  for (list = device->sequences; list; list = list->next)
    {
      GdkEventSequence *sequence = list->data;

      if (sequence->sequence_id == sequence_id)
        return sequence;
    }

  return NULL;
}

GdkEventSequence *
gdk_event_sequence_ref (GdkEventSequence *sequence)
{
  sequence->ref_count++;

  return sequence;
}

void
gdk_event_sequence_unref (GdkEventSequence *sequence)
{
  sequence->ref_count--;
  if (sequence->ref_count > 0)
    return;

  if (sequence->device)
    {
      GdkDevice *device = sequence->device;

      device->sequences = g_slist_remove (device->sequences, sequence);
    }

  g_free (sequence->axes);
  g_slice_free (GdkEventSequence, sequence);
}
