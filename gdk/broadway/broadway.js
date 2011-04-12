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

function getStackTrace()
{
    var callstack = [];
    var isCallstackPopulated = false;
    try {
	i.dont.exist+=0;
    } catch(e) {
	if (e.stack) { // Firefox
	    var lines = e.stack.split("\n");
	    for (var i=0, len=lines.length; i<len; i++) {
		if (lines[i].match(/^\s*[A-Za-z0-9\-_\$]+\(/)) {
		    callstack.push(lines[i]);
		}
	    }
	    // Remove call to getStackTrace()
	    callstack.shift();
	    isCallstackPopulated = true;
	} else if (window.opera && e.message) { // Opera
	    var lines = e.message.split("\n");
	    for (var i=0, len=lines.length; i<len; i++) {
		if (lines[i].match(/^\s*[A-Za-z0-9\-_\$]+\(/)) {
		    var entry = lines[i];
		    // Append next line also since it has the file info
		    if (lines[i+1]) {
			entry += " at " + lines[i+1];
			i++;
		    }
		    callstack.push(entry);
		}
	    }
	    // Remove call to getStackTrace()
	    callstack.shift();
	    isCallstackPopulated = true;
	}
    }
    if (!isCallstackPopulated) { //IE and Safari
	var currentFunction = arguments.callee.caller;
	while (currentFunction) {
	    var fn = currentFunction.toString();
	    var fname = fn.substring(fn.indexOf("function") + 8, fn.indexOf("(")) || "anonymous";
	    callstack.push(fname);
	    currentFunction = currentFunction.caller;
	}
    }
    return callstack;
}

function logStackTrace(len) {
    var callstack = getStackTrace();
    var end = callstack.length;
    if (len > 0)
	end = Math.min(len + 1, end);
    for (var i = 1; i < end; i++)
	log(callstack[i]);
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
];

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
    var context = surface.canvas.getContext("2d");
    context.globalCompositeOperation = "source-over";
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
	if (surface.toplevelElement == oldCanvas)
	    surface.toplevelElement = canvas;
	surface.document = doc;
    }
}

function sendConfigureNotify(surface)
{
    sendInput("w", [surface.id, surface.x, surface.y, surface.width, surface.height]);
}

var windowGeometryTimeout = null;

function updateBrowserWindowGeometry(win, alwaysSendConfigure) {
    if (win.closed)
	return;

    var surface = win.surface;

    var innerW = win.innerWidth;
    var innerH = win.innerHeight;

    var x = surface.x;
    var y = surface.y;

    if (win.mozInnerScreenX != undefined) {
	x = win.mozInnerScreenX;
	y = win.mozInnerScreenY;
    } else if (win.screenTop != undefined) {
	x = win.screenTop;
	y = win.screenLeft;
    } else {
	alert("No implementation to get window position");
    }

    if (alwaysSendConfigure || x != surface.x || y != surface.y ||
       innerW != surface.width || innerH != surface.height) {
	var oldX = surface.x;
	var oldY = surface.y;
	surface.x = x;
	surface.y = y;
	if (surface.width != innerW || surface.height != innerH)
	    resizeCanvas(surface.canvas, innerW, innerH);
	surface.width = innerW;
	surface.height = innerH;
	sendConfigureNotify(surface);
	for (id in surfaces) {
	    var childSurface = surfaces[id];
	    var transientToplevel = getTransientToplevel(childSurface);
	    if (transientToplevel != null && transientToplevel == surface) {
		childSurface.x += surface.x - oldX;
		childSurface.y += surface.y - oldY;
		sendConfigureNotify(childSurface);
	    }
	}
    }
}

function browserWindowClosed(win) {
    var surface = win.surface;

    sendInput ("W", [surface.id]);
    for (id in surfaces) {
	var childSurface = surfaces[id];
	var transientToplevel = getTransientToplevel(childSurface);
	if (transientToplevel != null && transientToplevel == surface) {
	    sendInput ("W", [childSurface.id]);
	}
    }
}

function registerWindow(win)
{
    toplevelWindows.push(win);
    win.onresize = function(ev) { updateBrowserWindowGeometry(ev.target, false); };
    if (!windowGeometryTimeout)
	windowGeometryTimeout = setInterval(function () {
						for (var i = 0; i < toplevelWindows.length; i++)
						    updateBrowserWindowGeometry(toplevelWindows[i], false);
					    }, 2000);
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
    while (surface && surface.transientParent != 0) {
	surface = surfaces[surface.transientParent];
	if (surface && surface.window)
	    return surface;
    }
    return null;
}

