/* 
 * gxid version 0.3
 *
 * Copyright 1997 Owen Taylor <owt1@cornell.edu>
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <X11/Xlib.h>
#include <X11/extensions/XInput.h>

#include "gxid_proto.h"

/* #define DEBUG_CLIENTS  */
/* #define DEBUG_EVENTS */

char *program_name;
Display *dpy;
Window root_window;		/* default root window of dpy */
int port = 0;		        /* port to listen on */
int socket_fd = 0;		/* file descriptor of socket */
typedef struct GxidWindow_ GxidWindow;

typedef struct GxidDevice_ GxidDevice;
struct GxidDevice_ {
  XID id;
  int exclusive;
  int ispointer;
  
  XDevice *xdevice;
  int motionnotify_type;
  int changenotify_type;
};

struct GxidWindow_ {
  Window xwindow;
  /* Immediate child of root that is ancestor of window */
  Window root_child;
  int num_devices;
  GxidDevice **devices;
};

GxidDevice **devices = NULL;
int num_devices = 0;
GxidWindow **windows = NULL;
int num_windows = 0;

void
handler(int signal)
{
  fprintf(stderr,"%s: dying on signal %d\n",program_name,signal);
  if (socket_fd)
    close(socket_fd);
  exit(1);
}

void
init_socket(void)
{
  struct sockaddr_in sin;

  socket_fd = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
  if (socket_fd < 0)
    {
      fprintf (stderr, "%s: error getting socket\n",
	       program_name);
      exit(1);
    }
  
  sin.sin_family = AF_INET;
  sin.sin_port = htons(port);
  sin.sin_addr.s_addr = INADDR_ANY;
  
  if (bind(socket_fd,(struct sockaddr *)(&sin),
	   sizeof(struct sockaddr_in)) < 0)
    {
      fprintf (stderr,"%s: cannot bind to port %d\n",
	       program_name,port);
      exit(1);
    }

  if (listen(socket_fd,5) < 0)
    {
      fprintf (stderr,"%s: error listening on socket\n",
	       program_name);
      exit(1);
    };
}

#define NUM_EVENTC 2
static void
enable_device(GxidDevice *dev)
{
  XEventClass xevc[NUM_EVENTC];
  int num_eventc = NUM_EVENTC;
  int i,j;

  if (!dev->xdevice) 
    {
      if (dev->ispointer) return;

      dev->xdevice = XOpenDevice(dpy, dev->id);
      if (!dev->xdevice) return;
      
      DeviceMotionNotify (dev->xdevice, dev->motionnotify_type,
			  xevc[0]);
      ChangeDeviceNotify (dev->xdevice, dev->changenotify_type,
			  xevc[1]);

      /* compress out zero event classes */
      for (i=0,j=0;i<NUM_EVENTC;i++)
	{
	  if (xevc[i]) {
	    xevc[j] = xevc[i];
	    j++;
	  }
      }
      num_eventc = j;
      
      XSelectExtensionEvent (dpy, root_window, xevc, num_eventc);
    }
}

/* switch the core pointer from whatever it is now to something else,
   return true on success, false otherwise */
static int
switch_core_pointer(void)
{
  GxidDevice *old_pointer = 0;
  GxidDevice *new_pointer = 0;
  int result;
  int i;

  for (i=0;i<num_devices;i++)
    {
      if (devices[i]->ispointer)
	old_pointer = devices[i];
      else
	if (!new_pointer && !devices[i]->exclusive)
	  new_pointer = devices[i];
    }

  if (!old_pointer || !new_pointer)
    return 0;

#ifdef DEBUG_EVENTS
  fprintf(stderr,"gxid: Switching core from %ld to %ld\n",
	 old_pointer->id,new_pointer->id);
#endif
  result = XChangePointerDevice(dpy,new_pointer->xdevice, 0, 1);
  if (result != Success)
    {
      fprintf(stderr,"gxid: Error %d switching core from %ld to %ld\n",
	      result, old_pointer->id, new_pointer->id);
    }
  else
    {
      new_pointer->ispointer = 1;
      old_pointer->ispointer = 0;
      if (!old_pointer->xdevice)
	enable_device(old_pointer);
    }

  return 1;
}

void
disable_device(GxidDevice *dev)
{
  if (dev->xdevice)
    {
      if (dev->ispointer)
	return;
      XCloseDevice(dpy,dev->xdevice);
      dev->xdevice = 0;
    }
}

