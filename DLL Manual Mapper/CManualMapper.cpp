#include "CManualMapper.h"

void CManualMapper::GetDllBytes(std::string fullDllPath)
{
	HANDLE hDllFile = CreateFileA(fullDllPath.c_str(), GENERIC_ALL, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	int dllFileSize = GetFileSize(hDllFile, nullptr);
	m_dllBytes.resize(dllFileSize);

	ReadFile(hDllFile, m_dllBytes.data(), dllFileSize, nullptr, nullptr);
}

void CManualMapper::GetDllHeaders()
{
	m_DosHeader = *reinterpret_cast<IMAGE_DOS_HEADER*>(m_dllBytes.data());
	m_NtHeaders = *reinterpret_cast<IMAGE_NT_HEADERS*>(m_dllBytes.data() + m_DosHeader.e_lfanew);
}

void CManualMapper::AllocateMemoryOnProcess()
{
	m_moduleBase = reinterpret_cast<uintptr_t>(VirtualAllocEx(m_handle, nullptr, m_NtHeaders.OptionalHeader.SizeOfImage, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
	m_stubCaller = VirtualAllocEx(m_handle, nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	m_stubParams = VirtualAllocEx(m_handle, nullptr, sizeof(StubParameters), MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
}

void CManualMapper::WriteDllSectionsToProcess()
{
	for (int i = 0; i < m_NtHeaders.FileHeader.NumberOfSections; i++)
	{
		IMAGE_SECTION_HEADER CurrentSection = *reinterpret_cast<IMAGE_SECTION_HEADER*>(m_dllBytes.data() + m_DosHeader.e_lfanew + sizeof(IMAGE_NT_HEADERS) + (i * sizeof(IMAGE_SECTION_HEADER)));
		WriteMemory(m_moduleBase + CurrentSection.VirtualAddress, m_dllBytes.data() + CurrentSection.PointerToRawData, CurrentSection.SizeOfRawData);
	}
}

void CManualMapper::FixModuleRelocations()
{
	struct IMAGE_RELOCATION_ENTRY
	{
		WORD offset : 12; //Rva from current base relocation block 
		WORD type : 4;
	};

	uintptr_t moduleBaseOffset = m_moduleBase - m_NtHeaders.OptionalHeader.ImageBase;

	uintptr_t base_reloc_block_address = m_moduleBase + m_NtHeaders.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress;

	while (true)
	{
		IMAGE_BASE_RELOCATION BaseRelocationBlock = ReadMemory<IMAGE_BASE_RELOCATION>(base_reloc_block_address);

		if (!BaseRelocationBlock.SizeOfBlock)
			break;

		int relocationsCount = (BaseRelocationBlock.SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(IMAGE_RELOCATION_ENTRY);

		for (int i = 0; i < relocationsCount; i++)
		{
			IMAGE_RELOCATION_ENTRY RelocationEntry = ReadMemory<IMAGE_RELOCATION_ENTRY>(base_reloc_block_address + (i * sizeof(IMAGE_RELOCATION_ENTRY)));

			uintptr_t valueAddress = m_moduleBase + BaseRelocationBlock.VirtualAddress + RelocationEntry.offset;
			if (RelocationEntry.type == IMAGE_REL_BASED_DIR64)
			{
				uintptr_t value = ReadMemory<uintptr_t>(valueAddress);
				WriteMemory(valueAddress, value + moduleBaseOffset);
			}

			if (RelocationEntry.type == IMAGE_REL_BASED_HIGHLOW)
			{
				DWORD value = ReadMemory<DWORD>(valueAddress);
				WriteMemory(valueAddress, value + moduleBaseOffset);

			}
		}

		base_reloc_block_address += BaseRelocationBlock.SizeOfBlock;
	}
}

DWORD ManualMapStub(void* parameters)
{
	StubParameters* StubParams = reinterpret_cast<StubParameters*>(parameters);

	uintptr_t moduleBase = StubParams->moduleBase;
	IMAGE_NT_HEADERS NtHeaders = StubParams->NtHeaders;

	HMODULE(__stdcall* fnLoadLibraryA)(LPCSTR) = StubParams->fnLoadLibraryA;
	FARPROC(__stdcall* fnGetProcAddress)(HMODULE, const char*) = StubParams->fnGetProcAddress;


	IMAGE_IMPORT_DESCRIPTOR* ImportDescriptor = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(moduleBase + NtHeaders.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);

	for (; ; ImportDescriptor++)
	{
		//End of descriptors is a zeroed out descriptor
		if (!ImportDescriptor->Characteristics)
			break;

		const char* moduleName = reinterpret_cast<char*>(moduleBase + ImportDescriptor->Name);
		HMODULE hModule = StubParams->fnLoadLibraryA(moduleName);
		
		//OriginalThunk is start of Import Name Table and FirstThunk is start of Import Address Table
		IMAGE_THUNK_DATA* OriginalThunk = reinterpret_cast<IMAGE_THUNK_DATA*>(moduleBase + ImportDescriptor->OriginalFirstThunk); 
		IMAGE_THUNK_DATA* FirstThunk = reinterpret_cast<IMAGE_THUNK_DATA*>(moduleBase + ImportDescriptor->FirstThunk); 

		for (; ; OriginalThunk++, FirstThunk++)
		{
			//End of thunks are a zeroed out thunk 
			if (!OriginalThunk->u1.AddressOfData)
				break;

			IMAGE_IMPORT_BY_NAME* ImportedFunction = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(moduleBase + OriginalThunk->u1.AddressOfData);

			uintptr_t functionAddress = reinterpret_cast<uintptr_t>(fnGetProcAddress(hModule, ImportedFunction->Name));
			FirstThunk->u1.Function = functionAddress;
		}

	}

	using pDllMain = BOOL(__stdcall*)(HMODULE, DWORD, LPVOID);
	pDllMain fnDllMain = reinterpret_cast<pDllMain>(StubParams->entryPoint);
	fnDllMain(reinterpret_cast<HMODULE>(moduleBase), 1, nullptr);

	return 0;
}

void CManualMapper::CallManualMapStub()
{
	StubParameters StubParams = { 
		m_moduleBase, 
		m_NtHeaders, 
		m_moduleBase + m_NtHeaders.OptionalHeader.AddressOfEntryPoint,
		LoadLibraryA,
		GetProcAddress };

	WriteMemory(m_stubParams, StubParams);
	WriteMemory(m_stubCaller, ManualMapStub, 4096);

	HANDLE hEntryCallerThread = CreateRemoteThread(m_handle, nullptr, 0, static_cast<LPTHREAD_START_ROUTINE>(m_stubCaller), m_stubParams, 0, nullptr);
	if (hEntryCallerThread)
		CloseHandle(hEntryCallerThread);
}


void CManualMapper::FreeMemoryOnProcess()
{
	VirtualFreeEx(m_handle, reinterpret_cast<void*>(m_moduleBase), m_NtHeaders.OptionalHeader.SizeOfImage, MEM_RELEASE | MEM_DECOMMIT); 
	VirtualFreeEx(m_handle, m_stubParams, sizeof(StubParameters), MEM_RELEASE | MEM_DECOMMIT);
	VirtualFreeEx(m_handle, m_stubCaller, 4096, MEM_RELEASE | MEM_DECOMMIT); 
}