function getStyle(el, styleProp)
{
    if (el.currentStyle) {
	return el.currentStyle[styleProp];
    }  else if (window.getComputedStyle) {
	var win = el.ownerDocument.defaultView;
	return win.getComputedStyle(el, null).getPropertyValue(styleProp);
    }
    return undefined;
}

function parseOffset(value)
{
    var px = value.indexOf("px");
    if (px > 0)
	return parseInt(value.slice(0,px));
    return 0;
}

function getFrameOffset(surface) {
    var x = 0;
    var y = 0;
    var el = surface.canvas;
    while (el != null && el != surface.frame) {
	x += el.offsetLeft;
	y += el.offsetTop;

	/* For some reason the border is not includes in the offsets.. */
	x += parseOffset(getStyle(el, "border-left-width"));
	y += parseOffset(getStyle(el, "border-top-width"));

	el = el.offsetParent;
    }

    /* Also include frame border as per above */
    x += parseOffset(getStyle(el, "border-left-width"));
    y += parseOffset(getStyle(el, "border-top-width"));

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
    surface.frame = null;

    var canvas = document.createElement("canvas");
    canvas.width = width;
    canvas.height = height;
    canvas.surface = surface;
    surface.canvas = canvas;
    var toplevelElement;

    if (useToplevelWindows || isTemp) {
	toplevelElement = canvas;
	document.body.appendChild(canvas);
    } else {
	var frame = document.createElement("div");
	frame.frameFor = surface;
	frame.className = "frame-window";
	surface.frame = frame;

	var button = document.createElement("center");
	var X = document.createTextNode("X");
	button.appendChild(X);
	button.className = "frame-close";
	frame.appendChild(button);

	var contents = document.createElement("div");
	contents.className = "frame-contents";
	frame.appendChild(contents);

	canvas.style["display"] = "block";
	contents.appendChild(canvas);

	toplevelElement = frame;
	document.body.appendChild(frame);

	surface.x = 100 + positionIndex * 10;
	surface.y = 100 + positionIndex * 10;
	positionIndex = (positionIndex + 1) % 20;
    }

    surface.toplevelElement = toplevelElement;
    toplevelElement.style["position"] = "absolute";
    /* This positioning isn't strictly right for apps in another topwindow,
     * but that will be fixed up when showing. */
    toplevelElement.style["left"] = surface.x + "px";
    toplevelElement.style["top"] = surface.y + "px";
    toplevelElement.style["display"] = "inline";

    /* We hide the frame with visibility rather than display none
     * so getFrameOffset still works with hidden windows. */
    toplevelElement.style["visibility"] = "hidden";

    surfaces[id] = surface;
    stackingOrder.push(surface);

    sendConfigureNotify(surface);
}

function cmdShowSurface(id)
{
    var surface = surfaces[id];

    if (surface.visible)
	return;
    surface.visible = true;

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
	    var transientToplevel = getTransientToplevel(surface);
	    if (transientToplevel) {
		doc = transientToplevel.window.document;
		xOffset = surface.x - transientToplevel.x;
		yOffset = surface.y - transientToplevel.y;
	    }
	}

	ensureSurfaceInDocument(surface, doc);
    } else {
	if (surface.frame) {
	    var offset = getFrameOffset(surface);
	    xOffset -= offset.x;
	    yOffset -= offset.y;
	}
    }

    surface.toplevelElement.style["left"] = xOffset + "px";
    surface.toplevelElement.style["top"] = yOffset + "px";
    surface.toplevelElement.style["visibility"] = "visible";

    restackWindows();

    if (surface.window)
	updateBrowserWindowGeometry(surface.window, false);
}

function cmdHideSurface(id)
{
    var surface = surfaces[id];

    if (!surface.visible)
	return;
    surface.visible = false;

    var element = surface.toplevelElement;

    element.style["visibility"] = "hidden";

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
    if (parentId != 0 && surfaces[parentId]) {
	moveToHelper(surface, stackingOrder.indexOf(surfaces[parentId])+1);
    }

    if (surface.visible) {
	restackWindows();
    }
}

function restackWindows() {
    for (var i = 0; i < stackingOrder.length; i++) {
	var surface = stackingOrder[i];
	surface.toplevelElement.style.zIndex = i;
    }
}

function moveToHelper(surface, position) {
    var i = stackingOrder.indexOf(surface);
    stackingOrder.splice(i, 1);
    if (position != undefined)
	stackingOrder.splice(position, 0, surface);
    else
	stackingOrder.push(surface);

    for (var cid in surfaces) {
	var child = surfaces[cid];
	if (child.transientParent == surface.id)
	    moveToHelper(child, stackingOrder.indexOf(surface) + 1);
    }
}

