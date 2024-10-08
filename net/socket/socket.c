/****************************************************************************
 * net/socket/socket.c
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <sys/socket.h>
#include <errno.h>
#include <assert.h>
#include <debug.h>

#include "usrsock/usrsock.h"
#include "socket/socket.h"

#ifdef CONFIG_NET

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: psock_socket
 *
 * Description:
 *   socket() creates an endpoint for communication and returns a socket
 *   structure.
 *
 * Input Parameters:
 *   domain   (see sys/socket.h)
 *   type     (see sys/socket.h)
 *   protocol (see sys/socket.h)
 *   psock    A pointer to a user allocated socket structure to be
 *            initialized.
 *
 * Returned Value:
 *  Returns zero (OK) on success.  On failure, it returns a negated errno
 *  value to indicate the nature of the error:
 *
 *   EACCES
 *     Permission to create a socket of the specified type and/or protocol
 *     is denied.
 *   EAFNOSUPPORT
 *     The implementation does not support the specified address family.
 *   EINVAL
 *     Unknown protocol, or protocol family not available.
 *   EMFILE
 *     Process file table overflow.
 *   ENFILE
 *     The system limit on the total number of open files has been reached.
 *   ENOBUFS or ENOMEM
 *     Insufficient memory is available. The socket cannot be created until
 *     sufficient resources are freed.
 *   EPROTONOSUPPORT
 *     The protocol type or the specified protocol is not supported within
 *     this domain.
 *
 ****************************************************************************/

int psock_socket(int domain, int type, int protocol,
                 FAR struct socket *psock)
{
  FAR const struct sock_intf_s *sockif = NULL;
  int ret;

  if (type & ~(SOCK_CLOEXEC | SOCK_NONBLOCK | SOCK_TYPE_MASK))
    {
      return -EINVAL;
    }

  /* Initialize the socket structure */

  psock->s_domain = domain;
  psock->s_proto  = protocol;
  psock->s_conn   = NULL;
  psock->s_type   = type & SOCK_TYPE_MASK;

#ifdef CONFIG_NET_USRSOCK
  /* Get the usrsock interface */

  sockif = &g_usrsock_sockif;
  psock->s_sockif = sockif;

  ret = sockif->si_setup(psock);

  /* When usrsock daemon returns -ENOSYS or -ENOTSUP, it means to use
   * kernel's network stack, so fallback to kernel socket.
   * When -ENETDOWN is returned, it means the usrsock daemon was never
   * launched or is no longer running, so fallback to kernel socket.
   */

  if (ret == 0 || (ret != -ENOSYS && ret != -ENOTSUP && ret != -ENETDOWN))
    {
      return ret;
    }

#endif

  /* Get the socket interface */

  sockif = net_sockif(domain, psock->s_type, psock->s_proto);
  if (sockif == NULL)
    {
      nerr("ERROR: socket address family unsupported: %d\n", domain);
#ifdef CONFIG_NET_USRSOCK

      /* We tried to fallback to kernel socket, but one is not available,
       * so use the return code from usrsock.
       */

      return ret;
#endif
      return -EAFNOSUPPORT;
    }

  /* The remaining of the socket initialization depends on the address
   * family.
   */

  DEBUGASSERT(sockif->si_setup != NULL);
  psock->s_sockif = sockif;

  ret = sockif->si_setup(psock);
  if (ret >= 0)
    {
      FAR struct socket_conn_s *conn = psock->s_conn;

      if (type & SOCK_NONBLOCK)
        {
          conn->s_flags |= _SF_NONBLOCK;
        }

      /* The socket has been successfully initialized */

      conn->s_flags |= _SF_INITD;
    }
  else
    {
      nerr("ERROR: socket si_setup() failed: %d\n", ret);
    }

  return ret;
}

#endif /* CONFIG_NET */