GxidDevice *
init_device(XDeviceInfo *xdevice)
{
  GxidDevice *dev = (GxidDevice *)malloc(sizeof(GxidDevice));
  XAnyClassPtr class;
  int num_axes, i;

  dev->id = xdevice->id;
  dev->exclusive = 0;
  dev->xdevice = NULL;

  dev->ispointer = (xdevice->use == IsXPointer);

  /* step through the classes */

  num_axes = 0;
  class = xdevice->inputclassinfo;
  for (i=0;i<xdevice->num_classes;i++) 
    {
      if (class->class == ValuatorClass) 
	{
	  XValuatorInfo *xvi = (XValuatorInfo *)class;
	  num_axes = xvi->num_axes;
	}
      class = (XAnyClassPtr)(((char *)class) + class->length);
    }

  /* return NULL if insufficient axes */
  if (num_axes < 2)
    {
      free((void *)dev);
      return NULL;
    }

  if (!dev->ispointer)
      enable_device(dev);
  return dev;
}

void
init_xinput(void)
{
  char **extensions;
  XDeviceInfo   *xdevices;
  int num_xdevices;
  int num_extensions;
  int i;

  extensions = XListExtensions(dpy, &num_extensions);
  for (i = 0; i < num_extensions &&
	 (strcmp(extensions[i], "XInputExtension") != 0); i++);
  XFreeExtensionList(extensions);
  if (i == num_extensions)	/* XInput extension not found */
    {
      fprintf(stderr,"XInput extension not found\n");
      exit(1);
    }

  xdevices = XListInputDevices(dpy, &num_xdevices);
  devices = (GxidDevice **)malloc(num_xdevices * sizeof(GxidDevice *));

  num_devices = 0;
  for(i=0; i<num_xdevices; i++)
    {
      GxidDevice *dev = init_device(&xdevices[i]);
      if (dev)
	  devices[num_devices++] = dev;
    }
  XFreeDeviceList(xdevices);
}

/* If this routine needs fixing, the corresponding routine
   in gdkinputgxi.h will need it too. */

Window
gxi_find_root_child(Display *dpy, Window w)
{
  Window root,parent;
  Window *children;
  int nchildren;

  parent = w;
  do 
    {
      w = parent;
      XQueryTree(dpy,w,&root,&parent,&children,&nchildren);
      if (children) XFree(children);
    } 
  while (parent != root);
  
  return w;
}

int
handle_claim_device(GxidClaimDevice *msg)
{
  int i,j;
  XID devid;
  XID winid;
  int exclusive;
  GxidDevice *device = NULL;
  GxidWindow *window = NULL;

  if (msg->length != sizeof(GxidClaimDevice))
    {
      fprintf(stderr,"Bad length for ClaimDevice message\n");
      return GXID_RETURN_ERROR;
    }

  devid = ntohl(msg->device);
  winid = ntohl(msg->window);
  exclusive = ntohl(msg->exclusive);

#ifdef DEBUG_CLIENTS
  fprintf(stderr,"device %ld claimed (window 0x%lx)\n",devid,winid);
#endif  

  for (i=0;i<num_devices;i++)
    {
      if (devices[i]->id == devid)
	{
	  device = devices[i];
	  break;
	}
    }
  if (!device)
    {
      fprintf(stderr,"%s: Unknown device id %ld\n",program_name,devid);
      return GXID_RETURN_ERROR;
    }

  if (device->exclusive)
    {
      /* already in use */
      fprintf(stderr,
	      "%s: Device %ld already claimed in exclusive mode\n",
	      program_name,devid);
      return GXID_RETURN_ERROR;
    }

  if (exclusive)
    {
      for (i=0;i<num_windows;i++)
	{
	  for (j=0;j<windows[i]->num_devices;j++)
	    if (windows[i]->devices[j]->id == devid)
	      {
		/* already in use */
		fprintf(stderr,
			"%s: Can't establish exclusive use of device %ld\n",
			program_name,devid);
		return GXID_RETURN_ERROR;
	      }
	}
      if (device->ispointer)
	if (!switch_core_pointer())
	  {
	    fprintf(stderr,
		    "%s: Can't free up core pointer %ld\n",
		    program_name,devid);
	    return GXID_RETURN_ERROR;
	  }

      device->exclusive = 1;
      disable_device(device);
      XSelectInput(dpy,winid,StructureNotifyMask);
    }
  else				/* !exclusive */
    {
      /* FIXME: this is a bit improper. We probably should do this only
	 when a window is first claimed. But we might be fooled if
	 an old client died without releasing it's windows. So until
	 we look for client-window closings, do it here 
	 
	 (We do look for closings now...)
	 */
      
      XSelectInput(dpy,winid,EnterWindowMask|StructureNotifyMask);
    }

  for (i=0;i<num_windows;i++)
    {
      if (windows[i]->xwindow == winid)
        {
	  window = windows[i];
	  break;
	}
    }

  /* Create window structure if no devices have been previously
     claimed on it */
  if (!window)
    {
      num_windows++;
      windows = (GxidWindow **)realloc(windows,
				       sizeof(GxidWindow*)*num_windows);
      window = (GxidWindow *)malloc(sizeof(GxidWindow));
      windows[num_windows-1] = window;

      window->xwindow = winid;
      window->root_child = gxi_find_root_child(dpy,winid);
      window->num_devices = 0;
      window->devices = 0;
    }

  
  for (i=0;i<window->num_devices;i++)
    {
      if (window->devices[i] == device)
	return GXID_RETURN_OK;
    }
  
  window->num_devices++;
  window->devices = (GxidDevice **)realloc(window->devices,
					    sizeof(GxidDevice*)*num_devices);
  /* we need add the device to the window */
  window->devices[i] = device;

  return GXID_RETURN_OK;
}