function moveToTop(surface) {
    moveToHelper(surface);
    restackWindows();
}


function cmdDeleteSurface(id)
{
    var surface = surfaces[id];
    var i = stackingOrder.indexOf(surface);
    if (i >= 0)
	stackingOrder.splice(i, 1);
    var canvas = surface.canvas;
    canvas.parentNode.removeChild(canvas);
    var frame = surface.frame;
    if (frame)
	frame.parentNode.removeChild(frame);
    delete surfaces[id];
}

function cmdMoveResizeSurface(id, has_pos, x, y, has_size, w, h)
{
    var surface = surfaces[id];
    if (has_pos) {
	surface.positioned = true;
	surface.x = x;
	surface.y = y;
    }
    if (has_size) {
	surface.width = w;
	surface.height = h;
    }

    /* Flush any outstanding draw ops before (possibly) changing size */
    flushSurface(surface);

    if (has_size)
	resizeCanvas(surface.canvas, w, h);

    if (surface.visible) {
	if (surface.window) {
	    /* TODO: This moves the outer frame position, we really want the inner position.
	     * However this isn't *strictly* invalid, as any WM could have done whatever it
	     * wanted with the positioning of the window.
	     */
	    if (has_pos)
		surface.window.moveTo(surface.x, surface.y);
	    if (has_size)
		resizeBrowserWindow(surface.window, w, h);
	} else {
	    if (has_pos) {
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

    if (surface.window) {
	updateBrowserWindowGeometry(surface.window, true);
    } else {
	sendConfigureNotify(surface);
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
	var id, x, y, w, h, q;
	var command = cmd[i++];
	lastSerial = base64_32(cmd, i);
	i = i + 6;
	switch (command) {
	case 's': // create new surface
	    id = base64_16(cmd, i);
	    i = i + 3;
	    x = base64_16s(cmd, i);
	    i = i + 3;
	    y = base64_16s(cmd, i);
	    i = i + 3;
	    w = base64_16(cmd, i);
	    i = i + 3;
	    h = base64_16(cmd, i);
	    i = i + 3;
	    var isTemp = cmd[i] == '1';
	    i = i + 1;
	    cmdCreateSurface(id, x, y, w, h, isTemp);
	    break;

	case 'S': // Show a surface
	    id = base64_16(cmd, i);
	    i = i + 3;
	    cmdShowSurface(id);
	    break;

	case 'H': // Hide a surface
	    id = base64_16(cmd, i);
	    i = i + 3;
	    cmdHideSurface(id);
	    break;

	case 'p': // Set transient parent
	    id = base64_16(cmd, i);
	    i = i + 3;
	    var parentId = base64_16(cmd, i);
	    i = i + 3;
	    cmdSetTransientFor(id, parentId);
	    break;

	case 'd': // Delete surface
	    id = base64_16(cmd, i);
	    i = i + 3;
	    cmdDeleteSurface(id);
	    break;

	case 'm': // Move a surface
	    id = base64_16(cmd, i);
	    i = i + 3;
	    var ops = cmd.charCodeAt(i++) - 48;
	    var has_pos = ops & 1;
	    if (has_pos) {
		x = base64_16s(cmd, i);
		i = i + 3;
		y = base64_16s(cmd, i);
		i = i + 3;
	    }
	    var has_size = ops & 2;
	    if (has_size) {
		w = base64_16(cmd, i);
		i = i + 3;
		h = base64_16(cmd, i);
		i = i + 3;
	    }
	    cmdMoveResizeSurface(id, has_pos, x, y, has_size, w, h);
	    break;

	case 'i': // Put image data surface
	    q = new Object();
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
	    q = new Object();
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
	    id = base64_16(cmd, i);
	    i = i + 3;

	    cmdFlushSurface(id);
	    break;

	case 'g': // Grab
	    id = base64_16(cmd, i);
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
	updateBrowserWindowGeometry(win, false);
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
	sendConfigureNotify(surface);
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
	return cancelEvent(ev);
    var keyCode = ev.keyCode;
    if (keyCode != lastKeyDown) {
	sendInput ("k", [keyCode]);
	lastKeyDown = keyCode;
    }
    return cancelEvent(ev);
}

function onKeyUp (ev) {
    updateForEvent(ev);
    if (localGrab)
	return cancelEvent(ev);
    var keyCode = ev.keyCode;
    sendInput ("K", [keyCode]);
    lastKeyDown = 0;
    return cancelEvent(ev);
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
	return false;
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
