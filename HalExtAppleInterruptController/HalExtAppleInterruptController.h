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
// Macros used for convenience purposes, such as generating bitmasks or toggling bits on and off.
//
// Borrowed from m1n1
//
#define BIT(x) (1UL << (x))
#define GENMASK(msb, lsb) ((BIT((msb + 1) - (lsb)) - 1) << (lsb))

#define _FIELD_LSB(field)      ((field) & ~(field - 1))

#define FIELD_PREP(field, val) ((val) * (_FIELD_LSB(field)))
#define FIELD_GET(field, val)  (((val) & (field)) / _FIELD_LSB(field))

#define DIV_ROUND_UP(n,d) (((n) + (d) - 1) / (d))

#define ALIGN(x,a)		__ALIGN_MASK((x),(typeof(x))(a)-1)
#define ALIGN_DOWN(x, a)	ALIGN((x) - ((a) - 1), (a))
#define __ALIGN_MASK(x,mask)	(((x)+(mask))&~(mask))
#define PTR_ALIGN(p, a)		((typeof(p))ALIGN((unsigned long)(p), (a)))


//
// AIC version enum, used to track the version of AIC on the current platform.
//

typedef enum {
    APPLE_INTERRUPT_CONTROLLER_V1 = 1,
    APPLE_INTERRUPT_CONTROLLER_V2 = 2,
    APPLE_INTERRUPT_CONTROLLER_V3 = 3,
    APPLE_INTERRUPT_CONTROLLER_VER_UNKNOWN = 0xFFFF
} APPLE_INTERRUPT_CONTROLLER_VERSION;

//
// AIC defines that apply to all versions.
//

#define AIC_MASK_REG(num) (4 * ((num) >> 5))
#define AIC_MASK_BIT(num) BIT(num) & GENMASK(4, 0)

#define AIC_NUM_IRQ_MASK GENMASK(15, 0)

// AIC Event Types
// which CPU die did this occur on?
#define AIC_EVENT_NUM_DIE GENMASK(31, 24)
// are we an FIQ, IRQ, or IPI?
#define AIC_EVENT_INTERRUPT_TYPE GENMASK(23, 16)
// Interrupt number
#define AIC_EVENT_IRQ_NUM GENMASK(15, 0)

// IRQ Mask macros

#define AIC_MASK_REG(num) (4 * ((num) >> 5))
#define AIC_MASK_BIT(num) BIT(num) & GENMASK(4, 0)


//
// AICv1 defines.
// 

#define AIC_V1_MAX_IRQ	0x400
#define AIC_TARGET_CPU 0x3000
#define AIC_V1_HW_INFO 0x0004
// AIC_WHOAMI in m1n1/Linux sources
// on AICv1, used in CPU affinity modifications
#define AIC_V1_CPU_IDENTIFIER_REG 0x2000
#define AIC_V1_EVENT_REG 0x2004
#define AIC_V1_SEND_IPI_REG 0x2008
#define AIC_V1_ACKNOWLEDGE_IPI_REG 0x200c
// mask/clear IPIs
#define AIC_V1_SET_IPI_MASK_REG 0x2024
#define AIC_V1_CLEAR_IPI_MASK_REG 0x2028

//
// AICv2 defines.
//

#define AIC_V2_INFO_REG1 0x0004
#define AIC_V2_INFO_REG2 0x0008
#define AIC_V2_INFO_REG3 0x000c
//
// Writing bit 0 to AICv2 will reset it and it's configuration, with bit 25 set by default.
//
#define AIC_V2_RESET_REG 0x0010
#define AIC_V2_TRIGGER_RESET BIT(0)
//
// AIC_V2_CONFIG bit descriptions (to the best of my ability):
// only bit 0 and bits [31:25] can actually be written to (at least on M2 Pro)
// The following bits are known:
// - bit 28 - prefer P-cores for IRQs
// - bit 0 - enable the IRQ controller
// The following bits are at the moment unknown, but should be tested soon:
// [31:29], [27:25]
//
#define AIC_V2_CONFIG 0x0014
#define AIC_V2_CFG_ENABLE BIT(0)
#define AIC_V2_CFG_PREFER_P_CORE_IRQ BIT(28)

