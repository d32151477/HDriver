
#include <ntifs.h>
#include <windef.h>

#include <ntddk.h>
#include <strsafe.h>

#define DEVICE_NAME L"\\Device\\HDriver"
#define DOS_DEVICE_NAME L"\\DosDevices\\HDriver"
#define DEVICE_CLIENT_NAME L"\\\\.\\HDriver"

#define IOCTL_READ_MEMORY 0x9c402410
#define IOCTL_WRITE_MEMORY 0x9c402414
#define IOCTL_SET_PID 0x9c402418
#define IOCTL_SET_ADRESS 0x9c40241c

#define GetDirectoryTableOffset 0x280
#define Bit64 0x28
#define Bit32 0x18

static UINT32 g_PID;
static ULONG64 g_Address;

// CR3 �������� �ּҸ� �������� �Լ�
ULONG64 GetCR3(PEPROCESS peprocess);

// ����� ���� �ּҿ��� ���� ������ �ּҷ� ��ȯ�ϴ� �Լ�
// CR3 ���������� ���� 4��Ʈ�� 0���� �����Ͽ� ���� ��Ʈ�� ���ܽ�Ŵ
/*
	cr3: ���� �ּ�
	VirtualAddress: ���� �ּ�
*/
ULONG64 TransformationCR3(ULONG64 cr3, ULONG64 VirtualAddress);

// ���� �޸� �б� ���� �Լ�
// �� �ڵ�� ���� �ּҸ� Ŀ�� ������ �����ϰ� �ش� ���� �ּ��� �����͸� ������ ���ۿ� �о���� ����� �����մϴ�.
/*
	address: �о�� ���� �ּ�
	buffer: �о�� �����Ͱ� ����� ����
	size: �о�� ������ ũ��
	BytesTransferred: ������ �о�� ������ ũ��
*/
NTSTATUS ReadPhysicalAddress(PVOID address, PVOID buffer, SIZE_T size, SIZE_T* BytesTransferred);

// ���� �޸� ���� �Լ�
// �� �ڵ�� �����͸� ���� �ּҿ� ���� ����� �����մϴ�.
/*
	address: �� ���� �ּ�
	buffer: �� ������ ����
	size: �� ������ ũ��
	BytesTransferred: ������ �� ������ ũ��
*/
NTSTATUS WritePhysicalAddress(PVOID address, PVOID buffer, SIZE_T size, SIZE_T* BytesTransferred);

NTSTATUS ReadPhysicalAddressByVirtualAdress(UINT32 pid, ULONG64 address, BYTE* buffer, SIZE_T size, SIZE_T* BytesTransferred);
NTSTATUS WritePhysicalAddressByVirtualAdress(UINT32 pid, ULONG64 address, BYTE* buffer, SIZE_T size, SIZE_T* BytesTransferred);

typedef struct
{
	PUCHAR Position;
} MEMORYIO_DEVICE_EXTENSION, * PMEMORYIO_DEVICE_EXTENSION;
