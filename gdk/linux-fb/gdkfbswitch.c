#include <config.h>
#include <glib.h>

#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "gdkfbmanager.h"

int
main (int argc, char *argv[])
{
  int fd;
  struct sockaddr_un addr;
  struct msghdr msg = {0};
  struct cmsghdr *cmsg;
  struct ucred credentials;
  struct FBManagerMessage init_msg;
  struct iovec iov;
  char buf[CMSG_SPACE (sizeof (credentials))];  /* ancillary data buffer */
  int *fdptr;
  int res;

  if (argc != 2)
    {
      g_print ("usage: fbswitch <pid>\n");
      return 1;
    }

  fd = socket (PF_UNIX, SOCK_STREAM, 0);
  
  if (fd < 0)
    return 1;

  addr.sun_family = AF_UNIX;
  strcpy (addr.sun_path, "/tmp/.fb.manager");

  if (connect(fd, (struct sockaddr *)&addr, sizeof (addr)) < 0) 
    {
      g_print ("connect failed\n");
      close (fd);
      return 1;
    }
  
  credentials.pid = getpid ();
  credentials.uid = geteuid ();
  credentials.gid = getegid ();

  init_msg.msg_type = FB_MANAGER_NEW_CLIENT;
  iov.iov_base = &init_msg;
  iov.iov_len = sizeof (init_msg);

  msg.msg_name = NULL;
  msg.msg_namelen = 0;
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  msg.msg_control = buf;
  msg.msg_controllen = sizeof buf;
  cmsg = CMSG_FIRSTHDR(&msg);
  cmsg->cmsg_level = SOL_SOCKET;
  cmsg->cmsg_type = SCM_CREDENTIALS;
  cmsg->cmsg_len = CMSG_LEN (sizeof (credentials));
  /* Initialize the payload: */
  fdptr = (int *)CMSG_DATA (cmsg);
  memcpy (fdptr, &credentials, sizeof (credentials));
  /* Sum of the length of all control messages in the buffer: */
  msg.msg_controllen = cmsg->cmsg_len;

  res = sendmsg (fd, &msg, 0);

  init_msg.msg_type = FB_MANAGER_REQUEST_SWITCH_TO_PID;
  init_msg.data = atoi (argv[1]);
  /* Request a switch-to */
  send (fd, &init_msg, sizeof (init_msg), 0);
  g_print ("requested a switch to pid %d\n", init_msg.data);

  return 0;
}
