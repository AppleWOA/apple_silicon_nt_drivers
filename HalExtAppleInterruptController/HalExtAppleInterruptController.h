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
 *     SPDX-License-Identifier: (BSD-2-Clause-Patent OR MIT) AND GPL-2.0
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

#define MPIDR_AFF0(mpidr)  ((mpidr) & 0xFF)
#define MPIDR_AFF1(mpidr)  (((mpidr) >> 8) & 0xFF)
#define MPIDR_AFF2(mpidr)  (((mpidr) >> 16) & 0xFF)
#define MPIDR_AFF3(mpidr)  (((mpidr) >> 32) & 0xFF)

//#define ALIGN(x,a)		__ALIGN_MASK((x),(typeof(x))(a)-1)
//#define ALIGN_DOWN(x, a)	ALIGN((x) - ((a) - 1), (a))
//#define __ALIGN_MASK(x,mask)	(((x)+(mask))&~(mask))
//#define PTR_ALIGN(p, a)		((typeof(p))ALIGN((unsigned long)(p), (a)))


//
// The maximum number of cores known on an Apple silicon platform. (This would correspond to the two die configuration of most
// Pro series chips)
//
#define MAX_KNOWN_CPUS 32

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
// MSR definitions for Fast IPIs
//
#define APPLE_FAST_IPI_REQUEST_LOCAL_REG_EL1 ARM64_SYSREG(3, 5, 15, 0, 0)
#define APPLE_FAST_IPI_REQUEST_GLOBAL_REG_EL1 ARM64_SYSREG(3, 5, 15, 0, 1)
#define APPLE_FAST_IPI_STATUS_REG_EL1 ARM64_SYSREG(3, 5, 15, 1, 1)
#define APPLE_FAST_IPI_COUNTDOWN_REG_EL1 ARM64_SYSREG(3, 5, 15, 3, 1)

//
// Some definitions borrowed from Asahi Linux linux tree
//
#define IPI_RR_CPU			GENMASK(7, 0)
#define IPI_RR_CLUSTER			GENMASK(23, 16)
#define IPI_RR_TYPE			GENMASK(29, 28)
#define IPI_RR_IMMEDIATE		0
#define IPI_RR_RETRACT			1
#define IPI_RR_DEFERRED			2
#define IPI_RR_NOWAKE			3

//
// AIC controller general structure.
//

typedef struct _AIC_INFO {
	//
	// AIC base address. (Do we need a physical and virtual view? If we do, then this definition is the physical
	// and should have PHYSICAL_ADDRESS type, the other will use a PUINT32 type, since AIC uses 32 bit MMIO accesses.)
	//
	PHYSICAL_ADDRESS AppleInterruptControllerBasePhys;
	volatile PULONG AppleInterruptControllerBaseVirt;
	UINT32 AppleInterruptControllerSize;

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
	// Offsets to software-defined IRQ mask set/clear registers. The calculation is similar to the above for AICv2 and v3.
	//
	UINT32 AicSwIrqMaskSetOffset;
	UINT32 AicSwIrqMaskClearOffset;

	//
	// Offset to HW state registers. (A read-only view of the IRQ states)
	//
	UINT32 AicHwStateOffset;

	//
	// A boolean indicating the AIC is initialized and ready to go. (All ARM64 interrupt controllers in the HAL support a similar mechanism)
	//
	BOOLEAN AicInitialized;

	//
	// A boolean indicating whether we should use Fast IPIs or not on devices that support this. Set the value based on SoC Chip ID
	// (We always use Fast IPIs on devices that support it, only using AIC-based IPIs on devices that don't.)
	//
	BOOLEAN AicUseFastIpis;

	//
	// A copy of the handle that the kernel passes all HAL Extensions. Needed in some places, chiefly to get ACPI tables.
	//
	ULONG HalExtHandle;

	//
	// MPIDR values, ordered by processor index (Our ordering should match the ADT here to avoid issues...)
	//
	UINT64 Mpidrs[MAX_KNOWN_CPUS];
} AIC_INFO, *P_AIC_INFO;

