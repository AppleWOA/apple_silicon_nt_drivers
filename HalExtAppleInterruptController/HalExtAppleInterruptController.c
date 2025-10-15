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

#include <nthalext.h>
#include "HalExtAppleInterruptController.h"

STATIC UINT64 gAppleInterruptControllerBase;
STATIC APPLE_INTERRUPT_CONTROLLER_VERSION gAicVersion;
STATIC UINT32 gAicMaxIrqs, gAicMaskSetOffset, gAicMaskClearOffset;

//
// The Apple Interrupt Controller (AIC) is a non-standard IRQ controller used on Apple's ARM-based platforms since the A5 to
// manage hardware interrupts. 
// 
// AIC works off the concept of separate set/clear registers for IRQ state (you write to separate set/clear registers, and read from
// either to get IRQ state generally), and AIC doesn't have the concept of a hardware distributor or
// CPU-specific redistributors, with the controller doing all the interrupt routing itself. In AICv1 this is done via normal CPU affinities,
// while in AICv2 and AICv3, there's a hardware heuristic that relies on cores opting in and out of interrupts as needed.
// 
// Note that some interrupts on AIC platforms are actually not delivered by the AIC itself and are instead delivered by the core's peripheral directly as an FIQ
// (all of these interrupts are what would be PPIs in a GIC-based system, such as interrupts from the PMU or timer interrupts), not fully sure how we want to handle these
// yet.
// 
// Since it's non-standard, the Windows HAL doesn't include any support for it, and as a result,
// we need to add support via a HAL Extension, but since the normal HAL Extension imports don't support registering IRQ chips specifically, we
// need to find the function that does the registration and call it directly.
//

NTSTATUS AppleInterruptControllerInterruptMaskSet(UINT32 IrqNum) {
	UINT32 CpuDieOffset;
	switch (gAicVersion) {
		case APPLE_INTERRUPT_CONTROLLER_V1:
			WRITE_REGISTER_ULONG(gAppleInterruptControllerBase + gAicMaskSetOffset + AIC_MASK_BIT(IrqNumber), AIC_MASK_BIT(IrqNum));
			break;
		case APPLE_INTERRUPT_CONTROLLER_V2:
		case APPLE_INTERRUPT_CONTROLLER_V3:
			//
			// TODO: this. (divide by max IRQs then multiply by die stride to get the right offset for IRQs that originate on the other CPU die.)
			//
			WRITE_REGISTER_ULONG(gAppleInterruptControllerBase + gAicMaskSetOffset + CpuDieOffset + AIC_MASK_BIT(IrqNumber), AIC_MASK_BIT(IrqNum));
			break;
	}
	return NT_SUCCESS;
}

NTSTATUS AppleInterruptControllerSetInterruptMaskClear(UINT32 IrqNum) {
	UINT32 CpuDieOffset;
	switch (gAicVersion) {
	case APPLE_INTERRUPT_CONTROLLER_V1:
		WRITE_REGISTER_ULONG(gAppleInterruptControllerBase + gAicMaskClearOffset + AIC_MASK_BIT(IrqNumber), AIC_MASK_BIT(IrqNum));
		break;
	case APPLE_INTERRUPT_CONTROLLER_V2:
	case APPLE_INTERRUPT_CONTROLLER_V3:
		//
		// TODO: this. (divide by max IRQs then multiply by die stride to get the right offset for IRQs that originate on the other CPU die.)
		//
		WRITE_REGISTER_ULONG(gAppleInterruptControllerBase + gAicMaskClearOffset + CpuDieOffset + AIC_MASK_BIT(IrqNumber), AIC_MASK_BIT(IrqNum));
		break;
	}
	return NT_SUCCESS;
}

NTSTATUS HalExtAppleInterruptControllerEntry(VOID) {
	//
	// TODO: literally everything, including the following:
	// - find the AIC version, this should be inferrable via the ACPI tables the UEFI hands us.
	// - get the number of total supported IRQs and actually implemented ones on our platform.
	// - mask all interrupts, this is easy.
	// - register the IRQ controller with the HAL by registering the address usage (this *is* in the HAL extension interface), and registering the
	// controller itself (this is not in the HAL extension interface)
	//
	
	//
	// Commented out for now while the code is worked on.
	//
	//HalRegisterPermanentAddressUsage(gAppleInterruptControllerBase, 0xC000);
}