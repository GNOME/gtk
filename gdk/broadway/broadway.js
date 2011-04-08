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

/* This resizes the window so the *inner* size is the specified size */
function resizeBrowserWindow(window, w, h) {
    var innerW = window.innerWidth;
    var innerH = window.innerHeight;

    var outerW = window.outerWidth;
    var outerH = window.outerHeight;

    window.resizeTo(w + outerW - innerW,
		    h + outerH - innerH);
}

function resizeCanvas(canvas, w, h)
{
    /* Canvas resize clears the data, so we need to save it first */
    var tmpCanvas = canvas.ownerDocument.createElement("canvas");
    tmpCanvas.width = canvas.width;
    tmpCanvas.height = canvas.height;
    var tmpContext = tmpCanvas.getContext("2d");
    tmpContext.globalCompositeOperation = "copy";
    tmpContext.drawImage(canvas, 0, 0, tmpCanvas.width, tmpCanvas.height);

    canvas.width = w;
    canvas.height = h;

    var context = canvas.getContext("2d");

    context.globalCompositeOperation = "copy";
    context.drawImage(tmpCanvas, 0, 0, tmpCanvas.width, tmpCanvas.height);
}

var useToplevelWindows = false;
var toplevelWindows = [];
var grab = new Object();
grab.window = null;
grab.ownerEvents = false;
grab.implicit = false;
var localGrab = null;
var lastSerial = 0;
var lastX = 0;
var lastY = 0;
var lastState;
var lastTimeStamp = 0;
var realWindowWithMouse = 0;
var windowWithMouse = 0;
var surfaces = {};
var stackingOrder = [];
var outstandingCommands = new Array();
var inputSocket = null;
var frameSizeX = -1;
var frameSizeY = -1;

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
    var context = surface.context;
    var i = 0;
    for (i = 0; i < commands.length; i++) {
	var cmd = commands[i];
	switch (cmd.op) {
	case 'i': // put image data surface
	    context.globalCompositeOperation = "source-over";
	    context.drawImage(cmd.img, cmd.x, cmd.y);
	    break;

	case 'b': // copy rects
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

function ensureSurfaceInDocument(surface, doc)
{
    if (surface.document != doc) {
	var oldCanvas = surface.canvas;
	var canvas = doc.importNode(oldCanvas, false);
	doc.body.appendChild(canvas);
	canvas.surface = surface;
	oldCanvas.parentNode.removeChild(oldCanvas);

	surface.canvas = canvas;
	surface.document = doc;

	var context = canvas.getContext("2d");
	context.globalCompositeOperation = "source-over";
	surface.context = context;
    }
}

var windowGeometryTimeout = null;

function updateBrowserWindowGeometry(win) {
    if (win.closed)
	return;

    var surface = win.surface;

    var innerW = win.innerWidth;
    var innerH = win.innerHeight;

    var x = surface.x;
    var y = surface.y;
    if (frameSizeX > 0) {
	x = win.screenX + frameSizeX;
	y = win.screenY + frameSizeY;
    }

    if (x != surface.x || y != surface.y ||
       innerW != surface.width || innerH != surface.height) {
	var oldX = surface.x;
	var oldY = surface.y;
	surface.x = x;
	surface.y = y;
	if (surface.width != innerW || surface.height != innerH)
	    resizeCanvas(surface.canvas, innerW, innerH);
	surface.width = innerW;
	surface.height = innerH;
	sendInput ("w", [surface.id, surface.x, surface.y, surface.width, surface.height]);
	for (id in surfaces) {
	    if (surfaces[id].transientToplevel != null && surfaces[id].transientToplevel == surface) {
		var childSurface = surfaces[id];
		childSurface.x += surface.x - oldX;
		childSurface.y += surface.y - oldY;
		sendInput ("w", [childSurface.id, childSurface.x, childSurface.y, childSurface.width, childSurface.height]);
	    }
	}
    }
}

function browserWindowClosed(win) {
    var surface = win.surface;

    sendInput ("W", [surface.id]);
    for (id in surfaces) {
	if (surfaces[id].transientToplevel != null && 
	    surfaces[id].transientToplevel == surface) {
	    var childSurface = surfaces[id];
	    sendInput ("W", [childSurface.id]);
	}
    }
}

function registerWindow(win)
{
    toplevelWindows.push(win);
    win.onresize = function(ev) { updateBrowserWindowGeometry(ev.target); };
    if (!windowGeometryTimeout)
	windowGeometryTimeout = setInterval(function () { toplevelWindows.forEach(updateBrowserWindowGeometry); }, 2000);
    win.onunload = function(ev) { browserWindowClosed(ev.target.defaultView); };
}

function unregisterWindow(win)
{
    var i = toplevelWindows.indexOf(win);
    if (i >= 0)
	toplevelWindows.splice(i, 1);

    if (windowGeometryTimeout && toplevelWindows.length == 0) {
	clearInterval(windowGeometryTimeout);
	windowGeometryTimeout = null;
    }
}

function getTransientToplevel(surface)
{
    while (surface.transientParent != 0) {
	surface = surfaces[surface.transientParent];
	if (surface.window)
	    return surface;
    }
    return null;
}

function getFrameOffset(surface) {
    var x = 1;
    var y = 1;
    var el = surface.canvas;
    while (el != null && el != surface.frame) {
	x += el.offsetLeft;
	y += el.offsetTop;
	el = el.offsetParent;
    }
    return {x: x, y: y};
}

var positionIndex = 0;
function cmdCreateSurface(id, x, y, width, height, isTemp)
{
    var surface = { id: id, x: x, y:y, width: width, height: height, isTemp: isTemp };
    surface.positioned = isTemp;
    surface.drawQueue = [];
    surface.transientParent = 0;
    surface.visible = false;
    surface.window = null;
    surface.document = document;
    surface.transientToplevel = null;
    surface.frame = null;
    stackingOrder.push(id);

    var canvas = document.createElement("canvas");
    canvas.width = width;
    canvas.height = height;
    canvas.surface = surface;
    surface.canvas = canvas;

    if (useToplevelWindows || isTemp) {
	canvas.style["position"] = "absolute";
	canvas.style["left"] = "0px";
	canvas.style["top"] = "0px";
	canvas.style["display"] = "none";
	document.body.appendChild(canvas);
    } else {
	var frame = document.createElement("div");
	frame.frameFor = surface;
	frame.style["position"] = "absolute";
	frame.style["left"] = "0px";
	frame.style["top"] = "0px";
	frame.style["display"] = "inline";
	// We hide the frame with visibility rather than display none
	// so getFrameOffset still works with hidden windows
	frame.style["visibility"] = "hidden";
	frame.className = "frame-window";

	var button = document.createElement("center");
	var X = document.createTextNode("X");
	button.appendChild(X);
	button.className = "frame-close";
	frame.appendChild(button);

	var contents = document.createElement("div");
	contents.className = "frame-contents";
	frame.appendChild(contents);

	document.body.appendChild(frame);
	contents.appendChild(canvas);
	canvas.style["display"] = "block";

	surface.frame = frame;

	surface.x = 100 + positionIndex * 10;
	surface.y = 100 + positionIndex * 10;
	positionIndex = (positionIndex + 1) % 20;
	sendInput ("w", [surface.id, surface.x, surface.y, surface.width, surface.height]);
    }

    var context = canvas.getContext("2d");
    context.globalCompositeOperation = "source-over";
    surface.context = context;

    surfaces[id] = surface;
}

function cmdShowSurface(id)
{
    var surface = surfaces[id];

    if (surface.visible)
	return;
    surface.visible = true;

    var element = surface.canvas;
    var xOffset = surface.x;
    var yOffset = surface.y;

    if (useToplevelWindows) {
	var doc = document;
	if (!surface.isTemp) {
	    var options =
		'width='+surface.width+',height='+surface.height+
		',location=no,menubar=no,scrollbars=no,toolbar=no';
	    if (surface.positioned)
		options = options +
		',left='+surface.x+',top='+surface.y+',screenX='+surface.x+',screenY='+surface.y;
	    var win = window.open('','_blank', options);
	    win.surface = surface;
	    registerWindow(win);
	    doc = win.document;
	    doc.open();
	    doc.write("<body></body>");
	    setupDocument(doc);

	    surface.window = win;
	    xOffset = 0;
	    yOffset = 0;
	} else {
	    surface.transientToplevel = getTransientToplevel(surface);
	    if (surface.transientToplevel) {
		doc = surface.transientToplevel.window.document;
		xOffset = surface.x - surface.transientToplevel.x;
		yOffset = surface.y - surface.transientToplevel.y;
	    }
	}

	ensureSurfaceInDocument(surface, doc);
	element = surface.canvas;
    } else {
	if (surface.frame) {
	    element = surface.frame;
	    var offset = getFrameOffset(surface);
	    xOffset -= offset.x;
	    yOffset -= offset.y;
	}
    }

    element.style["position"] = "absolute";
    element.style["left"] = xOffset + "px";
    element.style["top"] = yOffset + "px";
    element.style["display"] = "inline";
    if (surface.frame)
	surface.frame.style["visibility"] = "visible";
}

function cmdHideSurface(id)
{
    var surface = surfaces[id];

    if (!surface.visible)
	return;
    surface.visible = false;

    var element = surface.canvas;
    if (surface.frame)
	element = surface.frame;

    if (surface.frame)
	surface.frame.style["visibility"] = "hidden";
    else
	element.style["display"] = "none";

    // Import the canvas into the main document
    ensureSurfaceInDocument(surface, document);

    if (surface.window) {
	unregisterWindow(surface.window);
	surface.window.close();
	surface.window = null;
    }
}

function cmdSetTransientFor(id, parentId)
{
    var surface = surfaces[id];

    if (surface.transientParent == parentId)
	return;

    surface.transientParent = parentId;
    if (surface.visible && surface.isTemp) {
	alert("TODO: move temps between transient parents when visible");
    }
}

function restackWindows() {
    if (useToplevelWindows)
	return;

    for (var i = 0; i < stackingOrder.length; i++) {
	var id = stackingOrder[i];
	var surface = surfaces[id];
	if (surface.frame)
	    surface.frame.style.zIndex = i;
	else
	    surface.canvas.style.zIndex = i;
    }
}

function moveToTopHelper(surface) {
    var i = stackingOrder.indexOf(surface.id);
    stackingOrder.splice(i, 1);
    stackingOrder.push(surface.id);

    for (var cid in surfaces) {
	var child = surfaces[cid];
	if (child.transientParent == surface.id)
	    moveToTopHelper(child);
    }
}

function moveToTop(surface) {
    moveToTopHelper(surface);
    restackWindows();
}


function cmdDeleteSurface(id)
{
    var surface = surfaces[id];
    var i = stackingOrder.indexOf(id);
    if (i >= 0)
	stackingOrder.splice(i, 1);
    var canvas = surface.canvas;
    canvas.parentNode.removeChild(canvas);
    var frame = surface.frame;
    if (frame)
	frame.parentNode.removeChild(frame);
    delete surfaces[id];
}

function cmdMoveSurface(id, x, y)
{
    var surface = surfaces[id];
    surface.positioned = true;
    surface.x = x;
    surface.y = y;

    if (surface.visible) {
	if (surface.window) {
	    /* TODO: This moves the outer frame position, we really want the inner position.
	     * However this isn't *strictly* invalid, as any WM could have done whatever it
	     * wanted with the positioning of the window.
	     */
	    surface.window.moveTo(surface.x, surface.y);
	} else {
	    var xOffset = surface.x;
	    var yOffset = surface.y;

	    var transientToplevel = getTransientToplevel(surface);
	    if (transientToplevel) {
		xOffset = surface.x - transientToplevel.x;
		yOffset = surface.y - transientToplevel.y;
	    }

	    var element = surface.canvas;
	    if (surface.frame) {
		element = surface.frame;
		var offset = getFrameOffset(surface);
		xOffset -= offset.x;
		yOffset -= offset.y;
	    }

	    element.style["left"] = xOffset + "px";
	    element.style["top"] = yOffset + "px";
	}
    }
}

function cmdResizeSurface(id, w, h)
{
    var surface = surfaces[id];

    surface.width = w;
    surface.height = h;

    /* Flush any outstanding draw ops before changing size */
    flushSurface(surface);

    resizeCanvas(surface.canvas, w, h);

    if (surface.window) {
	resizeBrowserWindow(surface.window, w, h);
    }
}

function cmdFlushSurface(id)
{
    flushSurface(surfaces[id]);
}

function cmdGrabPointer(id, ownerEvents)
{
    doGrab(id, ownerEvents, false);
    sendInput ("g", []);
}

function cmdUngrabPointer()
{
    sendInput ("u", []);

    grab.window = null;
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
	    var isTemp = cmd[i] == '1';
	    i = i + 1;
	    cmdCreateSurface(id, x, y, w, h, isTemp);
	    break;

	case 'S': // Show a surface
	    var id = base64_16(cmd, i);
	    i = i + 3;
	    cmdShowSurface(id);
	    break;

	case 'H': // Hide a surface
	    var id = base64_16(cmd, i);
	    i = i + 3;
	    cmdHideSurface(id);
	    break;

	case 'p': // Set transient parent
	    var id = base64_16(cmd, i);
	    i = i + 3;
	    var parentId = base64_16(cmd, i);
	    i = i + 3;
	    cmdSetTransientFor(id, parentId);
	    break;

	case 'd': // Delete surface
	    var id = base64_16(cmd, i);
	    i = i + 3;
	    cmdDeleteSurface(id);
	    break;

	case 'm': // Move a surface
	    var id = base64_16(cmd, i);
	    i = i + 3;
	    var x = base64_16(cmd, i);
	    i = i + 3;
	    var y = base64_16(cmd, i);
	    i = i + 3;
	    cmdMoveSurface(id, x, y);
	    break;

	case 'r': // Resize a surface
	    var id = base64_16(cmd, i);
	    i = i + 3;
	    var w = base64_16(cmd, i);
	    i = i + 3;
	    var h = base64_16(cmd, i);
	    i = i + 3;
	    cmdResizeSurface(id, w, h);
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

	    cmdFlushSurface(id);
	    break;

	case 'g': // Grab
	    var id = base64_16(cmd, i);
	    i = i + 3;
	    var ownerEvents = cmd[i++] == '1';

	    cmdGrabPointer(id, ownerEvents);
	    break;

	case 'u': // Ungrab
	    cmdUngrabPointer();
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
    var surface = ev.target.surface;
    if (surface != undefined)
	return surface.id;
    return 0;
}

function sendInput(cmd, args)
{
    if (inputSocket != null) {
	inputSocket.send(cmd + ([lastSerial, lastTimeStamp].concat(args)).join(","));
    }
}

function getPositionsFromAbsCoord(absX, absY, relativeId) {
    var res = Object();

    res.rootX = absX;
    res.rootY = absY;
    res.winX = absX;
    res.winY = absY;
    if (relativeId != 0) {
	var surface = surfaces[relativeId];
	res.winX = res.winX - surface.x;
	res.winY = res.winY - surface.y;
    }

    return res;
}

function getPositionsFromEvent(ev, relativeId) {
    var absX, absY;
    if (useToplevelWindows) {
	absX = ev.screenX;
	absY = ev.screenY;
    } else {
	absX = ev.pageX;
	absY = ev.pageY;
    }
    var res = getPositionsFromAbsCoord(absX, absY, relativeId);

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

function updateForEvent(ev) {
    lastTimeStamp = ev.timeStamp;
    if (ev.target.surface && ev.target.surface.window) {
	var win = ev.target.surface.window;
	if (ev.screenX != undefined && ev.clientX != undefined) {
	    var newFrameSizeX = ev.screenX - ev.clientX - win.screenX;
	    var newFrameSizeY = ev.screenY - ev.clientY - win.screenY;
	    if (newFrameSizeX != frameSizeX || newFrameSizeY != frameSizeY) {
		frameSizeX = newFrameSizeX;
		frameSizeY = newFrameSizeY;
		toplevelWindows.forEach(updateBrowserWindowGeometry);
	    }
	}
	updateBrowserWindowGeometry(win);
    }
}

function onMouseMove (ev) {
    updateForEvent(ev);
    if (localGrab) {
	var dx = ev.pageX - localGrab.lastX;
	var dy = ev.pageY - localGrab.lastY;
	var surface = localGrab.frame.frameFor;
	surface.x += dx;
	surface.y += dy;
	var offset = getFrameOffset(surface);
	localGrab.frame.style["left"] = (surface.x - offset.x) + "px";
	localGrab.frame.style["top"] = (surface.y - offset.y) + "px";
	sendInput ("w", [surface.id, surface.x, surface.y, surface.width, surface.height]);
	localGrab.lastX = ev.pageX;
	localGrab.lastY = ev.pageY;
	return;
    }
    var id = getSurfaceId(ev);
    id = getEffectiveEventTarget (id);
    var pos = getPositionsFromEvent(ev, id);
    sendInput ("m", [realWindowWithMouse, id, pos.rootX, pos.rootY, pos.winX, pos.winY, lastState]);
}

function onMouseOver (ev) {
    updateForEvent(ev);
    if (localGrab)
	return;
    var id = getSurfaceId(ev);
    realWindowWithMouse = id;
    id = getEffectiveEventTarget (id);
    var pos = getPositionsFromEvent(ev, id);
    windowWithMouse = id;
    if (windowWithMouse != 0) {
	sendInput ("e", [realWindowWithMouse, id, pos.rootX, pos.rootY, pos.winX, pos.winY, lastState, GDK_CROSSING_NORMAL]);
    }
}

function onMouseOut (ev) {
    updateForEvent(ev);
    if (localGrab)
	return;
    var id = getSurfaceId(ev);
    var origId = id;
    id = getEffectiveEventTarget (id);
    var pos = getPositionsFromEvent(ev, id);

    if (id != 0) {
	sendInput ("l", [realWindowWithMouse, id, pos.rootX, pos.rootY, pos.winX, pos.winY, lastState, GDK_CROSSING_NORMAL]);
    }
    realWindowWithMouse = 0;
    windowWithMouse = 0;
}

function doGrab(id, ownerEvents, implicit) {
    var pos;

    if (windowWithMouse != id) {
	if (windowWithMouse != 0) {
	    pos = getPositionsFromAbsCoord(lastX, lastY, windowWithMouse);
	    sendInput ("l", [realWindowWithMouse, windowWithMouse, pos.rootX, pos.rootY, pos.winX, pos.winY, lastState, GDK_CROSSING_GRAB]);
	}
	pos = getPositionsFromAbsCoord(lastX, lastY, id);
	sendInput ("e", [realWindowWithMouse, id, pos.rootX, pos.rootY, pos.winX, pos.winY, lastState, GDK_CROSSING_GRAB]);
	windowWithMouse = id;
    }

    grab.window = id;
    grab.ownerEvents = ownerEvents;
    grab.implicit = implicit;
}

function doUngrab() {
    var pos;
    if (realWindowWithMouse != windowWithMouse) {
	if (windowWithMouse != 0) {
	    pos = getPositionsFromAbsCoord(lastX, lastY, windowWithMouse);
	    sendInput ("l", [realWindowWithMouse, windowWithMouse, pos.rootX, pos.rootY, pos.winX, pos.winY, lastState, GDK_CROSSING_UNGRAB]);
	}
	if (realWindowWithMouse != 0) {
	    pos = getPositionsFromAbsCoord(lastX, lastY, realWindowWithMouse);
	    sendInput ("e", [realWindowWithMouse, realWindowWithMouse, pos.rootX, pos.rootY, pos.winX, pos.winY, lastState, GDK_CROSSING_UNGRAB]);
	}
	windowWithMouse = realWindowWithMouse;
    }
    grab.window = null;
}

function onMouseDown (ev) {
    updateForEvent(ev);
    var button = ev.button + 1;
    lastState = lastState | getButtonMask (button);
    var id = getSurfaceId(ev);
    id = getEffectiveEventTarget (id);

    if (id == 0 && ev.target.frameFor) { /* mouse click on frame */
	localGrab = new Object();
	localGrab.frame = ev.target;
	localGrab.lastX = ev.pageX;
	localGrab.lastY = ev.pageY;
	moveToTop(localGrab.frame.frameFor);
	return;
    }

    var pos = getPositionsFromEvent(ev, id);
    if (grab.window == null)
	doGrab (id, false, true);
    sendInput ("b", [realWindowWithMouse, id, pos.rootX, pos.rootY, pos.winX, pos.winY, lastState, button]);
}

function onMouseUp (ev) {
    updateForEvent(ev);
    var button = ev.button + 1;
    lastState = lastState & ~getButtonMask (button);
    var evId = getSurfaceId(ev);
    id = getEffectiveEventTarget (evId);
    var pos = getPositionsFromEvent(ev, id);

    if (localGrab) {
	localGrab = null;
	realWindowWithMouse = evId;
	if (windowWithMouse != id) {
	    if (windowWithMouse != 0) {
		sendInput ("l", [realWindowWithMouse, windowWithMouse, pos.rootX, pos.rootY, pos.winX, pos.winY, lastState, GDK_CROSSING_NORMAL]);
	    }
	    windowWithMouse = id;
	    if (windowWithMouse != 0) {
		sendInput ("e", [realWindowWithMouse, id, pos.rootX, pos.rootY, pos.winX, pos.winY, lastState, GDK_CROSSING_NORMAL]);
	    }
	}
	return;
    }

    sendInput ("B", [realWindowWithMouse, id, pos.rootX, pos.rootY, pos.winX, pos.winY, lastState, button]);

    if (grab.window != null && grab.implicit)
	doUngrab(ev.timeStamp);
}

var lastKeyDown = 0;
function onKeyDown (ev) {
    updateForEvent(ev);
    if (localGrab)
	return;
    var keyCode = ev.keyCode;
    if (keyCode != lastKeyDown) {
	sendInput ("k", [keyCode]);
	lastKeyDown = keyCode;
    }
}

function onKeyUp (ev) {
    updateForEvent(ev);
    if (localGrab)
	return;
    var keyCode = ev.keyCode;
    sendInput ("K", [keyCode]);
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
    updateForEvent(ev);
    if (localGrab)
	return;
    ev = ev ? ev : window.event;

    var id = getSurfaceId(ev);
    var pos = getPositionsFromEvent(ev, id);

    var offset = ev.detail ? ev.detail : ev.wheelDelta;
    var dir = 0;
    if (offset > 0)
	dir = 1;
    sendInput ("s", [realWindowWithMouse, id, pos.rootX, pos.rootY, pos.winX, pos.winY, lastState, dir]);

    return cancelEvent(ev);
}

function setupDocument(document)
{
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

function connect()
{
    var url = window.location.toString();
    var query_string = url.split("?");
    if (query_string.length > 1) {
	var params = query_string[1].split("&");
	if (params[0].indexOf("toplevel") != -1)
	    useToplevelWindows = true;
    }
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
	    
	    var w, h;
	    if (useToplevelWindows) {
		w = window.screen.width;
		h = window.screen.height;
	    } else {
		w = window.innerWidth;
		h = window.innerHeight;
		window.onresize = function(ev) { 
		    var w, h;
		    w = window.innerWidth;
		    h = window.innerHeight;
		    sendInput ("d", [w, h]);
		};
	    }
	    sendInput ("d", [w, h]);
	};
	ws.onclose = function() {
	    inputSocket = null;
	};
    } else {
	alert("WebSocket not supported, input will not work!");
    }
    setupDocument(document);
    window.onunload = function (ev) {
	for (var i = 0; i < toplevelWindows.length; i++)
	    toplevelWindows[i].close();
    };
}