//
// This struct must always be in sync with it's counterpart in the UEFI implementation!
//

typedef struct
{
	ULONG Length;
	ULONG Type; // for now "1" will be AICv1, "2" will be AICv2 and "3" will be AICv3
	ULONGLONG ControllerBaseAddress;
	ULONG ControllerBaseSize;
	ULONG NumIrqs;
	ULONG MaxIrqs;
	BOOLEAN FastIpisSupported; // passed in by UEFI
} INTERRUPT_CONTROLLER_VENDOR_DATA;

typedef struct
{
	CSRT_RESOURCE_DESCRIPTOR_HEADER InterruptControllerHeader;
	INTERRUPT_CONTROLLER_VENDOR_DATA ControllerVendorData;
} RD_INTERRUPT_CONTROLLER;

//
// NT HAL specific typedefs, structs, and macros
//

//
// All of these definitions come from the type information that's put into the 26100 kernel public PDB symbols shipped publicly by Microsoft.
//

typedef enum _INTERRUPT_TARGET_TYPE {
	InterruptTargetInvalid,
	InterruptTargetAllIncludingSelf,
	InterruptTargetAllExcludingSelf,
	InterruptTargetSelfOnly,
	InterruptTargetPhysical,
	InterruptTargetLogicalFlat,
	InterruptTargetLogicalClustered,
	InterruptTargetRemapIndex,
	InterruptTargetHypervisor,
} INTERRUPT_TARGET_TYPE;

typedef enum _INTERRUPT_RESULT {
	InterruptBeginFatalError,
	InterruptBeginLine,
	InterruptBeginSpurious,
	InterruptBeginVector,
	InterruptBeginNone,
} INTERRUPT_RESULT;

typedef struct _INTERRUPT_TARGET {
	INTERRUPT_TARGET_TYPE Target;
} INTERRUPT_TARGET;

typedef struct _INTERRUPT_LINE {
	UINT32 UnitId;
	INT32 Line;
} INTERRUPT_LINE;

typedef struct _INTERRUPT_LINE_STATE {
	KINTERRUPT_POLARITY Polarity;
	UINT8 EmulateActiveBoth;
	KINTERRUPT_MODE TriggerMode;
	UINT32 Flags;
	INTERRUPT_LINE Routing;
	INTERRUPT_TARGET ProcessorTarget;
	UINT32 Vector;
	UINT32 Priority;
} INTERRUPT_LINE_STATE;

typedef enum _KNOWN_CONTROLLER_TYPE {
	InterruptControllerInvalid,
	InterruptControllerPic,
	InterruptControllerApic,
	InterruptControllerGic,
	InterruptControllerGicV3,
	InterruptControllerGicV4,
	InterruptControllerBcm,
	InterruptControllerUnknown = 0x1000,
} KNOWN_CONTROLLER_TYPE;

//
// as of Germanium (26100), this is the function table used for IRQ chips registered to the HAL.
//

