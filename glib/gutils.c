/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include "glib.h"


static GErrorFunc error_func = NULL;
static GWarningFunc warning_func = NULL;
static GPrintFunc message_func = NULL;
static GPrintFunc print_func = NULL;

extern char* g_vsprintf (gchar *fmt, va_list *args, va_list *args2);

gchar*
g_strdup (const gchar *str)
{
  gchar *new_str;
  
  new_str = NULL;
  if (str)
    {
      new_str = g_new (char, strlen (str) + 1);
      strcpy (new_str, str);
    }
  
  return new_str;
}

gchar*
g_strconcat (const gchar *string1, ...)
{
  guint	  l;
  va_list args;
  gchar	  *s;
  gchar	  *concat;
  
  g_return_val_if_fail (string1 != NULL, NULL);
  
  l = 1 + strlen (string1);
  va_start (args, string1);
  s = va_arg (args, gchar*);
  while (s)
    {
      l += strlen (s);
      s = va_arg (args, gchar*);
    }
  va_end (args);
  
  concat = g_new (gchar, l);
  concat[0] = 0;
  
  strcat (concat, string1);
  va_start (args, string1);
  s = va_arg (args, gchar*);
  while (s)
    {
      strcat (concat, s);
      s = va_arg (args, gchar*);
    }
  va_end (args);
  
  return concat;
}

gdouble
g_strtod (const gchar *nptr,
	  gchar **endptr)
{
  gchar *fail_pos_1;
  gchar *fail_pos_2;
  gdouble val_1;
  gdouble val_2 = 0;

  g_return_val_if_fail (nptr != NULL, 0);

  fail_pos_1 = NULL;
  fail_pos_2 = NULL;

  val_1 = strtod (nptr, &fail_pos_1);

  if (fail_pos_1 && fail_pos_1[0] != 0)
    {
      gchar *old_locale;

      old_locale = setlocale (LC_NUMERIC, "C");
      val_2 = strtod (nptr, &fail_pos_2);
      setlocale (LC_NUMERIC, old_locale);
    }

  if (!fail_pos_1 || fail_pos_1[0] == 0 || fail_pos_1 >= fail_pos_2)
    {
      if (endptr)
	*endptr = fail_pos_1;
      return val_1;
    }
  else
    {
      if (endptr)
	*endptr = fail_pos_2;
      return val_2;
    }
}

