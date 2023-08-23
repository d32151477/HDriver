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

		// ���� ������ ���� �������� ����
		PVOID map = MmMapIoSpace(Write, size, (MEMORY_CACHING_TYPE)PAGE_READWRITE);
		if (map)
		{
			// ������ ���� ����
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
	// ���μ��� ID�κ��� EPROCESS ���
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
				// CR3�� ��ȯ �� ���� ����
				ULONG64 PhysicalAddress = TransformationCR3(CR3, address + index);
				if (!PhysicalAddress)
					return STATUS_UNSUCCESSFUL;

				ULONG64 read = min(PAGE_SIZE - (PhysicalAddress & 0xfff), size);
				SIZE_T BytesTransferred = 0;

				// ���� �޸� �б�
				// reinterpret_cast�� PVOID Ÿ������ ���� ��ȯ
				Status = ReadPhysicalAddress(reinterpret_cast<PVOID>(PhysicalAddress), reinterpret_cast<PVOID>((PVOID*)buffer + read), read, &BytesTransferred);
				size -= BytesTransferred;
				index += BytesTransferred;

				if (!NT_SUCCESS(Status))
					break;

				if (!BytesTransferred)
					break;
			}
		}
		// ���� ����
		ObDereferenceObject(PEProcess);
	}
	return Status;
}

NTSTATUS WritePhysicalAddressByVirtualAdress(UINT32 pid, ULONG64 address, BYTE* buffer, SIZE_T size, SIZE_T* BytesTransferred)
{
	// ���μ��� ID�κ��� EPROCESS ���
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
				// CR3�� ��ȯ �� ���� ����
				ULONG64 PhysicalAddress = TransformationCR3(CR3, address + index);
				if (!PhysicalAddress)
					return STATUS_UNSUCCESSFUL;

				// ���� �޸� ����
				ULONG64 WriteSize = min(PAGE_SIZE - (PhysicalAddress & 0xfff), size);
				SIZE_T BytesTransferred = 0;

				// reinterpret_cast�� PVOID Ÿ������ ���� ��ȯ
				Status = WritePhysicalAddress(reinterpret_cast<PVOID>(PhysicalAddress), reinterpret_cast<PVOID>(buffer + index), WriteSize, &BytesTransferred);

				size -= BytesTransferred;
				index += BytesTransferred;

				// DbgPrint("[���� ������] => %d | %0x02X \n", WriteSize, ReadBuffer + read);
				if (!NT_SUCCESS(Status))
					break;

				if (!BytesTransferred)
					break;
			}
		}
		// ���� ����
		ObDereferenceObject(PEProcess);
	}
	return Status;
}


ULONG64 GetCR3(PEPROCESS pEprocess)
{
	// ��ȯ�� ���� CR3 ���
	PUCHAR Var = reinterpret_cast<PUCHAR>(pEprocess);
	ULONG64 CR3 = *(ULONG64*)(Var + Bit64);
	if (!CR3)
		CR3 = *(ULONG64*)(Var + GetDirectoryTableOffset);

	return CR3;
}

ULONG64 TransformationCR3(ULONG64 cr3, ULONG64 VirtualAddress)
{
	cr3 &= ~0xf;
	// ������ ������ ��������
	ULONG64 PAGE_OFFSET = VirtualAddress & ~(~0ul << 12);

	// ���� �ּҰ� ���� 3�ܰ� ������ ���̺� �׸� �б�
	SIZE_T BytesTransferred = 0;
	ULONG64 a = 0, b = 0, c = 0;

	ReadPhysicalAddress((PVOID)(cr3 + 8 * ((VirtualAddress >> 39) & (0x1ffll))), &a, sizeof(a), &BytesTransferred);

	// P ��Ʈ�� 0�̸�, �ش� ������ ���̺� �׸��� ���� �޸𸮿� ���εǾ� ���� ������ �ǹ��ϹǷ� 0 ��ȯ
	if (~a & 1)
	{
		return 0;
	}

	// ���� �ּҰ� ���� 2�ܰ� ������ ���̺� �׸� �б�
	ReadPhysicalAddress((PVOID)((a & ((~0xfull << 8) & 0xfffffffffull)) + 8 * ((VirtualAddress >> 30) & (0x1ffll))), &b, sizeof(b), &BytesTransferred);

	// P ��Ʈ�� 0�̸�, �ش� ������ ���̺� �׸��� ���� �޸𸮿� ���εǾ� ���� ������ �ǹ��ϹǷ� 0 ��ȯ
	if (~b & 1)
	{
		return 0;
	}

	// PS ��Ʈ�� 1�̸�, �ش� ������ ���̺� �׸��� 1GB ���� �޸𸮸� �����ϰ� �����Ƿ� ���� �ּҸ� ���� ����Ͽ� ��ȯ
	if (b & 0x80)
	{
		return (b & (~0ull << 42 >> 12)) + (VirtualAddress & ~(~0ull << 30));
	}

	// ���� �ּҰ� ���� 1�ܰ� ������ ���̺� �׸� �б�
	ReadPhysicalAddress((PVOID)((b & ((~0xfull << 8) & 0xfffffffffull)) + 8 * ((VirtualAddress >> 21) & (0x1ffll))), &c, sizeof(c), &BytesTransferred);

	// P ��Ʈ�� 0�̸�, �ش� ������ ���̺� �׸��� ���� �޸𸮿� ���εǾ� ���� ������ �ǹ��ϹǷ� 0 ��ȯ
	if (~c & 1)
	{
		return 0;
	}
	// PS ��Ʈ�� 1�̸�, �ش� ������ ���̺� �׸��� 2MB ���� �޸𸮸� �����ϰ� �����Ƿ� ���� �ּҸ� ���� ����Ͽ� ��ȯ
	if (c & 0x80)
	{
		return (c & ((~0xfull << 8) & 0xfffffffffull)) + (VirtualAddress & ~(~0ull << 21));
	}
	// ���� �ּҰ� ���� 0�ܰ� ������ ���̺� �׸� �б� �� ���� �ּ� ����Ͽ� ��ȯ
	ULONG64 address = 0;
	ReadPhysicalAddress((PVOID)((c & ((~0xfull << 8) & 0xfffffffffull)) + 8 * ((VirtualAddress >> 12) & (0x1ffll))), &address, sizeof(address), &BytesTransferred);
	address &= ((~0xfull << 8) & 0xfffffffffull);
	if (!address)
	{
		return 0;
	}

	return address + PAGE_OFFSET;
}