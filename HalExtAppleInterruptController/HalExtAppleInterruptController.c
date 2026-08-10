/**
 * Copyright (c) 2025, NTASP authors.
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
 *     SPDX-License-Identifier: (BSD-2-Clause-Patent OR MIT) AND GPL-2.0
*/

#include <nthalext.h>
#include <intrin.h>

//
// The sample DMA controller HAL Extension disables these warnings. Original descriptions below.
//
// Disable warning C4214: nonstandard extension used : bit field types other than int
// Disable warning C4201: nonstandard extension used : nameless struct/union
// Disable warning C4115: named type definition in parentheses
// Disable warning C4127: conditional expression is constant
// Disable warning C4200: zero-sized array in struct/union
// 
// The following warnings are newly disabled in the AIC HAL Extension's current implementation.
// Disable warning C4152: non standard extension, function/data ptr conversion in expression
//
#pragma warning(disable:4214 4201 4115 4127 4200 4152)

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
// handled appropriately. HW_STATE provides a read-only view of these registers for the purpose of checking an IRQ's mask state.
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
// Helper functions for AIC during normal operation. These are what tend to drive the actual hardware
// in our implementation.
//

static inline NTSTATUS AppleInterruptControllerMaskInterrupt(PVOID InterruptControllerContext, ULONG IrqNum) {
	ULONG CpuDieOffset;
	UINT32 MaskBit;
	UINT32 MaskReg;
	P_AIC_INFO AicInfo = (P_AIC_INFO)InterruptControllerContext;
	CpuDieOffset = 0;
	MaskBit = AIC_MASK_BIT(IrqNum);
	MaskReg = AIC_MASK_REG(IrqNum);
	switch (AicInfo->AicVersion) {
	case APPLE_INTERRUPT_CONTROLLER_V1:
		WRITE_REGISTER_ULONG(AicInfo->AppleInterruptControllerBaseVirt + AicInfo->AicIrqMaskClearOffset + MaskReg, MaskBit);
		break;
	case APPLE_INTERRUPT_CONTROLLER_V2:
	case APPLE_INTERRUPT_CONTROLLER_V3:
		//
		// TODO: this. (divide by max IRQs then multiply by die stride to get the right offset for IRQs that originate on the other CPU die.)
		//
		WRITE_REGISTER_ULONG(AicInfo->AppleInterruptControllerBaseVirt + AicInfo->AicIrqMaskClearOffset + CpuDieOffset + MaskReg, MaskBit);
		break;
	}
	return STATUS_SUCCESS;
}

static inline NTSTATUS AppleInterruptControllerMarkInterruptAsPending(PVOID InterruptControllerContext, ULONG IrqNum) {
	ULONG CpuDieOffset;
	UINT32 MaskBit;
	UINT32 MaskReg;
	CpuDieOffset = 0;
	MaskBit = AIC_MASK_BIT(IrqNum);
	MaskReg = AIC_MASK_REG(IrqNum);
	P_AIC_INFO AicInfo = (P_AIC_INFO)InterruptControllerContext;
	switch (AicInfo->AicVersion) {
	case APPLE_INTERRUPT_CONTROLLER_V1:
		WRITE_REGISTER_ULONG(AicInfo->AppleInterruptControllerBaseVirt + AicInfo->AicSwIrqMaskSetOffset + MaskReg, MaskBit);
		break;
	case APPLE_INTERRUPT_CONTROLLER_V2:
	case APPLE_INTERRUPT_CONTROLLER_V3:
		//
		// TODO: this. (divide by max IRQs then multiply by die stride to get the right offset for IRQs that originate on the other CPU die.)
		//
		WRITE_REGISTER_ULONG(AicInfo->AppleInterruptControllerBaseVirt + AicInfo->AicSwIrqMaskSetOffset + CpuDieOffset + MaskReg, MaskBit);
		break;
	}
	return STATUS_SUCCESS;
}

BOOLEAN AppleInterruptControllerInterruptIsMasked(PVOID InterruptControllerContext, ULONG IrqNum) {
	ULONG CpuDieOffset;
	UINT32 MaskBit;
	UINT32 MaskReg;
	CpuDieOffset = 0;
	MaskBit = AIC_MASK_BIT(IrqNum);
	MaskReg = AIC_MASK_REG(IrqNum);
	P_AIC_INFO AicInfo = (P_AIC_INFO)InterruptControllerContext;
	ULONG IrqState;
	switch (AicInfo->AicVersion) {
	case APPLE_INTERRUPT_CONTROLLER_V1:
		IrqState = READ_REGISTER_ULONG(AicInfo->AppleInterruptControllerBaseVirt + AicInfo->AicHwStateOffset + MaskReg);
		break;
	case APPLE_INTERRUPT_CONTROLLER_V2:
	case APPLE_INTERRUPT_CONTROLLER_V3:
		//
		// TODO: this. (divide by max IRQs then multiply by die stride to get the right offset for IRQs that originate on the other CPU die.)
		//
		IrqState = READ_REGISTER_ULONG(AicInfo->AppleInterruptControllerBaseVirt + AicInfo->AicHwStateOffset + CpuDieOffset + MaskReg);
		break;
	}
	return (((IrqState) & MaskBit) != 0);
}

