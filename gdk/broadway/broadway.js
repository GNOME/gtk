
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

var surfaces = {};
var outstanding_commands = new Array();
var input_socket = null;

function initContext(canvas, x, y, id)
{
  canvas.surface_id = id;
  canvas.style["position"] = "absolute"
  canvas.style["top"] = x + "px"
  canvas.style["left"] = y + "px"
  canvas.style["display"] = "none"
  context = canvas.getContext("2d")
  context.globalCompositeOperation = "src-over"
  context.fillRect(0, 0, canvas.width, canvas.height);
  document.body.appendChild(canvas)

  return context
}

function handleCommands(cmd_obj)
{
  var cmd = cmd_obj.data;
  var i = cmd_obj.pos;

  while (i < cmd.length) {
    var command = cmd[i++];
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
	surfaces[id].canvas.style["display"] = "inline";
        break;

      /* delete surface */
      case 'd':
        var id = base64_16(cmd, i);
        i = i + 3;
	var canvas = surfaces[id].canvas
	delete surfaces[id]
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

      /* put image data surface */
      case 'i':
        var id = base64_16(cmd, i);
        i = i + 3;
        var x = base64_16(cmd, i);
        i = i + 3;
        var y = base64_16(cmd, i);
        i = i + 3;
        var size = base64_32(cmd, i);
        i = i + 6;
	var url = cmd.slice(i, i + size);
	i = i + size;
        var img = new Image();
	img.src = url
	if (img.complete) {
          surfaces[id].drawImage(img, x, y);
	} else {
	  cmd_obj.pos = i;
	  img.onload = function() { surfaces[id].drawImage(img, x, y); handleOutstanding(); }
	  return false
	}

        break;

      /* copy rects */
      case 'b':
        var id = base64_16(cmd, i);
        i = i + 3;

	var nrects = base64_16(cmd, i);
        i = i + 3;

	var context = surfaces[id];
	context.save();

	for (var r = 0; r < nrects; r++) {
	  var x = base64_16(cmd, i);
          i = i + 3;
          var y = base64_16(cmd, i);
          i = i + 3;
          var w = base64_16(cmd, i);
          i = i + 3;
          var h = base64_16(cmd, i);
          i = i + 3;
	  context.rect(x, y, w, h);
	}

	context.clip()

        var dx = base64_16s(cmd, i);
        i = i + 3;
        var dy = base64_16s(cmd, i);
        i = i + 3;

        context.drawImage(context.canvas, dx, dy, context.canvas.width, context.canvas.height);

	context.restore();
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
    handleOutstanding()
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
      input_socket.send(cmd + args.join(","));
  }
}

function on_mouse_move (ev) {
  send_input ("m", [get_surface_id(ev), ev.pageX, ev.pageY, ev.timeStamp])
}

function on_mouse_down (ev) {
  send_input ("b", [get_surface_id(ev), ev.pageX, ev.pageY, ev.button, ev.timeStamp])
}

function on_mouse_up (ev) {
  send_input ("B", [get_surface_id(ev), ev.pageX, ev.pageY, ev.button, ev.timeStamp])
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

  document.onmousemove = on_mouse_move;
  document.onmousedown = on_mouse_down;
  document.onmouseup = on_mouse_up;
}