typedef struct _INTERRUPT_FUNCTION_TABLE {
	_IRQL_requires_same_ _IRQL_requires_max_(HIGH_LEVEL) NTSTATUS(*InitializeLocalUnit)(PVOID, ULONG, ULONG, ULONG, ULONG, PULONG); //0x0
	_IRQL_requires_same_ _IRQL_requires_max_(HIGH_LEVEL) NTSTATUS(*InitializeIoUnit)(PVOID); // 0x8
	_IRQL_requires_same_ _IRQL_requires_max_(HIGH_LEVEL) VOID(*SetPriority)(PVOID, ULONG); // 0x10
	_IRQL_requires_same_ _IRQL_requires_max_(HIGH_LEVEL) NTSTATUS(*GetLocalUnitError)(PVOID);// 0x18
	_IRQL_requires_same_ _IRQL_requires_max_(HIGH_LEVEL) VOID(*ClearLocalUnitError)(PVOID); //0x20
	_IRQL_requires_same_ _IRQL_requires_max_(HIGH_LEVEL) NTSTATUS(*GetLogicalId)(PVOID, INTERRUPT_TARGET); // 0x28
	_IRQL_requires_same_ _IRQL_requires_max_(HIGH_LEVEL) NTSTATUS(*SetLogicalId)(PVOID, INTERRUPT_TARGET); //0x30
	_IRQL_requires_same_ _IRQL_requires_max_(HIGH_LEVEL) INTERRUPT_RESULT(*AcceptAndGetSource)(PVOID, PLONG, PULONG); // 0x38
	_IRQL_requires_same_ _IRQL_requires_max_(HIGH_LEVEL) VOID(*EndOfInterrupt)(PVOID, ULONG); // 0x40
	_IRQL_requires_same_ _IRQL_requires_max_(HIGH_LEVEL) VOID(*FastEndOfInterrupt)(); // 0x48
	_IRQL_requires_same_ _IRQL_requires_max_(HIGH_LEVEL) NTSTATUS(*SetLineState)(PVOID, INTERRUPT_LINE*, INTERRUPT_LINE_STATE*); // 0x50
	_IRQL_requires_same_ _IRQL_requires_max_(HIGH_LEVEL) NTSTATUS(*RequestInterrupt)(PVOID, INTERRUPT_LINE*, INTERRUPT_TARGET*, ULONG, INTERRUPT_LINE*); // 0x58
	_IRQL_requires_same_ _IRQL_requires_max_(HIGH_LEVEL) NTSTATUS(*StartProcessor)(PVOID, ULONG, PVOID, ULONG); // 0x60
	_IRQL_requires_same_ _IRQL_requires_max_(HIGH_LEVEL) NTSTATUS(*GenerateMessage)(PVOID, INTERRUPT_LINE_STATE*, PUINT64, PUINT64); //0x68
	_IRQL_requires_same_ _IRQL_requires_max_(HIGH_LEVEL) NTSTATUS(*ConvertId)(PVOID, PULONG, INTERRUPT_TARGET*, UINT8); // 0x70
	_IRQL_requires_same_ _IRQL_requires_max_(HIGH_LEVEL) NTSTATUS(*SaveLocalInterrupts)(PVOID, PVOID); //0x78 (required if interrupts can be replayed)
	_IRQL_requires_same_ _IRQL_requires_max_(HIGH_LEVEL) NTSTATUS(*ReplayLocalInterrupts)(PVOID, PVOID); //0x80 (required if interrupts can be replayed)
	_IRQL_requires_same_ _IRQL_requires_max_(HIGH_LEVEL) NTSTATUS(*DeinitializeLocalUnit)(PVOID); //0x88
	_IRQL_requires_same_ _IRQL_requires_max_(HIGH_LEVEL) NTSTATUS(*DeinitializeIoUnit)(PVOID); // 0x90
	_IRQL_requires_same_ _IRQL_requires_max_(HIGH_LEVEL) INTERRUPT_RESULT(*QueryAndGetSource)(PVOID, PLONG, PULONG, PUINT8); // 0x98
	_IRQL_requires_same_ _IRQL_requires_max_(HIGH_LEVEL) VOID(*DeactivateInterrupt)(PVOID, ULONG); // 0xA0
	_IRQL_requires_same_ _IRQL_requires_max_(HIGH_LEVEL) VOID(*DirectedEndOfInterrupt)(PVOID, ULONG, ULONG); //0xA8
	_IRQL_requires_same_ _IRQL_requires_max_(HIGH_LEVEL) NTSTATUS(*QueryLocalUnitInfo)(PVOID, ULONG, PULONG, PULONG, KINTERRUPT_MODE*, KINTERRUPT_MODE*); //0xB0
	_IRQL_requires_same_ _IRQL_requires_max_(HIGH_LEVEL) NTSTATUS(*QueryPendingState)(PVOID, INTERRUPT_LINE*, PUINT8, PUINT8); //0xB8
	_IRQL_requires_same_ _IRQL_requires_max_(HIGH_LEVEL) VOID(*CaptureGlobalCrashdumpState)(PVOID); //0xC0
	_IRQL_requires_same_ _IRQL_requires_max_(HIGH_LEVEL) VOID(*CaptureProcessorCrashdumpState)(PVOID, ULONG); // 0xC8

} INTERRUPT_FUNCTION_TABLE;

