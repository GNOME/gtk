/* Helper functions for debugging */
var logDiv = null;
function log(str) {
    if (!logDiv) {
	logDiv = document.createElement('div');
	document.body.appendChild(logDiv);
	logDiv.style["position"] = "absolute";
	logDiv.style["right"] = "0px";
    }
    logDiv.appendChild(document.createTextNode(str));
    logDiv.appendChild(document.createElement('br'));
}

var base64_val = [
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255, 62,255,255,255, 63,
   52, 53, 54, 55, 56, 57, 58, 59, 60, 61,255,255,255,  0,255,255,
  255,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
   15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,255,255,255,255,255,
  255, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
   41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51,255,255,255,255,255
]

function base64_8(str, index) {
  var v =
    (base64_val[str.charCodeAt(index)]) +
    (base64_val[str.charCodeAt(index+1)] << 6);
  return v;
}

function base64_16(str, index) {
  var v =
    (base64_val[str.charCodeAt(index)]) +
    (base64_val[str.charCodeAt(index+1)] << 6) +
    (base64_val[str.charCodeAt(index+2)] << 12);
  return v;
}

function base64_16s(str, index) {
  var v = base64_16(str, index);
  if (v > 32767)
    return v - 65536;
  else
    return v;
}

function base64_24(str, index) {
  var v =
    (base64_val[str.charCodeAt(index)]) +
    (base64_val[str.charCodeAt(index+1)] << 6) +
    (base64_val[str.charCodeAt(index+2)] << 12) +
    (base64_val[str.charCodeAt(index+3)] << 18);
  return v;
}

function base64_32(str, index) {
  var v =
    (base64_val[str.charCodeAt(index)]) +
    (base64_val[str.charCodeAt(index+1)] << 6) +
    (base64_val[str.charCodeAt(index+2)] << 12) +
    (base64_val[str.charCodeAt(index+3)] << 18) +
    (base64_val[str.charCodeAt(index+4)] << 24) +
    (base64_val[str.charCodeAt(index+5)] << 30);
  return v;
}

function createXHR()
{
  try { return new XMLHttpRequest(); } catch(e) {}
  try { return new ActiveXObject("Msxml2.XMLHTTP.6.0"); } catch (e) {}
  try { return new ActiveXObject("Msxml2.XMLHTTP.3.0"); } catch (e) {}
  try { return new ActiveXObject("Msxml2.XMLHTTP"); } catch (e) {}
  try { return new ActiveXObject("Microsoft.XMLHTTP"); } catch (e) {}

  return null;
}

var grab = new Object();
grab.window = null;
grab.owner_events = false;
grab.time = 0;
grab.implicit = false;
var last_serial = 0;
var last_x = 0;
var last_y = 0;
var last_state;
var real_window_with_mouse = 0;
var window_with_mouse = 0;
var surfaces = {};
var outstanding_commands = new Array();
var input_socket = null;

function initContext(canvas, x, y, id)
{
  canvas.surface_id = id;
  canvas.style["position"] = "absolute";
  canvas.style["left"] = x + "px";
  canvas.style["top"] = y + "px";
  canvas.style["display"] = "none";
  var context = canvas.getContext("2d");
  context.globalCompositeOperation = "source-over";
  document.body.appendChild(canvas);
  context.drawQueue = [];

  return context;
}

var GDK_GRAB_SUCCESS = 0;
var GDK_GRAB_ALREADY_GRABBED = 1;
var GDK_GRAB_INVALID_TIME = 2;

var GDK_CROSSING_NORMAL = 0;
var GDK_CROSSING_GRAB = 1;
var GDK_CROSSING_UNGRAB = 2;

