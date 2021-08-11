Title: Key Bindings

# Key Bindings

[`class@Gtk.BindingSet`] provides a mechanism for configuring GTK key bindings
through CSS files. This eases key binding adjustments for application
developers as well as users and provides GTK users or administrators with
high key binding configurability which requires no application or toolkit
side changes.

In order for bindings to work in a custom widget implementation, the
widget’s [`property@Gtk.Widget:can-focus`] and
[`property@Gtk.Widget:has-focus`] properties must both be true. For example,
by calling [`method@Gtk.Widget.set_can_focus`]in the widget’s initialisation
function; and by calling [`method@Gtk.Widget.grab_focus`] when the widget is
clicked.

## Installing a key binding

A CSS file binding consists of a `binding-set` definition and a match
statement to apply the binding set to specific widget types. Details on the
matching mechanism are described under Selectors in the
[class@Gtk.CssProvider] documentation. Inside the binding set definition,
key combinations are bound to one or more specific signal emissions on the
target widget. Key combinations are strings consisting of an optional
[type@Gdk.ModifierType] name and key names such as those defined in
`gdk/gdkkeysyms.h` or returned from [`func@Gdk.keyval_name`], they have to
be parsable by [`func@Gtk.accelerator_parse`].  Specifications of signal
emissions consist of a string identifying the signal name, and a list of
signal specific arguments in parenthesis.

For example for binding Control and the left or right cursor keys of a
[class@Gtk.Entry] widget to the “move-cursor” signal (so movement occurs in
3-character steps), the following binding can be used:

```
@binding-set MoveCursor3
{
  bind "<Control>Right" { "move-cursor" (visual-positions, 3, 0) };
  bind "<Control>Left" { "move-cursor" (visual-positions, -3, 0) };
}

entry
{
  -gtk-key-bindings: MoveCursor3;
}
```

## Unbinding existing key bindings

GTK already defines a number of useful bindings for the widgets it provides.
Because custom bindings set up in CSS files take precedence over the default
bindings shipped with GTK, overriding existing bindings as demonstrated in
Installing a key binding works as expected. The same mechanism can not be
used to “unbind” existing bindings, however.

```
@binding-set MoveCursor3
{
  bind "<Control>Right" {  };
  bind "<Control>Left" {  };
}

entry
{
  -gtk-key-bindings: MoveCursor3;
}
```

The above example will not have the desired effect of causing
<kbd>Ctrl</kbd><kbd>→</kbd> and <kbd>Ctrl</kbd><kbd>←</kbd> key presses to
be ignored by GTK. Instead, it just causes any existing bindings from the
bindings set “MoveCursor3” to be deleted, so when
<kbd>Ctrl</kbd><kbd>→</kbd> or <kbd>Ctrl</kbd><kbd>←</kbd> are pressed, no
binding for these keys is found in binding set “MoveCursor3”. GTK will thus
continue to search for matching key bindings, and will eventually lookup and
find the default GTK bindings for entries which implement word movement. To
keep GTK from activating its default bindings, the “unbind” keyword can be
used like this:

```
@binding-set MoveCursor3
{
  unbind "<Control>Right";
  unbind "<Control>Left";
}

entry
{
  -gtk-key-bindings: MoveCursor3;
}
```

Now, GTK will find a match when looking up <kbd>Ctrl</kbd><kbd>→</kbd> and
<kbd>Ctrl</kbd><kbd>←</kbd> key presses before it resorts to its default bindings, and
the match instructs it to abort (“unbind”) the search, so the key presses
are not consumed by this widget. As usual, further processing of the key
presses, e.g. by an entry’s parent widget, is now possible.
