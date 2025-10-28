#include <ntddk.h>

#define DEBUG(Format, ...) DbgPrint("%s: " Format "\n", __FUNCTION__, __VA_ARGS__)

typedef struct _ANS_CONTROLLER_DEVICE_EXTENSION {
	PDEVICE_OBJECT PhysicalDeviceObject;
	PDEVICE_OBJECT LowerDeviceObject;

	PKINTERRUPT NvmeInterrupt;
	PKINTERRUPT AscRxNotEmptyInterrupt;

	PHYSICAL_ADDRESS AnsMmioPhysical;
	PHYSICAL_ADDRESS NvmeMmioPhysical;
	PHYSICAL_ADDRESS AscMmioPhysical;

	PVOID AnsMmio;
	PVOID NvmeMmio;
	PVOID AscMmio;

	ULONG AnsMmioLength;
	ULONG NvmeMmioLength;
	ULONG AscMmioLength;

	ULONG NvmeInterruptVector;
	ULONG AscRxNotEmptyInterruptVector;

	KIRQL NvmeInterruptLevel;
	KIRQL AscRxNotEmptyInterruptLevel;

	KAFFINITY NvmeInterruptAffinity;
	KAFFINITY AscRxNotEmptyInterruptAffinity;
} ANS_CONTROLLER_DEVICE_EXTENSION, * PANS_CONTROLLER_DEVICE_EXTENSION;

/**
 * Finds a resource descriptor of a given type in a resource list.
 * @param ResourceList The resource list to search.
 * @param Index The index of the descriptor of the given type to find.
 * @param DescriptorType The type of the descriptor to find.
 * @param Descriptor Receives a pointer to the found descriptor.
 * @return STATUS_SUCCESS if the descriptor was found, STATUS_NOT_FOUND otherwise.
 */
NTSTATUS
AppleNANDStorageFindResource(
	_In_ PCM_RESOURCE_LIST ResourceList,
	_In_ ULONG Index,
	_In_ ULONG DescriptorType,
	_Out_ PCM_PARTIAL_RESOURCE_DESCRIPTOR* Descriptor
) {
	ULONG DescriptorCount = 0;

	for (ULONG i = 0; i < ResourceList->Count; i++) {
		PCM_FULL_RESOURCE_DESCRIPTOR FullDescriptor = &ResourceList->List[i];
		PCM_PARTIAL_RESOURCE_LIST PartialList = &FullDescriptor->PartialResourceList;

		for (ULONG j = 0; j < PartialList->Count; j++) {
			PCM_PARTIAL_RESOURCE_DESCRIPTOR ThisDescriptor = &PartialList->PartialDescriptors[j];

			if (ThisDescriptor->Type != DescriptorType) {
				continue;
			}

			if (DescriptorCount != Index) {
				DescriptorCount++;
				continue;
			}

			*Descriptor = ThisDescriptor;
			return STATUS_SUCCESS;
		}
	}

	return STATUS_NOT_FOUND;
}

/**
 * NVMe interrupt service routine.
 */
BOOLEAN
AppleNANDStorageNvmeInterruptServiceRoutine(
	_In_ PKINTERRUPT Interrupt,
	_In_ PVOID ServiceContext
) {
	UNREFERENCED_PARAMETER(Interrupt);
	UNREFERENCED_PARAMETER(ServiceContext);

	DEBUG("We got an NVMe interrupt!");

	return FALSE;
}

/**
 * ASC (mailbox) interrupt service routine.
 */
BOOLEAN
AppleNANDStorageAscInterruptServiceRoutine(
	_In_ PKINTERRUPT Interrupt,
	_In_ PVOID ServiceContext
) {
	UNREFERENCED_PARAMETER(Interrupt);
	UNREFERENCED_PARAMETER(ServiceContext);

	DEBUG("We got an ASC interrupt!");

	return FALSE;
}