// GdkModifierType
var GDK_SHIFT_MASK = 1 << 0;
var GDK_LOCK_MASK     = 1 << 1;
var GDK_CONTROL_MASK  = 1 << 2;
var GDK_MOD1_MASK     = 1 << 3;
var GDK_MOD2_MASK     = 1 << 4;
var GDK_MOD3_MASK     = 1 << 5;
var GDK_MOD4_MASK     = 1 << 6;
var GDK_MOD5_MASK     = 1 << 7;
var GDK_BUTTON1_MASK  = 1 << 8;
var GDK_BUTTON2_MASK  = 1 << 9;
var GDK_BUTTON3_MASK  = 1 << 10;
var GDK_BUTTON4_MASK  = 1 << 11;
var GDK_BUTTON5_MASK  = 1 << 12;
var GDK_SUPER_MASK    = 1 << 26;
var GDK_HYPER_MASK    = 1 << 27;
var GDK_META_MASK     = 1 << 28;
var GDK_RELEASE_MASK  = 1 << 30;

function getButtonMask (button) {
    if (button == 1)
	return GDK_BUTTON1_MASK;
    if (button == 2)
	return GDK_BUTTON2_MASK;
    if (button == 3)
	return GDK_BUTTON3_MASK;
    if (button == 4)
	return GDK_BUTTON4_MASK;
    if (button == 5)
	return GDK_BUTTON5_MASK;
    return 0;
}

function flushSurface(surface)
{
    var commands = surface.drawQueue;
    surface.queue = [];
    var i = 0;
    for (i = 0; i < commands.length; i++) {
	var cmd = commands[i];
	var context = surfaces[cmd.id];
	switch (cmd.op) {
      /* put image data surface */
	case 'i':
	    context.globalCompositeOperation = "source-over";
	    context.drawImage(cmd.img, cmd.x, cmd.y);
	    break;

      /* copy rects */
	case 'b':
	    context.save();
	    context.beginPath();

	    var minx;
	    var miny;
	    var maxx;
	    var maxy;
	    for (var j = 0; j < cmd.rects.length; j++) {
		var rect = cmd.rects[j];
		context.rect(rect.x, rect.y, rect.w, rect.h);
		if (j == 0) {
		    minx = rect.x;
		    miny = rect.y;
		    maxx = rect.x + rect.w;
		    maxy = rect.y + rect.h;
		} else {
		    if (rect.x < minx)
			minx = rect.x;
		    if (rect.y < miny)
			miny = rect.y;
		    if (rect.x + rect.w > maxx)
			maxx = rect.x + rect.w;
		    if (rect.y + rect.h > maxy)
			maxy = rect.y + rect.h;
		}
	    }
	    context.clip();
	    context.globalCompositeOperation = "copy";
	    context.drawImage(context.canvas,
			      minx - cmd.dx, miny - cmd.dy, maxx - minx, maxy - miny,
			      minx, miny, maxx - minx, maxy - miny);
	    context.restore();
	    break;

      default:
	    alert("Unknown drawing op " + cmd.op);
	}
    }
}

