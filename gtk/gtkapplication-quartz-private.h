/*
 * Copyright © 2010 Codethink Limited
 * Copyright © 2013 Canonical Limited
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the licence, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Ryan Lortie <desrt@desrt.ca>
 */

#include "gtkmenutrackerprivate.h"
#import <Cocoa/Cocoa.h>

@interface GNSMenuItem : NSMenuItem
{
  GtkMenuTrackerItem *trackerItem;
  gulong trackerItemChangedHandler;
  GCancellable *cancellable;
  BOOL isSpecial;
}

- (id)initWithTrackerItem:(GtkMenuTrackerItem *)aTrackerItem;

- (void)didChangeLabel;
- (void)didChangeIcon;
- (void)didChangeVisible;
- (void)didChangeToggled;
- (void)didChangeAccel;

- (void)didSelectItem:(id)sender;
- (BOOL)validateMenuItem:(NSMenuItem *)menuItem;

@end