// only the lower 3 bits are writable for this register, it defaults to 0x6
#define AIC_V2_UNKNOWN_0 0x0018

// the lower 13 bits of this register are writable, defaults to 0x64

#define AIC_V2_UNKNOWN_1 0x001C

//
// bits [3:0] control the target of IRQs (0 is AP cores, others as far as I can tell actually go to ASCs so not useful
// for this driver.)
//
#define AIC_V2_IRQ_CFG_REG 0x2000

#define AIC_V2_NUM_AND_MAX_IRQS_MASK GENMASK(15, 0)
#define AIC_V2_INFO_REG3_MAX_DIE_COUNT_BITFIELD GENMASK(27, 24)
#define AIC_V2_INFO_REG1_LAST_CPU_DIE_BITFIELD GENMASK(27, 24)

//
// AIC controller general structure.
//

typedef struct _AIC_INFO {
	//
	// AIC base address. (Do we need a physical and virtual view? If we do, then this definition is the physical
	// and should have PHYSICAL_ADDRESS type, the other will use a PUINT32 type, since AIC uses 32 bit MMIO accesses.)
	//
	UINT64 AppleInterruptControllerBase;

	//
	// AIC version. This is passed in by CSRT.
	//
	APPLE_INTERRUPT_CONTROLLER_VERSION AicVersion;

	//
	// Event register offset from AIC base.
	//
	UINT32 EventRegisterOffset;

	//
	// Number of IRQs implemented on the current platform.
	//
	UINT32 AicNumIrqs;

	//
	// Maximum number of IRQs supported by the current SoC/family. (For AICv1, this is a fixed number, while
	// for AICv2 and AICv3, this needs to be determined by reading AIC info registers.)
	//
	UINT32 AicMaxIrqs;

	//
	// Offsets to the IRQ mask set and IRQ mask clear registers. On AICv2 and v3, these have to be calculated based on the maximum
	// IRQs supported on the SoC/family.
	//
	UINT32 AicIrqMaskSetOffset;
	UINT32 AicIrqMaskClearOffset;

	//
	//	Offsets to software-defined IRQ mask set/clear registers. The calculation is similar to the above for AICv2 and v3.
	//
	UINT32 AicSwIrqMaskSetOffset;
	UINT32 AicSwIrqMaskClearOffset;

	//
	// Offset to HW state registers.
	//


} AIC_INFO, *P_AIC_INFO;

//
// NT HAL specific typedefs, structs, and macros
//

//
// All of these definitions come from the type information that's put into the 26100 kernel public PDB symbols shipped publicly by Microsoft.
//

enum _INTERRUPT_TARGET_TYPE {
	InterruptTargetInvalid,
	InterruptTargetAllIncludingSelf,
	InterruptTargetAllExcludingSelf,
	InterruptTargetSelfOnly,
	InterruptTargetPhysical,
	InterruptTargetLogicalFlat,
	InterruptTargetLogicalClustered,
	InterruptTargetRemapIndex,
	InterruptTargetHypervisor,
};

enum _INTERRUPT_RESULT {
	InterruptBeginFatalError,
	InterruptBeginLine,
	InterruptBeginSpurious,
	InterruptBeginVector,
	InterruptBeginNone,
};

enum _KINTERRUPT_PRIORITY {
	InterruptPolarityUnknown = 0,
	InterruptActiveHigh = 1,
	InterruptRisingEdge = 1,
	InterruptActiveLow = 2,
	InterruptFallingEdge = 2,
	InterruptActiveBoth = 3,
	InterruptActiveBothTriggerLow = 3,
	InterruptActiveBothTriggerHigh = 4,
};

enum _KINTERRUPT_MODE {
	LevelSensitive,
	Latched
};

struct _INTERRUPT_TARGET {
	_INTERRUPT_TARGET_TYPE Target;
};

struct _INTERRUPT_LINE {
	UINT32 UnitId;
	INT32 Line;
};

struct _INTERRUPT_LINE_STATE {
	_KINTERRUPT_POLARITY Polarity;
	UINT8 EmulateActiveBoth;
	_KINTERRUPT_MODE TriggerMode;
	UINT32 Flags;
	_INTERRUPT_LINE Routing;
	_INTERRUPT_TARGET ProcessorTarget;
	UINT32 Vector;
	UINT32 Priority;
};

