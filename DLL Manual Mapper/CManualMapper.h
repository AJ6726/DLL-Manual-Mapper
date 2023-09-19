#pragma once
#include "CProcess.h"
#include <vector>

struct StubParameters
{
	uintptr_t moduleBase;
	IMAGE_NT_HEADERS NtHeaders;
	uintptr_t entryPoint;
	HMODULE(__stdcall* fnLoadLibraryA)(LPCSTR moduleName);
	FARPROC(__stdcall* fnGetProcAddress)(HMODULE, const char*);

};

struct CManualMapper : CProcess
{
	void LoadDll(std::string fullDllPath)
	{
		GetDllBytes(fullDllPath);
		GetDllHeaders(); 
		
		AllocateMemoryOnProcess();

		WriteDllSectionsToProcess(); 

		FixModuleRelocations();
		
		CallManualMapStub();

		FreeMemoryOnProcess();
	}

	//Gets the dll bytes on file 
	void GetDllBytes(std::string fullDllPath);

	void GetDllHeaders(); 

	//Allocates space for module and for manual map stub with its params
	void AllocateMemoryOnProcess(); 

	void WriteDllSectionsToProcess(); 
	
	//Fixes Relocations externally
	void FixModuleRelocations(); 
	
	//Writes stub into process and calls it
	void CallManualMapStub(); 

	void FreeMemoryOnProcess(); 

	std::vector<BYTE> m_dllBytes;
	IMAGE_DOS_HEADER m_DosHeader;
	IMAGE_NT_HEADERS m_NtHeaders;

	//All addresses
	uintptr_t m_moduleBase;  //uintptr_t for arithmetic
	void* m_stubCaller; 
	void* m_stubParams; 
};