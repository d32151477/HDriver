#include "HDriver.h"

NTSTATUS ReadPhysicalAddress(PVOID address, PVOID buffer, SIZE_T size, SIZE_T* BytesTransferred)
{
	MM_COPY_ADDRESS Read = { 0 };
	Read.PhysicalAddress.QuadPart = (LONG64)address;
	return MmCopyMemory(buffer, Read, size, MM_COPY_MEMORY_PHYSICAL, BytesTransferred);
}

NTSTATUS WritePhysicalAddress(PVOID address, PVOID buffer, SIZE_T size, SIZE_T* BytesTransferred)
{
	if (address)
	{
		PHYSICAL_ADDRESS Write = { 0 };
		Write.QuadPart = (LONG64)address;

		// 물리 공간을 가상 공간으로 매핑
		PVOID map = MmMapIoSpace(Write, size, (MEMORY_CACHING_TYPE)PAGE_READWRITE);
		if (map)
		{
			// 데이터 복사 시작
			RtlCopyMemory(map, buffer, size);
			*BytesTransferred = size;
			MmUnmapIoSpace(map, size);
			return STATUS_SUCCESS;
		}
	}
	return STATUS_UNSUCCESSFUL;
}

NTSTATUS ReadPhysicalAddressByVirtualAdress(UINT32 pid, ULONG64 address, BYTE* buffer, SIZE_T size, SIZE_T* BytesTransferred)
{
	// 프로세스 ID로부터 EPROCESS 얻기
	PEPROCESS PEProcess = NULL;
	NTSTATUS Status = PsLookupProcessByProcessId((HANDLE)pid, &PEProcess);

	if (NT_SUCCESS(Status))
	{
		ULONG64 CR3 = GetCR3(PEProcess);
		ULONG64 PhysicalAddress = TransformationCR3(CR3, address);

		if (PhysicalAddress)
		{
			SIZE_T index = 0;
			while (size)
			{
				// CR3로 변환 및 루프 시작
				ULONG64 PhysicalAddress = TransformationCR3(CR3, address + index);
				if (!PhysicalAddress)
					return STATUS_UNSUCCESSFUL;

				ULONG64 read = min(PAGE_SIZE - (PhysicalAddress & 0xfff), size);
				SIZE_T BytesTransferred = 0;

				// 물리 메모리 읽기
				// reinterpret_cast로 PVOID 타입으로 강제 변환
				Status = ReadPhysicalAddress(reinterpret_cast<PVOID>(PhysicalAddress), reinterpret_cast<PVOID>((PVOID*)buffer + read), read, &BytesTransferred);
				size -= BytesTransferred;
				index += BytesTransferred;

				if (!NT_SUCCESS(Status))
					break;

				if (!BytesTransferred)
					break;
			}
		}
		// 참조 해제
		ObDereferenceObject(PEProcess);
	}
	return Status;
}

NTSTATUS WritePhysicalAddressByVirtualAdress(UINT32 pid, ULONG64 address, BYTE* buffer, SIZE_T size, SIZE_T* BytesTransferred)
{
	// 프로세스 ID로부터 EPROCESS 얻기
	PEPROCESS PEProcess = NULL;
	NTSTATUS Status = PsLookupProcessByProcessId((HANDLE)pid, &PEProcess);

	if (NT_SUCCESS(Status))
	{
		ULONG64 CR3 = GetCR3(PEProcess);
		ULONG64 PhysicalAddress = TransformationCR3(CR3, address);

		if (PhysicalAddress)
		{
			SIZE_T index = 0;
			while (size)
			{
				// CR3로 변환 및 루프 시작
				ULONG64 PhysicalAddress = TransformationCR3(CR3, address + index);
				if (!PhysicalAddress)
					return STATUS_UNSUCCESSFUL;

				// 물리 메모리 쓰기
				ULONG64 WriteSize = min(PAGE_SIZE - (PhysicalAddress & 0xfff), size);
				SIZE_T BytesTransferred = 0;

				// reinterpret_cast로 PVOID 타입으로 강제 변환
				Status = WritePhysicalAddress(reinterpret_cast<PVOID>(PhysicalAddress), reinterpret_cast<PVOID>(buffer + index), WriteSize, &BytesTransferred);

				size -= BytesTransferred;
				index += BytesTransferred;

				// DbgPrint("[쓰인 데이터] => %d | %0x02X \n", WriteSize, ReadBuffer + read);
				if (!NT_SUCCESS(Status))
					break;

				if (!BytesTransferred)
					break;
			}
		}
		// 참조 해제
		ObDereferenceObject(PEProcess);
	}
	return Status;
}


