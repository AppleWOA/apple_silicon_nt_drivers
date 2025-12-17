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
STATIC UINT32 gAicNumIrqs, gAicMaxIrqs, gAicMaskSetOffset, gAicMaskClearOffset;
STATIC INTERRUPT_INITIALIZATION_BLOCK gAicInitBlock;
STATIC NTSTATUS (*HalpInterruptRegisterController)(PINTERRUPT_INITIALIZATION_BLOCK InterruptLoaderBlock, UINT32 Unused1, UINT64* Unused2); //this third variable *is* used on the GICv2/GICv3 HAL drivers...

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
// (all of these interrupts are what would be PPIs in a GIC-based system, such as interrupts from the PMU or timer interrupts).
// 
// Since it's non-standard, the Windows HAL doesn't include any support for it, and as a result,
// we need to add support via a HAL Extension, but since the normal HAL Extension imports don't support registering IRQ chips specifically, we
// need to find the function that does the registration and call it directly.
// 
//

//
// The parameters here have yet to be RE'd, just use a stub for now for anything that isn't clear on purpose.
//
NTSTATUS AppleInterruptControllerInitializeLocalUnit(PVOID InterruptControllerContext, UINT32 Param1, UINT32 Param2, UINT32 Param3, UINT32 Param4, PUINT32 Param5) {
	return NT_SUCCESS;
}

NTSTATUS AppleInterruptControllerIniitalizeIoUnit(PVOID InterruptControllerContext) {
	return NT_SUCCESS;
}

VOID AppleInterruptControllerSetPriority(PVOID InterruptControllerContext, UINT32 Priority) {
	//
	// This one's tricky. AICv1 is the only version of AIC that supports manually setting affinity.
	// For now, use a stub function for AICv2 (we might want to bring in that weird per-core IRQ opt-out at some point)
	//
	switch (gAicVersion) {
		default:
			break;
	}
	return;
}

VOID AppleInterruptControllerClearLocalUnitError(PVOID InterruptControllerContext) {
	//
	// AIC technically has no conception of a "local unit" (per-core MMIO), all the per-core interrupts
	// on Apple platforms come as direct FIQs from the core itself.
	//
	return;
}

NTSTATUS AppleInterruptControllerGetLogicalId(PVOID InterruptControllerContext, _INTERRUPT_TARGET InterruptTarget) {
	return NT_SUCCESS;
}

NTSTATUS AppleInterruptControllerSetLogicalId(PVOID InterruptControllerContext, _INTERRUPT_TARGET InterruptTarget) {
	return NT_SUCCESS;
}

