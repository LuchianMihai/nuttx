# ##############################################################################
# libs/libc/tls/CMakeLists.txt
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

set(SRCS task_getinfo.c tls_getinfo.c)

if(NOT CONFIG_TLS_TASK_NELEM EQUAL 0)
  list(APPEND SRCS task_tls.c task_tls_destruct.c)
endif()

if(NOT CONFIG_TLS_NELEM EQUAL 0)
  list(APPEND SRCS tls_destruct.c)
endif()

if(NOT CONFIG_TLS_NCLEANUP EQUAL 0)
  list(APPEND SRCS tls_cleanup.c)
endif()

target_sources(c PRIVATE ${SRCS})