NTSTATUS AppleInterruptControllerUnmaskInterrupt(PVOID InterruptControllerContext, ULONG IrqNum) {
	ULONG CpuDieOffset;
	UINT32 MaskBit;
	UINT32 MaskReg;
	CpuDieOffset = 0;
	MaskBit = AIC_MASK_BIT(IrqNum);
	MaskReg = AIC_MASK_REG(IrqNum);
	P_AIC_INFO AicInfo = (P_AIC_INFO)InterruptControllerContext;
	switch (AicInfo->AicVersion) {
	case APPLE_INTERRUPT_CONTROLLER_V1:
		WRITE_REGISTER_ULONG(AicInfo->AppleInterruptControllerBaseVirt + AicInfo->AicIrqMaskClearOffset + MaskReg, MaskBit);
		break;
	case APPLE_INTERRUPT_CONTROLLER_V2:
	case APPLE_INTERRUPT_CONTROLLER_V3:
		//
		// TODO: this. (divide by max IRQs then multiply by die stride to get the right offset for IRQs that originate on the other CPU die.)
		//
		WRITE_REGISTER_ULONG(AicInfo->AppleInterruptControllerBaseVirt + AicInfo->AicIrqMaskClearOffset + CpuDieOffset + MaskReg, MaskBit);
		break;
	}
	return STATUS_SUCCESS;
}

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
	UNREFERENCED_PARAMETER(InterruptControllerContext);
	UNREFERENCED_PARAMETER(Param1);
	UNREFERENCED_PARAMETER(Param2);
	UNREFERENCED_PARAMETER(Param3);
	UNREFERENCED_PARAMETER(Param4);
	UNREFERENCED_PARAMETER(Aff0);
	return STATUS_SUCCESS;
}

BOOLEAN AppleInterruptControllerIsSpecialInterrupt(ULONG InterruptLineNumber) {
	return (InterruptLineNumber >= 0xFFFFF000);
}

