/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#ifndef __GDK_FB_MANAGER_H__
#define __GDK_FB_MANAGER_H__

enum FBManagerMessageType {
  /* manager -> client */
  FB_MANAGER_SWITCH_TO,
  FB_MANAGER_SWITCH_FROM, /* requires ack */

  /* client -> manager */
  FB_MANAGER_NEW_CLIENT,
  FB_MANAGER_REQUEST_SWITCH_TO_PID,
  FB_MANAGER_ACK,
};

struct FBManagerMessage {
  enum FBManagerMessageType msg_type;
  int data;
};
#endif /* __GDK_FB_MANAGER_H__ */
