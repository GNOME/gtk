/*
 * Copyright © 2011 William Hua, Ryan Lortie
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
 * Author: William Hua <william@attente.ca>
 *         Ryan Lortie <desrt@desrt.ca>
 */

#include "config.h"

#include "gtkapplicationprivate.h"

#include <gdk/quartz/gdkquartz.h>
#include <gdk/gdkkeysyms.h>
#include "gtkmenutracker.h"
#include "gtkicontheme.h"
#import <Cocoa/Cocoa.h>

#define BUFFER_SIZE 1024
#define ICON_SIZE 16

@interface GNSMenu : NSMenu
{
  GtkMenuTracker *tracker;
}

- (id)initWithTitle:(NSString *)title model:(GMenuModel *)model observable:(GtkActionObservable *)observable;

- (id)initWithTitle:(NSString *)title trackerItem:(GtkMenuTrackerItem *)trackerItem;

@end

@interface NSMenuItem (GtkMenuTrackerItem)

+ (id)menuItemForTrackerItem:(GtkMenuTrackerItem *)trackerItem;

@end

@interface GNSMenuItem : NSMenuItem
{
  GtkMenuTrackerItem *trackerItem;
  gulong trackerItemChangedHandler;
  NSMutableData *iconData;
  gpointer iconBuffer;
  GCancellable *cancellable;
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

@interface GNSMenuItem ()

- (void)setImageFromLoadableIcon:(GLoadableIcon *)icon;
- (void)readDataFromStream:(GInputStream *)stream;
- (void)readDataFromStream:(GInputStream *)stream count:(gssize)count;
- (void)reset;

@end

/*
 * Code for key code conversion
 *
 * Copyright (C) 2009 Paul Davis
 */
static unichar
get_key_equivalent (guint key)
{
  if (key >= GDK_KEY_A && key <= GDK_KEY_Z)
    return key + (GDK_KEY_a - GDK_KEY_A);

  if (key >= GDK_KEY_space && key <= GDK_KEY_asciitilde)
    return key;

  switch (key)
    {
      case GDK_KEY_BackSpace:
        return NSBackspaceCharacter;
      case GDK_KEY_Delete:
        return NSDeleteFunctionKey;
      case GDK_KEY_Pause:
        return NSPauseFunctionKey;
      case GDK_KEY_Scroll_Lock:
        return NSScrollLockFunctionKey;
      case GDK_KEY_Sys_Req:
        return NSSysReqFunctionKey;
      case GDK_KEY_Home:
        return NSHomeFunctionKey;
      case GDK_KEY_Left:
      case GDK_KEY_leftarrow:
        return NSLeftArrowFunctionKey;
      case GDK_KEY_Up:
      case GDK_KEY_uparrow:
        return NSUpArrowFunctionKey;
      case GDK_KEY_Right:
      case GDK_KEY_rightarrow:
        return NSRightArrowFunctionKey;
      case GDK_KEY_Down:
      case GDK_KEY_downarrow:
        return NSDownArrowFunctionKey;
      case GDK_KEY_Page_Up:
        return NSPageUpFunctionKey;
      case GDK_KEY_Page_Down:
        return NSPageDownFunctionKey;
      case GDK_KEY_End:
        return NSEndFunctionKey;
      case GDK_KEY_Begin:
        return NSBeginFunctionKey;
      case GDK_KEY_Select:
        return NSSelectFunctionKey;
      case GDK_KEY_Print:
        return NSPrintFunctionKey;
      case GDK_KEY_Execute:
        return NSExecuteFunctionKey;
      case GDK_KEY_Insert:
        return NSInsertFunctionKey;
      case GDK_KEY_Undo:
        return NSUndoFunctionKey;
      case GDK_KEY_Redo:
        return NSRedoFunctionKey;
      case GDK_KEY_Menu:
        return NSMenuFunctionKey;
      case GDK_KEY_Find:
        return NSFindFunctionKey;
      case GDK_KEY_Help:
        return NSHelpFunctionKey;
      case GDK_KEY_Break:
        return NSBreakFunctionKey;
      case GDK_KEY_Mode_switch:
        return NSModeSwitchFunctionKey;
      case GDK_KEY_F1:
        return NSF1FunctionKey;
      case GDK_KEY_F2:
        return NSF2FunctionKey;
      case GDK_KEY_F3:
        return NSF3FunctionKey;
      case GDK_KEY_F4:
        return NSF4FunctionKey;
      case GDK_KEY_F5:
        return NSF5FunctionKey;
      case GDK_KEY_F6:
        return NSF6FunctionKey;
      case GDK_KEY_F7:
        return NSF7FunctionKey;
      case GDK_KEY_F8:
        return NSF8FunctionKey;
      case GDK_KEY_F9:
        return NSF9FunctionKey;
      case GDK_KEY_F10:
        return NSF10FunctionKey;
      case GDK_KEY_F11:
        return NSF11FunctionKey;
      case GDK_KEY_F12:
        return NSF12FunctionKey;
      case GDK_KEY_F13:
        return NSF13FunctionKey;
      case GDK_KEY_F14:
        return NSF14FunctionKey;
      case GDK_KEY_F15:
        return NSF15FunctionKey;
      case GDK_KEY_F16:
        return NSF16FunctionKey;
      case GDK_KEY_F17:
        return NSF17FunctionKey;
      case GDK_KEY_F18:
        return NSF18FunctionKey;
      case GDK_KEY_F19:
        return NSF19FunctionKey;
      case GDK_KEY_F20:
        return NSF20FunctionKey;
      case GDK_KEY_F21:
        return NSF21FunctionKey;
      case GDK_KEY_F22:
        return NSF22FunctionKey;
      case GDK_KEY_F23:
        return NSF23FunctionKey;
      case GDK_KEY_F24:
        return NSF24FunctionKey;
      case GDK_KEY_F25:
        return NSF25FunctionKey;
      case GDK_KEY_F26:
        return NSF26FunctionKey;
      case GDK_KEY_F27:
        return NSF27FunctionKey;
      case GDK_KEY_F28:
        return NSF28FunctionKey;
      case GDK_KEY_F29:
        return NSF29FunctionKey;
      case GDK_KEY_F30:
        return NSF30FunctionKey;
      case GDK_KEY_F31:
        return NSF31FunctionKey;
      case GDK_KEY_F32:
        return NSF32FunctionKey;
      case GDK_KEY_F33:
        return NSF33FunctionKey;
      case GDK_KEY_F34:
        return NSF34FunctionKey;
      case GDK_KEY_F35:
        return NSF35FunctionKey;
      default:
        break;
    }

  return '\0';
}

static void
pixbuf_loaded (GObject      *object,
               GAsyncResult *result,
               gpointer      user_data)
{
  GtkIconInfo *info = GTK_ICON_INFO (object);
  GNSMenuItem *item = user_data;
  GError *error = NULL;
  GdkPixbuf *pixbuf = gtk_icon_info_load_symbolic_finish (info, result, NULL, &error);

  if (pixbuf != NULL)
    {
      if (G_IS_LOADABLE_ICON (pixbuf))
        [item setImageFromLoadableIcon:G_LOADABLE_ICON (pixbuf)];

      g_object_unref (pixbuf);
    }

  if (error != NULL)
    g_error_free (error);
}

static void
input_stream_created (GObject      *object,
                      GAsyncResult *result,
                      gpointer      user_data)
{
  GLoadableIcon *icon = G_LOADABLE_ICON (object);
  GNSMenuItem *item = user_data;
  GError *error = NULL;
  GInputStream *stream = g_loadable_icon_load_finish (icon, result, NULL, &error);

  if (stream != NULL)
    {
      [item readDataFromStream:stream];
      g_object_unref (stream);
    }
  else
    [item reset];

  if (error != NULL)
    g_error_free (error);
}

static void
input_stream_read (GObject      *object,
                   GAsyncResult *result,
                   gpointer      user_data)
{
  GInputStream *stream = G_INPUT_STREAM (object);
  GNSMenuItem *item = user_data;
  GError *error = NULL;
  gssize count = g_input_stream_read_finish (stream, result, &error);

  if (count >= 0)
    [item readDataFromStream:stream count:count];
  else
    [item reset];

  if (error != NULL)
    g_error_free (error);
}

static void
tracker_item_changed (GObject    *object,
                      GParamSpec *pspec,
                      gpointer    user_data)
{
  static const gchar *label = NULL;
  static const gchar *icon = NULL;
  static const gchar *visible = NULL;
  static const gchar *toggled = NULL;
  static const gchar *accel = NULL;

  GNSMenuItem *item = user_data;
  const gchar *name = g_param_spec_get_name (pspec);

  if (G_UNLIKELY (label == NULL))
    label = g_intern_static_string ("label");
  if (G_UNLIKELY (icon == NULL))
    icon = g_intern_static_string ("icon");
  if (G_UNLIKELY (visible == NULL))
    visible = g_intern_static_string ("visible");
  if (G_UNLIKELY (toggled == NULL))
    toggled = g_intern_static_string ("toggled");
  if (G_UNLIKELY (accel == NULL))
    accel = g_intern_static_string ("accel");

  if (name == label)
    [item didChangeLabel];
  else if (name == icon)
    [item didChangeIcon];
  else if (name == visible)
    [item didChangeVisible];
  else if (name == toggled)
    [item didChangeToggled];
  else if (name == accel)
    [item didChangeAccel];
}

@implementation GNSMenuItem

- (BOOL)validateMenuItem:(NSMenuItem *)menuItem
{
  return gtk_menu_tracker_item_get_sensitive (trackerItem) ? YES : NO;
}

- (id)initWithTrackerItem:(GtkMenuTrackerItem *)aTrackerItem
{
  if ((self = [super initWithTitle:@""
                            action:@selector(didSelectItem:)
                     keyEquivalent:@""]) != nil)
    {
      const gchar *special = gtk_menu_tracker_item_get_special (aTrackerItem);

      if (special && g_str_equal (special, "hide-this"))
        {
          [self setAction:@selector(hide:)];
          [self setTarget:NSApp];
        }
      else if (special && g_str_equal (special, "hide-others"))
        {
          [self setAction:@selector(hideOtherApplications:)];
          [self setTarget:NSApp];
        }
      else if (special && g_str_equal (special, "show-all"))
        {
          [self setAction:@selector(unhideAllApplications:)];
          [self setTarget:NSApp];
        }
      else
        [self setTarget:self];

      trackerItem = g_object_ref (aTrackerItem);
      trackerItemChangedHandler = g_signal_connect (trackerItem, "notify", G_CALLBACK (tracker_item_changed), self);

      [self didChangeLabel];
      [self didChangeIcon];
      [self didChangeVisible];
      [self didChangeToggled];
      [self didChangeAccel];

      if (gtk_menu_tracker_item_get_has_submenu (trackerItem))
        {
          [self setSubmenu:[[[GNSMenu alloc] initWithTitle:[self title] trackerItem:trackerItem] autorelease]];

          if (special && g_str_equal (special, "services-submenu"))
            [NSApp setServicesMenu:[self submenu]];
        }
    }

  return self;
}

- (void)dealloc
{
  if (cancellable != NULL)
    {
      g_cancellable_cancel (cancellable);
      [self reset];
    }

  g_signal_handler_disconnect (trackerItem, trackerItemChangedHandler);
  g_object_unref (trackerItem);

  [super dealloc];
}

- (void)reset
{
  g_clear_object (&cancellable);
  g_free (iconBuffer);
  iconBuffer = NULL;
  [iconData release];
  iconData = nil;
}

- (void)readDataFromStream:(GInputStream *)stream
{
  g_return_if_fail (G_IS_INPUT_STREAM (stream));

  g_input_stream_read_async (stream,
                             iconBuffer,
                             BUFFER_SIZE,
                             G_PRIORITY_LOW,
                             cancellable,
                             input_stream_read,
                             self);
}

- (void)readDataFromStream:(GInputStream *)stream count:(gssize)count
{
  g_return_if_fail (G_IS_INPUT_STREAM (stream));
  g_return_if_fail (count >= 0);

  if (count > 0)
    {
      [iconData appendBytes:iconBuffer length:count];
      [self readDataFromStream:stream];
    }
  else if (count == 0)
    {
      NSImage *image = [[[NSImage alloc] initWithData:iconData] autorelease];
      [image setSize:NSMakeSize(16, 16)];
      [self setImage:image];
      [self reset];
    }
}

- (void)setImageFromLoadableIcon:(GLoadableIcon *)icon
{
  if (GDK_IS_PIXBUF (icon))
    {
      NSImage *image = gdk_quartz_pixbuf_to_ns_image_libgtk_only (GDK_PIXBUF (icon));
      [image setSize:NSMakeSize(16, 16)];
      [self setImage:image];
    }
  else
    {
      iconData = [[NSMutableData alloc] init];
      iconBuffer = g_malloc (BUFFER_SIZE);
      cancellable = g_cancellable_new ();
      g_loadable_icon_load_async (icon, ICON_SIZE, cancellable, input_stream_created, self);
    }
}

- (void)didChangeLabel
{
  NSString *label = [NSString stringWithUTF8String:gtk_menu_tracker_item_get_label (trackerItem) ? : ""];
  NSMutableString *title = [NSMutableString stringWithCapacity:[label length]];
  NSRange range;
  int i;

  range = [label rangeOfString:@"`gtk-private-appname`"];
  if (range.location != NSNotFound)
    label = [label stringByReplacingCharactersInRange:range withString:[[NSProcessInfo processInfo] processName]];

  for (i = 0; i < [label length]; i++)
    {
      unichar c = [label characterAtIndex:i];

      if (c == '_')
        {
          i++;

          if (i >= [label length])
            break;

          c = [label characterAtIndex:i];
        }

      [title appendString:[NSString stringWithCharacters:&c length:1]];
    }

  [self setTitle:title];
}

- (void)didChangeIcon
{
  GIcon *icon = gtk_menu_tracker_item_get_icon (trackerItem);

  if (cancellable != NULL)
    {
      g_cancellable_cancel (cancellable);
      [self reset];
    }

  while (G_IS_EMBLEM (icon) || G_IS_EMBLEMED_ICON (icon))
    {
      if (G_IS_EMBLEM (icon))
        icon = g_emblem_get_icon (G_EMBLEM (icon));
      else
        icon = g_emblemed_icon_get_icon (G_EMBLEMED_ICON (icon));
    }

  if (G_IS_FILE_ICON (icon))
    {
      NSImage *image = nil;
      GFile *file = g_file_icon_get_file (G_FILE_ICON (icon));
      gchar *path = g_file_get_path (file);

      if (path != NULL)
        {
          image = [[[NSImage alloc] initByReferencingFile:[NSString stringWithUTF8String:path]] autorelease];
          g_free (path);
        }
      else
        {
          path = g_file_get_uri (file);

          if (path != NULL)
            {
              image = [[[NSImage alloc] initByReferencingURL:[NSURL URLWithString:[NSString stringWithUTF8String:path]]] autorelease];
              g_free (path);
            }
        }

      [image setSize:NSMakeSize(16, 16)];
      [self setImage:image];
    }
  else if (G_IS_THEMED_ICON (icon))
    {
      GtkIconTheme *theme = gtk_icon_theme_get_default ();

      if (theme != NULL)
        {
          GtkIconInfo *info = gtk_icon_theme_lookup_by_gicon (theme, icon, ICON_SIZE, GTK_ICON_LOOKUP_USE_BUILTIN);

          if (info != NULL)
            {
              GdkRGBA foreground = { 0.0, 0.0, 0.0, 1.0 };
              GdkRGBA success = { 0.0, 1.0, 0.0, 1.0 };
              GdkRGBA warning = { 1.0, 1.0, 0.0, 1.0 };
              GdkRGBA error = { 1.0, 0.0, 0.0, 1.0 };

              if (gtk_icon_info_is_symbolic (info))
                {
                  cancellable = g_cancellable_new ();
                  gtk_icon_info_load_symbolic_async (info, &foreground, &success, &warning, &error, cancellable, pixbuf_loaded, self);
                }
              else
                {
                  const gchar *path = gtk_icon_info_get_filename (info);

                  if (path != NULL)
                    {
                      NSImage *image = [[[NSImage alloc] initByReferencingFile:[NSString stringWithUTF8String:path]] autorelease];
                      [image setSize:NSMakeSize(16, 16)];
                      [self setImage:image];
                    }
                  else
                    {
                      GdkPixbuf *pixbuf = gtk_icon_info_get_builtin_pixbuf (info);

                      if (G_IS_LOADABLE_ICON (pixbuf))
                        [self setImageFromLoadableIcon:G_LOADABLE_ICON (pixbuf)];
                      else
                        {
                          cancellable = g_cancellable_new ();
                          gtk_icon_info_load_symbolic_async (info, &foreground, &success, &warning, &error, cancellable, pixbuf_loaded, self);
                        }
                    }
                }

              g_object_unref (info);
            }
        }
    }
  else if (G_IS_LOADABLE_ICON (icon))
    [self setImageFromLoadableIcon:G_LOADABLE_ICON (icon)];
  else
    [self setImage:nil];
}

- (void)didChangeVisible
{
  [self setHidden:gtk_menu_tracker_item_get_visible (trackerItem) ? NO : YES];
}

- (void)didChangeToggled
{
  [self setState:gtk_menu_tracker_item_get_toggled (trackerItem) ? NSOnState : NSOffState];
}

- (void)didChangeAccel
{
  const gchar *accel = gtk_menu_tracker_item_get_accel (trackerItem);

  if (accel != NULL)
    {
      guint key;
      GdkModifierType mask;
      unichar character;
      NSUInteger modifiers;

      gtk_accelerator_parse (accel, &key, &mask);

      character = get_key_equivalent (key);
      [self setKeyEquivalent:[NSString stringWithCharacters:&character length:1]];

      modifiers = 0;
      if (mask & GDK_SHIFT_MASK)
        modifiers |= NSShiftKeyMask;
      if (mask & GDK_CONTROL_MASK)
        modifiers |= NSControlKeyMask;
      if (mask & GDK_MOD1_MASK)
        modifiers |= NSAlternateKeyMask;
      if (mask & GDK_META_MASK)
        modifiers |= NSCommandKeyMask;
      [self setKeyEquivalentModifierMask:modifiers];
    }
  else
    {
      [self setKeyEquivalent:@""];
      [self setKeyEquivalentModifierMask:0];
    }
}

- (void)didSelectItem:(id)sender
{
  gtk_menu_tracker_item_activated (trackerItem);
}

@end

@implementation NSMenuItem (GtkMenuTrackerItem)

+ (id)menuItemForTrackerItem:(GtkMenuTrackerItem *)trackerItem
{
  if (gtk_menu_tracker_item_get_is_separator (trackerItem))
    return [NSMenuItem separatorItem];

  return [[[GNSMenuItem alloc] initWithTrackerItem:trackerItem] autorelease];
}

@end

static void
menu_item_inserted (GtkMenuTrackerItem *item,
                    gint                position,
                    gpointer            user_data)
{
  GNSMenu *menu = user_data;

  [menu insertItem:[NSMenuItem menuItemForTrackerItem:item] atIndex:position];
}

static void
menu_item_removed (gint     position,
                   gpointer user_data)
{
  GNSMenu *menu = user_data;

  [menu removeItemAtIndex:position];
}

@implementation GNSMenu

- (id)initWithTitle:(NSString *)title model:(GMenuModel *)model observable:(GtkActionObservable *)observable
{
  if ((self = [super initWithTitle:title]) != nil)
    {
      tracker = gtk_menu_tracker_new (observable,
                                      model,
                                      NO,
                                      NULL,
                                      menu_item_inserted,
                                      menu_item_removed,
                                      self);
    }

  return self;
}

- (id)initWithTitle:(NSString *)title trackerItem:(GtkMenuTrackerItem *)trackerItem
{
  if ((self = [super initWithTitle:title]) != nil)
    {
      tracker = gtk_menu_tracker_new_for_item_submenu (trackerItem,
                                                       menu_item_inserted,
                                                       menu_item_removed,
                                                       self);
    }

  return self;
}

- (void)dealloc
{
  gtk_menu_tracker_free (tracker);

  [super dealloc];
}

@end

void
gtk_application_impl_quartz_setup_menu (GMenuModel     *model,
                                        GtkActionMuxer *muxer)
{
  NSMenu *menu;

  if (model != NULL)
    menu = [[GNSMenu alloc] initWithTitle:@"Main Menu" model:model observable:GTK_ACTION_OBSERVABLE (muxer)];
  else
    menu = [[NSMenu alloc] init];

  [NSApp setMainMenu:menu];
  [menu release];
}
