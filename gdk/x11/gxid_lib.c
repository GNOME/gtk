/* 
 * gxid version 0.3
 *
 * Copyright 1997 Owen Taylor <owt1@cornell.edu>
*/

#include "../config.h"
#include "gxid_lib.h"

#ifdef XINPUT_GXI

#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

/* handles mechanics of communicating with a client */
static int 
gxid_send_message(char *host, int port, GxidMessage *msg)
{
  int socket_fd;
  struct sockaddr_in sin;
  int count;
  GxidI32 retval;
  struct hostent *he;

  if (!port) port = 6951;

  if (!host || strcmp(host,"localhost") )
    {
      /* looking it up as localhost can be _SLOW_ on ppp systems */
      /* FIXME: Could localhost be anything other than loopback? */
      host = "127.0.0.1";
    }

  he = gethostbyname(host);
  if (!he)
    {
      fprintf(stderr,"gxid_lib: error looking up %s\n",host);
      return GXID_RETURN_ERROR;
    }

  sin.sin_family = he->h_addrtype;
  sin.sin_port = htons(port);
  memcpy(&sin.sin_addr,he->h_addr_list[0],he->h_length);

  socket_fd = socket(AF_INET,SOCK_STREAM,0);
  if (socket_fd < 0)
    {
      fprintf(stderr,"gxid_lib: can't get socket");
      return GXID_RETURN_ERROR;
    }

  if (connect(socket_fd, (struct sockaddr *)&sin, 
	      sizeof sin) < 0)
    {
      fprintf(stderr,"gxid_lib: can't connect to %s:%d\n",host,port);
      close(socket_fd);
      return GXID_RETURN_ERROR;
    }

  count = write(socket_fd,(char *)msg,ntohl(msg->any.length));
  if (count != ntohl(msg->any.length))
    {
      fprintf(stderr,"gxid_lib: error writing");
      close(socket_fd);
      return GXID_RETURN_ERROR;
    }

  /* now read the return code */
  count = read(socket_fd,(char *)&retval,sizeof(GxidI32));
  if (count != sizeof(GxidI32))
    {
      fprintf(stderr,"gxid_lib: error reading return code");
      close(socket_fd);
      return GXID_RETURN_ERROR;
    }

  close (socket_fd);
  return ntohl(retval);
}

/* claim a device. If exclusive, device is claimed exclusively */
int 
gxid_claim_device(char *host, int port, GxidU32 device, GxidU32 window,
		  int exclusive)
{
  GxidClaimDevice msg;
  msg.type = htonl(GXID_CLAIM_DEVICE);
  msg.length = htonl(sizeof(GxidClaimDevice));
  msg.device = htonl(device);
  msg.window = htonl(window);
  msg.exclusive = htonl(exclusive);

  return gxid_send_message(host,port,(GxidMessage *)&msg);
}

/* release a device/window pair */
int 
gxid_release_device(char *host, int port, GxidU32 device, GxidU32 window)
{
  GxidReleaseDevice msg;
  msg.type = htonl(GXID_RELEASE_DEVICE);
  msg.length = htonl(sizeof(GxidReleaseDevice));
  msg.device = htonl(device);
  msg.window = htonl(window);

  return gxid_send_message(host,port,(GxidMessage *)&msg);
}

#else /* !XINPUT_GXI */

/* Some compilers don't like empty source files */
int 
gxid_claim_device(char *host, int port, GxidU32 device, GxidU32 window,
		  int exclusive)
{
  return 0;
}

#endif /* XINPUT_GXI */

