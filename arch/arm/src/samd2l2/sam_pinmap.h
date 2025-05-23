/****************************************************************************
 * arch/arm/src/samd2l2/sam_pinmap.h
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

#ifndef __ARCH_ARM_SRC_SAMD2L2_SAM_PINMAP_H
#define __ARCH_ARM_SRC_SAMD2L2_SAM_PINMAP_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#if defined(CONFIG_ARCH_FAMILY_SAMD20)
#  include "hardware/samd20_pinmap.h"
#elif defined(CONFIG_ARCH_FAMILY_SAMD21)
#  include "hardware/samd21_pinmap.h"
#elif defined(CONFIG_ARCH_FAMILY_SAML21)
#  include "hardware/saml21_pinmap.h"
#else
#  error Unrecognized SAMD/L architecture
#endif

#endif /* __ARCH_ARM_SRC_SAMD2L2_SAM_PINMAP_H */
