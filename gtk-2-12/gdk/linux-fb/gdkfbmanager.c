#include <config.h>
#include <glib.h>
#include <glib/gprintf.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#include "gdkfbmanager.h"

typedef struct {
  int socket;
  int pid; /* -1 if not initialized */
} Client;

GHashTable *clients = NULL;
GHashTable *new_clients = NULL;
Client *current_owner = NULL;

int master_socket;

int create_master_socket (void)
{
  int fd;
  struct sockaddr_un addr;

  fd = socket (PF_UNIX, SOCK_STREAM, 0);

  if (fd < 0) 
    {
      g_fprintf (stderr, "Error creating socket: %s\n", strerror(errno));
      return -1;
    }

  unlink ("/tmp/.fb.manager");

  addr.sun_family = AF_UNIX;
  strcpy (addr.sun_path, "/tmp/.fb.manager");

  if (bind(fd, (struct sockaddr *)&addr, sizeof (addr)) < 0)
    {
      g_fprintf (stderr, "Unable to bind socket: %s\n", strerror (errno));
      close (fd);
      return -1;
    }
  

  if (listen (fd, 10) < 0)
    {
      g_fprintf (stderr, "Unable to listen on socket: %s\n", strerror (errno));
      close (fd);
      return -1;
    }

  master_socket = fd;
  return 0;
}

void
handle_new_client (void)
{
  int fd;
  Client *client;
  int true_val;

  fd = accept (master_socket, NULL, NULL);
  
  client = g_new (Client, 1);
  client->socket = fd;
  client->pid = -1;

  true_val = 1;
  setsockopt (fd, SOL_SOCKET, SO_PASSCRED, 
	      &true_val, sizeof (true_val));

  g_print ("Handling new client %p conntecting, fd = %d\n", client, fd);

  g_hash_table_insert (new_clients, client, client);
}

struct fd_data
{
  fd_set *read_fds;
  fd_set *exception_fds;
  int max_fd;
};

void 
send_message (Client *client, enum FBManagerMessageType type, int data)
{
  struct FBManagerMessage msg;

  msg.msg_type = type;
  msg.data = data;

  send (client->socket, &msg, sizeof (msg), 0);
}

gboolean
wait_for_ack (Client *client, int timeout_secs)
{
  struct FBManagerMessage msg;
  int res;
  fd_set rfds;
  struct timeval tv;

  while (1)
    {
      FD_ZERO(&rfds);
      FD_SET(client->socket, &rfds);
      
      tv.tv_sec = timeout_secs;
      tv.tv_usec = 0;
      
      res = select (client->socket+1, &rfds, NULL, NULL, &tv);
      
      if (res == 0)
	return FALSE;
      
      res = recv (client->socket, &msg, sizeof (msg), 0);
      if (res != sizeof (msg))
	return FALSE;
      
      if (msg.msg_type == FB_MANAGER_ACK)
	return TRUE;
    }
}

void
find_another_client (gpointer key,
		     gpointer value,
		     gpointer user_data)
{
  Client **res;
  Client *client;

  res = user_data;
  
  if (*res)
    return;

  client = value;
  if (client != current_owner)
    *res = client;
}

void
switch_to_client (Client *client)
{
  g_print ("Switch_to_client, client=%p, current_owner=%p\n", client, current_owner);

  if ((current_owner == client) && (client != NULL))
    return;

  if (current_owner)
    {
      g_print ("switching from client fd=%d\n", current_owner->socket);
      send_message (current_owner, FB_MANAGER_SWITCH_FROM, 0);
      wait_for_ack (current_owner, 3);
    }

  current_owner = client;

  if (current_owner)
    {
      g_print ("switching to client fd=%d\n", current_owner->socket);
      send_message (current_owner, FB_MANAGER_SWITCH_TO, 0);
    }
}

void
close_client (Client *client)
{
  Client *other_client;
  g_print ("Closing client %p (fd=%d)\n", 
	   client, client->socket);

  if (current_owner == client)
    {
      other_client = NULL;
      g_hash_table_foreach (clients,
			    find_another_client,
			    &other_client);
      current_owner = NULL;
      /* FIXME: This is a hack around the fact that the serial 
	 mouse driver had problems with opening and closing
	 the device almost at the same time. 
      */
      sleep (1);
      switch_to_client (other_client);
    }
   
  close (client->socket);
  g_free (client);
}


