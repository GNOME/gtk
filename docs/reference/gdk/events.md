Title: Events

## Handling events from the window system

In GTK applications the events are handled automatically by toplevel
widgets and passed on to the event controllers of appropriate widgets,
so using [class@Gdk.Event] and its related API is rarely needed.

[class@Gdk.Event] and its derived types are immutable data structures,
created by GTK itself to represent windowing system events.
