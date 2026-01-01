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

//
// The sample DMA controller HAL Extension disables these warnings. Original descriptions below.
//
// Disable warning C4214: nonstandard extension used : bit field types other than int
// Disable warning C4201: nonstandard extension used : nameless struct/union
// Disable warning C4115: named type definition in parentheses
// Disable warning C4127: conditional expression is constant
// Disable warning C4200: zero-sized array in struct/union
//
#pragma warning(disable:4214 4201 4115 4127 4200)

#include "HalExtAppleInterruptController.h"

//
// The Apple Interrupt Controller (AIC) is a non-standard IRQ controller used on Apple's ARM-based platforms since the A5 to
// manage hardware interrupts. 
// 
// AIC works off the concept of separate set/clear registers for IRQ state (you write to separate set/clear registers, and read from
// either to get IRQ state generally), and AIC doesn't have the concept of a hardware distributor or
// CPU-specific redistributors, with the controller doing all the interrupt routing itself. In AICv1 this is done via normal CPU affinities,
// while in AICv2 and AICv3, there's a hardware heuristic that relies on cores opting in and out of interrupts as needed.
// 
// For the AICv2 case, there are registers (both MMIO and CPU) that permit influence over this heuristic, 
// such as what clusters should be targeted first, however because a lot of this has yet to be fully understood,
// we're not going to influence any of this ourselves for now and rely on the heuristic working correctly with the default settings.
// (This should be equivalent to GICv3 with 1-of-N semantics enabled without reliance on affinity.)
// 
// When reading from MASK_SET or MASK_CLR registers (symmetrically), the bits returned indicate IRQ mask status 
// (1 = IRQ masked, 0 = IRQ not masked).
// SW_SET and SW_CLR registers will read 0 bits for any IRQ that isn't assigned to a software function, and 1 for any that is assigned
// to be software-generated. Note that the corresponding mask must still be cleared for the software interrupt to assert itself, and it must still be
// handled appropriately.
// 
// Note that some interrupts on AIC platforms are actually not delivered by the AIC itself and 
// are instead delivered by the core's peripheral directly as an FIQ.
// (all of these interrupts are what would be PPIs in a GIC-based system, such as interrupts from the PMU or timer interrupts)
// This driver needs to handle FIQ-based Fast IPIs and PMU interrupts, but should only minimally handle the timer 
// (as other than the interrupt type used to signal it, the Apple timer is the standard ARM64 timer, and thus Windows has official support for it.)
// 
// Since AIC is non-standard, the Windows HAL doesn't include any support for it, and as a result,
// we need to add support via a HAL Extension, but since the normal HAL Extension imports don't support registering IRQ chips specifically, we
// need to find the function that does the registration and call it directly. Additionally, any private function that is needed to initialize
// the interrupt controller must also be found directly.
// 
//

//
// The parameters here have yet to be RE'd, just use a stub for now for anything that isn't immediately obvious.
// "InterruptControllerContext" here is the InternalData pointer passed into the initialization block. In our case,
// this will be an AIC_INFO structure, filled out in AppleInterruptControllerRegisterIoUnit.
//
NTSTATUS AppleInterruptControllerInitializeLocalUnit(PVOID InterruptControllerContext, ULONG Param1, ULONG Param2, ULONG Param3, ULONG Param4, PULONG Aff0) {
	//
	// AIC does not have a per-processor local unit control, so this function will probably get NULLed out at
	// some point.
	//
	return NT_SUCCESS;
}

