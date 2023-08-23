#include "HDriver.h"

extern "C"
{
    NTSTATUS MajorDeviceControl(PDEVICE_OBJECT pDeviceObject, PIRP pIrp);
    NTSTATUS MajorCreate(IN PDEVICE_OBJECT pDeviceObject, IN PIRP pIrp);
    NTSTATUS MajorClose(IN PDEVICE_OBJECT pDeviceObject, IN PIRP pIrp);
    VOID DriverUnload(PDRIVER_OBJECT pDriverObject);
    NTSTATUS DriverEntry(PDRIVER_OBJECT pDriverObject, PUNICODE_STRING pRegistryPath);
}

NTSTATUS MajorCreate(IN PDEVICE_OBJECT pDeviceObject, IN PIRP pIrp)
{
    UNREFERENCED_PARAMETER(pDeviceObject);

    NTSTATUS ntStatus;

    pIrp->IoStatus.Information = 0;
    pIrp->IoStatus.Status = STATUS_SUCCESS;

    ntStatus = pIrp->IoStatus.Status;
    IoCompleteRequest(pIrp, IO_NO_INCREMENT);

    return STATUS_SUCCESS;
}

NTSTATUS MajorClose(IN PDEVICE_OBJECT pDeviceObject, IN PIRP pIrp)
{
    UNREFERENCED_PARAMETER(pDeviceObject);

    NTSTATUS ntStatus;

    pIrp->IoStatus.Status = STATUS_SUCCESS;
    pIrp->IoStatus.Information = 0;

    ntStatus = pIrp->IoStatus.Status;
    IoCompleteRequest(pIrp, IO_NO_INCREMENT);

    return ntStatus;
}
NTSTATUS MajorDeviceControl(PDEVICE_OBJECT pDeviceObject, PIRP pIrp)
{
	PIO_STACK_LOCATION irpSp;
	irpSp = IoGetCurrentIrpStackLocation(pIrp);

	NTSTATUS status = STATUS_SUCCESS;

	ULONG_PTR bytesTransferred = pIrp->IoStatus.Information;
	PVOID io = pIrp->AssociatedIrp.SystemBuffer;

	SIZE_T length;
	switch (irpSp->Parameters.DeviceIoControl.IoControlCode)
	{
	case IOCTL_SET_PID:
		memcpy(&g_PID, io, sizeof(UINT32));
		break;

	case IOCTL_SET_ADRESS:
		memcpy(&g_Address, io, sizeof(ULONG64));
		break;

	case IOCTL_READ_MEMORY:
		length = irpSp->Parameters.DeviceIoControl.OutputBufferLength;
		ReadPhysicalAddressByVirtualAdress(g_PID, g_Address, (BYTE*)io, length, &bytesTransferred);
		break;

	case IOCTL_WRITE_MEMORY:
		length = irpSp->Parameters.DeviceIoControl.InputBufferLength;
		ReadPhysicalAddressByVirtualAdress(g_PID, g_Address, (BYTE*)io, length, &bytesTransferred);
		break;

	default:
		break;
	}

	pIrp->IoStatus.Status = status;
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);
	return status;
}

VOID DriverUnload(PDRIVER_OBJECT pDriverObject)
{
    UNICODE_STRING uniDOSString;

    RtlInitUnicodeString(&uniDOSString, DOS_DEVICE_NAME);
    IoDeleteSymbolicLink(&uniDOSString);
    IoDeleteDevice(pDriverObject->DeviceObject);

    DbgPrint("'HDriver' unloaded\n");
}

NTSTATUS DriverEntry(PDRIVER_OBJECT pDriverObject, PUNICODE_STRING pRegistryPath)
{
    UNREFERENCED_PARAMETER(pRegistryPath);

    NTSTATUS ntStatus;
    PDEVICE_OBJECT DeviceObject = NULL;
    UNICODE_STRING uniNameString, uniDOSString;

    RtlInitUnicodeString(&uniNameString, DEVICE_NAME);
    RtlInitUnicodeString(&uniDOSString, DOS_DEVICE_NAME);

    ntStatus = IoCreateDevice(pDriverObject,
        0,
        &uniNameString,
        FILE_DEVICE_UNKNOWN,
        0,
        FALSE,
        &DeviceObject);

    if (!NT_SUCCESS(ntStatus))
        return ntStatus;

    ntStatus = IoCreateSymbolicLink(&uniDOSString, &uniNameString);

    if (!NT_SUCCESS(ntStatus))
        return ntStatus;

    pDriverObject->MajorFunction[IRP_MJ_CREATE] = MajorCreate;
    pDriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = MajorDeviceControl;
    pDriverObject->MajorFunction[IRP_MJ_CLOSE] = MajorClose;
    pDriverObject->DriverUnload = DriverUnload;

    DbgPrint("'HDriver' loaded\n");

    return STATUS_SUCCESS;
}