gchar*
g_strerror (gint errnum)
{
  static char msg[64];
  
#ifdef HAVE_STRERROR
  return strerror (errnum);
#elif NO_SYS_ERRLIST
  switch (errnum)
    {
#ifdef E2BIG
    case E2BIG: return "argument list too long";
#endif
#ifdef EACCES
    case EACCES: return "permission denied";
#endif
#ifdef EADDRINUSE
    case EADDRINUSE: return "address already in use";
#endif
#ifdef EADDRNOTAVAIL
    case EADDRNOTAVAIL: return "can't assign requested address";
#endif
#ifdef EADV
    case EADV: return "advertise error";
#endif
#ifdef EAFNOSUPPORT
    case EAFNOSUPPORT: return "address family not supported by protocol family";
#endif
#ifdef EAGAIN
    case EAGAIN: return "try again";
#endif
#ifdef EALIGN
    case EALIGN: return "EALIGN";
#endif
#ifdef EALREADY
    case EALREADY: return "operation already in progress";
#endif
#ifdef EBADE
    case EBADE: return "bad exchange descriptor";
#endif
#ifdef EBADF
    case EBADF: return "bad file number";
#endif
#ifdef EBADFD
    case EBADFD: return "file descriptor in bad state";
#endif
#ifdef EBADMSG
    case EBADMSG: return "not a data message";
#endif
#ifdef EBADR
    case EBADR: return "bad request descriptor";
#endif
#ifdef EBADRPC
    case EBADRPC: return "RPC structure is bad";
#endif
#ifdef EBADRQC
    case EBADRQC: return "bad request code";
#endif
#ifdef EBADSLT
    case EBADSLT: return "invalid slot";
#endif
#ifdef EBFONT
    case EBFONT: return "bad font file format";
#endif
#ifdef EBUSY
    case EBUSY: return "mount device busy";
#endif
#ifdef ECHILD
    case ECHILD: return "no children";
#endif
#ifdef ECHRNG
    case ECHRNG: return "channel number out of range";
#endif
#ifdef ECOMM
    case ECOMM: return "communication error on send";
#endif
#ifdef ECONNABORTED
    case ECONNABORTED: return "software caused connection abort";
#endif
#ifdef ECONNREFUSED
    case ECONNREFUSED: return "connection refused";
#endif
#ifdef ECONNRESET
    case ECONNRESET: return "connection reset by peer";
#endif
#if defined(EDEADLK) && (!defined(EWOULDBLOCK) || (EDEADLK != EWOULDBLOCK))
    case EDEADLK: return "resource deadlock avoided";
#endif
#ifdef EDEADLOCK
    case EDEADLOCK: return "resource deadlock avoided";
#endif
#ifdef EDESTADDRREQ
    case EDESTADDRREQ: return "destination address required";
#endif
#ifdef EDIRTY
    case EDIRTY: return "mounting a dirty fs w/o force";
#endif
#ifdef EDOM
    case EDOM: return "math argument out of range";
#endif
#ifdef EDOTDOT
    case EDOTDOT: return "cross mount point";
#endif
#ifdef EDQUOT
    case EDQUOT: return "disk quota exceeded";
#endif
#ifdef EDUPPKG
    case EDUPPKG: return "duplicate package name";
#endif
#ifdef EEXIST
    case EEXIST: return "file already exists";
#endif
#ifdef EFAULT
    case EFAULT: return "bad address in system call argument";
#endif
#ifdef EFBIG
    case EFBIG: return "file too large";
#endif
#ifdef EHOSTDOWN
    case EHOSTDOWN: return "host is down";
#endif
#ifdef EHOSTUNREACH
    case EHOSTUNREACH: return "host is unreachable";
#endif
#ifdef EIDRM
    case EIDRM: return "identifier removed";
#endif
#ifdef EINIT
    case EINIT: return "initialization error";
#endif
#ifdef EINPROGRESS
    case EINPROGRESS: return "operation now in progress";
#endif
#ifdef EINTR
    case EINTR: return "interrupted system call";
#endif
#ifdef EINVAL
    case EINVAL: return "invalid argument";
#endif
#ifdef EIO
    case EIO: return "I/O error";
#endif
#ifdef EISCONN
    case EISCONN: return "socket is already connected";
#endif
#ifdef EISDIR
    case EISDIR: return "illegal operation on a directory";
#endif
#ifdef EISNAME
    case EISNAM: return "is a name file";
#endif
#ifdef ELBIN
    case ELBIN: return "ELBIN";
#endif
#ifdef EL2HLT
    case EL2HLT: return "level 2 halted";
#endif
#ifdef EL2NSYNC
    case EL2NSYNC: return "level 2 not synchronized";
#endif
#ifdef EL3HLT
    case EL3HLT: return "level 3 halted";
#endif
#ifdef EL3RST
    case EL3RST: return "level 3 reset";
#endif
#ifdef ELIBACC
    case ELIBACC: return "can not access a needed shared library";
#endif
#ifdef ELIBBAD
    case ELIBBAD: return "accessing a corrupted shared library";
#endif
#ifdef ELIBEXEC
    case ELIBEXEC: return "can not exec a shared library directly";
#endif
#ifdef ELIBMAX
    case ELIBMAX: return "attempting to link in more shared libraries than system limit";
#endif
#ifdef ELIBSCN
    case ELIBSCN: return ".lib section in a.out corrupted";
#endif
#ifdef ELNRNG
    case ELNRNG: return "link number out of range";
#endif
#ifdef ELOOP
    case ELOOP: return "too many levels of symbolic links";
#endif
#ifdef EMFILE
    case EMFILE: return "too many open files";
#endif
#ifdef EMLINK
    case EMLINK: return "too many links";
#endif
#ifdef EMSGSIZE
    case EMSGSIZE: return "message too long";
#endif
#ifdef EMULTIHOP
    case EMULTIHOP: return "multihop attempted";
#endif
#ifdef ENAMETOOLONG
    case ENAMETOOLONG: return "file name too long";
#endif
#ifdef ENAVAIL
    case ENAVAIL: return "not available";
#endif
#ifdef ENET
    case ENET: return "ENET";
#endif
#ifdef ENETDOWN
    case ENETDOWN: return "network is down";
#endif
#ifdef ENETRESET
    case ENETRESET: return "network dropped connection on reset";
#endif
#ifdef ENETUNREACH
    case ENETUNREACH: return "network is unreachable";
#endif
#ifdef ENFILE
    case ENFILE: return "file table overflow";
#endif
#ifdef ENOANO
    case ENOANO: return "anode table overflow";
#endif
#if defined(ENOBUFS) && (!defined(ENOSR) || (ENOBUFS != ENOSR))
    case ENOBUFS: return "no buffer space available";
#endif
#ifdef ENOCSI
    case ENOCSI: return "no CSI structure available";
#endif
#ifdef ENODATA
    case ENODATA: return "no data available";
#endif
#ifdef ENODEV
    case ENODEV: return "no such device";
#endif
#ifdef ENOENT
    case ENOENT: return "no such file or directory";
#endif
#ifdef ENOEXEC
    case ENOEXEC: return "exec format error";
#endif
#ifdef ENOLCK
    case ENOLCK: return "no locks available";
#endif
#ifdef ENOLINK
    case ENOLINK: return "link has be severed";
#endif
#ifdef ENOMEM
    case ENOMEM: return "not enough memory";
#endif
#ifdef ENOMSG
    case ENOMSG: return "no message of desired type";
#endif
#ifdef ENONET
    case ENONET: return "machine is not on the network";
#endif
#ifdef ENOPKG
    case ENOPKG: return "package not installed";
#endif
#ifdef ENOPROTOOPT
    case ENOPROTOOPT: return "bad proocol option";
#endif
#ifdef ENOSPC
    case ENOSPC: return "no space left on device";
#endif
#ifdef ENOSR
    case ENOSR: return "out of stream resources";
#endif
#ifdef ENOSTR
    case ENOSTR: return "not a stream device";
#endif
#ifdef ENOSYM
    case ENOSYM: return "unresolved symbol name";
#endif
#ifdef ENOSYS
    case ENOSYS: return "function not implemented";
#endif
#ifdef ENOTBLK
    case ENOTBLK: return "block device required";
#endif
#ifdef ENOTCONN
    case ENOTCONN: return "socket is not connected";
#endif
#ifdef ENOTDIR
    case ENOTDIR: return "not a directory";
#endif
#ifdef ENOTEMPTY
    case ENOTEMPTY: return "directory not empty";
#endif
#ifdef ENOTNAM
    case ENOTNAM: return "not a name file";
#endif
#ifdef ENOTSOCK
    case ENOTSOCK: return "socket operation on non-socket";
#endif
#ifdef ENOTTY
    case ENOTTY: return "inappropriate device for ioctl";
#endif
#ifdef ENOTUNIQ
    case ENOTUNIQ: return "name not unique on network";
#endif
#ifdef ENXIO
    case ENXIO: return "no such device or address";
#endif
#ifdef EOPNOTSUPP
    case EOPNOTSUPP: return "operation not supported on socket";
#endif
#ifdef EPERM
    case EPERM: return "not owner";
#endif
#ifdef EPFNOSUPPORT
    case EPFNOSUPPORT: return "protocol family not supported";
#endif
#ifdef EPIPE
    case EPIPE: return "broken pipe";
#endif
#ifdef EPROCLIM
    case EPROCLIM: return "too many processes";
#endif
#ifdef EPROCUNAVAIL
    case EPROCUNAVAIL: return "bad procedure for program";
#endif
#ifdef EPROGMISMATCH
    case EPROGMISMATCH: return "program version wrong";
#endif
#ifdef EPROGUNAVAIL
    case EPROGUNAVAIL: return "RPC program not available";
#endif
#ifdef EPROTO
    case EPROTO: return "protocol error";
#endif
#ifdef EPROTONOSUPPORT
    case EPROTONOSUPPORT: return "protocol not suppored";
#endif
#ifdef EPROTOTYPE
    case EPROTOTYPE: return "protocol wrong type for socket";
#endif
#ifdef ERANGE
    case ERANGE: return "math result unrepresentable";
#endif
#if defined(EREFUSED) && (!defined(ECONNREFUSED) || (EREFUSED != ECONNREFUSED))
    case EREFUSED: return "EREFUSED";
#endif
#ifdef EREMCHG
    case EREMCHG: return "remote address changed";
#endif
#ifdef EREMDEV
    case EREMDEV: return "remote device";
#endif
#ifdef EREMOTE
    case EREMOTE: return "pathname hit remote file system";
#endif
#ifdef EREMOTEIO
    case EREMOTEIO: return "remote i/o error";
#endif
#ifdef EREMOTERELEASE
    case EREMOTERELEASE: return "EREMOTERELEASE";
#endif
#ifdef EROFS
    case EROFS: return "read-only file system";
#endif
#ifdef ERPCMISMATCH
    case ERPCMISMATCH: return "RPC version is wrong";
#endif
#ifdef ERREMOTE
    case ERREMOTE: return "object is remote";
#endif
#ifdef ESHUTDOWN
    case ESHUTDOWN: return "can't send afer socket shutdown";
#endif
#ifdef ESOCKTNOSUPPORT
    case ESOCKTNOSUPPORT: return "socket type not supported";
#endif
#ifdef ESPIPE
    case ESPIPE: return "invalid seek";
#endif
#ifdef ESRCH
    case ESRCH: return "no such process";
#endif
#ifdef ESRMNT
    case ESRMNT: return "srmount error";
#endif
#ifdef ESTALE
    case ESTALE: return "stale remote file handle";
#endif
#ifdef ESUCCESS
    case ESUCCESS: return "Error 0";
#endif
#ifdef ETIME
    case ETIME: return "timer expired";
#endif
#ifdef ETIMEDOUT
    case ETIMEDOUT: return "connection timed out";
#endif
#ifdef ETOOMANYREFS
    case ETOOMANYREFS: return "too many references: can't splice";
#endif
#ifdef ETXTBSY
    case ETXTBSY: return "text file or pseudo-device busy";
#endif
#ifdef EUCLEAN
    case EUCLEAN: return "structure needs cleaning";
#endif
#ifdef EUNATCH
    case EUNATCH: return "protocol driver not attached";
#endif
#ifdef EUSERS
    case EUSERS: return "too many users";
#endif
#ifdef EVERSION
    case EVERSION: return "version mismatch";
#endif
#if defined(EWOULDBLOCK) && (!defined(EAGAIN) || (EWOULDBLOCK != EAGAIN))
    case EWOULDBLOCK: return "operation would block";
#endif
#ifdef EXDEV
    case EXDEV: return "cross-domain link";
#endif
#ifdef EXFULL
    case EXFULL: return "message tables full";
#endif
    }
#else /* NO_SYS_ERRLIST */
  extern int sys_nerr;
  extern char *sys_errlist[];

  if ((errnum > 0) && (errnum <= sys_nerr))
    return sys_errlist [errnum];
#endif /* NO_SYS_ERRLIST */

  sprintf (msg, "unknown error (%d)", errnum);
  return msg;
}