//
// Description:
//   This function registers the IRQ lines for the HAL to use. Note that this also requires a function that the HAL Extensions API doesn't
//   export so we'll need to find it ourselves as well. This type of function is used by the GIC and GICv3 "InitializeIoUnit" functions.
//
// Return value:
//  an NTSTATUS code, NT_SUCCESS in the success case.
//
NTSTATUS AppleInterruptControllerDescribeLines(VOID) {
	//UINT64 KernelExceptionHandler;
	//NTSTATUS Status;
	//NTSTATUS (*HalpInterruptRegisterLine)(PINTERRUPT_LINE_INITIALIZATION_BLOCK InterruptLine);
	//KernelExceptionHandler = _ReadSystemReg(ARM64_SYSREG(3, 0, 12, 0, 0)); // read VBAR_EL1
	//HalpInterruptRegisterLine = (PVOID)((UINT64)(KernelExceptionHandler - 0x19B930));

	//INTERRUPT_LINE_INITIALIZATION_BLOCK IrqLineInitBlock = { 0 };
	//IrqLineInitBlock.Type = InterruptLineStandardPin;

	//
	// TODO: have a means to iterate through all the interrupts we want registered.
	//

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

VOID AppleInterruptControllerSetPriority(PVOID InterruptControllerContext, ULONG Priority) {
	//
	// AIC does not permit us any control over IRQ priority in any version, 
	// lower IRQs are treated as higher priority always. (per the Asahi Linux documentation of the driver in linux tree)
	// This function will probably be NULLed out at some point.
	// Note that due to being FIQs, per-core interrupts such as IPIs or timer interrupts have higher priority than even AIC interrupts.
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

//
// These two functions are unimplemented by any of the ARM64-supported interrupt controllers,
// so these might also get NULLed out (unless AIC needs these?)
//
NTSTATUS AppleInterruptControllerGetLogicalId(PVOID InterruptControllerContext, _INTERRUPT_TARGET InterruptTarget) {
	return NT_SUCCESS;
}

NTSTATUS AppleInterruptControllerSetLogicalId(PVOID InterruptControllerContext, _INTERRUPT_TARGET InterruptTarget) {
	return NT_SUCCESS;
}

_INTERRUPT_RESULT AppleInterruptControllerAcceptAndGetSource(PVOID InterruptControllerContext, PINT32 IrqId, PULONG IrqEventValue) {
	//
	// The GICv3 driver does an interrupt acknowledge, then writes the IRQ ID and the full event value to IrqId and IrqEventValue
	// respectively. In our case, we will read from the event register, then write it's full value to IrqEventValue, with the extracted
	// IRQ ID going into IrqId. Note that some IRQ IDs will be message-signaled, but we're treating all IRQ IDs as "Line" interrupts for now.
	//
	P_AIC_INFO AicInfo = (P_AIC_INFO)InterruptControllerContext;
	ULONG AicEvent;
	AicEvent = READ_REGISTER_ULONG(AicInfo->AppleInterruptControllerBaseVirt + (UINT64)(AicInfo->EventRegisterOffset));
	*IrqId = AicEvent & 0xFFFF; // the lower 16 bits encode the IRQ number.
	*IrqEventValue = AicEvent;
	return InterruptBeginLine;
}

VOID AppleInterruptControllerEndOfInterrupt(PVOID InterruptControllerContext, ULONG IrqNum) {
	//
	// For interrupts that originate from AIC itself (peripheral interrupts mostly), reading from the event register 
	// automatically acknowledges and masks the IRQ (what would be the EOI event),
	// so for safety's sake, read the event register just in case to clear any IRQs pending, then unmask the IRQ if we have to.
	//
	//
	// For FIQs, we need to manually acknowledge and mask them ourselves. We might have to check for all the FIQ sources first, then
	// check if it's an IRQ if it's not an FIQ source.
	//
	P_AIC_INFO AicInfo = (P_AIC_INFO)InterruptControllerContext;
	ULONG AicEvent;

	AicEvent = READ_REGISTER_ULONG(AicInfo->AppleInterruptControllerBaseVirt + (UINT64)(AicInfo->EventRegisterOffset));
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

NTSTATUS AppleInterruptControllerRequestInterrupt(PVOID InterruptControllerContext, ULONG IrqNum) {
	ULONG CpuDieOffset;
	P_AIC_INFO AicInfo = (P_AIC_INFO)InterruptControllerContext;
	switch (AicInfo->AicVersion) {
	case APPLE_INTERRUPT_CONTROLLER_V1:
		WRITE_REGISTER_ULONG(AicInfo->AppleInterruptControllerBaseVirt + AicInfo->AicIrqMaskClearOffset + AIC_MASK_BIT(IrqNumber), AIC_MASK_BIT(IrqNum));
		break;
	case APPLE_INTERRUPT_CONTROLLER_V2:
	case APPLE_INTERRUPT_CONTROLLER_V3:
		//
		// TODO: this. (divide by max IRQs then multiply by die stride to get the right offset for IRQs that originate on the other CPU die.)
		//
		WRITE_REGISTER_ULONG(AicInfo->AppleInterruptControllerBaseVirt + AicInfo->AicIrqMaskClearOffset + CpuDieOffset + AIC_MASK_BIT(IrqNumber), AIC_MASK_BIT(IrqNum));
		break;
	}
	return NT_SUCCESS;
}

NTSTATUS AppleInterruptControllerStartProcessor(PVOID InterruptControllerContext, ULONG Param1, PVOID Param2, ULONG Param3) {
	//
	// Very likely what triggers IPIs.
	//
	return NT_SUCCESS;
}

NTSTATUS AppleInterruptControllerGenerateMessage(PVOID InterruptControllerContext, _INTERRUPT_LINE_STATE* IrqLineState, PUINT64 Param2, PUINT64 Param3) {
	return NT_SUCCESS;
}

NTSTATUS AppleInterruptControllerConvertId(PVOID InterruptControllerContext, PULONG Param1, _INTERRUPT_TARGET* IrqTarget, UINT8 Param3) {
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
	// - On AICv2, turn off the AIC itself to disable it sending interrupts (AICv1 does not have an off switch as such, masking all IRQs is the best we can do there.)
	//
	return NT_SUCCESS;
}

//_INTERRUPT_RESULT AppleInterruptControllerQueryAndGetSource(PVOID InterruptControllerInternalData, PINT32 IrqId, PULONG Param2, PUINT8 Param3) {
//	//
//	// This seems to be to detect if something is a line based or vector based IRQ?
//	//
//	return InterruptResultNone;
//}

VOID AppleInterruptControllerDeactivateInterrupt(PVOID InterruptControllerContext, ULONG IrqNum) {
	ULONG CpuDieOffset;
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

VOID AppleInterruptControllerDirectedEndOfInterrupt(PVOID InterruptControllerContext, ULONG Param1, ULONG Param2) {
	//
	// None of the ARM64-supported IRQ controllers in the HAL implement DirectedEndOfInterrupt, so this will probably get NULLed out at some point.
	//
	return;
}

NTSTATUS AppleInterruptControllerQueryLocalUnitInfo(PVOID InterruptControllerContext, ULONG Param1, PULONG Param2, PULONG Param3, _KINTERRUPT_MODE* Param4, _KINTERRUPT_MODE* Param5) {
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
INTERRUPT_FUNCTION_TABLE gAicFunctionTable = {
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
	NULL, //QueryAndGetSource is nulled out for now.
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
NTSTATUS AppleInterruptControllerRegisterIoUnit(PCSRT_RESOURCE_DESCRIPTOR_HEADER CsrtResourceDescriptor) {

	//
	// TODO: literally everything, including the following:
	// - get the AIC version and base address via CSRT (Num/Max IRQs are easier to get in-driver)
	// - mask all interrupts based on this information.
	// - store the function pointer to HalpInterruptRegisterController, then call it.
	//
	UINT64 KernelExceptionHandler;
	UINT64 AicVirtualAddress;
	RD_INTERRUPT_CONTROLLER* CsrtAicData = (RD_INTERRUPT_CONTROLLER*)CsrtResourceDescriptor;
	//
	// This is the Windows standard structure required for all interrupt controllers before being registered with
	// the HAL.
	//
	INTERRUPT_INITIALIZATION_BLOCK AicInitBlock;
	//
	// This is our own internal data structure that each of the functions in our function table
	// will be using.
	//
	AIC_INFO AicInfo;
	RtlZeroMemory(&AicInitBlock, sizeof(INTERRUPT_INITIALIZATION_BLOCK));
	RtlZeroMemory(&AicInfo, sizeof(AIC_INFO));
	//
	// this third variable *is* used on the GICv2/GICv3 HAL drivers...
	// (as far as I can tell, it's related to hypervisor management so not useful for us)
	//
	NTSTATUS (*HalpInterruptRegisterController)(PINTERRUPT_INITIALIZATION_BLOCK InterruptLoaderBlock, ULONG Unused1, UINT64 * Unused2);

	//
	// This is the standard header used by all blocks registered via the HAL SoC API.
	//
	AicInitBlock.Header.TableVersion = 1;
	AicInitBlock.Header.TableSize = sizeof(INTERRUPT_INITIALIZATION_BLOCK);

	//
	// get the AIC base address and version from CSRT.
	//
	AicInfo.AppleInterruptControllerBasePhys.QuadPart = CsrtAicData->ControllerVendorData.ControllerBaseAddress;
	AicInfo.AicVersion = CsrtAicData->ControllerVendorData.Type;
	AicInfo.AppleInterruptControllerSize = CsrtAicData->ControllerVendorData.ControllerBaseSize;

	//
	// Temporarily map the AIC to get the values we need, then unmap it once we're done (to not interfere with normal HAL operation)
	//
	AicVirtualAddress = ((UINT64)(HalMapIoSpace(AicInfo.AppleInterruptControllerBasePhys, CsrtAicData->ControllerVendorData.ControllerBaseSize, MmNonCached)));

	switch (AicInfo.AicVersion) {
	case APPLE_INTERRUPT_CONTROLLER_V1:
		AicInfo.AicNumIrqs = (READ_REGISTER_ULONG(AicVirtualAddress + AIC_V1_HW_INFO) & AIC_NUM_IRQ_MASK);
		AicInfo.AicMaxIrqs = AIC_V1_MAX_IRQ;
		break;
	case APPLE_INTERRUPT_CONTROLLER_V2:
		AicInfo.AicNumIrqs = (READ_REGISTER_ULONG(AicVirtualAddress + AIC_V2_INFO_REG1) & AIC_NUM_IRQ_MASK);
		AicInfo.AicMaxIrqs = (READ_REGISTER_ULONG(AicVirtualAddress + AIC_V2_INFO_REG3) & AIC_NUM_IRQ_MASK);
		break;
	default:
		ASSERTMSG("Failed to get number of AIC interrupts!", FALSE);
	}

	HalUnmapIoSpace((PVOID)AicVirtualAddress, CsrtAicData->ControllerVendorData.ControllerBaseSize);

	//
	// read VBAR_EL1 which contains the kernel exception handlers. For now (this will
	// only work on 26100.1 until we develop better patch-find routines)
	// apply the offset to get to HalpInterruptRegisterController
	//
	KernelExceptionHandler = _ReadSystemReg(ARM64_SYSREG(3, 0, 12, 0, 0)); // read VBAR_EL1
	HalpInterruptRegisterController = (PVOID)((UINT64)((KernelExceptionHandler - 0x19BD20))); // this mess of typecasts is so that the inner expression is a UINT64, *then* cast to pointer...

	//
	// Internal data (our AIC info structure) needs to be specified here.
	//
	AicInitBlock.InternalData = &AicInfo;
	AicInitBlock.InternalDataSize = sizeof(AIC_INFO);

	//
	// In all known platforms where AIC exists, there is only one functional AIC.
	// (On platforms that use chips with two dies, all interrupt management is done by the AIC on the first die,
	// with a stride distance used to access the interrupt state of other dies.)
	//
	AicInitBlock.UnitId = 0;

	//
	// Mark our interrupt controller type as unknown. 
	// (if we do need to fake a controller, fake the GICv2 controller, but unknown helps us dodge some GIC quirks in the kernel.)
	//
	AicInitBlock.KnownType = InterruptControllerUnknown;

	AicInitBlock.FunctionTable = gAicFunctionTable;

	//
	// Capabilities wise, initially we're using the following:
	// - IRQ controller is *not* controlled per processor (bit 0 clear)
	// - IRQ controller does *not* support interrupt priorities (bit 1 clear)
	// - IRQ controller has a "logical flat limit" on processors (bit 2 set)
	// - we will set bits [6:4] for now to have the same IPI behavior as GIC does.
	// (Capability bitmask = 0x72 for now for this)
	//
	AicInitBlock.Capabilities = (INTERRUPT_CONTROLLER_HAS_LOGICAL_FLAT_LIMIT | INTERRUPT_CONTROLLER_IPI_CONTROL_MASK);

	//
	// Register the AIC MMIO addresses with the HAL.
	//
	HalRegisterPermanentAddressUsage(gAppleInterruptControllerFunctionBase, 0xC000);

	//
	// Call HalpInterruptRegisterController to register the controller.
	// The entry should find that function pointer (right now this only works on 26100.1)
	//
	Status = HalpInterruptRegisterController(&AicInitBlock, 0, NULL);
	ASSERT("AIC initialization failed!", Status == NT_SUCCESS);
}

NTSTATUS HalExtAppleInterruptControllerEntry(VOID) {

}

//
// Description:
//   This routine registers all of the CSRT resource descriptors in a resource group 
//   with the HAL to allow the usage of the associated hardware. Of note is that this is the only function
//   a HAL Extension must export to the kernel to be seen as valid.
// 
// Return value:
//   - NT_SUCCESS if resource descriptor registration succeeds, 
//     ASSERT on failure in our case (since it would lead to horribly undefined behavior)
//

NTSTATUS AddResourceGroup(ULONG Handle, PCSRT_RESOURCE_GROUP_HEADER CsrtResourceGroup) {
	ULONG IrqControllerId;
	PCSRT_RESOURCE_DESCRIPTOR_HEADER CsrtResourceDescriptor;
	IrqControllerId = 0;
	CsrtResourceDescriptor = NULL;
	
	//
	// We *should* only have one resource descriptor here, the one for the AIC controller itself.
	//
	// TODO: if we want to add more descriptors, we'll need to use an iterative approach similar to Microsoft's HalExtSampleDma sample.
	//
	CsrtResourceDescriptor = GetNextResourceDescriptor(Handle, CsrtResourceGroup, CsrtResourceDescriptor, CSRT_RD_TYPE_INTERRUPT, CSRT_RD_SUBTYPE_INTERRUPT_CONTROLLER, CSRT_RD_UID_ANY);

	if (ResourceDescriptor == NULL) {
		ASSERTMSG("CSRT resource descriptor for AIC is NULL!", FALSE);
	}

	AppleInterruptControllerRegisterIoUnit(CsrtResourceDescriptor);
}