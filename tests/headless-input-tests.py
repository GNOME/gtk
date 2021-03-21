import sys
import subprocess
import gi

gi.require_version('Gdk', '4.0')
gi.require_version('Gtk', '4.0')

from gi.repository import GLib, Gdk, Gtk
from pydbus import SessionBus

verbose = False

remote_desktop = None
screen_cast = None
session = None
stream_path = None
done = False

def terminate():
    sys.exit(1)

loop = None

def quit_cb(loop):
    loop.quit()

def wait(millis):
    global loop
    loop = GLib.MainLoop()
    GLib.timeout_add(millis, quit_cb, loop)
    loop.run()

display = None
window = None
expected_change = None

def key_pressed_cb (controller, keyval, keycode, state):
    global expected_change
    global loop

    if verbose:
        print(f'got key press: {keyval}, state {state}')
    assert expected_change != None, "Unexpected key press"
    assert expected_change['type'] == 'press', "Key press event expected"
    assert keyval == expected_change['keyval'], "Unexpected keyval in key press event"
    assert state == expected_change['state'], "Unexpected state in key press event"

    expected_change = None
    loop.quit()

def key_released_cb (controller, keyval, keycode, state):
    global expected_change
    global loop

    if verbose:
        print(f'got key release: {keyval}, state {state}')
    assert expected_change != None, "Unexpected key release"
    assert expected_change['type'] == 'release', "Key release event expected"
    assert keyval == expected_change['keyval'], "Unexpected keyval in key release event"
    assert state == expected_change['state'], "Unexpected state in key release event"

    expected_change = None
    loop.quit()

def motion_cb (controller, x, y):
    global expected_change
    global loop

    if verbose:
        print(f'got motion: {x}, {y}')
    assert expected_change != None, "Unexpected motion"
    assert expected_change['type'] == 'motion', "Motion event expected"
    assert x == expected_change['x'], "Unexpected x coord in motion event"
    assert y == expected_change['y'], "Unexpected y coord in motion event"

    expected_change = None
    loop.quit()

def enter_cb (controller, x, y):
    global expected_change
    global loop

    if verbose:
        print(f'got enter: {x}, {y}')
    assert expected_change != None, "Unexpected enter"
    assert expected_change['type'] == 'enter', "Enter event expected"
    assert x == expected_change['x'], "Unexpected x coord in enter event"
    assert y == expected_change['y'], "Unexpected y coord in enter event"

    expected_change = None
    loop.quit()

def expect_key_press(keyval, state, timeout):
    global expected_change
    expected_change = {
      'type' : 'press',
      'keyval' : keyval,
      'state' : state
    }
    wait(timeout)
    assert expected_change == None, "Expected event did not happen"

def expect_key_release(keyval, state, timeout):
    global expected_change
    expected_change = {
      'type' : 'release',
      'keyval' : keyval,
      'state' : state
    }
    wait(timeout)
    assert expected_change == None, "Expected event did not happen"

def expect_motion(x, y, timeout):
    global expected_change
    expected_change = {
      'type' : 'motion',
      'x' : x,
      'y' : y
    }
    wait(timeout)
    assert expected_change == None, "Expected event did not happen"

def expect_enter(x, y, timeout):
    global expected_change
    expected_change = {
      'type' : 'enter',
      'x' : x,
      'y' : y
    }
    wait(timeout)
    assert expected_change == None, "Expected event did not happen"

def got_active(object, pspec):
    global loop
    loop.quit()

def launch_observer():
    global display
    global window

    if verbose:
        print('launch observer')

    if display == None:
        Gdk.set_allowed_backends('wayland')
        display = Gdk.Display.open('gtk-test')

    window = Gtk.Window.new()
    window.set_display(display)

    controller = Gtk.EventControllerKey.new()
    controller.set_propagation_phase(Gtk.PropagationPhase.CAPTURE)
    controller.connect('key-pressed', key_pressed_cb)
    controller.connect('key-released', key_released_cb)
    window.add_controller(controller)

    controller = Gtk.EventControllerMotion.new()
    controller.set_propagation_phase(Gtk.PropagationPhase.CAPTURE)
    controller.connect('enter', enter_cb)
    controller.connect('motion', motion_cb)
    window.add_controller(controller)

    window.connect('notify::is-active', got_active)
    window.maximize()
    window.present()

    wait(500)

    assert window.is_active(), "Observer not active"
    assert window.get_width() == 1024, "Window not maximized"
    assert window.get_height() == 768, "Window not maximized"

    # we need to wait out the map animation, or pointer coords will be off
    wait(1000)

