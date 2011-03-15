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

var base64Values = [
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
	(base64Values[str.charCodeAt(index)]) +
	(base64Values[str.charCodeAt(index+1)] << 6);
    return v;
}

function base64_16(str, index) {
    var v =
	(base64Values[str.charCodeAt(index)]) +
	(base64Values[str.charCodeAt(index+1)] << 6) +
	(base64Values[str.charCodeAt(index+2)] << 12);
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
	(base64Values[str.charCodeAt(index)]) +
	(base64Values[str.charCodeAt(index+1)] << 6) +
	(base64Values[str.charCodeAt(index+2)] << 12) +
	(base64Values[str.charCodeAt(index+3)] << 18);
    return v;
}

function base64_32(str, index) {
    var v =
	(base64Values[str.charCodeAt(index)]) +
	(base64Values[str.charCodeAt(index+1)] << 6) +
	(base64Values[str.charCodeAt(index+2)] << 12) +
	(base64Values[str.charCodeAt(index+3)] << 18) +
	(base64Values[str.charCodeAt(index+4)] << 24) +
	(base64Values[str.charCodeAt(index+5)] << 30);
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
grab.ownerEvents = false;
grab.time = 0;
grab.implicit = false;
var lastSerial = 0;
var lastX = 0;
var lastY = 0;
var lastState;
var realWindowWithMouse = 0;
var windowWithMouse = 0;
var surfaces = {};
var outstandingCommands = new Array();
var inputSocket = null;

function initContext(canvas, x, y, id)
{
    canvas.surfaceId = id;
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

function handleCommands(cmdObj)
{
    var cmd = cmdObj.data;
    var i = cmdObj.pos;

    while (i < cmd.length) {
	var command = cmd[i++];
	lastSerial = base64_32(cmd, i);
	i = i + 6;
	switch (command) {
	case 's': // create new surface
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

	case 'S': // Show a surface
	    var id = base64_16(cmd, i);
	    i = i + 3;
	    surfaces[id].canvas.style["display"] = "inline";
	    break;

	case 'H': // Hide a surface
	    var id = base64_16(cmd, i);
	    i = i + 3;
	    surfaces[id].canvas.style["display"] = "none";
	    break;

	case 'd': // Delete surface
	    var id = base64_16(cmd, i);
	    i = i + 3;
	    var canvas = surfaces[id].canvas;
	    delete surfaces[id];
	    canvas.parentNode.removeChild(canvas);

	    break;

	case 'm': // Move a surface
	    var id = base64_16(cmd, i);
	    i = i + 3;
	    var x = base64_16(cmd, i);
	    i = i + 3;
	    var y = base64_16(cmd, i);
	    i = i + 3;
	    surfaces[id].canvas.style["left"] = x + "px";
	    surfaces[id].canvas.style["top"] = y + "px";
	    break;

	case 'r': // Resize a surface
	    var id = base64_16(cmd, i);
	    i = i + 3;
	    var w = base64_16(cmd, i);
	    i = i + 3;
	    var h = base64_16(cmd, i);
	    i = i + 3;
	    var surface = surfaces[id];

	    /* Flush any outstanding draw ops before changing size */
	    flushSurface(surface);

	    /* Canvas resize clears the data, so we need to save it first */
	    var tmpCanvas = document.createElement("canvas");
	    tmpCanvas.width = surface.canvas.width;
	    tmpCanvas.height = surface.canvas.height;
	    var tmpContext = tmpCanvas.getContext("2d");
	    tmpContext.globalCompositeOperation = "copy";
	    tmpContext.drawImage(surface.canvas, 0, 0, tmpCanvas.width, tmpCanvas.height);

	    surface.canvas.width = w;
	    surface.canvas.height = h;

	    surface.globalCompositeOperation = "copy";
	    surface.drawImage(tmpCanvas, 0, 0, tmpCanvas.width, tmpCanvas.height);

	    break;

	case 'i': // Put image data surface
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
		cmdObj.pos = i;
		q.img.onload = function() { handleOutstanding(); };
		return false;
	    }
	    break;

	case 'b': // Copy rects
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

	    var pos = getPositionsFromAbsCoord(lastX, lastY, id);

	    sendInput ("q", [pos.rootX, pos.rootY, pos.winX, pos.winY, windowWithMouse]);
	    break;

	case 'g': // Grab
	    var id = base64_16(cmd, i);
	    i = i + 3;
	    var ownerEvents = cmd[i++] == '1';
	    var time = base64_32(cmd, i);
	    i = i + 6;

	    if (grab.window != null) {
		/* Previous grab, compare times */
		if (time != 0 && grab.time != 0 &&
		    time > grab.time) {
		    sendInput ("g", [GDK_GRAB_INVALID_TIME]);
		    break;
		}
	    }

	    doGrab(id, ownerEvents, time, false);

	    sendInput ("g", [GDK_GRAB_SUCCESS]);
	    break;

	case 'u': // Ungrab
	    var time = base64_32(cmd, i);
	    i = i + 6;
	    sendInput ("u", []);

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
    while (outstandingCommands.length > 0) {
	var cmd = outstandingCommands.shift();
	if (!handleCommands(cmd)) {
	    outstandingCommands.unshift(cmd);
	    return;
	}
    }
}

function handleLoad(event)
{
    var cmdObj = {};
    cmdObj.data = event.target.responseText;
    cmdObj.pos = 0;

    outstandingCommands.push(cmdObj);
    if (outstandingCommands.length == 1) {
	handleOutstanding();
    }
}

function getSurfaceId(ev) {
    var id = ev.target.surfaceId;
    if (id != undefined)
	return id;
    return 0;
}

function sendInput(cmd, args)
{
    if (inputSocket != null) {
	inputSocket.send(cmd + ([lastSerial].concat(args)).join(","));
    }
}

function getDocumentCoordinates(element)
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

    res.rootX = absX;
    res.rootY = absY;
    res.winX = absX;
    res.winY = absY;
    if (relativeId != 0) {
	var pos = getDocumentCoordinates(surfaces[relativeId].canvas);
	res.winX = res.winX - pos.x;
	res.winY = res.winY - pos.y;
    }

    return res;
}

function getPositionsFromEvent(ev, relativeId) {
    var res = getPositionsFromAbsCoord(ev.pageX, ev.pageY, relativeId);

    lastX = res.rootX;
    lastY = res.rootY;

    return res;
}

function getEffectiveEventTarget (id) {
    if (grab.window != null) {
	if (!grab.ownerEvents)
	    return grab.window;
	if (id == 0)
	    return grab.window;
    }
    return id;
}

function onMouseMove (ev) {
    var id = getSurfaceId(ev);
    id = getEffectiveEventTarget (id);
    var pos = getPositionsFromEvent(ev, id);
    sendInput ("m", [id, pos.rootX, pos.rootY, pos.winX, pos.winY, lastState, ev.timeStamp]);
}

function onMouseOver (ev) {
    var id = getSurfaceId(ev);
    realWindowWithMouse = id;
    id = getEffectiveEventTarget (id);
    var pos = getPositionsFromEvent(ev, id);
    windowWithMouse = id;
    if (windowWithMouse != 0) {
	sendInput ("e", [id, pos.rootX, pos.rootY, pos.winX, pos.winY, lastState, ev.timeStamp, GDK_CROSSING_NORMAL]);
    }
}

function onMouseOut (ev) {
    var id = getSurfaceId(ev);
    var origId = id;
    id = getEffectiveEventTarget (id);
    var pos = getPositionsFromEvent(ev, id);

    if (id != 0) {
	sendInput ("l", [id, pos.rootX, pos.rootY, pos.winX, pos.winY, lastState, ev.timeStamp, GDK_CROSSING_NORMAL]);
    }
    realWindowWithMouse = 0;
    windowWithMouse = 0;
}

function doGrab(id, ownerEvents, time, implicit) {
    var pos;

    if (windowWithMouse != id) {
	if (windowWithMouse != 0) {
	    pos = getPositionsFromAbsCoord(lastX, lastY, windowWithMouse);
	    sendInput ("l", [windowWithMouse, pos.rootX, pos.rootY, pos.winX, pos.winY, lastState, time, GDK_CROSSING_GRAB]);
	}
	pos = getPositionsFromAbsCoord(lastX, lastY, id);
	sendInput ("e", [id, pos.rootX, pos.rootY, pos.winX, pos.winY, lastState, time, GDK_CROSSING_GRAB]);
	windowWithMouse = id;
    }

    grab.window = id;
    grab.ownerEvents = ownerEvents;
    grab.time = time;
    grab.implicit = implicit;
}

function doUngrab(time) {
    var pos;
    if (realWindowWithMouse != windowWithMouse) {
	if (windowWithMouse != 0) {
	    pos = getPositionsFromAbsCoord(lastX, lastY, windowWithMouse);
	    sendInput ("l", [windowWithMouse, pos.rootX, pos.rootY, pos.winX, pos.winY, lastState, time, GDK_CROSSING_UNGRAB]);
	}
	if (realWindowWithMouse != 0) {
	    pos = getPositionsFromAbsCoord(lastX, lastY, realWindowWithMouse);
	    sendInput ("e", [realWindowWithMouse, pos.rootX, pos.rootY, pos.winX, pos.winY, lastState, time, GDK_CROSSING_UNGRAB]);
	}
	windowWithMouse = realWindowWithMouse;
    }
    grab.window = null;
}

function onMouseDown (ev) {
    var id = getSurfaceId(ev);
    id = getEffectiveEventTarget (id);
    var pos = getPositionsFromEvent(ev, id);
    if (grab.window != null)
	doGrab (id, false, ev.timeStamp, true);
    var button = ev.button + 1;
    lastState = lastState | getButtonMask (button);
    sendInput ("b", [id, pos.rootX, pos.rootY, pos.winX, pos.winY, lastState, ev.timeStamp, button]);
}

function onMouseUp (ev) {
    var id = getSurfaceId(ev);
    id = getEffectiveEventTarget (id);
    var pos = getPositionsFromEvent(ev, id);
    var button = ev.button + 1;
    lastState = lastState & ~getButtonMask (button);
    sendInput ("B", [id, pos.rootX, pos.rootY, pos.winX, pos.winY, lastState, ev.timeStamp, button]);

    if (grab.window != null && grab.implicit)
	doUngrab(ev.timeStamp);
}

var lastKeyDown = 0;
function onKeyDown (ev) {
    var keyCode = ev.keyCode;
    if (keyCode != lastKeyDown) {
	sendInput ("k", [keyCode, ev.timeStamp]);
	lastKeyDown = keyCode;
    }
}

function onKeyUp (ev) {
    var keyCode = ev.keyCode;
    sendInput ("K", [keyCode, ev.timeStamp]);
    lastKeyDown = 0;
}

function cancelEvent(ev)
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

function onMouseWheel(ev)
{
    ev = ev ? ev : window.event;

    var id = getSurfaceId(ev);
    var pos = getPositionsFromEvent(ev, id);

    var offset = ev.detail ? ev.detail : ev.wheelDelta;
    var dir = 0;
    if (offset > 0)
	dir = 1;
    sendInput ("s", [id, pos.rootX, pos.rootY, pos.winX, pos.winY, lastState, ev.timeStamp, dir]);

    return cancelEvent(ev);
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
	    inputSocket = ws;
	};
	ws.onclose = function() {
	    inputSocket = null;
	};
    } else {
	alert("WebSocket not supported, input will not work!");
    }
    document.oncontextmenu = function () { return false; };
    document.onmousemove = onMouseMove;
    document.onmouseover = onMouseOver;
    document.onmouseout = onMouseOut;
    document.onmousedown = onMouseDown;
    document.onmouseup = onMouseUp;
    document.onkeydown = onKeyDown;
    document.onkeyup = onKeyUp;

    if (document.addEventListener) {
	document.addEventListener('DOMMouseScroll', onMouseWheel, false);
	document.addEventListener('mousewheel', onMouseWheel, false);
    } else if (document.attachEvent) {
	element.attachEvent("onmousewheel", onMouseWheel);
    }
}
