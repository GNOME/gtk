Title: Threading in GTK
Slug: gtk-threading

GTK requires that the default GLib main context is iterated, in
order for idles and timeouts to work (the GTK frame clock relies
on this). This can be done in various ways: [method@Gio.Application.run],
[method@GLib.MainLoop.run] and [method@GLib.MainContext.iteration]
are ways to do it.

The thread which iterates the main context is known as the
**_main thread_**. In C applications, this is almost always the
thread that runs `main()`.

## Threadsafe Objects

Objects may or may not be **_threadsafe_** - which means they are safe
to be used (i.e. passed as arguments to functions) concurrently from
multiple threads. Being threadsafe guarantees that all access to mutable
state of the object is internally serialized.

Objects that are not threadsafe can only be used from one thread - the
thread that they were created in.

Most GTK objects are *not* threadsafe and can only be used from the
main thread, since they are all connected by references (all widgets
are connected via the widget tree, and they all reference the display
object).

## Immutable Objects and Builders

A special class of threadsafe objects are **_immutable_** objects.
These can safely be passed between threads and used in any thread.

Examples of immutable objects are

 - [class@Gdk.Texture]
 - [struct@Gdk.ColorState]
 - [class@Gsk.RenderNode]
 - [struct@Gsk.Path]
 - [struct@Gsk.Transform]

In connection with immutable objects, GTK frequently employs a
**_builder_** pattern: A builder object collects all the data that
is necessary to create an immutable object, and is discarded when
the object is created. Builder objects can generally be used on any
thread, as long as only one thread accesses it.

Examples of builders are

 - [class@Gdk.MemoryTextureBuilder]
 - [class@Gdk.DmabufTextureBuilder]
 - [class@Gtk.Snapshot]
 - [struct@Gsk.PathBuilder]

## Patterns for Safe Use of Threads

Here are some recommendations for how to use threads in GTK
applications in a safe way.

The first recommendation is to use async APIs for IO. GIO uses
a thread pool to avoid blocking the main thread.

The second recommendation is to push long-running tasks to
worker threads, and return the results back to the main thread
via an idle or with [method@GLib.MainContext.invoke]. This can be
achieved without manually juggling threads by using [class@Gio.Task]
or [libdex](https://gnome.pages.gitlab.gnome.org/libdex/libdex-1/index.html).

A common scenario for the second recommendation is when the construction
of an object has high overhead (e.g. creating GdkTexture may require
loading and parsing a big image file). In this case, you may want to
create the object in another thread, using a builder object there, and
pass the immutable object back to the main thread after it has been created.

GTK provides some convenience API for this use case that is explicitly
marked as threadsafe:
 - [ctor@Gdk.Texture.new_from_resource]
 - [ctor@Gdk.Texture.new_from_file]
 - [ctor@Gdk.Texture.new_from_filename]
 - [ctor@Gdk.Texture.new_from_bytes]
