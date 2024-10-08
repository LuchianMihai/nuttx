/****************************************************************************
 * binfmt/binfmt_copyargv.c
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

#include <string.h>
#include <debug.h>
#include <errno.h>

#include <nuttx/kmalloc.h>
#include <nuttx/binfmt/binfmt.h>

#include "binfmt.h"

#if defined(CONFIG_ARCH_ADDRENV) && defined(CONFIG_BUILD_KERNEL) && !defined(CONFIG_BINFMT_DISABLE)

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* This is an artificial limit to detect error conditions where an argv[]
 * list is not properly terminated.
 */

#define MAX_EXEC_ARGS 256

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: binfmt_copyargv
 *
 * Description:
 *   In the kernel build, the argv list will likely lie in the caller's
 *   address environment and, hence, be inaccessible when we switch to the
 *   address environment of the new process address environment.  So we
 *   do not have any real option other than to copy the callers argv[] list.
 *
 * Input Parameters:
 *   argv     - Argument list
 *
 * Returned Value:
 *   A non-zero copy is returned on success.
 *
 ****************************************************************************/

int binfmt_copyargv(FAR char * const **copy, FAR char * const *argv)
{
  FAR char **argvbuf = NULL;
  FAR char *ptr;
  size_t argvsize;
  size_t argsize = 0;
  int nargs = 0;
  int i;

  /* Get the number of arguments and the size of the argument list */

  if (argv)
    {
      for (i = 0; argv[i]; i++)
        {
          /* Increment the size of the allocation with the size of the next
           * string
           */

          argsize += strlen(argv[i]) + 1;
          nargs++;

          /* This is a sanity check to prevent running away with an
           * unterminated argv[] list.
           * MAX_EXEC_ARGS should be sufficiently large that this
           * never happens in normal usage.
           */

          if (nargs > MAX_EXEC_ARGS)
            {
              berr("ERROR: Too many arguments: %zu\n", argsize);
              return -E2BIG;
            }
        }

      binfo("args=%d argsize=%zu\n", nargs, argsize);

      /* Allocate the argv array and an argument buffer */

      if (argsize > 0)
        {
          argvsize = (nargs + 1) * sizeof(FAR char *);
          ptr      = kmm_malloc(argvsize + argsize);
          if (!ptr)
            {
              berr("ERROR: Failed to allocate the argument buffer\n");
              return -ENOMEM;
            }

          /* Copy the argv list */

          argvbuf = (FAR char **)ptr;
          ptr    += argvsize;
          for (i = 0; argv[i]; i++)
            {
              argvbuf[i] = ptr;
              argsize    = strlen(argv[i]) + 1;
              memcpy(ptr, argv[i], argsize);
              ptr       += argsize;
            }

          /* Terminate the argv[] list */

          argvbuf[i] = NULL;
        }
    }

  *copy = argvbuf;
  return OK;
}

/****************************************************************************
 * Name: binfmt_freeargv
 *
 * Description:
 *   Release the copied argv[] list.
 *
 * Input Parameters:
 *   argv     - Argument list
 *
 * Returned Value:
 *   None
 *
 ****************************************************************************/

void binfmt_freeargv(FAR char * const *argv)
{
  /* Is there an allocated argument buffer */

  if (argv)
    {
      /* Free the argument buffer */

      kmm_free((FAR char **)argv);
    }
}

#endif