function handleCommands(cmd_obj)
{
  var cmd = cmd_obj.data;
  var i = cmd_obj.pos;

  while (i < cmd.length) {
    var command = cmd[i++];
    last_serial = base64_32(cmd, i);
    i = i + 6;
    switch (command) {
      /* create new surface */
      case 's':
        var id = base64_16(cmd, i);
        i = i + 3;
        var x = base64_16(cmd, i);
        i = i + 3;
        var y = base64_16(cmd, i);
        i = i + 3;
        var w = base64_16(cmd, i);
        i = i + 3;
        var h = base64_16(cmd, i);
        i = i + 3;
        var surface = document.createElement("canvas");
	surface.width = w;
	surface.height = h;
	surfaces[id] = initContext(surface, x, y, id);
        break;

      /* show a surface */
      case 'S':
        var id = base64_16(cmd, i);
        i = i + 3;
	surfaces[id].canvas.style["display"] = "inline";
        break;

      /* hide a surface */
      case 'H':
        var id = base64_16(cmd, i);
        i = i + 3;
	surfaces[id].canvas.style["display"] = "none";
        break;

      /* delete surface */
      case 'd':
        var id = base64_16(cmd, i);
        i = i + 3;
	var canvas = surfaces[id].canvas;
	delete surfaces[id];
	canvas.parentNode.removeChild(canvas);

        break;

      /* move a surface */
      case 'm':
        var id = base64_16(cmd, i);
        i = i + 3;
        var x = base64_16(cmd, i);
        i = i + 3;
        var y = base64_16(cmd, i);
        i = i + 3;
	surfaces[id].canvas.style["left"] = x + "px";
	surfaces[id].canvas.style["top"] = y + "px";
        break;

      /* resize a surface */
      case 'r':
        var id = base64_16(cmd, i);
        i = i + 3;
        var w = base64_16(cmd, i);
        i = i + 3;
        var h = base64_16(cmd, i);
        i = i + 3;
	flushSurface(surfaces[id]);
	surfaces[id].canvas.width = w;
	surfaces[id].canvas.height = h;
        break;

      /* put image data surface */
      case 'i':
	var q = new Object();
	q.op = 'i';
        q.id = base64_16(cmd, i);
        i = i + 3;
        q.x = base64_16(cmd, i);
        i = i + 3;
        q.y = base64_16(cmd, i);
        i = i + 3;
        var size = base64_32(cmd, i);
        i = i + 6;
	var url = cmd.slice(i, i + size);
	i = i + size;
        q.img = new Image();
	q.img.src = url;
	surfaces[q.id].drawQueue.push(q);
	if (!q.img.complete) {
	  cmd_obj.pos = i;
	  q.img.onload = function() { handleOutstanding(); };
	  return false;
	}

        break;

      /* copy rects */
      case 'b':
	var q = new Object();
	q.op = 'b';
        q.id = base64_16(cmd, i);
        i = i + 3;

	var nrects = base64_16(cmd, i);
        i = i + 3;

	q.rects = [];
	for (var r = 0; r < nrects; r++) {
	  var rect = new Object();
	  rect.x = base64_16(cmd, i);
          i = i + 3;
          rect.y = base64_16(cmd, i);
          i = i + 3;
          rect.w = base64_16(cmd, i);
          i = i + 3;
          rect.h = base64_16(cmd, i);
          i = i + 3;
	  q.rects.push (rect);
	}

        q.dx = base64_16s(cmd, i);
        i = i + 3;
        q.dy = base64_16s(cmd, i);
        i = i + 3;
	surfaces[q.id].drawQueue.push(q);
        break;

      case 'f': // Flush surface
        var id = base64_16(cmd, i);
        i = i + 3;

	flushSurface(surfaces[id]);
	break;

      case 'q': // Query pointer
        var id = base64_16(cmd, i);
        i = i + 3;

	var pos = getPositionsFromAbsCoord(last_x, last_y, id);

	send_input ("q", [pos.root_x, pos.root_y, pos.win_x, pos.win_y, window_with_mouse]);
	break;

      case 'g': // Grab
        var id = base64_16(cmd, i);
        i = i + 3;
	var owner_events = cmd[i++] == '1';
	var time = base64_32(cmd, i);
	i = i + 6;

	if (grab.window != null) {
	    /* Previous grab, compare times */
	    if (time != 0 && grab.time != 0 &&
		time > grab.time) {
		send_input ("g", [GDK_GRAB_INVALID_TIME]);
		break;
	    }
	}

	doGrab(id, owner_events, time, false);

	send_input ("g", [GDK_GRAB_SUCCESS]);

	break;

      case 'u': // Ungrab
	var time = base64_32(cmd, i);
	i = i + 6;
	send_input ("u", []);

	if (grab.window != null) {
	    if (grab.time == 0 || time == 0 ||
		grab.time < time)
		grab.window = null;
	}

	break;
      default:
        alert("Unknown op " + command);
    }
  }
  return true;
}

function handleOutstanding()
{
  while (outstanding_commands.length > 0) {
    var cmd = outstanding_commands.shift();
    if (!handleCommands(cmd)) {
      outstanding_commands.unshift(cmd);
      return;
    }
  }
}

function handleLoad(event)
{
  var cmd_obj = {};
  cmd_obj.data = event.target.responseText;
  cmd_obj.pos = 0;

  outstanding_commands.push(cmd_obj);
  if (outstanding_commands.length == 1) {
    handleOutstanding();
  }
}