gchar*
g_strsignal (gint signum)
{
  static char msg[64];

#ifdef HAVE_STRSIGNAL
  extern char *strsignal (int sig);
  return strsignal (signum);
#elif NO_SYS_SIGLIST
  switch (signum)
    {
#ifdef SIGHUP
    case SIGHUP: return "Hangup";
#endif
#ifdef SIGINT
    case SIGINT: return "Interrupt";
#endif
#ifdef SIGQUIT
    case SIGQUIT: return "Quit";
#endif
#ifdef SIGILL
    case SIGILL: "Illegal instruction";
#endif
#ifdef SIGTRAP
    case SIGTRAP: "Trace/breakpoint trap";
#endif
#ifdef SIGABRT
    case SIGABRT: "IOT trap/Abort";
#endif
#ifdef SIGBUS
    case SIGBUS: "Bus error";
#endif
#ifdef SIGFPE
    case SIGFPE: "Floating point exception";
#endif
#ifdef SIGKILL
    case SIGKILL: "Killed";
#endif
#ifdef SIGUSR1
    case SIGUSR1: "User defined signal 1";
#endif
#ifdef SIGSEGV
    case SIGSEGV: "Segmentation fault";
#endif
#ifdef SIGUSR2
    case SIGUSR2: "User defined signal 2";
#endif
#ifdef SIGPIPE
    case SIGPIPE: "Broken pipe";
#endif
#ifdef SIGALRM
    case SIGALRM: "Alarm clock";
#endif
#ifdef SIGTERM
    case SIGTERM: "Terminated";
#endif
#ifdef SIGSTKFLT
    case SIGSTKFLT: "Stack fault";
#endif
#ifdef SIGCHLD
    case SIGCHLD: "Child exited";
#endif
#ifdef SIGCONT
    case SIGCONT: "Continued";
#endif
#ifdef SIGSTOP
    case SIGSTOP: "Stopped (signal)";
#endif
#ifdef SIGTSTP
    case SIGTSTP: "Stopped";
#endif
#ifdef SIGTTIN
    case SIGTTIN: "Stopped (tty input)";
#endif
#ifdef SIGTTOU
    case SIGTTOU: "Stopped (tty output)";
#endif
#ifdef SIGURG
    case SIGURG: "Urgent condition";
#endif
#ifdef SIGXCPU
    case SIGXCPU: "CPU time limit exceeded";
#endif
#ifdef SIGXFSZ
    case SIGXFSZ: "File size limit exceeded";
#endif
#ifdef SIGVTALRM
    case SIGVTALRM: "Virtual time alarm";
#endif
#ifdef SIGPROF
    case SIGPROF: "Profile signal";
#endif
#ifdef SIGWINCH
    case SIGWINCH: "Window size changed";
#endif
#ifdef SIGIO
    case SIGIO: "Possible I/O";
#endif
#ifdef SIGPWR
    case SIGPWR: "Power failure";
#endif
#ifdef SIGUNUSED
    case SIGUNUSED: return "Unused signal";
#endif
    }
#else /* NO_SYS_SIGLIST */
  extern char *sys_siglist[];
  return sys_siglist [signum];
#endif /* NO_SYS_SIGLIST */

  sprintf (msg, "unknown signal (%d)", signum);
  return msg;
}