enum _KNOWN_CONTROLLER_TYPE {
	InterruptControllerInvalid,
	InterruptControllerPic,
	InterruptControllerApic,
	InterruptControllerGic,
	InterruptControllerGicV3,
	InterruptControllerGicV4,
	InterruptControllerBcm,
	InterruptControllerUnknown = 0x1000,
};

//
// as of Germanium (26100), this is the function table used for IRQ chips registered to the HAL.
//

typedef struct _INTERRUPT_FUNCTION_TABLE {
	_IRQL_requires_same_ _IRQL_requires_max_(HIGH_LEVEL) NTSTATUS(*InitializeLocalUnit)(PVOID, UINT32, UINT32, UINT32, UINT32, PUINT32); //0x0
	_IRQL_requires_same_ _IRQL_requires_max_(HIGH_LEVEL) NTSTATUS(*InitializeIoUnit)(PVOID); // 0x8
	_IRQL_requires_same_ _IRQL_requires_max_(HIGH_LEVEL) VOID(*SetPriority)(PVOID, UINT32); // 0x10
	_IRQL_requires_same_ _IRQL_requires_max_(HIGH_LEVEL) NTSTATUS(*GetLocalUnitError)(PVOID);// 0x18
	_IRQL_requires_same_ _IRQL_requires_max_(HIGH_LEVEL) VOID(*ClearLocalUnitError)(PVOID); //0x20
	_IRQL_requires_same_ _IRQL_requires_max_(HIGH_LEVEL) NTSTATUS(*GetLogicalId)(PVOID, _INTERRUPT_TARGET); // 0x28
	_IRQL_requires_same_ _IRQL_requires_max_(HIGH_LEVEL) NTSTATUS(*SetLogicalId)(PVOID, _INTERRUPT_TARGET); //0x30
	_IRQL_requires_same_ _IRQL_requires_max_(HIGH_LEVEL) _INTERRUPT_RESULT(*AcceptAndGetSource)(PVOID, PINT32, PUINT32); // 0x38
	_IRQL_requires_same_ _IRQL_requires_max_(HIGH_LEVEL) VOID(*EndOfInterrupt)(PVOID, UINT32); // 0x40
	_IRQL_requires_same_ _IRQL_requires_max_(HIGH_LEVEL) VOID(*FastEndOfInterrupt)(); // 0x48
	_IRQL_requires_same_ _IRQL_requires_max_(HIGH_LEVEL) NTSTATUS(*SetLineState)(PVOID, _INTERRUPT_LINE*, _INTERRUPT_LINE_STATE*); // 0x50
	_IRQL_requires_same_ _IRQL_requires_max_(HIGH_LEVEL) NTSTATUS(*RequestInterrupt)(PVOID, _INTERRUPT_LINE*, _INTERRUPT_TARGET*, UINT32, _INTERRUPT_LINE*); // 0x58
	_IRQL_requires_same_ _IRQL_requires_max_(HIGH_LEVEL) NTSTATUS(*StartProcessor)(PVOID, UINT32, PVOID, UINT32); // 0x60
	_IRQL_requires_same_ _IRQL_requires_max_(HIGH_LEVEL) NTSTATUS(*GenerateMessage)(PVOID, _INTERRUPT_LINE_STATE*, PUINT64, PUINT64); //0x68
	_IRQL_requires_same_ _IRQL_requires_max_(HIGH_LEVEL) NTSTATUS(*ConvertId)(PVOID, PUINT32, _INTERRUPT_TARGET*, UINT8); // 0x70
	_IRQL_requires_same_ _IRQL_requires_max_(HIGH_LEVEL) NTSTATUS(*SaveLocalInterrupts)(PVOID, PVOID); //0x78 (required if interrupts can be replayed)
	_IRQL_requires_same_ _IRQL_requires_max_(HIGH_LEVEL) NTSTATUS(*ReplayLocalInterrupts)(PVOID, PVOID); //0x80 (required if interrupts can be replayed)
	_IRQL_requires_same_ _IRQL_requires_max_(HIGH_LEVEL) NTSTATUS(*DeinitializeLocalUnit)(PVOID); //0x88
	_IRQL_requires_same_ _IRQL_requires_max_(HIGH_LEVEL) NTSTATUS(*DeinitializeIoUnit)(PVOID); // 0x90
	_IRQL_requires_same_ _IRQL_requires_max_(HIGH_LEVEL) _INTERRUPT_RESULT(*QueryAndGetSource)(PVOID, PINT32, PUINT32, PUINT8); // 0x98
	_IRQL_requires_same_ _IRQL_requires_max_(HIGH_LEVEL) VOID(*DeactivateInterrupt)(PVOID, UINT32); // 0xA0
	_IRQL_requires_same_ _IRQL_requires_max_(HIGH_LEVEL) VOID(*DirectedEndOfInterrupt)(PVOID, UINT32, UINT32); //0xA8
	_IRQL_requires_same_ _IRQL_requires_max_(HIGH_LEVEL) NTSTATUS(*QueryLocalUnitInfo)(PVOID, UINT32, PUINT32, PUINT32, _KINTERRUPT_MODE*, _KINTERRUPT_MODE*); //0xB0
	_IRQL_requires_same_ _IRQL_requires_max_(HIGH_LEVEL) NTSTATUS(*QueryPendingState)(PVOID, _INTERRUPT_LINE*, PUINT8, PUINT8); //0xB8
	_IRQL_requires_same_ _IRQL_requires_max_(HIGH_LEVEL) VOID(*CaptureGlobalCrashdumpState)(PVOID); //0xC0
	_IRQL_requires_same_ _IRQL_requires_max_(HIGH_LEVEL) VOID(*CaptureProcessorCrashdumpState)(PVOID, UINT32); // 0xC8

} INTERRUPT_FUNCTION_TABLE;