//
// Description:
//   This function registers the IRQ lines for the HAL to use. Note that this also requires a function that the HAL Extensions API doesn't
//   export so we'll need to find it ourselves as well. This type of function is used by the GIC and GICv3 "InitializeIoUnit" functions.
//
// Return value:
//  an NTSTATUS code, STATUS_SUCCESS in the success case.
//
NTSTATUS AppleInterruptControllerDescribeLines(P_AIC_INFO pAicInfo) {
	UINT64 KernelExceptionHandler;
	NTSTATUS Status;
	NTSTATUS (*HalpInterruptRegisterLine)(PINTERRUPT_LINE_INITIALIZATION_BLOCK InterruptLine);
	KernelExceptionHandler = _ReadStatusReg(ARM64_SYSREG(3, 0, 12, 0, 0)); // read VBAR_EL1
	HalpInterruptRegisterLine = (PVOID)((UINT64)(KernelExceptionHandler - 0x19B930));

	//
	// The way Windows registers interrupt lines is by describing the upper/lower bounds of a range, defining what said range
	// of interrupt lines is for, then calling HalpRegisterInterruptLine. 
	// We have several types we need to register, namely the following:
	// - the IPI lines. Note that in all cases, the interrupt line number here will be a sentinel value, as both Fast IPIs
	// and AIC-based "slow" IPIs on T8103 do not use a normal IRQ number.
	// - the timer/PMU interrupts. These are FIQs on all platforms we care about, so again, sentinel values are needed (the FIQs
	// don't have a hardware number so we can use numbers >= 0xFFFFF000 but < 0xFFFFFFFF for these FIQs for example)
	// - the AIC-backed hardware interrupts for peripherals. These use distinct hardware numbers so make sure Windows's understanding
	// matches the hardware's here.
	// - PCIe MSIs. Currently not using PCIe so this will go unused, however keep this in mind as PCIe will need the MSI range registered.
	// - Windows also seems to always register IRQ line 1 as an output with output controller ID and GSI base 0xFFFFFFFF so register this too.
	// (AIC reserves this number for software, so this should be fine)
	// Given our design, lines that are used for hardware IRQs and lines used for special interrupts or software-only interrupts should
	// *never* intersect. This way, we preserve a "one interrupt number to one interrupt" relationship in the driver.
	//

	//
	// For reference, here are the mappings of the "special" interrupt line numbers to their functions.
	//
	// - 0xFFFFF001 - physical timer FIQ
	// - 0xFFFFF002 - virtual timer FIQ
	// - 0xFFFFF003 - reserved for HV view of phys timer as Linux does (this shouldn't be necessary as Windows kernel never runs in EL2)
	// - 0xFFFFF004 - reserved for HV view of virtual timer (again, shouldn't be necessary, but reserving it nonetheless)
	// - 0xFFFFF005 - Apple PMU (E-core)
	// - 0xFFFFF006 - Apple PMU (P-core)
	// - 0xFFFFF007 - Fast IPIs (on AICv1, this sentinel number will also be used for the AIC-based "slow" IPIs to keep implementation simple)
	// - 0xFFFFF008-0xFFFFFE00 - reserved.
	//

	//
	// Step 1: Register the Windows-specific output line.
	//

	INTERRUPT_LINE_INITIALIZATION_BLOCK IrqLineInitBlock = { 0 };
	IrqLineInitBlock.Type = InterruptLineOutputPin;
	IrqLineInitBlock.SubType = InterruptLineSubTypeNone;
	IrqLineInitBlock.ControllerIdForOutput = 0xFFFFFFFF; // Windows needs this for some reason, every valid ARM64 IRQ controller sets this.
	IrqLineInitBlock.GsiBase = 0xFFFFFFFF; // ditto the above. (This seems to be a sentinel that says "this is not linked to the GSI")
	IrqLineInitBlock.MinLine = 1;
	IrqLineInitBlock.MaxLine = 2;
	// This is *probably* safe - we are only registering one AIC here in all scenarios at the moment. 
	// If we do register other controllers, this will have to change.
	IrqLineInitBlock.ControllerId = 0;
	IrqLineInitBlock.MsiAddress = 0;
	IrqLineInitBlock.MsiData = 0;
	Status = HalpInterruptRegisterLine(&IrqLineInitBlock);
	if (Status != STATUS_SUCCESS) {
		return Status;
	}

	//
	// Step 2: Register the normal hardware IRQs.
	//
	IrqLineInitBlock.Type = InterruptLineStandardPin;
	IrqLineInitBlock.SubType = InterruptLineSubTypeNone;
	IrqLineInitBlock.ControllerIdForOutput = 0;
	IrqLineInitBlock.GsiBase = 4; // ditto the above.
	IrqLineInitBlock.MinLine = 4; // 4 is a good baseline for normal hardware IRQs
	IrqLineInitBlock.MaxLine = pAicInfo->AicNumIrqs + 1;
	// This is *probably* safe - we are only registering one AIC here in all scenarios at the moment. 
	// If we do register other controllers, this will have to change.
	IrqLineInitBlock.ControllerId = 0;
	IrqLineInitBlock.MsiAddress = 0;
	IrqLineInitBlock.MsiData = 0;
	Status = HalpInterruptRegisterLine(&IrqLineInitBlock);
	if (Status != STATUS_SUCCESS) {
		return Status;
	}

	//
	// Step 3: Register the peripheral FIQs.
	//
	IrqLineInitBlock.Type = InterruptLineProcessorLocal;
	IrqLineInitBlock.SubType = InterruptLineSubTypeNone;
	IrqLineInitBlock.ControllerIdForOutput = 0;
	//
	// since this doesn't line up with normal AIC numbering, we should probably use the sentinel for "not mapped to system interrupt numbers"
	//
	IrqLineInitBlock.GsiBase = 0xFFFFFFFF;
	IrqLineInitBlock.MinLine = 0xFFFFF001; // 0xFFFFF001 is our timer FIQ number.
	IrqLineInitBlock.MaxLine = 0xFFFFF006 + 1; // this is just for representational purposes, to indicate that 0xFFFFF006 is the last line being registered (Windows wants MaxLine to be last line number + 1)
	// This is *probably* safe - we are only registering one AIC here in all scenarios at the moment. 
	// If we do register other controllers, this will have to change.
	IrqLineInitBlock.ControllerId = 0;
	IrqLineInitBlock.MsiAddress = 0;
	IrqLineInitBlock.MsiData = 0;
	Status = HalpInterruptRegisterLine(&IrqLineInitBlock);
	if (Status != STATUS_SUCCESS) {
		return Status;
	}

	//
	// Step 4: Register the IPIs.
	//
	IrqLineInitBlock.Type = InterruptLineSoftwareOnlyProcessorLocal;
	IrqLineInitBlock.SubType = InterruptLineSubTypeNone;
	IrqLineInitBlock.ControllerIdForOutput = 0;
	//
	// since this doesn't line up with normal AIC numbering, we should probably use the sentinel for "not mapped to system interrupt numbers"
	//
	IrqLineInitBlock.GsiBase = 0xFFFFFFFF;
	IrqLineInitBlock.MinLine = 0xFFFFF007; // 0xFFFFF007 is our IPI number.
	IrqLineInitBlock.MaxLine = 0xFFFFF007 + 1; // this is just for representational purposes, to indicate that 0xFFFFF007 is the last line being registered (Windows wants MaxLine to be last line number + 1)
	// This is *probably* safe - we are only registering one AIC here in all scenarios at the moment. 
	// If we do register other controllers, this will have to change.
	IrqLineInitBlock.ControllerId = 0;
	IrqLineInitBlock.MsiAddress = 0;
	IrqLineInitBlock.MsiData = 0;
	Status = HalpInterruptRegisterLine(&IrqLineInitBlock);
	return Status;
}

