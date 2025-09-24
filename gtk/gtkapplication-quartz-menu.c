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
#include "gtkicontheme.h"
#include "gtkprivate.h"
#include "gtkwidgetprivate.h"

#include <gdk/macos/gdkmacos.h>
#include <gdk/macos/gdkmacoskeymap-private.h>

#import "gtkapplication-quartz-private.h"

#define ICON_SIZE 16

#define BLACK               "#000000"
#define TANGO_CHAMELEON_3   "#4e9a06"
#define TANGO_ORANGE_2      "#f57900"
#define TANGO_SCARLET_RED_2 "#cc0000"

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

static void
tracker_item_changed (GObject    *object,
                      GParamSpec *pspec,
                      gpointer    user_data)
{
  GNSMenuItem *item = user_data;
  const char *name = g_param_spec_get_name (pspec);

  if (name != NULL)
    {
      if (g_str_equal (name, "label"))
        [item didChangeLabel];
      else if (g_str_equal (name, "icon"))
        [item didChangeIcon];
      else if (g_str_equal (name, "is-visible"))
        [item didChangeVisible];
      else if (g_str_equal (name, "toggled"))
        [item didChangeToggled];
      else if (g_str_equal (name, "accel"))
        [item didChangeAccel];
    }
}

@implementation GNSMenuItem

- (id)initWithTrackerItem:(GtkMenuTrackerItem *)aTrackerItem
{
  self = [super initWithTitle:@""
                       action:@selector(didSelectItem:)
                keyEquivalent:@""];

  if (self != nil)
    {
      const char *action_name = gtk_menu_tracker_item_get_action_name (aTrackerItem);
      const char *special = gtk_menu_tracker_item_get_special (aTrackerItem);

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
      else if (special && g_str_equal (special, "services-submenu"))
        {
          [self setSubmenu:[[[NSMenu alloc] init] autorelease]];
          [NSApp setServicesMenu:[self submenu]];
          [self setTarget:self];
        }
      else if (action_name && g_str_equal (action_name, "text.undo"))
        [self setAction:@selector(undo:)];
      else if (action_name && g_str_equal (action_name, "text.redo"))
        [self setAction:@selector(redo:)];
      else if (action_name && g_str_equal (action_name, "clipboard.cut"))
        [self setAction:@selector(cut:)];
      else if (action_name && g_str_equal (action_name, "clipboard.copy"))
        [self setAction:@selector(copy:)];
      else if (action_name && g_str_equal (action_name, "clipboard.paste"))
        [self setAction:@selector(paste:)];
      else if (action_name && g_str_equal (action_name, "selection.select-all"))
        [self setAction:@selector(selectAll:)];
      else
        [self setTarget:self];

      trackerItem = g_object_ref (aTrackerItem);
      trackerItemChangedHandler = g_signal_connect (trackerItem, "notify", G_CALLBACK (tracker_item_changed), self);
      isSpecial = (special != NULL);

      [self didChangeLabel];
      [self didChangeIcon];
      [self didChangeVisible];
      [self didChangeToggled];
      [self didChangeAccel];

      if (gtk_menu_tracker_item_get_has_link (trackerItem, G_MENU_LINK_SUBMENU))
        {
          NSMenu *submenu = [[GNSMenu alloc] initWithTitle:[self title] trackerItem:trackerItem];

          if (special && g_str_equal (special, "window-submenu"))
            [NSApp setWindowsMenu:[submenu autorelease]];

          [self setSubmenu:submenu];
        }
    }

  return self;
}

- (void)dealloc
{
  if (cancellable != NULL)
    {
      g_cancellable_cancel (cancellable);
      g_clear_object (&cancellable);
    }

  g_signal_handler_disconnect (trackerItem, trackerItemChangedHandler);
  g_object_unref (trackerItem);

  [super dealloc];
}

- (void)didChangeLabel
{
  char *label = _gtk_elide_underscores (gtk_menu_tracker_item_get_label (trackerItem));

  NSString *title = [NSString stringWithUTF8String:label ? : ""];

  if (isSpecial)
    {
      NSRange range = [title rangeOfString:@"%s"];

      if (range.location != NSNotFound)
        {
          NSBundle *bundle = [NSBundle mainBundle];
          NSString *name = [[bundle localizedInfoDictionary] objectForKey:@"CFBundleName"];

          if (name == nil)
            name = [[bundle infoDictionary] objectForKey:@"CFBundleName"];

          if (name == nil)
            name = [[NSProcessInfo processInfo] processName];

          if (name != nil)
            title = [title stringByReplacingCharactersInRange:range withString:name];
        }
    }

  [self setTitle:title];

  g_free (label);
}

