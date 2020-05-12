/* gdkcursor-macos.c
 *
 * Copyright (C) 2005-2007 Imendio AB
 * Copyright (C) 2020 Red Hat, Inc.
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gdkmacoscursor-private.h"

/* OS X only exports a number of cursor types in its public NSCursor interface.
 * By overriding the private _coreCursorType method, we can tell OS X to load
 * one of its internal cursors instead (since cursor images are loaded on demand
 * instead of in advance). WebKit does this too.
 */

@interface gdkCoreCursor : NSCursor {
@private
        int type;
        BOOL override;
}
@end

@implementation gdkCoreCursor

- (long long)_coreCursorType
{
  if (self->override)
    return self->type;
  return [super _coreCursorType];
}

#define CUSTOM_CURSOR_CTOR(name, id) \
        + (gdkCoreCursor *)name \
        { \
                gdkCoreCursor *obj; \
                obj = [self new]; \
                if (obj) { \
                        obj->override = YES; \
                        obj->type = id; \
                } \
                return obj; \
        }
CUSTOM_CURSOR_CTOR(gdkHelpCursor, 40)
CUSTOM_CURSOR_CTOR(gdkProgressCursor, 4)
/* TODO OS X doesn't seem to have a way to get this. There is an undocumented
 * method +[NSCursor _waitCursor], but it doesn't actually return this cursor,
 * but rather some odd low-quality non-animating version of this cursor. Use
 * the progress cursor instead for now.
 */
CUSTOM_CURSOR_CTOR(gdkWaitCursor, 4)
CUSTOM_CURSOR_CTOR(gdkAliasCursor, 2)
CUSTOM_CURSOR_CTOR(gdkMoveCursor, 39)
/* TODO OS X doesn't seem to provide one; copy the move cursor for now
 *  since it looks similar to what we want. */
CUSTOM_CURSOR_CTOR(gdkAllScrollCursor, 39)
CUSTOM_CURSOR_CTOR(gdkNEResizeCursor, 29)
CUSTOM_CURSOR_CTOR(gdkNWResizeCursor, 33)
CUSTOM_CURSOR_CTOR(gdkSEResizeCursor, 35)
CUSTOM_CURSOR_CTOR(gdkSWResizeCursor, 37)
CUSTOM_CURSOR_CTOR(gdkEWResizeCursor, 28)
CUSTOM_CURSOR_CTOR(gdkNSResizeCursor, 32)
CUSTOM_CURSOR_CTOR(gdkNESWResizeCursor, 30)
CUSTOM_CURSOR_CTOR(gdkNWSEResizeCursor, 34)
CUSTOM_CURSOR_CTOR(gdkZoomInCursor, 42)
CUSTOM_CURSOR_CTOR(gdkZoomOutCursor, 43)
#undef CUSTOM_CURSOR_CTOR

@end

struct CursorsByName {
  const gchar *name;
  NSString *selector;
};

static const struct CursorsByName cursors_by_name[] = {
  /* Link & Status */
  { "context-menu", @"contextualMenuCursor" },
  { "help", @"gdkHelpCursor" },
  { "pointer", @"pointingHandCursor" },
  { "progress", @"gdkProgressCursor" },
  { "wait", @"gdkWaitCursor" },
  /* Selection */
  { "cell", @"crosshairCursor" },
  { "crosshair", @"crosshairCursor" },
  { "text", @"IBeamCursor" },
  { "vertical-text", @"IBeamCursorForVerticalLayout" },
  /* Drag & Drop */
  { "alias", @"gdkAliasCursor" },
  { "copy", @"dragCopyCursor" },
  { "move", @"gdkMoveCursor" },
  { "no-drop", @"operationNotAllowedCursor" },
  { "not-allowed", @"operationNotAllowedCursor" },
  { "grab", @"openHandCursor" },
  { "grabbing", @"closedHandCursor" },
  /* Resize & Scrolling */
  { "all-scroll", @"gdkAllScrollCursor" },
  { "col-resize", @"resizeLeftRightCursor" },
  { "row-resize", @"resizeUpDownCursor" },
  { "n-resize", @"resizeUpCursor" },
  { "e-resize", @"resizeRightCursor" },
  { "s-resize", @"resizeDownCursor" },
  { "w-resize", @"resizeLeftCursor" },
  { "ne-resize", @"gdkNEResizeCursor" },
  { "nw-resize", @"gdkNWResizeCursor" },
  { "se-resize", @"gdkSEResizeCursor" },
  { "sw-resize", @"gdkSWResizeCursor" },
  { "ew-resize", @"gdkEWResizeCursor" },
  { "ns-resize", @"gdkNSResizeCursor" },
  { "nesw-resize", @"gdkNESWResizeCursor" },
  { "nwse-resize", @"gdkNWSEResizeCursor" },
  /* Zoom */
  { "zoom-in", @"gdkZoomInCursor" },
  { "zoom-out", @"gdkZoomOutCursor" },
  { NULL, NULL },
};

NSCursor *
_gdk_macos_cursor_get_ns_cursor (GdkCursor *cursor)
{
  return NULL;
}
