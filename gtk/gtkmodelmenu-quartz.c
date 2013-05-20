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

#include "gtkmodelmenu-quartz.h"

#include <gdk/gdkkeysyms.h>
#include "gtkaccelmapprivate.h"
#include "gtkactionhelper.h"
#include "../gdk/quartz/gdkquartz.h"

#import <Cocoa/Cocoa.h>

/*
 * Code for key code conversion
 *
 * Copyright (C) 2009 Paul Davis
 */
static unichar
gtk_quartz_model_menu_get_unichar (gint key)
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



@interface GNSMenu : NSMenu
{
  GtkApplication *application;
  GMenuModel     *model;
  guint           update_idle;
  GSList         *connected;
  gboolean        with_separators;
}

- (id)initWithTitle:(NSString *)title model:(GMenuModel *)aModel application:(GtkApplication *)application hasSeparators:(BOOL)hasSeparators;

- (void)model:(GMenuModel *)model didChangeAtPosition:(NSInteger)position removed:(NSInteger)removed added:(NSInteger)added;

- (gboolean)handleChanges;

@end



@interface GNSMenuItem : NSMenuItem
{
  GtkActionHelper *helper;
}

- (id)initWithModel:(GMenuModel *)model index:(NSInteger)index application:(GtkApplication *)application;

- (void)didSelectItem:(id)sender;

- (void)helperChanged;

@end



static gboolean
gtk_quartz_model_menu_handle_changes (gpointer user_data)
{
  GNSMenu *menu = user_data;

  return [menu handleChanges];
}

static void
gtk_quartz_model_menu_items_changed (GMenuModel *model,
                               gint        position,
                               gint        removed,
                               gint        added,
                               gpointer    user_data)
{
  GNSMenu *menu = user_data;

  [menu model:model didChangeAtPosition:position removed:removed added:added];
}

void
gtk_quartz_set_main_menu (GMenuModel     *model,
                          GtkApplication *application)
{
  [NSApp setMainMenu:[[[GNSMenu alloc] initWithTitle:@"Main Menu" model:model application:application hasSeparators:NO] autorelease]];
}

void
gtk_quartz_clear_main_menu (void)
{
  // ensure that we drop all GNSMenuItem (to ensure 'application' has no extra references)
  [NSApp setMainMenu:[[[NSMenu alloc] init] autorelease]];
}

@interface GNSMenu ()

- (void)appendFromModel:(GMenuModel *)aModel withSeparators:(BOOL)withSeparators;

@end



@implementation GNSMenu

- (void)model:(GMenuModel *)model didChangeAtPosition:(NSInteger)position removed:(NSInteger)removed added:(NSInteger)added
{
  if (update_idle == 0)
    update_idle = gdk_threads_add_idle (gtk_quartz_model_menu_handle_changes, self);
}

- (void)appendItemFromModel:(GMenuModel *)aModel atIndex:(gint)index withHeading:(gchar **)heading
{
  GMenuModel *section;

  if ((section = g_menu_model_get_item_link (aModel, index, G_MENU_LINK_SECTION)))
    {
      g_menu_model_get_item_attribute (aModel, index, G_MENU_ATTRIBUTE_LABEL, "s", heading);
      [self appendFromModel:section withSeparators:NO];
      g_object_unref (section);
    }
  else
    [self addItem:[[[GNSMenuItem alloc] initWithModel:aModel index:index application:application] autorelease]];
}

- (void)appendFromModel:(GMenuModel *)aModel withSeparators:(BOOL)withSeparators
{
  gint n, i;

  g_signal_connect (aModel, "items-changed", G_CALLBACK (gtk_quartz_model_menu_items_changed), self);
  connected = g_slist_prepend (connected, g_object_ref (aModel));

  n = g_menu_model_get_n_items (aModel);

  for (i = 0; i < n; i++)
    {
      NSInteger ourPosition = [self numberOfItems];
      gchar *heading = NULL;

      [self appendItemFromModel:aModel atIndex:i withHeading:&heading];

      if (withSeparators && ourPosition < [self numberOfItems])
        {
          NSMenuItem *separator = nil;

          if (heading)
            {
              separator = [[[NSMenuItem alloc] initWithTitle:[NSString stringWithUTF8String:heading] action:NULL keyEquivalent:@""] autorelease];

              [separator setEnabled:NO];
            }
          else if (ourPosition > 0)
            separator = [NSMenuItem separatorItem];

          if (separator != nil)
            [self insertItem:separator atIndex:ourPosition];
        }

      g_free (heading);
    }
}

- (void)populate
{
  /* removeAllItems is available only in 10.6 and later, but it's more
     efficient than iterating over the array of
     NSMenuItems. performSelector: suppresses a compiler warning when
     building on earlier OSX versions. */
  if ([self respondsToSelector: @selector (removeAllItems)])
    [self performSelector: @selector (removeAllItems)];
  else
    {
      /* Iterate from the bottom up to save reindexing the NSArray. */
      int i;
      for (i = [self numberOfItems]; i > 0; i--)
	[self removeItemAtIndex: i];
    }

  [self appendFromModel:model withSeparators:with_separators];
}

