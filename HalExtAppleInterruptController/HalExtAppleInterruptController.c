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

STATIC AIC_INFO gAicInfo;
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
// "InterruptControllerContext" here is the InternalData pointer passed into the initialization block. For now,
// we're going to prefer using our own AIC_INFO structure, however it is good to keep CSRT InternalData around in case we need it.
//
NTSTATUS AppleInterruptControllerInitializeLocalUnit(PVOID InterruptControllerContext, UINT32 Param1, UINT32 Param2, UINT32 Param3, UINT32 Param4, PUINT32 Param5) {
	return NT_SUCCESS;
}

NTSTATUS AppleInterruptControllerIniitalizeIoUnit(PVOID InterruptControllerContext) {
	//
	// The GICv2 driver seems to register IO units for every GIC device, but the GICv3 and BC2836 drivers in the HAL only register the single
	// IO unit.
	// In our case, we should only have the one IO unit, so we would probably follow the GICv3/BC2836 case here.
	//
	return NT_SUCCESS;
}

VOID AppleInterruptControllerSetPriority(PVOID InterruptControllerContext, UINT32 Priority) {
	//
	// AIC does not permit us any control over priority in any version, 
	// lower IRQs are treated as higher priority always. (per the Asahi Linux documentation of the driver in linux tree)
	// This function will probably be NULLed out at some point.
	//
	return;
}

VOID AppleInterruptControllerClearLocalUnitError(PVOID InterruptControllerContext) {
	//
	// AIC technically has no conception of a "local unit" (per-core MMIO), all the per-core interrupts
	// on Apple platforms come as direct FIQs from the core itself.
	// Regardless, this function won't be NULLed just yet.
	//
	return;
}

NTSTATUS AppleInterruptControllerGetLogicalId(PVOID InterruptControllerContext, _INTERRUPT_TARGET InterruptTarget) {
	return NT_SUCCESS;
}

NTSTATUS AppleInterruptControllerSetLogicalId(PVOID InterruptControllerContext, _INTERRUPT_TARGET InterruptTarget) {
	return NT_SUCCESS;
}

//_INTERRUPT_RESULT AppleInterruptControllerAcceptAndGetSource(PVOID InterruptControllerContext, PINT32 Param1, PUINT32 Param2) {
//
//}

VOID AppleInterruptControllerEndOfInterrupt(PVOID InterruptControllerContext, UINT32 IrqNum) {
	//
	// For interrupts that originate from AIC itself (peripheral interrupts mostly), reading from the event register 
	// automatically acknowledges and masks the IRQ (what would be the EOI event),
	// so for safety's sake, read the event register just in case to clear any IRQs pending, then unmask the IRQ if we have to.
	//
	//
	// For FIQs, we need to manually acknowledge and mask them ourselves. We might have to check for all the FIQ sources first, then
	// check if it's an IRQ if it's not an FIQ source.
	//

	READ_REGISTER_ULONG(gAicInfo.AppleInterruptControllerBase + (UINT64)(gAicInfo.EventRegisterOffset));
	return;
}

VOID AppleInterruptControllerFastEndOfInterrupt(VOID) {
	//
	// FastEndOfInterrupt would imply FIQs. Do we want to use this to signal EOI on the timer and other FIQs on Apple platforms?
	//
	return;
}

NTSTATUS AppleInterruptControllerSetLineState(PVOID InterruptControllerContext, _INTERRUPT_LINE* IrqLine, _INTERRUPT_LINE_STATE* IrqLineState) {
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

NTSTATUS AppleInterruptControllerStartProcessor(PVOID InterruptControllerContext, UINT32 Param1, PVOID Param2, UINT32 Param3) {
	//
	// Very likely what triggers IPIs.
	//
	return NT_SUCCESS;
}

NTSTATUS AppleInterruptControllerGenerateMessage(PVOID InterruptControllerContext, _INTERRUPT_LINE_STATE* IrqLineState, PUINT64 Param2, PUINT64 Param3) {
	return NT_SUCCESS;
}

NTSTATUS AppleInterruptControllerConvertId(PVOID InterruptControllerContext, PUINT32 Param1, _INTERRUPT_TARGET* IrqTarget, UINT8 Param3) {
	return NT_SUCCESS;
}

NTSTATUS AppleInterruptControllerSaveLocalInterrupts(PVOID InterruptControllerContext, PVOID Param1) {
	return NT_SUCCESS;
}

NTSTATUS AppleInterruptControllerReplayLocalInterrupts(PVOID InterruptControllerContext, PVOID Param1) {
	//
	// None of the supported IRQ controllers for ARM64 implement the ReplayLocalInterrupts function.
	// Almost definitely behavior that's mostly for x86/AMD64 APIC or something else for non-ARM platforms.
	// This will be NULLed out later.
	//
	return NT_SUCCESS;
}

NTSTATUS AppleInterruptControllerDeinitializeLocalUnit(PVOID InterruptControllerContext) {
	//
	// As before, AIC devices technically do not have a conception of a local, per-core unit
	// as those instead are done via per-core FIQs.
	//
	return NT_SUCCESS;
}

NTSTATUS AppleInterruptControllerDeinitializeIoUnit(PVOID InterruptControllerContext) {
	//
	// What needs to be done here:
	// - Mask all pending IRQs, and signal EOI on any pending interrupts.
	// - Mask all FIQs.
	// - On AICv2, turn off the AIC itself to disable it sending interrupts (AICv1 does not have an off switch as such, masking all IRQs is the best we can do there.
	//
	return NT_SUCCESS;
}

//_INTERRUPT_RESULT AppleInterruptControllerQueryAndGetSource(PVOID InterruptControllerInternalData, PINT32 IrqId, PUINT32 Param2, PUINT8 Param3) {
//	//
//	// This seems to be to detect if something is a line based or vector based IRQ?
//	//
//	return InterruptResultNone;
//}

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

VOID AppleInterruptControllerDirectedEndOfInterrupt(PVOID InterruptControllerContext, UINT32 Param1, UINT32 Param2) {
	//
	// None of the ARM64-supported IRQ controllers in the HAL implement DirectedEndOfInterrupt, so this will probably get NULLed out at some point.
	//
	return;
}

NTSTATUS AppleInterruptControllerQueryLocalUnitInfo(PVOID InterruptControllerContext, UINT32 Param1, PUINT32 Param2, PUINT32 Param3, _KINTERRUPT_MODE* Param4, _KINTERRUPT_MODE* Param5) {
	return NT_SUCCESS;
}

NTSTATUS AppleInterruptControllerQueryPendingState(PVOID InterruptControllerContext, _INTERRUPT_LINE* IrqLine, PUINT8 Param2, PUINT8 Param3) {
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
	NULL, //AcceptAndGetSource is nulled out for now, too risky to have a stub.
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
	NULL, //QueryAndGetSource is nulled out for now for similar reasons as AcceptAndGetSource.
	AppleInterruptControllerDeactivateInterrupt,
	AppleInterruptControllerDirectedEndOfInterrupt,
	AppleInterruptControllerQueryLocalUnitInfo,
	AppleInterruptControllerQueryPendingState,
	NULL, //CaptureGlobalCrashdumpState is nulled out for now.
	NULL, //CaptureProcessorCrashdumpState is nulled out for now.
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
	KernelExceptionHandler = _ReadSystemReg(ARM64_SYSREG(3, 0, 12, 0, 0));
	HalpInterruptRegisterController = (PVOID)((KernelExceptionHandler - 0x19BD20));

	Status = AppleInterruptControllerRegisterIoUnit();

	return Status;
}