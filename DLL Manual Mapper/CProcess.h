#pragma once
#include <Windows.h>
#include <string>
struct CProcess
{
	void GetProcessId(std::string processName); 
	void GetProcessHandle();

	int m_id; 
	HANDLE m_handle; 


	template <typename T, typename J>
	T ReadMemory(J address)
	{
		T buffer; 
		ReadProcessMemory(m_handle, reinterpret_cast<void*>(address), &buffer, sizeof(T), nullptr); 
		return buffer;
	}

	template <typename T, typename J>
	void WriteMemory(T address, J newValue)
	{
		WriteProcessMemory(m_handle, reinterpret_cast<void*>(address), &newValue, sizeof(J), nullptr); 
	}

	template <typename T>
	void WriteMemory(T address, void* newBytes, size_t totalBytes)
	{
		WriteProcessMemory(m_handle, reinterpret_cast<void*>(address), newBytes, totalBytes , nullptr);
	}
};