NTSTATUS AppleInterruptControllerEnsureIoUnitMapped(P_AIC_INFO pAicInfo) {
	//
	// Check if the virtual address entry for the AIC is NULL or not.
	//
	NTSTATUS Status = STATUS_SUCCESS;
	if (pAicInfo->AppleInterruptControllerBaseVirt == NULL) {
		//
		// Attempt to map the AIC into virtual address space if the entry is NULL.
		//
		pAicInfo->AppleInterruptControllerBaseVirt = HalMapIoSpace(pAicInfo->AppleInterruptControllerBasePhys, pAicInfo->AppleInterruptControllerSize, MmNonCached);
		if (pAicInfo->AppleInterruptControllerBaseVirt == NULL) {
			//
			// we don't have any virtual memory left over for the allocation, fail appropriately.
			//
			Status = STATUS_INSUFFICIENT_RESOURCES;
		}
	}
	return Status;
}

NTSTATUS AppleInterruptControllerInitializeIoUnit(PVOID InterruptControllerContext) {
	//
	// The GICv2 driver seems to register IO units for every GIC device, but the GICv3 and BC2836 drivers in the HAL only register the single
	// IO unit.
	// In our case, we should only have the one IO unit, so we would probably follow the GICv3/BC2836 case here.
	//
	P_AIC_INFO AicInfo = (P_AIC_INFO)InterruptControllerContext;
	UINT32 AicV2Config;
	BOOLEAN IsAicEnabled;
	NTSTATUS Status;

	//
	// Make sure the AIC is mapped in virtual address space.
	//
	Status = AppleInterruptControllerEnsureIoUnitMapped(AicInfo);

	if (Status != STATUS_SUCCESS) {
		//
		// do not proceed further if we can't map the AIC.
		//
		DbgPrint("AppleInterruptControllerInitializeIoUnit: [ERR] AIC is not mapped!\n");
		return Status;
	}

	if (AicInfo->AicInitialized != FALSE) {
		//
		// do not attempt to initialize twice.
		//
		DbgPrint("AppleInterruptControllerInitializeIoUnit: [INF] AIC is already initialized.\n");
		return STATUS_SUCCESS;
	}

	//
	// Check if AIC was inadvertently left enabled from UEFI (AICv2 only, AICv1 won't have this bit) and if it was,
	// disable it for now.
	// (We require this check because we enable AIC in our UEFI just in case we need it.)
	// (This should not be the case since we disable AIC in our ExitBootServices callback)
	//
	if ((AicInfo->AicVersion >= APPLE_INTERRUPT_CONTROLLER_V2)) {
		AicV2Config = READ_REGISTER_ULONG(AicInfo->AppleInterruptControllerBaseVirt + AIC_V2_CONFIG);
		IsAicEnabled = ((AicV2Config) & (AIC_V2_CFG_ENABLE)) != 0;
		if (IsAicEnabled) {
			AicV2Config &= ~(AIC_V2_CFG_ENABLE);
			_DataSynchronizationBarrier(); // "dsb sy"
			MemoryBarrier(); // this is a "dmb"
			_InstructionSynchronizationBarrier(); // "isb sy"
			WRITE_REGISTER_ULONG(AicInfo->AppleInterruptControllerBaseVirt + AIC_V2_CONFIG, AicV2Config);
			_InstructionSynchronizationBarrier(); // "isb sy"
			_DataSynchronizationBarrier(); // "dsb sy"
		}
	}

	//
	// Mask all the IRQs.
	//
	for (ULONG InterruptNum = 0; InterruptNum < AicInfo->AicNumIrqs; InterruptNum++) {
		AppleInterruptControllerMaskInterrupt(InterruptControllerContext, InterruptNum);
	}

	//
	// Register the IRQ lines being used with the HAL.
	//
	Status = AppleInterruptControllerDescribeLines(AicInfo);

	if (Status != STATUS_SUCCESS) {
		DbgPrint("AppleInterruptControllerInitializeIoUnit: [ERR] Failed to register IRQ lines!\n");
		return Status;
	}

	//
	// Initialization is done, mark ourselves as initialized in the AicInfo structure.
	//
	AicInfo->AicInitialized = TRUE;
	//
	// For AICv2 platforms, (re-)enable the AIC.
	//
	if ((AicInfo->AicVersion >= APPLE_INTERRUPT_CONTROLLER_V2)) {
		AicV2Config = READ_REGISTER_ULONG(AicInfo->AppleInterruptControllerBaseVirt + AIC_V2_CONFIG);
		IsAicEnabled = ((AicV2Config) & (AIC_V2_CFG_ENABLE)) != 0;
		if (!IsAicEnabled) {
			AicV2Config |= (AIC_V2_CFG_ENABLE);
			_DataSynchronizationBarrier(); // "dsb sy"
			MemoryBarrier(); // this is a "dmb"
			_InstructionSynchronizationBarrier(); // "isb sy"
			WRITE_REGISTER_ULONG(AicInfo->AppleInterruptControllerBaseVirt + AIC_V2_CONFIG, AicV2Config);
			_InstructionSynchronizationBarrier(); // "isb sy"
			_DataSynchronizationBarrier(); // "dsb sy"
		}
	}
	return STATUS_SUCCESS;
}