int
handle_release_device(GxidReleaseDevice *msg)
{
  int i,j;
  XID devid;
  XID winid;

  GxidDevice *device = NULL;

  if (msg->length != sizeof(GxidReleaseDevice))
    {
      fprintf(stderr,"Bad length for ReleaseDevice message\n");
      return GXID_RETURN_ERROR;
    }

  devid = ntohl(msg->device);
  winid = ntohl(msg->window);

#ifdef DEBUG_CLIENTS
  fprintf(stderr,"device %ld released (window 0x%lx)\n",devid,winid);
#endif  

  for (i=0;i<num_devices;i++)
    {
      if (devices[i]->id == devid)
	{
	  device = devices[i];
	  break;
	}
    }
  if (!device)
    {
      fprintf(stderr,"%s: Unknown device id %ld\n",program_name,devid);
      return GXID_RETURN_ERROR;
    }

  for (i=0;i<num_windows;i++)
    {
      GxidWindow *w = windows[i];
      
      if (w->xwindow == winid)
	for (j=0;j<w->num_devices;j++)
	  if (w->devices[j]->id == devid)
	    {
	      if (j<w->num_devices-1)
		w->devices[j] = w->devices[w->num_devices-1];
	      w->num_devices--;

	      if (w->num_devices == 0)
		{
		  if (i<num_windows-1)
		    windows[i] = windows[num_windows-1];
		  num_windows--;

		  free((void *)w);
		  /* FIXME: should we deselect input? But what
		     what if window is already destroyed */
		}

	      if (device->exclusive)
		{
		  device->exclusive = 0;
		  enable_device(device);
		}
	      return GXID_RETURN_OK;
	    }
    }
  
  /* device/window combination not found */
  fprintf(stderr,
	  "%s: Device %ld not claimed for window 0x%lx\n",
	  program_name,devid,winid);
  return GXID_RETURN_ERROR;
}