void
g_error (gchar *format, ...)
{
  va_list args, args2;
  char *buf;

  va_start (args, format);
  va_start (args2, format);
  buf = g_vsprintf (format, &args, &args2);
  va_end (args);
  va_end (args2);

  if (error_func)
    {
      (* error_func) (buf);
    }
  else
    {
      fputs ("\n** ERROR **: ", stderr);
      fputs (buf, stderr);
      fputc ('\n', stderr);
    }

  abort ();
}

void
g_warning (gchar *format, ...)
{
  va_list args, args2;
  char *buf;

  va_start (args, format);
  va_start (args2, format);
  buf = g_vsprintf (format, &args, &args2);
  va_end (args);
  va_end (args2);

  if (warning_func)
    {
      (* warning_func) (buf);
    }
  else
    {
      fputs ("\n** WARNING **: ", stderr);
      fputs (buf, stderr);
      fputc ('\n', stderr);
    }
}

void
g_message (gchar *format, ...)
{
  va_list args, args2;
  char *buf;

  va_start (args, format);
  va_start (args2, format);
  buf = g_vsprintf (format, &args, &args2);
  va_end (args);
  va_end (args2);

  if (message_func)
    {
      (* message_func) (buf);
    }
  else
    {
      fputs ("message: ", stdout);
      fputs (buf, stdout);
      fputc ('\n', stdout);
    }
}