/* Returns TRUE if the client was closed */
gboolean 
read_client_data (Client *client)
{
  struct FBManagerMessage fb_message;
  struct msghdr msg;
  struct iovec iov;
  char control_buffer[256];
  struct cmsghdr *cmsg;
  int res;
  struct ucred *creds;
  Client *new_client;

  iov.iov_base = &fb_message;
  iov.iov_len = sizeof (fb_message);

  cmsg = (struct cmsghdr *)control_buffer;
  msg.msg_name = NULL;
  msg.msg_namelen = 0;
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  msg.msg_control = cmsg;
  msg.msg_controllen = 256;
  msg.msg_flags = 0;
  
  g_print ("Reading client data:");
  res = recvmsg (client->socket, &msg, 0);
  g_print ("%d bytes, (error: %s)\n", res, 
	   strerror (errno));
  
  if (res < 0)
    return FALSE;

  if (res == 0) 
    {
      close_client (client);
      return TRUE;
    }

  if (res != sizeof (fb_message))
    {
      g_warning ("Packet with wrong size %d received", res);
      return FALSE;
    }

  switch (fb_message.msg_type) {
  case FB_MANAGER_NEW_CLIENT:
    if (client->pid != -1)
      {
	g_warning ("Got a NEW_CLIENT message from an old client");
	return FALSE;
      }
    creds = NULL;
    for (cmsg = CMSG_FIRSTHDR(&msg);
	 cmsg != NULL;
	 cmsg = CMSG_NXTHDR(&msg,cmsg))
      {
	if (cmsg->cmsg_level == SOL_SOCKET && 
	    cmsg->cmsg_type ==  SCM_CREDENTIALS) 
	  {
	    creds = (struct ucred *) CMSG_DATA(cmsg);
	    break;
	  }
      }
    if (creds == NULL) 
      {
	g_warning ("Got no credentials in NEW_CLIENT message");
	close_client (client);
	return TRUE;
      }
    client->pid = creds->pid;

    g_hash_table_insert (clients, GINT_TO_POINTER (client->pid), client);

    g_print ("New client connected. Pid=%d\n", (int)creds->pid);
    return TRUE;
    break;
  case FB_MANAGER_REQUEST_SWITCH_TO_PID:
    if (client->pid == -1)
      {
	g_warning ("Got a message from an uninitialized client");
	return FALSE;
      }

    new_client = g_hash_table_lookup (clients, GINT_TO_POINTER (fb_message.data));
    if (new_client)
      switch_to_client (new_client);
    else
      g_warning ("Switchto unknown PID");
    break;
  case FB_MANAGER_ACK:
    if (client->pid == -1)
      {
	g_warning ("Got a message from an uninitialized client");
	return FALSE;
      }
    g_warning ("Got an unexpected ACK");
    break;
  default:
    g_warning ("Got unknown package type %d", fb_message.msg_type);
    break;
  }
  return FALSE;
}

/* Returns TRUE if the client was closed */
gboolean
handle_client_data (gpointer key,
		    gpointer value,
		    gpointer user_data)
{
  Client *client;
  struct fd_data *data;

  client = value;
  data = user_data;

  if (FD_ISSET (client->socket, data->exception_fds))
    {
      close_client (client);
      return TRUE;
    }
  else if (FD_ISSET (client->socket, data->read_fds))
    {
      return read_client_data (client);
    }
  
  return FALSE;
}

void
set_fds (gpointer key,
	 gpointer value,
	 gpointer user_data)
{
  struct fd_data *data;
  Client *client;

  client = value;
  data = user_data;

  FD_SET (client->socket, data->read_fds);
  FD_SET (client->socket, data->exception_fds);
  data->max_fd = MAX (data->max_fd, 
		      client->socket);
}

void
main_loop (void)
{
  fd_set read_fds;
  fd_set exception_fds;
  struct fd_data data;
  int res;
  
  while (1)
    {
      FD_ZERO (&read_fds);
      FD_ZERO (&exception_fds);
      FD_SET (master_socket, &read_fds);

      data.read_fds = &read_fds;
      data.exception_fds = &exception_fds;
      data.max_fd = master_socket;
      
      g_hash_table_foreach (clients,
			    set_fds,
			    &data);
      g_hash_table_foreach (new_clients,
			    set_fds,
			    &data);
      

      res = select (data.max_fd+1, 
		    &read_fds, NULL, &exception_fds, 
		    NULL);

      if (FD_ISSET (master_socket, &read_fds)) 
	handle_new_client ();

      g_hash_table_foreach_remove (clients,
				   handle_client_data,
				   &data);
      g_hash_table_foreach_remove (new_clients,
				   handle_client_data,
				   &data);
    }
}


int
main (int argc, char *argv[])
{
  clients = g_hash_table_new (g_direct_hash,
			      g_direct_equal);
  new_clients = g_hash_table_new (g_direct_hash,
				  g_direct_equal);

  create_master_socket ();

  main_loop ();

  return 0;
}