//
// NT SoC API Interrupt Controller Capabilities (per reversing of 26100.1 ARM64 HAL, for documentation reasons)
// [0] - whether the IRQ controller can be controlled on a per-processor basis. (Whether "local units" exist or not)
// [1] - whether the IRQ controller supports setting priorities or not.
// [2] - whether there's a "logical flat limit" on the IRQ controller (might be number of total processors?)
// [3] - has to do with cluster order for interrupts?
// [4] - IPI related
// [5] - has to do with IPIs
// [6] - has to do with IPIs as well (bits [6:4] seem shorthand related?)
// [7] - unknown
// [8] - whether remapping is required/supported (GICv3 driver sets this bit if LPIs are supported)
// [9] - whether IRQs have to be masked before changing the state of an interrupt.
// [10] - whether the IRQ controller supports directing the EndOfInterrupt event.
// [11] - whether HV MSI remapping is supported (GICv3 driver sets this bit if LPIs are NOT supported)
//

//
// For the ARM64 interrupt controllers naturally supported by the HAL, the following are the default capability bitmasks set:
// - GIC(v2): 0x77 (INTERRUPT_CONTROLLER_SUPPORTS_PER_PROCESSOR_CONTROL 
// | INTERRUPT_CONTROLLER_HAS_PRIORITIES 
// | INTERRUPT_CONTROLLER_HAS_LOGICAL_FLAT_LIMIT 
// | INTERRUPT_CONTROLLER_CLUSTER_ORDER_IRQ_PROPERTY 
// | INTERRUPT_CONTROLLER_IPI_CONTROL_MASK)
// 
// - GICv3: in the LPI supported case, 0x12B (INTERRUPT_CONTROLLER_SUPPORTS_PER_PROCESSOR_CONTROL | INTERRUPT_CONTROLLER_HAS_PRIORITIES | INTERRUPT_CONTROLLER_CLUSTER_ORDER_IRQ_PROPERTY | BIT(5) | INTERRUPT_CONTROLLER_SUPPORTS_INTERRUPT_REMAP)
// (note it will sometimes leave the Capabilities bitmask alone)
// in the LPI not supported case: 0x82B (INTERRUPT_CONTROLLER_SUPPORTS_PER_PROCESSOR_CONTROL | INTERRUPT_CONTROLLER_HAS_PRIORITIES | INTERRUPT_CONTROLLER_CLUSTER_ORDER_IRQ_PROPERTY | BIT(5) | INTERRUPT_CONTROLLER_SUPPORTS_HV_MSI_REMAPPING)
// 
// - BCM2836: 0x277 (INTERRUPT_CONTROLLER_SUPPORTS_PER_PROCESSOR_CONTROL 
// | INTERRUPT_CONTROLLER_HAS_PRIORITIES 
// | INTERRUPT_CONTROLLER_HAS_LOGICAL_FLAT_LIMIT 
// | INTERRUPT_CONTROLLER_CLUSTER_ORDER_IRQ_PROPERTY 
// | INTERRUPT_CONTROLLER_IPI_CONTROL_MASK | INTERRUPT_CONTROLLER_REQUIRES_MASK_BEFORE_SET_LINE_STATE) (GICv2 + that mask requirement)
// 
//

