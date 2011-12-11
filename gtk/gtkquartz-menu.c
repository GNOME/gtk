/*
 * Copyright © 2011 William Hua
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: William Hua <william@attente.ca>
 */

#include "gtkquartz-menu.h"

#include <gdk/gdkkeysyms.h>
#include "gtkaccelmapprivate.h"

#import <Cocoa/Cocoa.h>

/*
 * Code for key code conversion
 *
 * Copyright (C) 2009 Paul Davis
 */
static unichar
gtk_quartz_menu_get_unichar (gint key)
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



typedef struct _GtkQuartzActionObserver GtkQuartzActionObserver;

@interface GNSMenuItem : NSMenuItem
{
  gchar                   *action;
  GVariant                *target;
  BOOL                     canActivate;
  GActionGroup            *actions;
  GtkQuartzActionObserver *observer;
}

- (id)initWithModel:(GMenuModel *)model index:(NSInteger)index observable:(GActionObservable *)observable;

- (void)observableActionAddedWithParameterType:(const GVariantType *)parameterType enabled:(BOOL)enabled state:(GVariant *)state;
- (void)observableActionEnabledChangedTo:(BOOL)enabled;
- (void)observableActionStateChangedTo:(GVariant *)state;
- (void)observableActionRemoved;

- (void)didSelectItem:(id)sender;

@end



struct _GtkQuartzActionObserver
{
  GObject parent_instance;

  GNSMenuItem *item;
};


typedef GObjectClass GtkQuartzActionObserverClass;

static void gtk_quartz_action_observer_observer_iface_init (GActionObserverInterface *iface);
G_DEFINE_TYPE_WITH_CODE (GtkQuartzActionObserver, gtk_quartz_action_observer, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_ACTION_OBSERVER, gtk_quartz_action_observer_observer_iface_init))

static void
gtk_quartz_action_observer_action_added (GActionObserver    *observer,
                                         GActionObservable  *observable,
                                         const gchar        *action_name,
                                         const GVariantType *parameter_type,
                                         gboolean            enabled,
                                         GVariant           *state)
{
  GtkQuartzActionObserver *qao = (GtkQuartzActionObserver *) observer;

  [qao->item observableActionAddedWithParameterType:parameter_type enabled:enabled state:state];
}

static void
gtk_quartz_action_observer_action_enabled_changed (GActionObserver   *observer,
                                                   GActionObservable *observable,
                                                   const gchar       *action_name,
                                                   gboolean           enabled)
{
  GtkQuartzActionObserver *qao = (GtkQuartzActionObserver *) observer;

  [qao->item observableActionEnabledChangedTo:enabled];
}

static void
gtk_quartz_action_observer_action_state_changed (GActionObserver   *observer,
                                                 GActionObservable *observable,
                                                 const gchar       *action_name,
                                                 GVariant          *state)
{
  GtkQuartzActionObserver *qao = (GtkQuartzActionObserver *) observer;

  [qao->item observableActionStateChangedTo:state];
}

static void
gtk_quartz_action_observer_action_removed (GActionObserver   *observer,
                                           GActionObservable *observable,
                                           const gchar       *action_name)
{
  GtkQuartzActionObserver *qao = (GtkQuartzActionObserver *) observer;

  [qao->item observableActionRemoved];
}

static void
gtk_quartz_action_observer_init (GtkQuartzActionObserver *item)
{
}

static void
gtk_quartz_action_observer_observer_iface_init (GActionObserverInterface *iface)
{
  iface->action_added = gtk_quartz_action_observer_action_added;
  iface->action_enabled_changed = gtk_quartz_action_observer_action_enabled_changed;
  iface->action_state_changed = gtk_quartz_action_observer_action_state_changed;
  iface->action_removed = gtk_quartz_action_observer_action_removed;
}

static void
gtk_quartz_action_observer_class_init (GtkQuartzActionObserverClass *class)
{
}

static GtkQuartzActionObserver *
gtk_quartz_action_observer_new (GNSMenuItem *item)
{
  GtkQuartzActionObserver *observer;

  observer = g_object_new (gtk_quartz_action_observer_get_type (), NULL);
  observer->item = item;

  return observer;
}