NTSTATUS AppleInterruptControllerRequestInterrupt(PVOID InterruptControllerContext, UINT32 IrqNum) {
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

VOID AppleInterruptControllerDeactivateInterrupt(PVOID InterruptControllerContext, UINT32 IrqNum) {
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

//
// The AIC IRQ controller function table.
// NOTE: I am pretty sure not all of these functions need to exist, and we probably won't need all of them, and if that's the case, the function itself can be replaced with a NULL, 
// but these are all the defined functions a registered interrupt controller can have with the HAL and so this is the initial blueprint for moving forward.
// Anything extra can absolutely be included, but this is the bare minimum "getting started" blueprint.
//
INTERRUPT_FUNCTION_TABLE gAicFunctionTable{
	AppleInterruptControllerInitializeLocalUnit,
	AppleInterruptControllerInitializeIoUnit,
	AppleInterruptControllerSetPriority,
	AppleInterruptControllerGetLocalUnitError,
	AppleInterruptControllerClearLocalUnitError,
	AppleInterruptControllerGetLogicalId,
	AppleInterruptControllerSetLogicalId,
	AppleInterruptControllerAcceptAndGetSource,
	AppleInterruptControllerEndOfInterrupt,
	AppleInterruptControllerFastEndOfInterrupt,
	AppleInterruptControllerSetLineState,
	AppleInterruptControllerRequestInterrupt,
	AppleInterruptControllerStartProcessor,
	AppleInterruptControllerGenerateMessage,
	AppleInterruptControllerConvertId,
	AppleInterruptControllerSaveLocalInterrupts,
	AppleInterruptControllerReplayLocalInterrupts,
	AppleInterruptControllerDeinitializeLocalUnit,
	AppleInterruptControllerDeinitializeIoUnit,
	AppleInterruptControllerQueryAndGetSource,
	AppleInterruptControllerDeactivateInterrupt,
	AppleInterruptControllerDirectedEndOfInterrupt,
	AppleInterruptControllerQueryLocalUnitInfo,
	AppleInterruptControllerQueryPendingState,
	AppleInterruptControllerCaptureGlobalCrashdumpState,
	AppleInterruptControllerCaptureProcessorCrashdumpState
};

//
// Description:
//   Registers the AIC with the HAL itself. Needs to call into HalpInterruptRegisterController
//   which isn't part of the exported HAL Extensions API so we need to find that function ourselves.
// 
NTSTATUS AppleInterruptControllerRegisterIoUnit() {

	gAicInitBlock.Header.TableVersion = 1;
	gAicInitBlock.Header.TableSize = sizeof(INTERRUPT_INITIALIZATION_BLOCK);

	//
	// Internal data (the CSRT vendor data table) needs to be specified here.
	//
	gAicInitBlock.InternalData = NULL;
	gAicInitBlock.InternalDataSize = 0;

	//
	// Fake our known controller type as a GICv1/GICv2 controller.
	//
	gAicInitBlock.KnownType = InterruptControllerGic;

	gAicInitBlock.FunctionTable = gAicFunctionTable;

	//

	//
	// Register the AIC MMIO addresses with the HAL.
	//
	HalRegisterPermanentAddressUsage(gAppleInterruptControllerFunctionBase, 0xC000);

	//
	// Call HalpInterruptRegisterController to register the controller.
	// The entry should find that function pointer (right now this only works on 26100.1)
	//
	return HalpInterruptRegisterController(&gAicInitBlock, 0, NULL);
}

NTSTATUS HalExtAppleInterruptControllerEntry(VOID) {
	//
	// TODO: literally everything, including the following:
	// - get the AIC version and base address via CSRT (Num/Max IRQs are easier to get in-driver)
	// - mask all interrupts based on this information.
	// - store the function pointer to HalpInterruptRegisterController, then call it.
	//
	UINT64 KernelExceptionHandler;

	switch (gAicVersion) {
	case APPLE_INTERRUPT_CONTROLLER_V1:
		gAicNumIrqs = (READ_REGISTER_ULONG(gAppleInterruptControllerBase + AIC_V1_HW_INFO) & AIC_NUM_IRQ_MASK);
		gAicMaxIrqs = AIC_V1_MAX_IRQ;
		break;
	case APPLE_INTERRUPT_CONTROLLER_V2:
		gAicNumIrqs = (READ_REGISTER_ULONG(gAppleInterruptControllerBase + AIC_V2_INFO_REG1) & AIC_NUM_IRQ_MASK);
		gAicMaxIrqs = (READ_REGISTER_ULONG(gAppleInterruptControllerBase + AIC_V2_INFO_REG3) & AIC_NUM_IRQ_MASK);
		break;
	default:
		ASSERTMSG("Failed to get number of AIC interrupts!", FALSE);
	}
	//
	// Mask all AIC interrupts.
	//
	for (UINT32 i = 0; i < gAicNumIrqs; i++) {
		AppleInterruptControllerDeactivateInterrupt(NULL, i);
	}

	//
	// read VBAR_EL1 which contains the kernel exception handlers. For now (this will
	// only work on 26100.1 until we develop better patch-find routines)
	// apply the offset to get to HalpInterruptRegisterController
	//
	KernelExceptionHandler = ARM64_SYSREG(3, 0, 12, 0, 0);
	HalpInterruptRegisterController = (PVOID)((KernelExceptionHandler - 0x19BD20));

	Status = AppleInterruptControllerRegisterIoUnit();
}