void
handle_connection (void)
{
  GxidMessage msg;
  GxidU32 type;
  GxidU32 length;
  GxidI32 retval;

  int conn_fd;
  struct sockaddr_in sin;
  int sin_length;
  int count;

  sin_length = sizeof(struct sockaddr_in);
  conn_fd = accept(socket_fd,(struct sockaddr *)&sin,&sin_length);
  if (conn_fd < 0)
    {
      fprintf(stderr,"%s: Error accepting connection\n",
	      program_name);
      exit(1);
    }

  /* read type and length of message */

  count = read(conn_fd,(char *)&msg,2*sizeof(GxidU32));
  if (count != 2*sizeof(GxidU32))
    {
      fprintf(stderr,"%s: Error reading message header\n",
	      program_name);
      close(conn_fd);
      return;
    }
  type = ntohl(msg.any.type);
  length = ntohl(msg.any.length);

  /* read rest of message */

  if ((length > sizeof(GxidMessage)) || (length < 2*sizeof(GxidU32)))
    {
      fprintf(stderr,"%s: Bad message length\n",
	      program_name);
      close(conn_fd);
      return;
    }

  count = read(conn_fd,2*sizeof(GxidU32) + (char *)&msg,
		    length - 2*sizeof(GxidU32));
  if (count != length - 2*sizeof(GxidU32))
    {
      fprintf(stderr,"%s: Error reading message body\n",
	      program_name);
      close(conn_fd);
      return;
    }

  switch (type)
    {
    case GXID_CLAIM_DEVICE:
      retval = handle_claim_device((GxidClaimDevice *)&msg);
      break;
    case GXID_RELEASE_DEVICE:
      retval = handle_release_device((GxidReleaseDevice *)&msg);
      break;
    default:
      fprintf(stderr,"%s: Unknown message type: %ld (ignoring)\n",
	      program_name,type);
      close(conn_fd);
      return;
    }

  count = write(conn_fd,&retval,sizeof(GxidI32));
  if (count != sizeof(GxidI32))
    {
      fprintf(stderr,"%s: Error writing return code\n",
	      program_name);
    }
  
  close(conn_fd);
}

void
handle_motion_notify(XDeviceMotionEvent *event)
{
  int i,j;
  GxidDevice *old_device = NULL;
  GxidDevice *new_device = NULL;
  Window w, root, child;
  int root_x, root_y, x, y, mask;
  
  for (j=0;j<num_devices;j++)
    {
      if (devices[j]->ispointer)
	old_device = devices[j];
      if (devices[j]->id == event->deviceid)
	new_device = devices[j];
    }

  if (new_device && !new_device->exclusive && !new_device->ispointer)
    {
      /* make sure we aren't stealing the pointer back from a slow
	 client */
      child = root_window;
      do
	{
	  w = child;
	  /* FIXME: this fails disasterously if child vanishes between
	     calls. (Which is prone to happening since we get events
	     on root just as the client exits) */
	     
	  XQueryPointer(dpy,w,&root,&child,&root_x,&root_y,
			&x,&y,&mask);
	}
      while (child != None);

      for (i=0;i<num_windows;i++)
	if (windows[i]->xwindow == w)
	  for (j=0;j<windows[i]->num_devices;j++)
	    if (windows[i]->devices[j] == new_device)
		return;
      
      /* FIXME: do something smarter with axes */
      XChangePointerDevice(dpy,new_device->xdevice, 0, 1);
      new_device->ispointer = 1;
      
      old_device->ispointer = 0;
      if (!old_device->xdevice)
	enable_device(old_device);
    }
}

void
handle_change_notify(XChangeDeviceNotifyEvent *event)
{
  int j;
  GxidDevice *old_device = NULL;
  GxidDevice *new_device = NULL;


  for (j=0;j<num_devices;j++)
    {
      if (devices[j]->ispointer)
	old_device = devices[j];
      if (devices[j]->id == event->deviceid)
	new_device = devices[j];
    }

#ifdef DEBUG_EVENTS
  fprintf(stderr,"gxid: ChangeNotify event; old = %ld; new = %ld\n",
	  old_device->id, new_device->id);
#endif

  if (old_device != new_device)
    {
      new_device->ispointer = 1;
      
      old_device->ispointer = 0;
      if (!old_device->xdevice)
	enable_device(old_device);
    }
}

void
handle_enter_notify(XEnterWindowEvent *event, GxidWindow *window)
{
  int i;
  GxidDevice *old_pointer = NULL;
  for (i=0;i<num_devices;i++)
    {
      if (devices[i]->ispointer)
	{
	  old_pointer = devices[i];
	  break;
	}
    }

#ifdef DEBUG_EVENTS
  fprintf(stderr,"gxid: Enter event; oldpointer = %ld\n",
	  old_pointer->id);
#endif

  if (old_pointer)
    for (i=0;i<window->num_devices;i++)
      {
	if (window->devices[i] == old_pointer)
	  {
	    switch_core_pointer();
	    break;
	  }
      }
}