static NSMenu *
gtk_quartz_menu_create_menu (const gchar       *title,
                             GMenuModel        *model,
                             GActionObservable *observable)
{
  if (model == NULL)
    return nil;

  NSMenu *menu = [[[NSMenu alloc] initWithTitle:[NSString stringWithUTF8String:title ? : ""]] autorelease];

  [menu setAutoenablesItems:NO];

  gint n = g_menu_model_get_n_items (model);
  gint i;

  for (i = 0; i < n; i++)
    {
      gchar *label = NULL;

      if (g_menu_model_get_item_attribute (model, i, G_MENU_ATTRIBUTE_LABEL, "s", &label))
        {
          gchar *from, *to;

          to = from = label;

          while (*from)
            {
              if (*from == '_' && from[1])
                from++;

              *to++ = *from++;
            }

          *to = '\0';
        }

      NSString *text = [NSString stringWithUTF8String:label ? : ""];

      GMenuModel *section_model = g_menu_model_get_item_link (model, i, G_MENU_LINK_SECTION);
      GMenuModel *submenu_model = g_menu_model_get_item_link (model, i, G_MENU_LINK_SUBMENU);

      NSMenu *section = gtk_quartz_menu_create_menu (label, section_model, observable);
      NSMenu *submenu = gtk_quartz_menu_create_menu (label, submenu_model, observable);

      if (section_model != NULL)
        g_object_unref (section_model);

      if (submenu_model != NULL)
        g_object_unref (submenu_model);

      if (section != nil)
        {
          if ([menu numberOfItems] > 0)
            [menu addItem:[NSMenuItem separatorItem]];

          if ([text length] > 0)
          {
            NSMenuItem *header = [[[NSMenuItem alloc] initWithTitle:text action:NULL keyEquivalent:@""] autorelease];

            [header setEnabled:NO];

            [menu addItem:header];
          }

          for (NSMenuItem *item in [section itemArray])
            {
              [item retain];
              [[item menu] removeItem:item];
              [menu addItem:item];
              [item release];
            }
        }
      else if (submenu != nil)
        {
          NSMenuItem *item = [[[NSMenuItem alloc] initWithTitle:text action:NULL keyEquivalent:@""] autorelease];

          [item setSubmenu:submenu];

          [menu addItem:item];
        }
      else
        [menu addItem:[[[GNSMenuItem alloc] initWithModel:model index:i observable:observable] autorelease]];
    }

  return menu;
}

void
gtk_quartz_set_main_menu (GMenuModel        *model,
                          GActionObservable *observable)
{
  [NSApp setMainMenu:gtk_quartz_menu_create_menu ("Main Menu", model, observable)];
}



@implementation GNSMenuItem

- (id)initWithModel:(GMenuModel *)model index:(NSInteger)index observable:(GActionObservable *)observable
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

      g_menu_model_get_item_attribute (model, index, G_MENU_ATTRIBUTE_ACTION, "s", &action);
      target = g_menu_model_get_item_attribute_value (model, index, G_MENU_ATTRIBUTE_TARGET, NULL);
      actions = g_object_ref (observable);
      observer = gtk_quartz_action_observer_new (self);

      if (action != NULL)
        {
          GtkAccelKey key;
          gchar *path;

          g_action_observable_register_observer (observable, action, G_ACTION_OBSERVER (observer));

          path = _gtk_accel_path_for_action (action, target);
          if (gtk_accel_map_lookup_entry (path, &key))
            {
              unichar character = gtk_quartz_menu_get_unichar (key.accel_key);

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

          gboolean            enabled;
          const GVariantType *parameterType;
          GVariant           *state;

          if (g_action_group_query_action (G_ACTION_GROUP (actions), action, &enabled, &parameterType, NULL, NULL, &state))
            [self observableActionAddedWithParameterType:parameterType enabled:enabled state:state];
          else
            [self setEnabled:NO];
        }
    }

  g_free (title);

  return self;
}

- (void)dealloc
{
  if (observer != NULL)
    g_object_unref (observer);

  if (actions != NULL)
    g_object_unref (actions);

  if (target != NULL)
    g_variant_unref (target);

  g_free (action);

  [super dealloc];
}

- (void)observableActionAddedWithParameterType:(const GVariantType *)parameterType enabled:(BOOL)enabled state:(GVariant *)state
{
  canActivate = (target == NULL && parameterType == NULL) ||
                (target != NULL && parameterType != NULL &&
                 g_variant_is_of_type (target, parameterType));

  if (canActivate)
    {
      if (target != NULL && state != NULL)
        {
          [self setOnStateImage:[NSImage imageNamed:@"NSMenuRadio"]];
          [self setState:g_variant_equal (state, target) ? NSOnState : NSOffState];
        }
      else if (state != NULL && g_variant_is_of_type (state, G_VARIANT_TYPE_BOOLEAN))
        {
          [self setOnStateImage:[NSImage imageNamed:@"NSMenuCheckmark"]];
          [self setState:g_variant_get_boolean (state) ? NSOnState : NSOffState];
        }
      else
        [self setState:NSOffState];

      [self setEnabled:enabled];
    }
  else
    [self setEnabled:NO];
}

- (void)observableActionEnabledChangedTo:(BOOL)enabled
{
  if (canActivate)
    [self setEnabled:enabled];
}

- (void)observableActionStateChangedTo:(GVariant *)state
{
  if (canActivate)
    {
      if (target != NULL)
        [self setState:g_variant_equal (state, target) ? NSOnState : NSOffState];
      else if (g_variant_is_of_type (state, G_VARIANT_TYPE_BOOLEAN))
        [self setState:g_variant_get_boolean (state) ? NSOnState : NSOffState];
    }
}

- (void)observableActionRemoved
{
  if (canActivate)
    [self setEnabled:NO];
}

- (void)didSelectItem:(id)sender
{
  if (canActivate)
    g_action_group_activate_action (actions, action, target);
}

@end
