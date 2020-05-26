# Using GTK with Broadway {#gtk-broadway}

The GDK Broadway backend provides support for displaying GTK
applications in a web browser, using HTML5 and web sockets. To run
your application in this way, select the Broadway backend by setting
`GDK_BACKEND=broadway`. Then you can make your application appear in
a web browser by pointing it at `http://127.0.0.1:8080`. Note that
you need to enable web sockets in your web browser.

You can choose a different port from the default 8080 by setting
the `BROADWAY_DISPLAY` environment variable to the port that you
want to use.

It is also possible to use multiple GTK applications in the same
web browser window, by using the Broadway server, `broadwayd`, that
ships with GTK. To use broadwayd, start it like this:

```
broadwayd :5
```

Then point your web browser at `http://127.0.0.1:8085`.
Start your applications like this:

```
GDK_BACKEND=broadway BROADWAY_DISPLAY=:5 gtk4-demo
```

## Broadway-specific environment variables {#broadway-envar}

### BROADWAY_DISPLAY

Specifies the Broadway display number. The default display is 0.
The display number determines the port to use when connecting
to a Broadway application via the following formula:
`port = 8080 + display`