//
// NT SoC API Interrupt Controller Capabilities (per reversing of 26100.1 ARM64 HAL, for documentation reasons)
// [0] - whether the IRQ controller can be controlled on a per-processor basis. (Whether "local units" exist or not)
// [1] - whether the IRQ controller supports setting priorities or not.
// [2] - whether there's a "logical flat limit" on the IRQ controller (per APIC documentation, 
// this seems to pertain to IRQ controller being able to address cores logically)
// [3] - has to do with cluster order for interrupts?
// [4] - IPIs can be addressed to all running cores including the current running core
// [5] - IPIs can be addressed to all running cores excluding the current running core
// [6] - IPIs can be addressed to just the running core
// [7] - unknown, isn't set by any of the ARM64 controllers and there don't seem to be checks for it.
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
// | INTERRUPT_CONTROLLER_IPI_CONTROL_MASK)
// 
// - GICv3: in the LPI supported case, 0x12B (INTERRUPT_CONTROLLER_SUPPORTS_PER_PROCESSOR_CONTROL 
// | INTERRUPT_CONTROLLER_HAS_PRIORITIES 
// | INTERRUPT_CONTROLLER_CLUSTER_ORDER_IRQ_PROPERTY 
// | INTERRUPT_CONTROLLER_IPI_TO_ALL_CORES_EXCLUDING_SELF 
// | INTERRUPT_CONTROLLER_SUPPORTS_INTERRUPT_REMAP)
// (note it will sometimes leave the Capabilities bitmask alone)
// in the LPI not supported case: 0x82B (INTERRUPT_CONTROLLER_SUPPORTS_PER_PROCESSOR_CONTROL 
// | INTERRUPT_CONTROLLER_HAS_PRIORITIES 
// | INTERRUPT_CONTROLLER_CLUSTER_ORDER_IRQ_PROPERTY 
// | INTERRUPT_CONTROLLER_IPI_TO_ALL_CORES_EXCLUDING_SELF 
// | INTERRUPT_CONTROLLER_SUPPORTS_HV_MSI_REMAPPING)
// 
// - BCM2836: 0x277 (INTERRUPT_CONTROLLER_SUPPORTS_PER_PROCESSOR_CONTROL 
// | INTERRUPT_CONTROLLER_HAS_PRIORITIES 
// | INTERRUPT_CONTROLLER_HAS_LOGICAL_FLAT_LIMIT 
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
// This bit indicates whether an IRQ controller has a logical flat limit. 
// (APIC terminology for "the core can address specific cores on the logical level" from some digging)
// This bit seems to be dependent on local units being supported.
//
#define INTERRUPT_CONTROLLER_HAS_LOGICAL_FLAT_LIMIT BIT(2)

//
// This bit indicates an unknown IRQ controller property regarding cluster ordering for interrupts.
// (Might be a similar thing as bit 2, OSDev wiki entry for APIC mentions a "logical cluster" mode)
// This bit seems to be dependent on local units being supported.
//
#define INTERRUPT_CONTROLLER_CLUSTER_ORDER_IRQ_PROPERTY BIT(3)

//
// Bits [6:4] all relate to IPI control.
//


//
// This bit indicates the interrupt controller is able to send IPIs to all cores including the running core.
//
#define INTERRUPT_CONTROLLER_IPI_TO_ALL_CORES_INCLUDING_SELF BIT(4)