VOID AppleInterruptControllerSetPriority(PVOID InterruptControllerContext, ULONG Priority) {
	//
	// AIC does not permit us any control over IRQ priority in any version, 
	// lower IRQs are treated as higher priority always. (per the Asahi Linux documentation of the driver in linux tree)
	// This function will probably be NULLed out at some point.
	// Note that due to being FIQs, per-core interrupts such as IPIs or timer interrupts have higher priority than even AIC interrupts.
	// An alternate approach we can take is that we can generate a priority list such that lower numbered IRQs are higher priority.
	// (BCM2836 has a priority list it maintains for local and global IRQs, we can do similar)
	//
	UNREFERENCED_PARAMETER(InterruptControllerContext);
	UNREFERENCED_PARAMETER(Priority);
	return;
}

VOID AppleInterruptControllerClearLocalUnitError(PVOID InterruptControllerContext) {
	//
	// AIC technically has no conception of a "local unit" (per-core MMIO), all the per-core interrupts
	// on Apple platforms come as direct FIQs from the core itself.
	// Regardless, this function won't be NULLed just yet.
	//
	UNREFERENCED_PARAMETER(InterruptControllerContext);
	return;
}

//
// These two functions are unimplemented by any of the ARM64-supported interrupt controllers,
// so these might also get NULLed out (unless AIC needs these?)
//
NTSTATUS AppleInterruptControllerGetLogicalId(PVOID InterruptControllerContext, INTERRUPT_TARGET InterruptTarget) {
	UNREFERENCED_PARAMETER(InterruptControllerContext);
	UNREFERENCED_PARAMETER(InterruptTarget);
	return STATUS_SUCCESS;
}

NTSTATUS AppleInterruptControllerSetLogicalId(PVOID InterruptControllerContext, INTERRUPT_TARGET InterruptTarget) {
	UNREFERENCED_PARAMETER(InterruptControllerContext);
	UNREFERENCED_PARAMETER(InterruptTarget);
	return STATUS_SUCCESS;
}