function get_surface_id(ev) {
  var id = ev.target.surface_id;
  if (id != undefined)
    return id;
  return 0;
}

function send_input(cmd, args)
{
  if (input_socket != null) {
      input_socket.send(cmd + ([last_serial].concat(args)).join(","));
  }
}

function get_document_coordinates(element)
{
    var res = new Object();
    res.x = element.offsetLeft;
    res.y = element.offsetTop;

    var offsetParent = element.offsetParent;
    while (offsetParent != null) {
	res.x += offsetParent.offsetLeft;
	res.y += offsetParent.offsetTop;
	offsetParent = offsetParent.offsetParent;
    }
    return res;
}

function getPositionsFromAbsCoord(absX, absY, relativeId) {
    var res = Object();

    res.root_x = absX;
    res.root_y = absY;
    res.win_x = absX;
    res.win_y = absY;
    if (relativeId != 0) {
	var pos = get_document_coordinates(surfaces[relativeId].canvas);
	res.win_x = res.win_x - pos.x;
	res.win_y = res.win_y - pos.y;
    }

    return res;
}

function getPositionsFromEvent(ev, relativeId) {
    var res = getPositionsFromAbsCoord(ev.pageX, ev.pageY, relativeId);

    last_x = res.root_x;
    last_y = res.root_y;

    return res;
}

function getEffectiveEventTarget (id) {
    if (grab.window != null) {
	if (!grab.owner_events)
	    return grab.window;
	if (id == 0)
	    return grab.window;
    }
    return id;
}

function on_mouse_move (ev) {
    var id = get_surface_id(ev);
    id = getEffectiveEventTarget (id);
    var pos = getPositionsFromEvent(ev, id);
    send_input ("m", [id, pos.root_x, pos.root_y, pos.win_x, pos.win_y, last_state, ev.timeStamp]);
}

function on_mouse_over (ev) {
    var id = get_surface_id(ev);
    real_window_with_mouse = id;
    id = getEffectiveEventTarget (id);
    var pos = getPositionsFromEvent(ev, id);
    window_with_mouse = id;
    if (window_with_mouse != 0) {
	send_input ("e", [id, pos.root_x, pos.root_y, pos.win_x, pos.win_y, last_state, ev.timeStamp, GDK_CROSSING_NORMAL]);
    }
}

function on_mouse_out (ev) {
    var id = get_surface_id(ev);
    var origId = id;
    id = getEffectiveEventTarget (id);
    var pos = getPositionsFromEvent(ev, id);

    if (id != 0) {
	send_input ("l", [id, pos.root_x, pos.root_y, pos.win_x, pos.win_y, last_state, ev.timeStamp, GDK_CROSSING_NORMAL]);
    }
    real_window_with_mouse = 0;
    window_with_mouse = 0;
}

function doGrab(id, owner_events, time, implicit) {
    var pos;

    if (window_with_mouse != id) {
	if (window_with_mouse != 0) {
	    pos = getPositionsFromAbsCoord(last_x, last_y, window_with_mouse);
	    send_input ("l", [window_with_mouse, pos.root_x, pos.root_y, pos.win_x, pos.win_y, last_state, time, GDK_CROSSING_GRAB]);
	}
	pos = getPositionsFromAbsCoord(last_x, last_y, id);
	send_input ("e", [id, pos.root_x, pos.root_y, pos.win_x, pos.win_y, last_state, time, GDK_CROSSING_GRAB]);
	window_with_mouse = id;
    }

    grab.window = id;
    grab.owner_events = owner_events;
    grab.time = time;
    grab.implicit = implicit;
}

