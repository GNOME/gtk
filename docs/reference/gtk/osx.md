Title: Using GTK on Apple macOS
Slug: gtk-osx

The Apple macOS port of GTK is an implementation of GDK (and therefore GTK)
on top of the Quartz API.

Currently, the macOS port does not use any additional commandline options
or environment variables.

For up-to-date information on building, installation, and bundling, see the
[GTK website](https://www.gtk.org/docs/installations/macos).

## Menus

By default a GTK app shows an app menu and an edit menu.
To make menu actions work well with native windows, such as a file dialog,
the most common edit commands are treated special. In addition, some default
keyboard shortcuts are registered.

| Name    | Action    | Shortcut |
|:--------|:----------|:------|
| Undo    | `text.undo` | <kbd>⌘</kbd>-<kbd>z</kbd> |
| Redo    | `text.redo` | <kbd>⌘</kbd>-<kbd>Shift</kbd>-<kbd>z |
| Cut     | `clipboard.cut` | <kbd>⌘</kbd>-<kbd>x</kbd> |
| Copy    | `clipboard.copy` | <kbd>⌘</kbd>-<kbd>c</kbd> |
| Paste   | `clipboard.paste` | <kbd>⌘</kbd>-<kbd>v</kbd> |
| Select All | `selection.select-all` | <kbd>⌘</kbd>-<kbd>a</kbd> |

Those actions map to their respective macOS counterparts.
The actions are enabled in GTK if the action is available on the focused widget
and is enabled.

The application menu shortcuts registered with GTK are:

| Name    | Action    | Shortcut |
|:--------|:----------|:------|
| Settings  | `app.preferences` | <kbd>⌘</kbd>-<kbd>,</kbd> |
| Quit | `app.quit` | <kbd>⌘</kbd>-<kbd>q</kbd> |

To extend the macOS system menu, add application and window actions to the
application with [`Application.set_accels_for_action()`](method.Application.set_accels_for_action.html).
The menubar can be configured via [`Application.set_menubar()`](method.Application.set_menubar.html).
Those actions can then be activated from the menu.

### Window menu

The Window menu is special in macOS: it provides a list of open windows and has options for moving and resizing
windows. To automatically add those options to the Window menu, add the property `gtk-macos-special` with
value `window-submenu`:

```xml
<submenu>
  <attribute name="label" translatable="yes">_Window</attribute>
  <attribute name="gtk-macos-special">window-submenu</attribute>
</submenu>
```

## Native window controls

By default, GTK applications use common window decorators (close/minimize/maximize) on all platforms.
They show as grey rounded buttons on the top-right corner of the window.

Since GTK 4.18, [`GtkHeaderBar`](class.HeaderBar.html) has the option to use native window controls.
The controls are positioned in their normal place: the top-left corner.
This feature can be enabled by setting the property [`use-native-controls`](property.HeaderBar.use-native-controls.html) to `TRUE` on a `GtkHeaderBar`.
The property [`decoration-layout`](property.HeaderBar.decoration-layout.html) can be used to
enable/disable buttons.

![The GTK demo application with macOS native window controls](macos-window-controls.png)

Native window controls are an opt-in feature. If you want a more macOS native experience for your app,
it's up to you to make sure all windows have native controls. You'll need to pay special attention in case of split header bars, since only the leftmost header bar should support the native window controls.

Window decorators only work for normal, toplevel windows.

::: important
    Native window controls are drawn on top of the window, and are not controlled by GTK.
    It's up to you, the application developer, to make sure the header bar that "contain" the
    native window controls, and occupy the space below the window controls.

## Content types

While GTK uses MIME types, macOS uses Unified Type descriptors.
GTK maps MIME to UTI types.

If you create a macOS app for your application, you can provide
custom UTI/MIME types mappings in the
[Information Property List](https://developer.apple.com/documentation/bundleresources/information_property_list)
for your application.

## Vulkan support

GTK can be compiled with Vulkan support on macOS, via
[MoltenVK](https://github.com/KhronosGroup/MoltenVK).

The macOS version links with the Vulkan loader library, and not with
the vulkan library (MoltenVK) directly.

### Homebrew

Install the following packages to enable Vulkan support:

```sh
brew install vulkan-loader shaderc molten-vk
```

### Vulkan SDK

Download the [Vulkan SDK for macOS](https://vulkan.lunarg.com/sdk/home) from LunarG.
Set it up according to their
[instructions](https://vulkan.lunarg.com/doc/sdk/latest/mac/getting_started.html).
Pay special attention to the environment variables that need to be set: `VULKAN_SDK`,
`DYLD_LIBRARY_PATH`, `VK_ICD_FILENAMES`, and `VK_LAYER_PATH`. You can also use the provided
`setup-env.sh` script in the SDK root folder.

## Publishing

GTK provides no means to package GTK applications (create a `.app`s out of them).

However, we can provide a few pointers.

GTK uses an OpenGL surface for drawing. On laptops with 2 GPU's it will take the heavy
(Radeon) GPU by default. If your app is not heavy on graphics, it may be better to use
the more energy efficient built-in GPU instead.
This can be configured by adding the following to the app's `Info.plist`:

```xml
<plist version="1.0">
  <dict>
    ...
    <key>NSSupportsAutomaticGraphicsSwitching</key>
    <true/>
    ...
  </dict>
</plist>
```