void
handle_destroy_notify(XDestroyWindowEvent *event)
{
  int i,j;

  for (i=0;i<num_windows;i++)
    if (windows[i]->xwindow == event->window)
      {
	GxidWindow *w = windows[i];
	
	for (j=0;j<w->num_devices;j++)
	  {
#ifdef DEBUG_CLIENTS
	    fprintf(stderr,"device %ld released on destruction of window 0x%lx.\n",
		    w->devices[j]->id,w->xwindow);
#endif  

	    if (w->devices[j]->exclusive)
	      {
		w->devices[j]->exclusive = 0;
		enable_device(devices[j]);
	      }
	  }
	
	if (i<num_windows-1)
	  windows[i] = windows[num_windows-1];
	num_windows--;
	
	if (w->devices)
	  free((void *)w->devices);
	free((void *)w);
	/* FIXME: should we deselect input? But what
	   what if window is already destroyed */
	
	return;
      }
}

void
handle_xevent(void)
{
  int i;
  XEvent event;
	
  XNextEvent (dpy, &event);

#ifdef DEBUG_EVENTS
  fprintf(stderr,"Event - type = %d; window = 0x%lx\n",
	  event.type,event.xany.window);
#endif

  if (event.type == ConfigureNotify)
    {
#ifdef DEBUG_EVENTS
      XConfigureEvent *xce = (XConfigureEvent *)&event;
      fprintf(stderr," configureNotify: window = 0x%lx\n",xce->window);
#endif
    }
  else if (event.type == EnterNotify)
    {
      /* pointer entered a claimed window */
      for (i=0;i<num_windows;i++)
	{
	  if (event.xany.window == windows[i]->xwindow)
	    handle_enter_notify((XEnterWindowEvent *)&event,windows[i]);
	}
    }
  else if (event.type == DestroyNotify)
    {
      /* A claimed window was destroyed */
      for (i=0;i<num_windows;i++)
	{
	  if (event.xany.window == windows[i]->xwindow)
	    handle_destroy_notify((XDestroyWindowEvent *)&event);
	}
    }
  else
    for (i=0;i<num_devices;i++)
      {
	if (event.type == devices[i]->motionnotify_type)
	  {
	    handle_motion_notify((XDeviceMotionEvent *)&event);
	    break;
	  }
	else if (event.type == devices[i]->changenotify_type)
	  {
	    handle_change_notify((XChangeDeviceNotifyEvent *)&event);
	    break;
	  }
      }
}

void 
usage(void)
{
  fprintf(stderr,"Usage: %s [-d display] [-p --gxid-port port]\n",
	  program_name);
  exit(1);
}

int
main(int argc, char **argv)
{
  int i;
  char *display_name = NULL;
  fd_set readfds;

  program_name = argv[0];

  for (i=1;i<argc;i++)
    {
      if (!strcmp(argv[i],"-d"))
	{
	    if (++i >= argc) usage();
	    display_name = argv[i];
	}
      else if (!strcmp(argv[i],"--gxid-port") ||
	       !strcmp(argv[i],"-p"))
	{
	  if (++i >= argc) usage();
	  port = atoi(argv[i]);
	  break;
	}
      else
	usage();
    }

  if (!port) 
    {
      char *t = getenv("GXID_PORT");
      if (t)
	port = atoi(t);
      else
	port = 6951;
    }
  /* set up a signal handler so we can clean up if killed */

  signal(SIGTERM,handler);
  signal(SIGINT,handler);
  
  /* initialize the X connection */
  
  dpy = XOpenDisplay (display_name);
  if (!dpy)
    {
      fprintf (stderr, "%s:  unable to open display '%s'\n",
	       program_name, XDisplayName (display_name));
      exit (1);
    }
  
  root_window = DefaultRootWindow(dpy);

  /* We'll want to do this in the future if we are to support
     gxid monitoring visibility information for clients */
#if 0
  XSelectInput(dpy,root_window,SubstructureNotifyMask);
#endif
  init_xinput();
  
  /* set up our server connection */
  
  init_socket();
  
  /* main loop */

  if (XPending(dpy))		/* this seems necessary to get things
				   in sync */
    handle_xevent();
  while (1) 
    {

      FD_ZERO(&readfds);
      FD_SET(ConnectionNumber(dpy),&readfds);
      FD_SET(socket_fd,&readfds);

      if (select(8*sizeof(readfds),&readfds,
		 (fd_set *)0,(fd_set *)0, (struct timeval *)0) < 0)
	{
	  fprintf(stderr,"Error in select\n");
	  exit(1);
	}

      if (FD_ISSET(socket_fd,&readfds))
	handle_connection();
	
      while (XPending(dpy))
	handle_xevent();
    }

  XCloseDisplay (dpy);
  exit (0);
}
