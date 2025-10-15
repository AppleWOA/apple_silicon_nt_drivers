/**
 * Copyright (c) 2025, AppleWOA authors.
 *
 * Module Name:
 *     HalExtAppleInterruptController.c
 *
 * Abstract:
 *     HAL Extension for the Apple Interrupt Controller found on Apple silicon platforms.
 *
 *
 * Environment:
 *     NT kernel mode.
 *
 * License:
 *     SPDX-License-Identifier: (BSD-2-Clause-Patent OR MIT)
*/

#ifndef HAL_EXT_APPLE_INTERRUPT_CONTROLLER_H
#define HAL_EXT_APPLE_INTERRUPT_CONTROLLER_H

//
// AIC version enum, used to track the version of AIC on the current platform.
//

typedef enum {
    APPLE_INTERRUPT_CONTROLLER_V1,
    APPLE_INTERRUPT_CONTROLLER_V2,
    APPLE_INTERRUPT_CONTROLLER_V3,
    APPLE_INTERRUPT_CONTROLLER_VER_UNKNOWN
} APPLE_INTERRUPT_CONTROLLER_VERSION;
#endif // !HAL_EXT_APPLE_INTERRUPT_CONTROLLER_H