- (gboolean)handleChanges
{
  while (connected)
    {
      g_signal_handlers_disconnect_by_func (connected->data, gtk_quartz_model_menu_items_changed, self);
      g_object_unref (connected->data);

      connected = g_slist_delete_link (connected, connected);
    }

  [self populate];

  update_idle = 0;

  return G_SOURCE_REMOVE;
}

- (id)initWithTitle:(NSString *)title model:(GMenuModel *)aModel application:(GtkApplication *)anApplication hasSeparators:(BOOL)hasSeparators
{
  if((self = [super initWithTitle:title]) != nil)
    {
      [self setAutoenablesItems:NO];

      model = g_object_ref (aModel);
      application = g_object_ref (anApplication);
      with_separators = hasSeparators;

      [self populate];
    }

  return self;
}

- (void)dealloc
{
  while (connected)
    {
      g_signal_handlers_disconnect_by_func (connected->data, gtk_quartz_model_menu_items_changed, self);
      g_object_unref (connected->data);

      connected = g_slist_delete_link (connected, connected);
    }

  g_object_unref (application);
  g_object_unref (model);

  [super dealloc];
}

@end



static void
gtk_quartz_action_helper_changed (GObject    *object,
                                  GParamSpec *pspec,
                                  gpointer    user_data)
{
  GNSMenuItem *item = user_data;

  [item helperChanged];
}

@implementation GNSMenuItem

- (id)initWithModel:(GMenuModel *)model index:(NSInteger)index application:(GtkApplication *)application
{
  gchar *title = NULL;

  if (g_menu_model_get_item_attribute (model, index, G_MENU_ATTRIBUTE_LABEL, "s", &title))
    {
      gchar *from, *to;

      to = from = title;

      while (*from)
        {
          if (*from == '_' && from[1])
            from++;

          *to++ = *from++;
        }

      *to = '\0';
    }

  if ((self = [super initWithTitle:[NSString stringWithUTF8String:title ? : ""] action:@selector(didSelectItem:) keyEquivalent:@""]) != nil)
    {
      GMenuModel *submenu;
      gchar      *action;
      GVariant   *target;

      action = NULL;
      g_menu_model_get_item_attribute (model, index, G_MENU_ATTRIBUTE_ACTION, "s", &action);
      target = g_menu_model_get_item_attribute_value (model, index, G_MENU_ATTRIBUTE_TARGET, NULL);

      if ((submenu = g_menu_model_get_item_link (model, index, G_MENU_LINK_SUBMENU)))
        {
          [self setSubmenu:[[[GNSMenu alloc] initWithTitle:[NSString stringWithUTF8String:title] model:submenu application:application hasSeparators:YES] autorelease]];
          g_object_unref (submenu);
        }

      else if (action != NULL)
        {
          GtkAccelKey key;
          gchar *path;

          helper = gtk_action_helper_new_with_application (application);
          gtk_action_helper_set_action_name         (helper, action);
          gtk_action_helper_set_action_target_value (helper, target);

          g_signal_connect (helper, "notify", G_CALLBACK (gtk_quartz_action_helper_changed), self);

          [self helperChanged];

          path = _gtk_accel_path_for_action (action, target);
          if (gtk_accel_map_lookup_entry (path, &key))
            {
              unichar character = gtk_quartz_model_menu_get_unichar (key.accel_key);

              if (character)
                {
                  NSUInteger modifiers = 0;

                  if (key.accel_mods & GDK_SHIFT_MASK)
                    modifiers |= NSShiftKeyMask;

                  if (key.accel_mods & GDK_MOD1_MASK)
                    modifiers |= NSAlternateKeyMask;

                  if (key.accel_mods & GDK_CONTROL_MASK)
                    modifiers |= NSControlKeyMask;

                  if (key.accel_mods & GDK_META_MASK)
                    modifiers |= NSCommandKeyMask;

                  [self setKeyEquivalent:[NSString stringWithCharacters:&character length:1]];
                  [self setKeyEquivalentModifierMask:modifiers];
                }
            }

          g_free (path);

          [self setTarget:self];
        }
    }

  g_free (title);

  return self;
}

- (void)dealloc
{
  if (helper != NULL)
    g_object_unref (helper);

  [super dealloc];
}

- (void)didSelectItem:(id)sender
{
  gtk_action_helper_activate (helper);
}

- (void)helperChanged
{
  [self setEnabled:gtk_action_helper_get_enabled (helper)];
  [self setState:gtk_action_helper_get_active (helper)];

  switch (gtk_action_helper_get_role (helper))
    {
      case GTK_ACTION_HELPER_ROLE_NORMAL:
        [self setOnStateImage:nil];
        break;
      case GTK_ACTION_HELPER_ROLE_TOGGLE:
        [self setOnStateImage:[NSImage imageNamed:@"NSMenuCheckmark"]];
        break;
      case GTK_ACTION_HELPER_ROLE_RADIO:
        [self setOnStateImage:[NSImage imageNamed:@"NSMenuRadio"]];
        break;
      default:
        g_assert_not_reached ();
    }
}

@end
