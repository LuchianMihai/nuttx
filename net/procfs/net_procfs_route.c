/****************************************************************************
 * net/procfs/net_procfs_route.c
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

#include <sys/types.h>
#include <sys/stat.h>

#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>
#include <errno.h>
#include <debug.h>

#include <nuttx/kmalloc.h>
#include <nuttx/fs/fs.h>
#include <nuttx/fs/procfs.h>

#include "route/route.h"

#if !defined(CONFIG_DISABLE_MOUNTPOINT) && defined(CONFIG_FS_PROCFS)
#ifndef CONFIG_FS_PROCFS_EXCLUDE_ROUTE

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Determines the size of an intermediate buffer that must be large enough
 * to handle the longest line generated by this logic.
 */

#define STATUS_LINELEN 58

/* Directory entry indices */

#if defined(CONFIG_NET_IPv4) && defined(CONFIG_NET_IPv6)
#  define IPv4_INDEX  0
#  define IPv6_INDEX  1
#elif defined(CONFIG_NET_IPv4)
#  define IPv4_INDEX  0
#elif defined(CONFIG_NET_IPv6)
#  define IPv6_INDEX  0
#endif

/****************************************************************************
 * Private Types
 ****************************************************************************/

/* This enumeration identifies all of the nodes that can be accessed via the
 * procfs file system.
 */

enum route_node_e
{
  PROC_ROUTE = 0                     /* The top-level directory */
#ifdef CONFIG_NET_IPv4
  , PROC_ROUTE_IPv4                  /* IPv4 routing table */
#endif
#ifdef CONFIG_NET_IPv6
  , PROC_ROUTE_IPv6                  /* IPv6 routing table */
#endif
};

/* This structure describes one open "file" */

struct route_file_s
{
  struct procfs_file_s base;          /* Base open file structure */
  FAR const char *name;               /* Terminal node segment name */
  uint8_t node;                       /* Type of node (see enum route_node_e) */
  char line[STATUS_LINELEN];          /* Pre-allocated buffer for formatted lines */
};

/* This structure describes one open "directory" */

struct route_dir_s
{
  struct procfs_dir_priv_s base;      /* Base directory private data */
  FAR const char *name;               /* Terminal node segment name */
  uint8_t node;                       /* Type of node (see enum route_node_e) */
};

/* The structure is used when traversing routing tables */