void
g_print (gchar *format, ...)
{
  va_list args, args2;
  char *buf;

  va_start (args, format);
  va_start (args2, format);
  buf = g_vsprintf (format, &args, &args2);
  va_end (args);
  va_end (args2);

  if (print_func)
    {
      (* print_func) (buf);
    }
  else
    {
      fputs (buf, stdout);
    }
}

GErrorFunc
g_set_error_handler (GErrorFunc func)
{
  GErrorFunc old_error_func;

  old_error_func = error_func;
  error_func = func;

  return old_error_func;
}

GWarningFunc
g_set_warning_handler (GWarningFunc func)
{
  GWarningFunc old_warning_func;

  old_warning_func = warning_func;
  warning_func = func;

  return old_warning_func;
}

GPrintFunc
g_set_message_handler (GPrintFunc func)
{
  GPrintFunc old_message_func;

  old_message_func = message_func;
  message_func = func;

  return old_message_func;
}

GPrintFunc
g_set_print_handler (GPrintFunc func)
{
  GPrintFunc old_print_func;

  old_print_func = print_func;
  print_func = func;
  
  return old_print_func;
}

gint
g_snprintf (gchar       *str,
	    gulong       n,
	    gchar const *fmt,
	    ...)
{
#ifdef HAVE_VSNPRINTF
  va_list args;
  gint retval;
  
  va_start (args, fmt);
  retval = vsnprintf (str, n, fmt, args);
  va_end (args);

  return retval;

#else
  gchar *printed;
  va_list args, args2;

  va_start (args, fmt);
  va_start (args2, fmt);
  
  printed = g_vsprintf (fmt, &args, &args2);
  strncpy (str, printed, n);
  str[n-1] = '\0';
  
  va_end (args2);
  va_end (args);

  return strlen (str);

#endif
}

gint
g_strcasecmp (const guchar *s1, const guchar *s2)
{
#ifdef HAVE_STRCASECMP
  return strcasecmp(s1, s2);
#else
  gint c1, c2;

  while (*s1 && *s2)
    {
      c1 = tolower(*s1++); c2 = tolower(*s2++);
      if (c1 != c2)
        return (c1 - c2);
    }

  return ((gint) *s1 - (gint) *s2);
#endif
}