- (void)didChangeIcon
{
#if 0
  GIcon *icon = gtk_menu_tracker_item_get_icon (trackerItem);

  if (cancellable != NULL)
    {
      g_cancellable_cancel (cancellable);
      g_clear_object (&cancellable);
    }

  if (icon != NULL)
    {
      static gboolean parsed;

      static GdkRGBA foreground;
      static GdkRGBA success;
      static GdkRGBA warning;
      static GdkRGBA error;

      GtkIconTheme *theme;
      GtkIconPaintable *icon;
      int scale = 1;

      if (!parsed)
        {
          gdk_rgba_parse (&foreground, BLACK);
          gdk_rgba_parse (&success, TANGO_CHAMELEON_3);
          gdk_rgba_parse (&warning, TANGO_ORANGE_2);
          gdk_rgba_parse (&error, TANGO_SCARLET_RED_2);

          parsed = TRUE;
        }

      theme = gtk_icon_theme_get_for_display (gdk_display_get_default ());

       /* we need a run-time check for the backingScaleFactor selector because we
        * may be compiling on a 10.7 framework, but targeting a 10.6 one
        */
      if ([[NSScreen mainScreen] respondsToSelector:@selector(backingScaleFactor)])
        scale = roundf ([[NSScreen mainScreen] backingScaleFactor]);

      icon = gtk_icon_theme_lookup_by_gicon (theme, icon, ICON_SIZE, scale, 0);

      if (icon != NULL)
        {
          cancellable = g_cancellable_new ();
          gtk_icon_load_symbolic_async (icon, &foreground, &success, &warning, &error,
                                        cancellable, icon_loaded, self);
          g_object_unref (icon);
          return;
        }
    }
#endif

  [self setImage:nil];
}

- (void)didChangeVisible
{
  [self setHidden:gtk_menu_tracker_item_get_is_visible (trackerItem) ? NO : YES];
}

- (void)didChangeToggled
{
  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  [self setState:gtk_menu_tracker_item_get_toggled (trackerItem) ? NSOnState : NSOffState];
  G_GNUC_END_IGNORE_DEPRECATIONS
}

- (void)didChangeAccel
{
  const char *accel = gtk_menu_tracker_item_get_accel (trackerItem);

  if (accel != NULL)
    {
      guint key;
      GdkModifierType mask;
      unichar character;
      NSUInteger modifiers;

      gtk_accelerator_parse (accel, &key, &mask);

      character = _gdk_macos_keymap_get_equivalent (key);
      [self setKeyEquivalent:[NSString stringWithCharacters:&character length:1]];

      modifiers = 0;
      if (mask & GDK_SHIFT_MASK)
        modifiers |= NSEventModifierFlagShift;
      if (mask & GDK_CONTROL_MASK)
        modifiers |= NSEventModifierFlagControl;
      if (mask & GDK_ALT_MASK)
        modifiers |= NSEventModifierFlagOption;
      if (mask & GDK_META_MASK)
        modifiers |= NSEventModifierFlagCommand;
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
  /* Mimic macOS' behavior of traversing the reponder chain. */
  GtkWidget *focus_widget = [self findFocusWidget];
  const char *action_name = gtk_menu_tracker_item_get_action_name (trackerItem);

  if (focus_widget != NULL && action_name != NULL)
    {
      GVariant *action_target = gtk_menu_tracker_item_get_action_target (trackerItem);
      gtk_widget_activate_action_variant (focus_widget, action_name, action_target);
      if (action_target)
        g_variant_unref (action_target);
    }
  else
    gtk_menu_tracker_item_activated (trackerItem);
}

- (BOOL)validateMenuItem:(NSMenuItem *)menuItem
{
  /* Mimic macOS' behavior of traversing the reponder chain. */
  GtkWidget *focus_widget = [self findFocusWidget];
  if (focus_widget != NULL && gtk_widget_get_sensitive (focus_widget))
    {
      const char *action_name = gtk_menu_tracker_item_get_action_name (trackerItem);
      gboolean enabled = FALSE;
      GtkActionMuxer *muxer =  _gtk_widget_get_action_muxer (focus_widget, FALSE);

      if (action_name == NULL || muxer == NULL)
        return gtk_menu_tracker_item_get_sensitive (trackerItem) ? YES : NO;

      if (gtk_action_muxer_query_action (muxer, action_name, &enabled, NULL, NULL, NULL, NULL))
        return enabled ? YES : NO;
    }
  return gtk_menu_tracker_item_get_sensitive (trackerItem) ? YES : NO;
}

-(GtkWidget *)findFocusWidget
{
  GApplication *app = g_application_get_default ();
  GtkWindow *window;

  if (!GTK_IS_APPLICATION (app))
    return NULL;

  window = gtk_application_get_active_window (GTK_APPLICATION (app));
  if (window != NULL)
    return gtk_window_get_focus (window);
  return NULL;
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
                    int                 position,
                    gpointer            user_data)
{
  GNSMenu *menu = user_data;

  [menu insertItem:[NSMenuItem menuItemForTrackerItem:item] atIndex:position];
}

static void
menu_item_removed (int      position,
                   gpointer user_data)
{
  GNSMenu *menu = user_data;

  [menu removeItemAtIndex:position];
}

@implementation GNSMenu

- (id)initWithTitle:(NSString *)title model:(GMenuModel *)model observable:(GtkActionObservable *)observable
{
  self = [super initWithTitle:title];

  if (self != nil)
    {
      tracker = gtk_menu_tracker_new (observable,
                                      model,
                                      NO,
                                      YES,
                                      YES,
                                      NULL,
                                      menu_item_inserted,
                                      menu_item_removed,
                                      self);
    }

  return self;
}

- (id)initWithTitle:(NSString *)title trackerItem:(GtkMenuTrackerItem *)trackerItem
{
  self = [super initWithTitle:title];

  if (self != nil)
    {
      tracker = gtk_menu_tracker_new_for_item_link (trackerItem,
                                                       G_MENU_LINK_SUBMENU,
                                                       YES,
                                                       YES,
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