struct route_info_s
{
  FAR char *line;                    /* Intermediate line buffer pointer */
  FAR char *buffer;                  /* User buffer */
  size_t    linelen;                 /* Size of the intermediate buffer */
  size_t    buflen;                  /* Size of the user buffer */
  size_t    remaining;               /* Bytes remaining in user buffer */
  size_t    totalsize;               /* Accumulated size of the copy */
  off_t     offset;                  /* Skip offset */
  bool      header;                  /* True: header has been generated */
  int16_t   index;                   /* Routing table index */
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/* Helpers */

static void    route_sprintf(FAR struct route_info_s *info,
                 FAR const char *fmt, ...) printf_like(2, 3);
#ifdef CONFIG_NET_IPv4
static int     route_ipv4_entry(FAR struct net_route_ipv4_s *route,
                 FAR void *arg);
#endif
#ifdef CONFIG_NET_IPv6
static int     route_ipv6_entry(FAR struct net_route_ipv6_s *route,
                 FAR void *arg);
#endif
#ifdef CONFIG_NET_IPv4
static ssize_t route_ipv4_table(FAR struct route_file_s *procfile,
                 FAR char *buffer, size_t buflen, off_t offset);
#endif
#ifdef CONFIG_NET_IPv6
static ssize_t route_ipv6_table(FAR struct route_file_s *procfile,
                 FAR char *buffer, size_t buflen, off_t offset);
#endif

/* File system methods */

static int     route_open(FAR struct file *filep, FAR const char *relpath,
                 int oflags, mode_t mode);
static int     route_close(FAR struct file *filep);
static ssize_t route_read(FAR struct file *filep, FAR char *buffer,
                 size_t buflen);

static int     route_dup(FAR const struct file *oldp,
                 FAR struct file *newp);

static int     route_opendir(const char *relpath,
                 FAR struct fs_dirent_s **dir);
static int     route_closedir(FAR struct fs_dirent_s *dir);
static int     route_readdir(FAR struct fs_dirent_s *dir,
                             FAR struct dirent *entry);
static int     route_rewinddir(FAR struct fs_dirent_s *dir);

static int     route_stat(FAR const char *relpath, FAR struct stat *buf);

/****************************************************************************
 * Public Data
 ****************************************************************************/

/* See fs_mount.c -- this structure is explicitly externed there.
 * We use the old-fashioned kind of initializers so that this will compile
 * with any compiler.
 */

const struct procfs_operations g_netroute_operations =
{
  route_open,          /* open */
  route_close,         /* close */
  route_read,          /* read */
  NULL,                /* write */
  NULL,                /* poll */

  route_dup,           /* dup */

  route_opendir,       /* opendir */
  route_closedir,      /* closedir */
  route_readdir,       /* readdir */
  route_rewinddir,     /* rewinddir */

  route_stat           /* stat */
};

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* Well-known paths */

static const char g_route_path[]        = "net/route";
#ifdef CONFIG_NET_IPv4
static const char g_route_ipv4_path[]   = "net/route/ipv4";
#endif
#ifdef CONFIG_NET_IPv6
static const char g_route_ipv6_path[]   = "net/route/ipv6";
#endif

/* Subdirectory names */

#ifdef CONFIG_NET_IPv4
static const char g_route_ipv4_subdir[] = "ipv4";
#endif
#ifdef CONFIG_NET_IPv6
static const char g_route_ipv6_subdir[] = "ipv6";
#endif

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: route_sprintf
 *
 * Description:
 *   Format:
 *
 *            11111111112222222222333
 *   12345678901234567890123456789012
 *   SEQ   TARGET   NETMASK  ROUTER
 *   nnnn. xxxxxxxx xxxxxxxx xxxxxxxx
 *
 ****************************************************************************/

static void route_sprintf(FAR struct route_info_s *info,
                         FAR const char *fmt, ...)
{
  size_t linesize;
  size_t copysize;
  va_list ap;

  /* Print the format and data to a line buffer */

  va_start(ap, fmt);
  linesize = vsnprintf(info->line, info->linelen, fmt, ap);
  va_end(ap);

  /* Copy the line buffer to the user buffer */

  copysize = procfs_memcpy(info->line, linesize,
                           info->buffer, info->remaining,
                           &info->offset);

  /* Update counts and pointers */

  info->totalsize += copysize;
  info->buffer    += copysize;
  info->remaining -= copysize;
}

/****************************************************************************
 * Name: route_ipv4_entry
 *
 * Description:
 *   Format:
 *
 *            11111111112222222222333333333344444444444555
 *   12345678901234567890123456789012345678901234567890123
 *   SEQ   TARGET          NETMASK         ROUTER
 *   nnnn. xxx.xxx.xxx.xxx xxx.xxx.xxx.xxx xxx.xxx.xxx.xxx
 *
 ****************************************************************************/

#ifdef CONFIG_NET_IPv4
static int route_ipv4_entry(FAR struct net_route_ipv4_s *route,
                            FAR void *arg)
{
  FAR struct route_info_s *info = (FAR struct route_info_s *)arg;
  char target[INET_ADDRSTRLEN];
  char netmask[INET_ADDRSTRLEN];
  char router[INET_ADDRSTRLEN];

  DEBUGASSERT(info != NULL);

  /* Generate the header before the first entry */

  if (info->index == 0 && !info->header)
    {
      route_sprintf(info, "%-4s  %-16s%-16s%-16s\n",
                    "SEQ", "TARGET", "NETMASK", "ROUTER");

      if (info->totalsize >= info->buflen)
        {
          /* Only part of the header was printed. */

          return 1;
        }

      /* The whole header was printed. */

      info->header = true;
    }

  /* Generate routing table entry on one line */

  inet_ntop(AF_INET, &route->target,  target,  INET_ADDRSTRLEN);
  inet_ntop(AF_INET, &route->netmask, netmask, INET_ADDRSTRLEN);
  inet_ntop(AF_INET, &route->router,  router,  INET_ADDRSTRLEN);

  info->index++;
  route_sprintf(info, "%4u. %-16s%-16s%-16s\n",
               info->index, target, netmask, router);

  return (info->totalsize >= info->buflen) ? 1 : 0;
}
#endif

/****************************************************************************
 * Name: route_ipv6_entry
 *
 * Description:
 *   Format:
 *
 *            11111111112222222222333333333344444444445555
 *   12345678901234567890123456789012345678901234567890123
 *   nnnn. target:  xxxx:xxxx:xxxx:xxxxxxxx:xxxx:xxxx:xxxx
 *         netmask: xxxx:xxxx:xxxx:xxxxxxxx:xxxx:xxxx:xxxx
 *         router:  xxxx:xxxx:xxxx:xxxxxxxx:xxxx:xxxx:xxxx
 *
 ****************************************************************************/

#ifdef CONFIG_NET_IPv6
static int route_ipv6_entry(FAR struct net_route_ipv6_s *route,
                            FAR void *arg)
{
  FAR struct route_info_s *info = (FAR struct route_info_s *)arg;
  char addr[INET6_ADDRSTRLEN];

  DEBUGASSERT(info != NULL);

  /* Generate routing table entry on three lines */

  info->index++;
  inet_ntop(AF_INET6, route->target,  addr, INET6_ADDRSTRLEN);

  route_sprintf(info, "%4u. TARGET  %s\n", info->index, addr);
  if (info->totalsize >= info->buflen)
    {
      return 1;
    }

  inet_ntop(AF_INET6, route->netmask, addr, INET6_ADDRSTRLEN);
  route_sprintf(info, "      NETMASK %s\n", addr);
  if (info->totalsize >= info->buflen)
    {
      return 1;
    }

  inet_ntop(AF_INET6, route->router,  addr, INET6_ADDRSTRLEN);
  route_sprintf(info, "      ROUTER  %s\n", addr);
  return (info->totalsize >= info->buflen) ? 1 : 0;
}
#endif

/****************************************************************************
 * Name: route_ipv4_table
 *
 * Description:
 *   Format:
 *
 *            11111111112222222222333333333344444444444555
 *   12345678901234567890123456789012345678901234567890123
 *   SEQ   TARGET          NETMASK         ROUTER
 *   nnnn. xxx.xxx.xxx.xxx xxx.xxx.xxx.xxx xxx.xxx.xxx.xxx
 *
 ****************************************************************************/

#ifdef CONFIG_NET_IPv4
static ssize_t route_ipv4_table(FAR struct route_file_s *procfile,
                                FAR char *buffer, size_t buflen,
                                off_t offset)
{
  struct route_info_s info;

  memset(&info, 0, sizeof(struct route_info_s));
  info.line      = procfile->line;
  info.buffer    = buffer;
  info.linelen   = STATUS_LINELEN;
  info.buflen    = buflen;
  info.remaining = buflen;
  info.offset    = offset;

  /* Generate each entry in the routing table */

  net_foreachroute_ipv4(route_ipv4_entry, &info);
  return info.totalsize;
}
#endif

/****************************************************************************
 * Name: route_ipv6_table
 ****************************************************************************/

#ifdef CONFIG_NET_IPv6
static ssize_t route_ipv6_table(FAR struct route_file_s *procfile,
                                FAR char *buffer, size_t buflen,
                                off_t offset)
{
  struct route_info_s info;

  memset(&info, 0, sizeof(struct route_info_s));
  info.line      = procfile->line;
  info.buffer    = buffer;
  info.linelen   = STATUS_LINELEN;
  info.buflen    = buflen;
  info.remaining = buflen;
  info.offset    = offset;

  /* Generate each entry in the routing table */

  net_foreachroute_ipv6(route_ipv6_entry, &info);
  return info.totalsize;
}
#endif

/****************************************************************************
 * Name: route_open
 ****************************************************************************/

static int route_open(FAR struct file *filep, FAR const char *relpath,
                      int oflags, mode_t mode)
{
  FAR struct route_file_s *procfile;
  FAR const char *name;
  uint8_t node;

  finfo("Open '%s'\n", relpath);

  /* PROCFS is read-only.  Any attempt to open with any kind of write
   * access is not permitted.
   *
   * REVISIT:  Write-able proc files could be quite useful.
   */

  if ((oflags & O_WRONLY) != 0 || (oflags & O_RDONLY) == 0)
    {
      ferr("ERROR: Only O_RDONLY supported\n");
      return -EACCES;
    }

  /* There are only two possibilities */

#ifdef CONFIG_NET_IPv4
  if (strcmp(relpath, g_route_ipv4_path) == 0)
    {
      name = g_route_ipv4_subdir;
      node = PROC_ROUTE_IPv4;
    }
  else
#endif
#ifdef CONFIG_NET_IPv6
  if (strcmp(relpath, g_route_ipv6_path) == 0)
    {
      name = g_route_ipv6_subdir;
      node = PROC_ROUTE_IPv6;
    }
  else
#endif
    {
      ferr("ERROR: Invalid path \"%s\"\n", relpath);
      return -ENOENT;
    }

  /* Allocate a container to hold the task and node selection */

  procfile = (FAR struct route_file_s *)
    kmm_zalloc(sizeof(struct route_file_s));
  if (!procfile)
    {
      ferr("ERROR: Failed to allocate file container\n");
      return -ENOMEM;
    }

  /* Initialize the file container */

  procfile->name = name; /* Terminal node segment name */
  procfile->node = node; /* Type of node (see enum route_node_e) */

  /* Save the index as the open-specific state in filep->f_priv */

  filep->f_priv = (FAR void *)procfile;
  return OK;
}

/****************************************************************************
 * Name: route_close
 ****************************************************************************/

static int route_close(FAR struct file *filep)
{
  FAR struct route_file_s *procfile;

  /* Recover our private data from the struct file instance */

  procfile = (FAR struct route_file_s *)filep->f_priv;
  DEBUGASSERT(procfile);

  /* Release the file container structure */

  kmm_free(procfile);
  filep->f_priv = NULL;
  return OK;
}

/****************************************************************************
 * Name: route_read
 ****************************************************************************/

static ssize_t route_read(FAR struct file *filep, FAR char *buffer,
                          size_t buflen)
{
  FAR struct route_file_s *procfile;
  ssize_t ret;

  finfo("buffer=%p buflen=%d\n", buffer, (int)buflen);

  /* Recover our private data from the struct file instance */

  procfile = (FAR struct route_file_s *)filep->f_priv;
  DEBUGASSERT(procfile);

  /* Provide the requested data */

  switch (procfile->node)
    {
#ifdef CONFIG_NET_IPv4
    case PROC_ROUTE_IPv4: /* IPv4 routing table */
      ret = route_ipv4_table(procfile, buffer, buflen, filep->f_pos);
      break;
#endif

#ifdef CONFIG_NET_IPv6
    case PROC_ROUTE_IPv6: /* IPv6 routing table */
      ret = route_ipv6_table(procfile, buffer, buflen, filep->f_pos);
      break;
#endif

     default:
      ret = -EINVAL;
      break;
    }

  /* Update the file offset */

  if (ret > 0)
    {
      filep->f_pos += ret;
    }

  return ret;
}

/****************************************************************************
 * Name: route_dup
 *
 * Description:
 *   Duplicate open file data in the new file structure.
 *
 ****************************************************************************/

static int route_dup(FAR const struct file *oldp, FAR struct file *newp)
{
  FAR struct route_file_s *oldfile;
  FAR struct route_file_s *newfile;

  finfo("Dup %p->%p\n", oldp, newp);

  /* Recover our private data from the old struct file instance */

  oldfile = (FAR struct route_file_s *)oldp->f_priv;
  DEBUGASSERT(oldfile);

  /* Allocate a new container to hold the task and node selection */

  newfile = (FAR struct route_file_s *)
    kmm_malloc(sizeof(struct route_file_s));
  if (!newfile)
    {
      ferr("ERROR: Failed to allocate file container\n");
      return -ENOMEM;
    }

  /* The copy the file information from the old container to the new */

  memcpy(newfile, oldfile, sizeof(struct route_file_s));

  /* Save the new container in the new file structure */

  newp->f_priv = (FAR void *)newfile;
  return OK;
}

/****************************************************************************
 * Name: route_opendir
 *
 * Description:
 *   Open a directory for read access
 *
 ****************************************************************************/

static int route_opendir(FAR const char *relpath,
                         FAR struct fs_dirent_s **dir)
{
  FAR struct route_dir_s *level2;

  finfo("relpath: \"%s\"\n", relpath ? relpath : "NULL");
  DEBUGASSERT(relpath);

  /* Check the relative path */

  if (strcmp(relpath, g_route_path) != 0)
    {
#ifdef CONFIG_NET_IPv4
      if (strcmp(relpath, g_route_ipv4_path) == 0)
        {
          return -ENOTDIR;
        }
#endif

#ifdef CONFIG_NET_IPv6
      if (strcmp(relpath, g_route_ipv6_path) == 0)
        {
          return -ENOTDIR;
        }
#endif

      return -ENOENT;
    }

  level2 = (FAR struct route_dir_s *)
    kmm_zalloc(sizeof(struct route_dir_s));
  if (!level2)
    {
      ferr("ERROR: Failed to allocate the directory structure\n");
      return -ENOMEM;
    }

  /* This is a second level directory */

  level2->base.level       = 2;
  level2->base.nentries    = 2;
  level2->name             = "";
  level2->node             = PROC_ROUTE;
  *dir                     = (FAR struct fs_dirent_s *)level2;
  return OK;
}

/****************************************************************************
 * Name: route_closedir
 *
 * Description: Close the directory listing
 *
 ****************************************************************************/

static int route_closedir(FAR struct fs_dirent_s *dir)
{
  DEBUGASSERT(dir);
  kmm_free(dir);
  return OK;
}

/****************************************************************************
 * Name: route_readdir
 *
 * Description: Read the next directory entry
 *
 ****************************************************************************/

static int route_readdir(FAR struct fs_dirent_s *dir,
                         FAR struct dirent *entry)
{
  FAR struct route_dir_s *level2;
  FAR const char *dname;
  unsigned int index;

  DEBUGASSERT(dir != NULL);
  level2 = (FAR struct route_dir_s *)dir;

  /* The index determines which entry to return */

  index = level2->base.index;
#ifdef CONFIG_NET_IPv4
  if (index == IPv4_INDEX)
    {
      dname = g_route_ipv4_subdir;
    }
  else
#endif
#ifdef CONFIG_NET_IPv6
  if (index == IPv6_INDEX)
    {
      dname = g_route_ipv6_subdir;
    }
  else
#endif
    {
      /* We signal the end of the directory by returning the special
       * error -ENOENT
       */

      finfo("Entry %d: End of directory\n", index);
      return -ENOENT;
    }

  /* Save the filename and file type */

  entry->d_type = DTYPE_FILE;
  strlcpy(entry->d_name, dname, sizeof(entry->d_name));

  /* Set up the next directory entry offset.  NOTE that we could use the
   * standard f_pos instead of our own private index.
   */

  level2->base.index = index + 1;
  return OK;
}

/****************************************************************************
 * Name: proc_rewindir
 *
 * Description: Reset directory read to the first entry
 *
 ****************************************************************************/

static int route_rewinddir(struct fs_dirent_s *dir)
{
  FAR struct route_dir_s *priv;

  DEBUGASSERT(dir);
  priv = (FAR struct route_dir_s *)dir;

  priv->base.index = 0;
  return OK;
}

/****************************************************************************
 * Name: route_stat
 *
 * Description: Return information about a file or directory
 *
 ****************************************************************************/

static int route_stat(const char *relpath, struct stat *buf)
{
  memset(buf, 0, sizeof(struct stat));

  if (strcmp(relpath, g_route_path) == 0)
    {
      buf->st_mode = S_IFDIR | S_IROTH | S_IRGRP | S_IRUSR;
    }
  else
#ifdef CONFIG_NET_IPv4
  if (strcmp(relpath, g_route_ipv4_path) == 0)
    {
      buf->st_mode = S_IFREG | S_IROTH | S_IRGRP | S_IRUSR;
    }
  else
#endif
#ifdef CONFIG_NET_IPv6
  if (strcmp(relpath, g_route_ipv6_path) == 0)
    {
      buf->st_mode = S_IFREG | S_IROTH | S_IRGRP | S_IRUSR;
    }
  else
#endif
    {
      return -ENOENT;
    }

  return OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

#endif /* CONFIG_FS_PROCFS_EXCLUDE_ROUTE */
#endif /* !CONFIG_DISABLE_MOUNTPOINT && CONFIG_FS_PROCFS */