//
// This bit indicates the interrupt controller can send IPIs to all cores excluding the running core.
//
#define INTERRUPT_CONTROLLER_IPI_TO_ALL_CORES_EXCLUDING_SELF BIT(5)

//
// This bit indicates the interrupt controller can send an IPI to the running core itself.
//
#define INTERRUPT_CONTROLLER_IPI_TO_SELF BIT(6)


#define INTERRUPT_CONTROLLER_IPI_CONTROL_MASK (7 << 4) // a GENMASK(6, 4) works here too

//
// This bit indicates that the IRQ controller requires interrupt remapping. (Implies the platform supports interrupt remapping)
// If this capability is set, then an IOMMU is also expected to be present and Windows must know about it.
//
#define INTERRUPT_CONTROLLER_REQUIRES_INTERRUPT_REMAP BIT(8)

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
// This defintion is extrapolated based on reversing the 26100 kernel + the timer initialization block's definition in nthalext.h.
//
typedef struct _INTERRUPT_INITIALIZATION_BLOCK {
	SOC_INITIALIZATION_HEADER Header; // 0x0
	INTERRUPT_FUNCTION_TABLE FunctionTable; // 0x8
	PVOID InternalData; //0xD8
	ULONG InternalDataSize; // 0xE0
	KNOWN_CONTROLLER_TYPE KnownType; // 0xE4
	ULONG UnitId; // 0xE8
	ULONG Capabilities; // 0xEC
	ULONG MaxPriority; // 0xF0
	ULONG MaxClusterSize; //0xF4
	ULONG MaxClusters; //0xF8
	ULONG InterruptReplayDataSize; //0xFC
} INTERRUPT_INITIALIZATION_BLOCK, *PINTERRUPT_INITIALIZATION_BLOCK;

//
// Yes, this is needed...
//

typedef enum _INTERRUPT_LINE_TYPE {
	InterruptLineInvalidType, // the default, just means "unassigned"
	InterruptLineUnusable, // this line is unusable
	InterruptLineStandardPin, // the equivalent to SPIs in GICv3, these are for hardware peripheral interrupts
	InterruptLineProcessorLocal, // the equivalent to LPIs in GICv3, these are PPIs (in AIC case, these are FIQs)
	InterruptLineSoftwareOnly, // other software-specific interrupts
	InterruptLineSoftwareOnlyProcessorLocal, // the equivalent to SGIs in GICv3, these are mostly used for IPIs, but it can be other stuff too
	InterruptLineOutputPin, // the Windows implementation of line 1 seems to always be an output pin.
	InterruptLineMsi // MSIs
} INTERRUPT_LINE_TYPE;

typedef enum _INTERRUPT_LINE_SUBTYPE {
	InterruptLineSubTypeNone,
	InterruptLineSubTypeV2m,
	InterruptLineSubTypeLpi
} INTERRUPT_LINE_SUBTYPE;

//
// This definition is based on reversing of the HAL of 26100.1.
//
typedef struct _INTERRUPT_LINE_INITIALIZATION_BLOCK {
	ULONG ControllerId; // 0x0 (IRQ controller identifier)
	LONG MinLine; // 0x4
	LONG MaxLine; // 0x8
	INTERRUPT_LINE_TYPE Type; // 0xC
	INTERRUPT_LINE_SUBTYPE SubType; // 0x10
	ULONG ControllerIdForOutput; // 0x14 (only used if the line type is OutputPin)
	ULONG GsiBase; // 0x18 (this *seems* to be "the mapping of the IRQ number to the HW's understanding of IRQ numbers" based on GICv3 code)
	ULONGLONG MsiAddress; //0x20
	ULONG MsiData; // 0x28
} INTERRUPT_LINE_INITIALIZATION_BLOCK, *PINTERRUPT_LINE_INITIALIZATION_BLOCK;

#endif // !HAL_EXT_APPLE_INTERRUPT_CONTROLLER_H
