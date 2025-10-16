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

//
// AIC defines that apply to all versions.
//

#define AIC_MASK_REG(num) (4 * ((num) >> 5))
#define AIC_MASK_BIT(num) BIT(num) & GENMASK(4, 0)

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