NTSTATUS
AppleNANDStorageStartDevice(
	_In_ PDEVICE_OBJECT DeviceObject,
	_In_ PIRP Irp
) {
	NTSTATUS Status;
	PCM_PARTIAL_RESOURCE_DESCRIPTOR Descriptor;
	IO_CONNECT_INTERRUPT_PARAMETERS InterruptParams;

	PIO_STACK_LOCATION IrpStack = IoGetCurrentIrpStackLocation(Irp);
	PCM_RESOURCE_LIST ResourceList = IrpStack->Parameters.StartDevice.AllocatedResources;

	if (ResourceList == NULL) {
		DEBUG("No resources allocated");
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	PANS_CONTROLLER_DEVICE_EXTENSION DeviceExtension = DeviceObject->DeviceExtension;

	// Find the needed resources.

	Status = AppleNANDStorageFindResource(ResourceList, 0, CmResourceTypeMemory, &Descriptor);

	if (!NT_SUCCESS(Status)) {
		DEBUG("Failed to find ANS MMIO resource");
		return Status;
	}

	DEBUG("ANS MMIO: Start=0x%llx, Length=%lu",
		Descriptor->u.Memory.Start.QuadPart,
		Descriptor->u.Memory.Length);

	DeviceExtension->AnsMmioPhysical = Descriptor->u.Memory.Start;
	DeviceExtension->AnsMmioLength = Descriptor->u.Memory.Length;

	Status = AppleNANDStorageFindResource(ResourceList, 1, CmResourceTypeMemory, &Descriptor);

	if (!NT_SUCCESS(Status)) {
		DEBUG("Failed to find NVMe MMIO resource");
		return Status;
	}

	DEBUG("NVMe MMIO: Start=0x%llx, Length=%lu",
		Descriptor->u.Memory.Start.QuadPart,
		Descriptor->u.Memory.Length);

	DeviceExtension->NvmeMmioPhysical = Descriptor->u.Memory.Start;
	DeviceExtension->NvmeMmioLength = Descriptor->u.Memory.Length;

	Status = AppleNANDStorageFindResource(ResourceList, 2, CmResourceTypeMemory, &Descriptor);

	if (!NT_SUCCESS(Status)) {
		DEBUG("Failed to find ASC MMIO resource");
		return Status;
	}

	DEBUG("ASC MMIO: Start=0x%llx, Length=%lu",
		Descriptor->u.Memory.Start.QuadPart,
		Descriptor->u.Memory.Length);

	DeviceExtension->AscMmioPhysical = Descriptor->u.Memory.Start;
	DeviceExtension->AscMmioLength = Descriptor->u.Memory.Length;

	Status = AppleNANDStorageFindResource(ResourceList, 0, CmResourceTypeInterrupt, &Descriptor);

	if (!NT_SUCCESS(Status)) {
		DEBUG("Failed to find NVMe interrupt resource");
		return Status;
	}

	DEBUG("NVMe Interrupt: Vector=%lu, Level=%lu, Affinity=0x%llx",
		Descriptor->u.Interrupt.Vector,
		Descriptor->u.Interrupt.Level,
		Descriptor->u.Interrupt.Affinity);

	DeviceExtension->NvmeInterruptVector = Descriptor->u.Interrupt.Vector;
	DeviceExtension->NvmeInterruptLevel = (KIRQL)Descriptor->u.Interrupt.Level;
	DeviceExtension->NvmeInterruptAffinity = Descriptor->u.Interrupt.Affinity;

	Status = AppleNANDStorageFindResource(ResourceList, 4, CmResourceTypeInterrupt, &Descriptor);

	if (!NT_SUCCESS(Status)) {
		DEBUG("Failed to find ASC RX interrupt resource");
		return Status;
	}

	DEBUG("ASC RX Not Empty Interrupt: Vector=%lu, Level=%lu, Affinity=0x%llx",
		Descriptor->u.Interrupt.Vector,
		Descriptor->u.Interrupt.Level,
		Descriptor->u.Interrupt.Affinity);

	DeviceExtension->AscRxNotEmptyInterruptVector = Descriptor->u.Interrupt.Vector;
	DeviceExtension->AscRxNotEmptyInterruptLevel = (KIRQL)Descriptor->u.Interrupt.Level;
	DeviceExtension->AscRxNotEmptyInterruptAffinity = Descriptor->u.Interrupt.Affinity;

	// Map the found memory regions.

	DeviceExtension->AnsMmio = MmMapIoSpace(
		DeviceExtension->AnsMmioPhysical,
		DeviceExtension->AnsMmioLength,
		MmNonCached);

	if (DeviceExtension->AnsMmio == NULL) {
		DEBUG("Failed to map ANS MMIO");
		Status = STATUS_INSUFFICIENT_RESOURCES;
		goto Cleanup;
	}

	DEBUG("Mapped ANS MMIO at %p", DeviceExtension->AnsMmio);

	DeviceExtension->NvmeMmio = MmMapIoSpace(
		DeviceExtension->NvmeMmioPhysical,
		DeviceExtension->NvmeMmioLength,
		MmNonCached);

	if (DeviceExtension->NvmeMmio == NULL) {
		DEBUG("Failed to map NVMe MMIO");
		Status = STATUS_INSUFFICIENT_RESOURCES;
		goto Cleanup;
	}

	DEBUG("Mapped NVMe MMIO at %p", DeviceExtension->NvmeMmio);

	DeviceExtension->AscMmio = MmMapIoSpace(
		DeviceExtension->AscMmioPhysical,
		DeviceExtension->AscMmioLength,
		MmNonCached);

	if (DeviceExtension->AscMmio == NULL) {
		DEBUG("Failed to map ASC MMIO");
		Status = STATUS_INSUFFICIENT_RESOURCES;
		goto Cleanup;
	}

	DEBUG("Mapped ASC MMIO at %p", DeviceExtension->AscMmio);

	// Connect interrupts.

	RtlZeroMemory(&InterruptParams, sizeof(InterruptParams));

	InterruptParams.Version = CONNECT_FULLY_SPECIFIED;
	InterruptParams.FullySpecified.PhysicalDeviceObject = DeviceExtension->PhysicalDeviceObject;
	InterruptParams.FullySpecified.InterruptObject = &DeviceExtension->NvmeInterrupt;
	InterruptParams.FullySpecified.ServiceRoutine = AppleNANDStorageNvmeInterruptServiceRoutine;
	InterruptParams.FullySpecified.ServiceContext = NULL;
	InterruptParams.FullySpecified.SpinLock = NULL;
	InterruptParams.FullySpecified.SynchronizeIrql = DeviceExtension->NvmeInterruptLevel;
	InterruptParams.FullySpecified.FloatingSave = FALSE;
	InterruptParams.FullySpecified.ShareVector = FALSE;
	InterruptParams.FullySpecified.Vector = DeviceExtension->NvmeInterruptVector;
	InterruptParams.FullySpecified.Irql = DeviceExtension->NvmeInterruptLevel;
	InterruptParams.FullySpecified.InterruptMode = LevelSensitive;
	InterruptParams.FullySpecified.ProcessorEnableMask = DeviceExtension->NvmeInterruptAffinity;
	InterruptParams.FullySpecified.Group = 0;

	Status = IoConnectInterruptEx(&InterruptParams);

	if (!NT_SUCCESS(Status)) {
		DEBUG("Failed to connect NVMe interrupt: 0x%08X", Status);
		goto Cleanup;
	}

	DEBUG("Connected NVMe interrupt");

	InterruptParams.Version = CONNECT_FULLY_SPECIFIED;
	InterruptParams.FullySpecified.PhysicalDeviceObject = DeviceExtension->PhysicalDeviceObject;
	InterruptParams.FullySpecified.InterruptObject = &DeviceExtension->AscRxNotEmptyInterrupt;
	InterruptParams.FullySpecified.ServiceRoutine = AppleNANDStorageAscInterruptServiceRoutine;
	InterruptParams.FullySpecified.ServiceContext = NULL;
	InterruptParams.FullySpecified.SpinLock = NULL;
	InterruptParams.FullySpecified.SynchronizeIrql = DeviceExtension->AscRxNotEmptyInterruptLevel;
	InterruptParams.FullySpecified.FloatingSave = FALSE;
	InterruptParams.FullySpecified.ShareVector = FALSE;
	InterruptParams.FullySpecified.Vector = DeviceExtension->AscRxNotEmptyInterruptVector;
	InterruptParams.FullySpecified.Irql = DeviceExtension->AscRxNotEmptyInterruptLevel;
	InterruptParams.FullySpecified.InterruptMode = LevelSensitive;
	InterruptParams.FullySpecified.ProcessorEnableMask = DeviceExtension->AscRxNotEmptyInterruptAffinity;
	InterruptParams.FullySpecified.Group = 0;

	Status = IoConnectInterruptEx(&InterruptParams);

	if (!NT_SUCCESS(Status)) {
		DEBUG("Failed to connect ASC interrupt: 0x%08X", Status);
		goto Cleanup;
	}

	DEBUG("Connected ASC interrupt");

	return STATUS_SUCCESS;

Cleanup:
	if (DeviceExtension->NvmeInterrupt != NULL) {
		IO_DISCONNECT_INTERRUPT_PARAMETERS DisconnectParams;

		DisconnectParams.Version = CONNECT_FULLY_SPECIFIED;
		DisconnectParams.ConnectionContext.InterruptObject = DeviceExtension->NvmeInterrupt;

		IoDisconnectInterruptEx(&DisconnectParams);

		DeviceExtension->NvmeInterrupt = NULL;
	}

	if (DeviceExtension->AscRxNotEmptyInterrupt != NULL) {
		IO_DISCONNECT_INTERRUPT_PARAMETERS DisconnectParams;

		DisconnectParams.Version = CONNECT_FULLY_SPECIFIED;
		DisconnectParams.ConnectionContext.InterruptObject = DeviceExtension->AscRxNotEmptyInterrupt;

		IoDisconnectInterruptEx(&DisconnectParams);

		DeviceExtension->AscRxNotEmptyInterrupt = NULL;
	}

	if (DeviceExtension->AnsMmio != NULL) {
		MmUnmapIoSpace(DeviceExtension->AnsMmio, DeviceExtension->AnsMmioLength);
		DeviceExtension->AnsMmio = NULL;
	}

	if (DeviceExtension->NvmeMmio != NULL) {
		MmUnmapIoSpace(DeviceExtension->NvmeMmio, DeviceExtension->NvmeMmioLength);
		DeviceExtension->NvmeMmio = NULL;
	}

	if (DeviceExtension->AscMmio != NULL) {
		MmUnmapIoSpace(DeviceExtension->AscMmio, DeviceExtension->AscMmioLength);
		DeviceExtension->AscMmio = NULL;
	}

	return Status;
}

NTSTATUS
AppleNANDStoragePnPDispatch(
	_In_ PDEVICE_OBJECT DeviceObject,
	_In_ PIRP Irp
) {
	NTSTATUS Status;

	PIO_STACK_LOCATION IrpStack = IoGetCurrentIrpStackLocation(Irp);
	PANS_CONTROLLER_DEVICE_EXTENSION DeviceExtension = DeviceObject->DeviceExtension;

	if (IrpStack->MinorFunction == IRP_MN_START_DEVICE) {
		Status = AppleNANDStorageStartDevice(DeviceObject, Irp);
		Irp->IoStatus.Status = Status;
		IoCompleteRequest(Irp, IO_NO_INCREMENT);
	}
	else {
		DEBUG("Unhandled PnP IRP: MinorFunction=0x%x", IrpStack->MinorFunction);
		IoSkipCurrentIrpStackLocation(Irp);
		Status = IoCallDriver(DeviceExtension->LowerDeviceObject, Irp);
	}

	return Status;
}

NTSTATUS
AppleNANDStorageAddDevice(
	_In_ PDRIVER_OBJECT DriverObject,
	_In_ PDEVICE_OBJECT PhysicalDeviceObject
) {
	PDEVICE_OBJECT DeviceObject = NULL;
	NTSTATUS Status = IoCreateDevice(
		DriverObject,
		sizeof(ANS_CONTROLLER_DEVICE_EXTENSION),
		NULL,
		FILE_DEVICE_MASS_STORAGE,
		0,
		FALSE,
		&DeviceObject);

	if (!NT_SUCCESS(Status)) {
		DEBUG("Failed to create device: 0x%x", Status);
		return Status;
	}

	PANS_CONTROLLER_DEVICE_EXTENSION DeviceExtension = DeviceObject->DeviceExtension;

	DeviceExtension->PhysicalDeviceObject = PhysicalDeviceObject;
	DeviceExtension->LowerDeviceObject = IoAttachDeviceToDeviceStack(DeviceObject, PhysicalDeviceObject);

	if (DeviceExtension->LowerDeviceObject == NULL) {
		DEBUG("Failed to attach device");
		IoDeleteDevice(DeviceObject);
		return STATUS_NO_SUCH_DEVICE;
	}

	DeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

	return STATUS_SUCCESS;
}

NTSTATUS
DriverEntry(
	_In_ PDRIVER_OBJECT DriverObject,
	_In_ PUNICODE_STRING RegistryPath
) {
	UNREFERENCED_PARAMETER(RegistryPath);

	DriverObject->DriverExtension->AddDevice = AppleNANDStorageAddDevice;
	DriverObject->MajorFunction[IRP_MJ_PNP] = AppleNANDStoragePnPDispatch;

	return STATUS_SUCCESS;
}
