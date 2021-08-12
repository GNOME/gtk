Title: Migrating from EggSMClient to GtkApplication

# Migrating from EggSMClient to GtkApplication

`EggSMClient` provides 'session management' support for applications. This
means a number of things:

- logout notification and negotiation
- application state saving
- restarting of applications with saved state

EggSMClient supports this functionality to varying degrees on Windows and OS
X, as well as with XSMP and D-Bus based session managers in X11.

Starting with GTK 3.4, GtkApplication supports logout notification and
negotiation similar to EggSMClient.

| EggSMClient | GtkApplication |
|-------------|----------------|
| `EggSMClient::quit-requested` | instead of calling `will_quit(FALSE,...)` in response to this signal, install an inhibitor |
| `EggSMClient::quit` | the [`signal@Gio.Application::shutdown`] signal |
| `EggSMClient::quit-cancelled` | no replacement |
| `egg_sm_client_will_quit()` | instead of calling `will_quit(FALSE,...)`, install an inhibitor |
| `egg_sm_client_end_session()` | no replacement |

As of the writing of this page, `GtkApplication` has no special support for
state saving and restarting. Applications can use [`class@Gio.Settings`] or
[`struct@GLib.KeyFile`] and save as much state as they see fit in response
to “shutdown”, or whenever they consider appropriate.