ULONG64 GetCR3(PEPROCESS pEprocess)
{
	// 변환을 위한 CR3 얻기
	PUCHAR Var = reinterpret_cast<PUCHAR>(pEprocess);
	ULONG64 CR3 = *(ULONG64*)(Var + Bit64);
	if (!CR3)
		CR3 = *(ULONG64*)(Var + GetDirectoryTableOffset);

	return CR3;
}

ULONG64 TransformationCR3(ULONG64 cr3, ULONG64 VirtualAddress)
{
	cr3 &= ~0xf;
	// 페이지 오프셋 가져오기
	ULONG64 PAGE_OFFSET = VirtualAddress & ~(~0ul << 12);

	// 가상 주소가 속한 3단계 페이지 테이블 항목 읽기
	SIZE_T BytesTransferred = 0;
	ULONG64 a = 0, b = 0, c = 0;

	ReadPhysicalAddress((PVOID)(cr3 + 8 * ((VirtualAddress >> 39) & (0x1ffll))), &a, sizeof(a), &BytesTransferred);

	// P 비트가 0이면, 해당 페이지 테이블 항목이 물리 메모리에 매핑되어 있지 않음을 의미하므로 0 반환
	if (~a & 1)
	{
		return 0;
	}

	// 가상 주소가 속한 2단계 페이지 테이블 항목 읽기
	ReadPhysicalAddress((PVOID)((a & ((~0xfull << 8) & 0xfffffffffull)) + 8 * ((VirtualAddress >> 30) & (0x1ffll))), &b, sizeof(b), &BytesTransferred);

	// P 비트가 0이면, 해당 페이지 테이블 항목이 물리 메모리에 매핑되어 있지 않음을 의미하므로 0 반환
	if (~b & 1)
	{
		return 0;
	}

	// PS 비트가 1이면, 해당 페이지 테이블 항목이 1GB 물리 메모리를 매핑하고 있으므로 물리 주소를 직접 계산하여 반환
	if (b & 0x80)
	{
		return (b & (~0ull << 42 >> 12)) + (VirtualAddress & ~(~0ull << 30));
	}

	// 가상 주소가 속한 1단계 페이지 테이블 항목 읽기
	ReadPhysicalAddress((PVOID)((b & ((~0xfull << 8) & 0xfffffffffull)) + 8 * ((VirtualAddress >> 21) & (0x1ffll))), &c, sizeof(c), &BytesTransferred);

	// P 비트가 0이면, 해당 페이지 테이블 항목이 물리 메모리에 매핑되어 있지 않음을 의미하므로 0 반환
	if (~c & 1)
	{
		return 0;
	}
	// PS 비트가 1이면, 해당 페이지 테이블 항목이 2MB 물리 메모리를 매핑하고 있으므로 물리 주소를 직접 계산하여 반환
	if (c & 0x80)
	{
		return (c & ((~0xfull << 8) & 0xfffffffffull)) + (VirtualAddress & ~(~0ull << 21));
	}
	// 가상 주소가 속한 0단계 페이지 테이블 항목 읽기 후 물리 주소 계산하여 반환
	ULONG64 address = 0;
	ReadPhysicalAddress((PVOID)((c & ((~0xfull << 8) & 0xfffffffffull)) + 8 * ((VirtualAddress >> 12) & (0x1ffll))), &address, sizeof(address), &BytesTransferred);
	address &= ((~0xfull << 8) & 0xfffffffffull);
	if (!address)
	{
		return 0;
	}

	return address + PAGE_OFFSET;
}