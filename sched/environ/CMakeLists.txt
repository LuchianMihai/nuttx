# ##############################################################################
# sched/environ/CMakeLists.txt
#
# SPDX-License-Identifier: Apache-2.0
#
# Licensed to the Apache Software Foundation (ASF) under one or more contributor
# license agreements.  See the NOTICE file distributed with this work for
# additional information regarding copyright ownership.  The ASF licenses this
# file to you under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License.  You may obtain a copy of
# the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
# License for the specific language governing permissions and limitations under
# the License.
#
# ##############################################################################

if(NOT CONFIG_DISABLE_ENVIRON)
  target_sources(
    sched
    PRIVATE env_getenvironptr.c
            env_dup.c
            env_release.c
            env_findvar.c
            env_removevar.c
            env_clearenv.c
            env_getenv.c
            env_putenv.c
            env_setenv.c
            env_unsetenv.c
            env_foreach.c)
endif()
