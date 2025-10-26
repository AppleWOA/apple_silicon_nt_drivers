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
    APPLE_INTERRUPT_CONTROLLER_V1,
    APPLE_INTERRUPT_CONTROLLER_V2,
    APPLE_INTERRUPT_CONTROLLER_V3,
    APPLE_INTERRUPT_CONTROLLER_VER_UNKNOWN
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

#define AIC_MAX_IRQ	0x400
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
#define AIC_V2_CONFIG 0x0014
#define AIC_V2_IRQ_CFG_REG 0x2000

#define AIC_V2_NUM_AND_MAX_IRQS_MASK GENMASK(15, 0)
#define AIC_V2_INFO_REG3_MAX_DIE_COUNT_BITFIELD GENMASK(27, 24)
#define AIC_V2_INFO_REG1_LAST_CPU_DIE_BITFIELD GENMASK(27, 24)
#define AIC_V2_CFG_ENABLE BIT(0)

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

//
// as of Germanium (26100), this is the function table used for IRQ chips registered to the HAL.
//

typedef struct _INTERRUPT_FUNCTION_TABLE {
	_IRQL_requires_same_ _IRQL_requires_max_(HIGH_LEVEL) NTSTATUS(*InitializeLocalUnit)(PVOID, UINT32, UINT32, UINT32, UINT32, PUINT32);
	_IRQL_requires_same_ _IRQL_requires_max_(HIGH_LEVEL) NTSTATUS(*InitializeIoUnit)(PVOID);
	_IRQL_requires_same_ _IRQL_requires_max_(HIGH_LEVEL) VOID(*SetPriority)(PVOID, UINT32);
	_IRQL_requires_same_ _IRQL_requires_max_(HIGH_LEVEL) VOID(*ClearLocalUnitError)(PVOID);
	_IRQL_requires_same_ _IRQL_requires_max_(HIGH_LEVEL) NTSTATUS(*GetLogicalId)(PVOID, _INTERRUPT_TARGET);
	_IRQL_requires_same_ _IRQL_requires_max_(HIGH_LEVEL) NTSTATUS(*SetLogicalId)(PVOID, _INTERRUPT_TARGET);
	_IRQL_requires_same_ _IRQL_requires_max_(HIGH_LEVEL) _INTERRUPT_RESULT(*AcceptAndGetSource)(PVOID, PINT32, PUINT32);
	_IRQL_requires_same_ _IRQL_requires_max_(HIGH_LEVEL) VOID(*EndOfInterrupt)(PVOID, UINT32);
	_IRQL_requires_same_ _IRQL_requires_max_(HIGH_LEVEL) VOID(*FastEndOfInterrupt)();
	_IRQL_requires_same_ _IRQL_requires_max_(HIGH_LEVEL) NTSTATUS(*SetLineState)(PVOID, _INTERRUPT_LINE*, _INTERRUPT_LINE_STATE*);
	_IRQL_requires_same_ _IRQL_requires_max_(HIGH_LEVEL) NTSTATUS(*RequestInterrupt)(PVOID, _INTERRUPT_LINE*, _INTERRUPT_TARGET*, UINT32, _INTERRUPT_LINE*);
	_IRQL_requires_same_ _IRQL_requires_max_(HIGH_LEVEL) NTSTATUS(*StartProcessor)(PVOID, UINT32, PVOID, UINT32);
	_IRQL_requires_same_ _IRQL_requires_max_(HIGH_LEVEL) NTSTATUS(*GenerateMessage)(PVOID, _INTERRUPT_LINE_STATE*, PUINT64, PUINT64);
	_IRQL_requires_same_ _IRQL_requires_max_(HIGH_LEVEL) NTSTATUS(*ConvertId)(PVOID, PUINT32, _INTERRUPT_TARGET*, UINT8);
	_IRQL_requires_same_ _IRQL_requires_max_(HIGH_LEVEL) NTSTATUS(*SaveLocalInterrupts)(PVOID, PVOID);
	_IRQL_requires_same_ _IRQL_requires_max_(HIGH_LEVEL) NTSTATUS(*ReplayLocalInterrupts)(PVOID, PVOID);
	_IRQL_requires_same_ _IRQL_requires_max_(HIGH_LEVEL) NTSTATUS(*DeinitializeLocalUnit)(PVOID);
	_IRQL_requires_same_ _IRQL_requires_max_(HIGH_LEVEL) NTSTATUS(*DeinitializeIoUnit)(PVOID);
	_IRQL_requires_same_ _IRQL_requires_max_(HIGH_LEVEL) _INTERRUPT_RESULT(*QueryAndGetSource)(PVOID, PINT32, PUINT32, PUINT8);
	_IRQL_requires_same_ _IRQL_requires_max_(HIGH_LEVEL) VOID(*DeactivateInterrupt)(PVOID, UINT32);
	_IRQL_requires_same_ _IRQL_requires_max_(HIGH_LEVEL) VOID(*DirectedEndOfInterrupt)(PVOID, UINT32, UINT32);
	_IRQL_requires_same_ _IRQL_requires_max_(HIGH_LEVEL) NTSTATUS(*QueryLocalUnitInfo)(PVOID, UINT32, PUINT32, PUINT32, _KINTERRUPT_MODE*, _KINTERRUPT_MODE*);
	_IRQL_requires_same_ _IRQL_requires_max_(HIGH_LEVEL) NTSTATUS(*QueryPendingState)(PVOID, _INTERRUPT_LINE*, PUINT8, PUINT8);
	_IRQL_requires_same_ _IRQL_requires_max_(HIGH_LEVEL) VOID(*CaptureGlobalCrashdumpState)(PVOID);
	_IRQL_requires_same_ _IRQL_requires_max_(HIGH_LEVEL) VOID(*CaptureProcessorCrashdumpState)(PVOID, UINT32);

} INTERRUPT_FUNCTION_TABLE;

#endif // !HAL_EXT_APPLE_INTERRUPT_CONTROLLER_H