function doUngrab(time) {
    var pos;
    if (real_window_with_mouse != window_with_mouse) {
	if (window_with_mouse != 0) {
	    pos = getPositionsFromAbsCoord(last_x, last_y, window_with_mouse);
	    send_input ("l", [window_with_mouse, pos.root_x, pos.root_y, pos.win_x, pos.win_y, last_state, time, GDK_CROSSING_UNGRAB]);
	}
	if (real_window_with_mouse != 0) {
	    pos = getPositionsFromAbsCoord(last_x, last_y, real_window_with_mouse);
	    send_input ("e", [real_window_with_mouse, pos.root_x, pos.root_y, pos.win_x, pos.win_y, last_state, time, GDK_CROSSING_UNGRAB]);
	}
	window_with_mouse = real_window_with_mouse;
    }
    grab.window = null;
}

function on_mouse_down (ev) {
    var id = get_surface_id(ev);
    id = getEffectiveEventTarget (id);
    var pos = getPositionsFromEvent(ev, id);
    if (grab.window != null)
	doGrab (id, false, ev.timeStamp, true);
    var button = ev.button + 1;
    last_state = last_state | getButtonMask (button);
    send_input ("b", [id, pos.root_x, pos.root_y, pos.win_x, pos.win_y, last_state, ev.timeStamp, button]);
}

function on_mouse_up (ev) {
    var id = get_surface_id(ev);
    id = getEffectiveEventTarget (id);
    var pos = getPositionsFromEvent(ev, id);
    var button = ev.button + 1;
    last_state = last_state & ~getButtonMask (button);
    send_input ("B", [id, pos.root_x, pos.root_y, pos.win_x, pos.win_y, last_state, ev.timeStamp, button]);

    if (grab.window != null && grab.implicit)
	doUngrab(ev.timeStamp);
}

var last_key_down = 0;
function on_key_down (ev) {
  var key_code = ev.keyCode;
  if (key_code != last_key_down) {
    send_input ("k", [key_code, ev.timeStamp]);
    last_key_down = key_code;
  }
}

function on_key_up (ev) {
  var key_code = ev.keyCode;
  send_input ("K", [key_code, ev.timeStamp]);
  last_key_down = 0;
}

function cancel_event(ev)
{
  ev = ev ? ev : window.event;
  if (ev.stopPropagation)
    ev.stopPropagation();
  if (ev.preventDefault)
    ev.preventDefault();
  ev.cancelBubble = true;
  ev.cancel = true;
  ev.returnValue = false;
  return false;
}

function on_mouse_wheel(ev)
{
  ev = ev ? ev : window.event;

  var id = get_surface_id(ev);
  var pos = getPositionsFromEvent(ev, id);

  var offset = ev.detail ? ev.detail : ev.wheelDelta;
  var dir = 0;
  if (offset > 0)
    dir = 1;
  send_input ("s", [id, pos.root_x, pos.root_y, pos.win_x, pos.win_y, last_state, ev.timeStamp, dir]);

  return cancel_event(ev);
}

function connect()
{
  var xhr = createXHR();
  if (xhr) {
    if (typeof xhr.multipart == 'undefined') {
      alert("Sorry, this example only works in browsers that support multipart.");
      return;
    }

    xhr.multipart = true;
    xhr.open("GET", "/output", true);
    xhr.onload = handleLoad;
    xhr.send(null);
  }

  if ("WebSocket" in window) {
    var loc = window.location.toString().replace("http:", "ws:");
    loc = loc.substr(0, loc.lastIndexOf('/')) + "/input";
    var ws = new WebSocket(loc, "broadway");
    ws.onopen = function() {
      input_socket = ws;
    };
    ws.onclose = function() {
      input_socket = null;
    };
  } else {
     alert("WebSocket not supported, input will not work!");
  }
  document.oncontextmenu = function () { return false; };
  document.onmousemove = on_mouse_move;
  document.onmouseover = on_mouse_over;
  document.onmouseout = on_mouse_out;
  document.onmousedown = on_mouse_down;
  document.onmouseup = on_mouse_up;
  document.onkeydown = on_key_down;
  document.onkeyup = on_key_up;

  if (document.addEventListener) {
    document.addEventListener('DOMMouseScroll', on_mouse_wheel, false);
    document.addEventListener('mousewheel', on_mouse_wheel, false);
  } else if (document.attachEvent) {
    element.attachEvent("onmousewheel", on_mouse_wheel);
  }

}