//
// This bit indicates that the IRQ controller has per-processor controls and has local units.
// If this capability bit is set, the interrupt function table must have non-NULL entries for the functions that deal with
// local units.
// Note: While AIC itself doesn't seem to have the conception of a "local" per processor unit control, this bit is somewhat necessary for the HAL
// to have per cluster controls so if we want to use some of the more granular controls, note that this bit will have to be enabled.
//
#define INTERRUPT_CONTROLLER_SUPPORTS_PER_PROCESSOR_CONTROL BIT(0)

//
// This bit sets whether an IRQ controller supports setting interrupt priorities.
// If this capability bit is set, the interrupt function table must have a non-NULL entry for the SetPriority function.
//
#define INTERRUPT_CONTROLLER_HAS_PRIORITIES BIT(1)

//
// This bit indicates whether an IRQ controller has a logical flat limit (might be related to how many total cores it can service?)
//
#define INTERRUPT_CONTROLLER_HAS_LOGICAL_FLAT_LIMIT BIT(2)

//
// This bit indicates an unknown IRQ controller property regarding cluster ordering for interrupts.
// GIC(v2) sets this bit.
//
#define INTERRUPT_CONTROLLER_CLUSTER_ORDER_IRQ_PROPERTY BIT(3)

//
// Bits [6:4] all relate to IPI control.
//
#define INTERRUPT_CONTROLLER_IPI_CONTROL_MASK (7 << 4) // a GENMASK(6, 4) works here too

//
// This bit indicates that the IRQ controller requires/supports interrupt remapping.
// If this capability is set, then an IOMMU is also expected to be present and Windows must know about it.
//
#define INTERRUPT_CONTROLLER_SUPPORTS_INTERRUPT_REMAP BIT(8)

//
// This bit indicates that before setting or changing the state of an interrupt, IRQs must be masked on the local processor first.
//
#define INTERRUPT_CONTROLLER_REQUIRES_MASK_BEFORE_SET_LINE_STATE BIT(9)

//
// This bit sets whether an IRQ controller supports directing the end-of-interrupt event.
// If this capability bit is set, the interrupt function table must have a non-NULL entry for the DirectedEndOfInterrupt function.
//
#define INTERRUPT_CONTROLLER_SUPPORTS_DIRECTED_END_OF_INTERRUPT BIT(10)


//
// This bit indicates whether hypervisor MSI remapping is supported. This seems like it would only matter for Hyper-V's SynIC?
//
#define INTERRUPT_CONTROLLER_SUPPORTS_HV_MSI_REMAPPING BIT(11)





//
// This defintiion is extrapolated based on reversing the 26100 kernel + the timer initialization block's definition in nthalext.h.
//
typedef struct _INTERRUPT_INITIALIZATION_BLOCK {
	SOC_INITIALIZATION_HEADER Header; // 0x0
	INTERRUPT_FUNCTION_TABLE FunctionTable; // 0x8
	PVOID InternalData; //0xD8
	UINT32 InternalDataSize; // 0xE0
	KNOWN_CONTROLLER_TYPE KnownType; // 0xE4
	UINT32 UnitId; // 0xE8
	UINT32 Capabilities; // 0xEC
	UINT32 MaxPriority; // 0xF0
	UINT32 MaxClusterSize; //0xF4
	UINT32 MaxClusters; //0xF8
	UINT32 InterruptReplayDataSize; //0xFC
} INTERRUPT_INITIALIZATION_BLOCK, *PINTERRUPT_INITIALIZATION_BLOCK;

#endif // !HAL_EXT_APPLE_INTERRUPT_CONTROLLER_H