def stop_observer():
    global window
    window.destroy()
    window = None

def key_press(keyval):
    if verbose:
        print(f'press key {keyval}')
    session.NotifyKeyboardKeysym(keyval, True)

def key_release(keyval):
    if verbose:
        print(f'release key {keyval}')
    session.NotifyKeyboardKeysym(keyval, False)

def pointer_move(x, y):
    if verbose:
        print(f'pointer move {x} {y}')
    session.NotifyPointerMotionAbsolute(stream_path, x, y)

def basic_keyboard_tests():
    try:
        launch_observer()

        key_press(Gdk.KEY_a)
        expect_key_press(keyval=Gdk.KEY_a, state=0, timeout=100)

        key_release(Gdk.KEY_a)
        expect_key_release(keyval=Gdk.KEY_a, state=0, timeout=100)

        key_press(Gdk.KEY_Control_L)
        expect_key_press(keyval=Gdk.KEY_Control_L, state=0, timeout=100)

        key_press(Gdk.KEY_x)
        expect_key_press(keyval=Gdk.KEY_x, state=Gdk.ModifierType.CONTROL_MASK, timeout=100)

        key_release(Gdk.KEY_Control_L)
        expect_key_release(keyval=Gdk.KEY_Control_L, state=Gdk.ModifierType.CONTROL_MASK, timeout=100)

        key_release(Gdk.KEY_x)
        expect_key_release(keyval=Gdk.KEY_x, state=0, timeout=100)

        stop_observer()
    except AssertionError as e:
        print("Error in basic_keyboard_tests: {0}".format(e))
        terminate()

def basic_pointer_tests():
    try:
      pointer_move(-100.0, -100.0)
      launch_observer()

      # observer window is maximized, so window coords == global coords
      pointer_move(500.0, 300.0)
      expect_enter(x=500, y=300, timeout=200)

      pointer_move(200.0, 200.0)
      expect_motion(x=200, y=200, timeout=200)

      stop_observer()
    except AssertionError as e:
        print("Error in basic_pointer_tests: {0}".format(e))
        terminate()

def session_closed_cb():
    print('Session closed')

def mutter_appeared(name):
    global remote_desktop
    global session
    global stream_path
    global done

    if verbose:
      print("mutter appeared on the bus")

    remote_desktop = bus.get('org.gnome.Mutter.RemoteDesktop',
                             '/org/gnome/Mutter/RemoteDesktop')
    device_types = remote_desktop.Get('org.gnome.Mutter.RemoteDesktop', 'SupportedDeviceTypes')
    assert device_types & 1 == 1, "No keyboard"
    assert device_types & 2 == 2, "No pointer"

    screen_cast = bus.get('org.gnome.Mutter.ScreenCast',
                          '/org/gnome/Mutter/ScreenCast')

    session_path = remote_desktop.CreateSession()
    session = bus.get('org.gnome.Mutter.RemoteDesktop', session_path)
    session.onClosed = session_closed_cb

    screen_cast_session_path = screen_cast.CreateSession({ 'remote-desktop-session-id' : GLib.Variant('s', session.SessionId)})
    screen_cast_session = bus.get('org.gnome.Mutter.ScreenCast', screen_cast_session_path)

    stream_path = screen_cast_session.RecordMonitor('Meta-0', {})

    session.Start()

    basic_keyboard_tests()
    basic_pointer_tests()

    session.Stop()

    done = True

def mutter_vanished():
    global done
    if remote_desktop != None:
        if verbose:
            print("mutter left the bus")
        done = True

bus = SessionBus()
bus.watch_name('org.gnome.Mutter.RemoteDesktop', 0, mutter_appeared, mutter_vanished)

try:
    while not done:
      GLib.MainContext.default().iteration(True)
except KeyboardInterrupt:
    print('Interrupted')
