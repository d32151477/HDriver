
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

// CR3 레지스터 주소를 가져오는 함수
ULONG64 GetCR3(PEPROCESS peprocess);

// 사용자 가상 주소에서 물리 페이지 주소로 변환하는 함수
// CR3 레지스터의 하위 4비트를 0으로 설정하여 정렬 비트를 제외시킴
/*
	cr3: 물리 주소
	VirtualAddress: 가상 주소
*/
ULONG64 TransformationCR3(ULONG64 cr3, ULONG64 VirtualAddress);

// 물리 메모리 읽기 래핑 함수
// 이 코드는 물리 주소를 커널 공간에 매핑하고 해당 물리 주소의 데이터를 지정한 버퍼에 읽어오는 기능을 구현합니다.
/*
	address: 읽어올 물리 주소
	buffer: 읽어온 데이터가 저장될 버퍼
	size: 읽어올 데이터 크기
	BytesTransferred: 실제로 읽어온 데이터 크기
*/
NTSTATUS ReadPhysicalAddress(PVOID address, PVOID buffer, SIZE_T size, SIZE_T* BytesTransferred);

// 물리 메모리 쓰기 함수
// 이 코드는 데이터를 물리 주소에 쓰는 기능을 구현합니다.
/*
	address: 쓸 물리 주소
	buffer: 쓸 데이터 버퍼
	size: 쓸 데이터 크기
	BytesTransferred: 실제로 쓴 데이터 크기
*/
NTSTATUS WritePhysicalAddress(PVOID address, PVOID buffer, SIZE_T size, SIZE_T* BytesTransferred);

NTSTATUS ReadPhysicalAddressByVirtualAdress(UINT32 pid, ULONG64 address, BYTE* buffer, SIZE_T size, SIZE_T* BytesTransferred);
NTSTATUS WritePhysicalAddressByVirtualAdress(UINT32 pid, ULONG64 address, BYTE* buffer, SIZE_T size, SIZE_T* BytesTransferred);

typedef struct
{
	PUCHAR Position;
} MEMORYIO_DEVICE_EXTENSION, * PMEMORYIO_DEVICE_EXTENSION;