INTERRUPT_RESULT AppleInterruptControllerAcceptAndGetSource(PVOID InterruptControllerContext, PLONG IrqId, PULONG IrqEventValue) {
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
	UNREFERENCED_PARAMETER(IrqNum);
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

NTSTATUS AppleInterruptControllerSetLineState(PVOID InterruptControllerContext, INTERRUPT_LINE* IrqLine, INTERRUPT_LINE_STATE* IrqLineState) {
	UNREFERENCED_PARAMETER(InterruptControllerContext);
	UNREFERENCED_PARAMETER(IrqLine);
	UNREFERENCED_PARAMETER(IrqLineState);
	return STATUS_SUCCESS;
}

NTSTATUS AppleInterruptControllerRequestInterrupt(PVOID InterruptControllerContext, INTERRUPT_LINE *IrqLine, INTERRUPT_TARGET *IrqTarget, ULONG Param4, INTERRUPT_LINE *IrqLine2) {
	//
	// The request interrupt function is what handles actually configuring the interrupt hardware to unmask and mask interrupts, 
	// including using IPIs to signal other processors.
	// TODO: Fast IPI and AIC-backed IPI support. (While we are going to use Fast IPIs on supported hardware, having AIC-backed IPI
	// support lets us support older hardware in the future that did not have this capability.)
	//

	//
	// The GICv3 Request Interrupt function does the following for reference:
	// - if it's requesting a SPI (that's not an extended SPI) it sets the bit for the interrupt in GICD_ISPENDR<x> depending on SPI number.
	// (per the GIC state machine, this marks an interrupt as pending )
	// - if it's requesting an extended SPI, it sets the bit for the interrupt in GICD_ISPENDR<x>E, depending on the SPI number
	// - If it's an LPI, either sends an ITS command if that's used, otherwise directly signals the redistributor by writing GICR_SETLPIR
	// - if it's an SGI (an IPI in other words), it writes ICC_SGI1R_EL1 with the right value to the right target dependent on IrqTarget/
	// 
	// The general gist here is that through whatever mechanism it has, the GICv3 state machine is transitioning an interrupt from inactive
	// (not being asserted) to pending (asserted, might not be serviced immediately due to priority, and unacknowledged).
	// 
	// For AIC, we have two paths to account for in this regard.
	// - For IRQs, the AIC has a mechanism for software to assert an interrupt (the SW_SET/SW_CLR registers), so this slots in pretty well for
	// most interrupts.
	// - For FIQs and special interrupts, we'll need to take special paths depending on the interrupt. (The timer is handled by the Windows HAL itself
	// due to being register-compatible with the ARM64 generic timer so again we do not need to handle it here.) IPIs seem to be the main concern here.
	//
	P_AIC_INFO AicInfo = (P_AIC_INFO)InterruptControllerContext;
	UINT32 Line = (UINT32)IrqLine->Line;
	BOOLEAN IsSpecialInterrupt = AppleInterruptControllerIsSpecialInterrupt(Line);
	BOOLEAN IsIpi = TRUE; // assume that if we are a special interrupt, we are an IPI to start out with.
	ULONG CurrentProcessorNumber = KeGetCurrentProcessorNumberEx(NULL);
	ULONGLONG CurrentProcessorMpidr = AicInfo->Mpidrs[CurrentProcessorNumber];

	UNREFERENCED_PARAMETER(IrqLine);
	UNREFERENCED_PARAMETER(Param4);
	UNREFERENCED_PARAMETER(IrqLine2);
	if (IsSpecialInterrupt == TRUE) {
		//
		// We are requesting an FIQ or IPI. Handle multiple cases here.
		// NOTE: we do *not* handle the ARM64 timer, Windows manages that itself, so basically we just manage PMC counters and IPIs.
		// (PMC counters not implemented yet as right now we are using emulation of normal PMUv3 registers via m1n1, 
		// PMUv3 counters are handled by Windows itself similar to timers.)
		//
		if (IsIpi) {
			//
			// AIC IPIs, how they work, and how our driver differs from the Linux driver:
			// - AIC-based devices have two known ways to receive and generate IPIs: "slow" IPIs configured via registers on AIC,
			// and "fast" IPIs configured via MSRs on the core. (Additionally, since the M1, there's a new extension which allows even faster IPIs via MSRs within the same cluster)
			// - If using AIC-backed IPIs, the requesting CPU writes to a register (Asahi Linux calls this IPI_SEND, on AICv1 offset is 0x2008) where the bit(s) written
			// affect(s) which cores receive an IPI. Writing bits [30:0] will send an IPI to that CPU index as an "other" IPI, while writing bit 31 sends an IPI to the current CPU as a "self" IPI.
			// - The Linux driver only uses one of the IPI vectors (the "other" vector) and has a virtual IPI controller in front of the physical hardware.
			// We will *not* be using this approach, currently the goal is to see if we can naively use the primitives Apple uses.
			// - If using Fast IPIs, you target a CPU based on it's MPIDR CPU/cluster value (or just CPU if you're targeting in the same cluster) and write that to the system register.
			// (In the Fast IPI case, targeting "self" is targeting an IPI against your own MPIDR value for core/cluster)
			// 
			// We are not going to be using a vIPI approach, and instead relying on Apple's own primitives being sufficient for now. We will support targeting
			// self only, all including/excluding self, and a physical CPU (we are not using logical flat or clustered modes, 
			// those depend on local unit support which we are not assuming right now.)
			//

			//
			// TODO: This switch statement.
			//

			//
			// TODO: slow IPI support
			//

			if (AicInfo->AicUseFastIpis) {
				switch (IrqTarget->Target) {
				case InterruptTargetSelfOnly:
					//
					// For Fast IPIs, addressing "self" means writing the right CPU number value to the MPIDR register relative to the current cluster.
					// 
					//
					_WriteStatusReg(ARM64_SYSREG(3, 5, 15, 0, 0), FIELD_PREP(IPI_RR_CPU, MPIDR_AFF0(CurrentProcessorMpidr)));
					break;

				case InterruptTargetAllExcludingSelf:
				case InterruptTargetAllIncludingSelf:
				case InterruptTargetPhysical:
				default:
					ASSERTMSG("Unimplemented Interrupt target!", FALSE);
				}
			}
			else {
				ASSERTMSG("Only Fast IPIs are currently supported in the driver!", FALSE);
			}
		}



	}
	else {
		//
		// We are requesting a normal AIC IRQ, take the line number and write the corresponding SW_SET bit.
		//
	}

	//
	// Right now we're a stub, but the control flow should be something like this:
	// - Check if we're requesting an FIQ-backed interrupt or an IRQ-backed interrupt
	// - In the FIQ-backed case, see if it's requesting for an IPI in particular (we'll probably need to use sentinel values here for all FIQs.)
	// - In the FIQ-backed case, follow the specific behavior for whatever FIQ is being requested.
	// - In the IRQ-backed case, unmask the IRQ.
	//
	return STATUS_SUCCESS;
}

NTSTATUS AppleInterruptControllerStartProcessor(PVOID InterruptControllerContext, ULONG Param1, PVOID Param2, ULONG Param3) {
	UNREFERENCED_PARAMETER(InterruptControllerContext);
	UNREFERENCED_PARAMETER(Param1);
	UNREFERENCED_PARAMETER(Param2);
	UNREFERENCED_PARAMETER(Param3);
	return STATUS_SUCCESS;
}

NTSTATUS AppleInterruptControllerGenerateMessage(PVOID InterruptControllerContext, INTERRUPT_LINE_STATE* IrqLineState, PUINT64 Param2, PUINT64 Param3) {
	UNREFERENCED_PARAMETER(InterruptControllerContext);
	UNREFERENCED_PARAMETER(IrqLineState);
	UNREFERENCED_PARAMETER(Param2);
	UNREFERENCED_PARAMETER(Param3);
	return STATUS_SUCCESS;
}

NTSTATUS AppleInterruptControllerConvertId(PVOID InterruptControllerContext, PULONG Param1, INTERRUPT_TARGET* IrqTarget, UINT8 Param3) {
	UNREFERENCED_PARAMETER(InterruptControllerContext);
	UNREFERENCED_PARAMETER(Param1);
	UNREFERENCED_PARAMETER(IrqTarget);
	UNREFERENCED_PARAMETER(Param3);
	return STATUS_SUCCESS;
}

NTSTATUS AppleInterruptControllerSaveLocalInterrupts(PVOID InterruptControllerContext, PVOID Param1) {
	UNREFERENCED_PARAMETER(InterruptControllerContext);
	UNREFERENCED_PARAMETER(Param1);
	return STATUS_SUCCESS;
}

NTSTATUS AppleInterruptControllerReplayLocalInterrupts(PVOID InterruptControllerContext, PVOID Param1) {
	//
	// None of the supported IRQ controllers for ARM64 implement the ReplayLocalInterrupts function.
	// Almost definitely behavior that's mostly for x86/AMD64 APIC or something else for non-ARM platforms.
	// This will be NULLed out later.
	//
	UNREFERENCED_PARAMETER(InterruptControllerContext);
	UNREFERENCED_PARAMETER(Param1);
	return STATUS_SUCCESS;
}

NTSTATUS AppleInterruptControllerDeinitializeLocalUnit(PVOID InterruptControllerContext) {
	//
	// As before, AIC devices technically do not have a conception of a local, per-core unit
	// as those instead are done via per-core FIQs.
	//
	UNREFERENCED_PARAMETER(InterruptControllerContext);
	return STATUS_SUCCESS;
}

NTSTATUS AppleInterruptControllerDeinitializeIoUnit(PVOID InterruptControllerContext) {
	//
	// What needs to be done here:
	// - Mask all pending IRQs, and signal EOI on any pending interrupts.
	// - Mask all FIQs.
	// - On AICv2, turn off the AIC itself to disable it sending interrupts (AICv1 does not have an off switch as such, masking all IRQs is the best we can do there.)
	//
	UNREFERENCED_PARAMETER(InterruptControllerContext);
	return STATUS_SUCCESS;
}

//_INTERRUPT_RESULT AppleInterruptControllerQueryAndGetSource(PVOID InterruptControllerInternalData, PINT32 IrqId, PULONG Param2, PUINT8 Param3) {
//	//
//	// This seems to be to detect if something is a line based or vector based IRQ?
//	//
//	return InterruptResultNone;
//}

VOID AppleInterruptControllerDeactivateInterrupt(PVOID InterruptControllerContext, ULONG IrqNum) {
	//
	// "Deactivate" here means that the interrupt is acknowledged such that it can be taken
	// again. Since in AIC, deactivate is the same operation as end of interrupt (equivalent to GICv3 EOImode = 0),
	// and we already handle EndOfInterrupt in a separate function (which comes first), we should be fine keeping this as a stub.
	//
	UNREFERENCED_PARAMETER(InterruptControllerContext);
	UNREFERENCED_PARAMETER(IrqNum);
}

VOID AppleInterruptControllerDirectedEndOfInterrupt(PVOID InterruptControllerContext, ULONG Param1, ULONG Param2) {
	//
	// None of the ARM64-supported IRQ controllers in the HAL implement DirectedEndOfInterrupt, so this will probably get NULLed out at some point.
	//
	UNREFERENCED_PARAMETER(InterruptControllerContext);
	UNREFERENCED_PARAMETER(Param1);
	UNREFERENCED_PARAMETER(Param2);
	return;
}

NTSTATUS AppleInterruptControllerQueryLocalUnitInfo(PVOID InterruptControllerContext, ULONG Param1, PULONG Param2, PULONG Param3, KINTERRUPT_MODE* Param4, KINTERRUPT_MODE* Param5) {
	UNREFERENCED_PARAMETER(InterruptControllerContext);
	UNREFERENCED_PARAMETER(Param1);
	UNREFERENCED_PARAMETER(Param2);
	UNREFERENCED_PARAMETER(Param3);
	UNREFERENCED_PARAMETER(Param4);
	UNREFERENCED_PARAMETER(Param5);
	return STATUS_SUCCESS;
}

NTSTATUS AppleInterruptControllerQueryPendingState(PVOID InterruptControllerContext, INTERRUPT_LINE* IrqLine, PUINT8 Param2, PUINT8 Param3) {
	UNREFERENCED_PARAMETER(InterruptControllerContext);
	UNREFERENCED_PARAMETER(IrqLine);
	UNREFERENCED_PARAMETER(Param2);
	UNREFERENCED_PARAMETER(Param3);
	return STATUS_SUCCESS;
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
	NULL,
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
NTSTATUS AppleInterruptControllerRegisterIoUnit(ULONG Handle, PCSRT_RESOURCE_DESCRIPTOR_HEADER CsrtResourceDescriptor) {

	//
	// TODO: literally everything, including the following:
	// - get the AIC version and base address via CSRT (Num/Max IRQs are easier to get in-driver)
	// - mask all interrupts based on this information.
	// - store the function pointer to HalpInterruptRegisterController, then call it.
	//
	UINT64 KernelExceptionHandler;
	NTSTATUS Status;
	volatile PULONG AicVirtualAddress;
	RD_INTERRUPT_CONTROLLER* CsrtAicData = (RD_INTERRUPT_CONTROLLER*)CsrtResourceDescriptor;

	//
	// Read the MADT ACPI table
	//
	PMAPIC MadtTable;
	MadtTable = GetAcpiTable(Handle, MADT_SIGNATURE, NULL, NULL);
	PPROCLOCALGIC GicLocalInformation = (PPROCLOCALGIC)MadtTable->APICTables;
		
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
	AicVirtualAddress = HalMapIoSpace(AicInfo.AppleInterruptControllerBasePhys, CsrtAicData->ControllerVendorData.ControllerBaseSize, MmNonCached);
	if (AicVirtualAddress == NULL) {
		//
		// we can't continue if we can't map the AIC.
		//
		ASSERTMSG("AppleInterruptControllerRegisterIoUnit: Failed to map AIC to virtual address!", FALSE);
	}

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
		ASSERTMSG("AppleInterruptControllerRegisterIoUnit: Failed to get number of AIC interrupts!", FALSE);
	}

	HalUnmapIoSpace((PVOID)AicVirtualAddress, CsrtAicData->ControllerVendorData.ControllerBaseSize);

	//
	// read VBAR_EL1 which contains the kernel exception handlers. For now (this will
	// only work on 26100.1 until we develop better patch-find routines)
	// apply the offset to get to HalpInterruptRegisterController
	//
	KernelExceptionHandler = _ReadStatusReg(ARM64_SYSREG(3, 0, 12, 0, 0)); // read VBAR_EL1
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
	// The firmware vendor data will carry a boolean indicating whether Fast IPIs are supported on the platform.
	//
	AicInfo.AicUseFastIpis = CsrtAicData->ControllerVendorData.FastIpisSupported;

	//
	// HACK: currently to keep our config sane and to get it to build, limit the MPIDR accesses to lowest
	// common number of cores on all M-series chips (8 cores)
	for (ULONG Index = 0; Index < 8; Index++) {
		AicInfo.Mpidrs[Index] = GicLocalInformation[Index].Mpidr;
	}

	//
	// Mark our interrupt controller type as unknown. 
	// (if we do need to fake a controller, fake the GICv2 controller, but unknown helps us dodge some GIC quirks in the kernel.)
	//
	AicInitBlock.KnownType = InterruptControllerUnknown;

	AicInitBlock.FunctionTable = gAicFunctionTable;

	//
	// Capabilities wise, initially we're using the following:
	// - IRQ controller does not have local per-processor units (bit 0 clear)
	// - IRQ controller does *not* support interrupt priorities (bit 1 clear)
	// (this fact is pretty definite, but we might want to fake priorities later on using a in-built list, so this could be set later)
	// - IRQ controller as a consequence of no local units, does not support logical flat or clustered modes (bit 2/3 clear)
	// - we will set bits [6:4] for now to have the same IPI behavior as GIC does. (AIC supports those by equivalence)
	// - IRQ controller can mask IRQs without needing to disable interrupts. (bit 9 clear)
	// (Capability bitmask = 0x70 for now for this)
	//
	AicInitBlock.Capabilities = (INTERRUPT_CONTROLLER_IPI_CONTROL_MASK);

	AicInfo.HalExtHandle = Handle;

	//
	// Register the AIC MMIO addresses with the HAL.
	//
	HalRegisterPermanentAddressUsage(AicInfo.AppleInterruptControllerBasePhys, 0xC000);

	//
	// Call HalpInterruptRegisterController to register the controller.
	// The entry should find that function pointer (right now this only works on 26100.1)
	//
	Status = HalpInterruptRegisterController(&AicInitBlock, 0, NULL);
	ASSERTMSG("AIC initialization failed!", Status == STATUS_SUCCESS);

	return Status;
}

//
// Description:
//   This routine registers all of the CSRT resource descriptors in a resource group 
//   with the HAL to allow the usage of the associated hardware. Of note is that this is the only function
//   a HAL Extension must export to the kernel to be seen as valid.
// 
// Return value:
//   - STATUS_SUCCESS if resource descriptor registration succeeds, 
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

	if (CsrtResourceDescriptor == NULL) {
		ASSERTMSG("AddResourceGroup: CSRT resource descriptor for AIC is NULL!", FALSE);
	}

	return AppleInterruptControllerRegisterIoUnit(Handle, CsrtResourceDescriptor);
}