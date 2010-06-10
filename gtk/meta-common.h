/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* This is the same as Mutter's common.h, but stripped-down to have
 * only the types and macros required by the theming code.
 */

/* Mutter common types shared by core.h and ui.h */

/* 
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2005 Elijah Newren
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#ifndef META_COMMON_H
#define META_COMMON_H

/* Don't include GTK or core headers here */
#include <glib.h>

typedef enum
{
  META_FRAME_ALLOWS_DELETE            = 1 << 0,
  META_FRAME_ALLOWS_MENU              = 1 << 1,
  META_FRAME_ALLOWS_MINIMIZE          = 1 << 2,
  META_FRAME_ALLOWS_MAXIMIZE          = 1 << 3,
  META_FRAME_ALLOWS_VERTICAL_RESIZE   = 1 << 4,
  META_FRAME_ALLOWS_HORIZONTAL_RESIZE = 1 << 5,
  META_FRAME_HAS_FOCUS                = 1 << 6,
  META_FRAME_SHADED                   = 1 << 7,
  META_FRAME_STUCK                    = 1 << 8,
  META_FRAME_MAXIMIZED                = 1 << 9,
  META_FRAME_ALLOWS_SHADE             = 1 << 10,
  META_FRAME_ALLOWS_MOVE              = 1 << 11,
  META_FRAME_FULLSCREEN               = 1 << 12,
  META_FRAME_IS_FLASHING              = 1 << 13,
  META_FRAME_ABOVE                    = 1 << 14
} MetaFrameFlags;

typedef enum
{
  META_FRAME_TYPE_NORMAL,
  META_FRAME_TYPE_DIALOG,
  META_FRAME_TYPE_MODAL_DIALOG,
  META_FRAME_TYPE_UTILITY,
  META_FRAME_TYPE_MENU,
  META_FRAME_TYPE_BORDER,
  META_FRAME_TYPE_LAST
} MetaFrameType;

/* Function a window button can have.  Note, you can't add stuff here
 * without extending the theme format to draw a new function and
 * breaking all existing themes.
 */
typedef enum
{
  META_BUTTON_FUNCTION_MENU,
  META_BUTTON_FUNCTION_MINIMIZE,
  META_BUTTON_FUNCTION_MAXIMIZE,
  META_BUTTON_FUNCTION_CLOSE,
  META_BUTTON_FUNCTION_SHADE,
  META_BUTTON_FUNCTION_ABOVE,
  META_BUTTON_FUNCTION_STICK,
  META_BUTTON_FUNCTION_UNSHADE,
  META_BUTTON_FUNCTION_UNABOVE,
  META_BUTTON_FUNCTION_UNSTICK,
  META_BUTTON_FUNCTION_LAST
} MetaButtonFunction;

#define MAX_BUTTONS_PER_CORNER META_BUTTON_FUNCTION_LAST

typedef struct _MetaButtonLayout MetaButtonLayout;
struct _MetaButtonLayout
{
  /* buttons in the group on the left side */
  MetaButtonFunction left_buttons[MAX_BUTTONS_PER_CORNER];
  gboolean left_buttons_has_spacer[MAX_BUTTONS_PER_CORNER];

  /* buttons in the group on the right side */
  MetaButtonFunction right_buttons[MAX_BUTTONS_PER_CORNER];
  gboolean right_buttons_has_spacer[MAX_BUTTONS_PER_CORNER];
};

#endif
