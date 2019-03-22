#include "stdafx.h"
#include "MemLoadDll.h.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
CMemLoadDll::CMemLoadDll()
{
	m_bIsLoadOk = FALSE;
	m_pImageBase = NULL;
	m_pDllMain = NULL;
}

CMemLoadDll::~CMemLoadDll()
{
	if (m_bIsLoadOk)
	{
		// ASSERT(m_pImageBase != NULL);
		// ASSERT(m_pDllMain   != NULL);
		 //脱钩，准备卸载dll
		m_pDllMain((HINSTANCE)m_pImageBase, DLL_PROCESS_DETACH, 0);
		VirtualFree((LPVOID)m_pImageBase, 0, MEM_RELEASE);
		for (auto phDll = hDllList.begin(); phDll != hDllList.end(); phDll++)
		{
			FreeLibrary(*phDll);
		}
		hDllList.clear();
	}
}

BOOL CMemLoadDll::CMemFreeLibrary()
{
	if (m_bIsLoadOk)
	{
		// ASSERT(m_pImageBase != NULL);
		// ASSERT(m_pDllMain   != NULL);
		 //脱钩，准备卸载dll
		m_pDllMain((HINSTANCE)m_pImageBase, DLL_PROCESS_DETACH, 0);
		VirtualFree((LPVOID)m_pImageBase, 0, MEM_RELEASE);
		m_bIsLoadOk = 0;
		m_pDllMain = 0;
		m_pImageBase = 0UL;
		for (auto phDll = hDllList.begin(); phDll != hDllList.end(); phDll++)
		{
			FreeLibrary(*phDll);
		}
		hDllList.clear();
		return 1;
	}
	return 0;
}

//MemLoadLibrary函数从内存缓冲区数据中加载一个dll到当前进程的地址空间，缺省位置0x10000000
//返回值： 成功返回TRUE , 失败返回FALSE
//lpFileData: 存放dll文件数据的缓冲区
//nDataLength: 缓冲区中数据的总长度
BOOL CMemLoadDll::MemLoadLibrary(void* lpFileData, DWORD nDataLength)
{
	if (m_pImageBase != NULL)
	{
		return FALSE;  //已经加载一个dll，还没有释放，不能加载新的dll
	}
	//检查数据有效性，并初始化
	if (!CheckDataValide(lpFileData, nDataLength))
	{
		return FALSE;
	}
	//计算所需的加载空间
	int nImageSize = CalcTotalImageSize();

	if (nImageSize == 0)
	{
		return FALSE;
	}
	// 分配虚拟内存
	void *pMemoryAddress = VirtualAlloc(NULL, nImageSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	if (pMemoryAddress == NULL)
	{
		return FALSE;
	}
	else
	{
		CopyDllDatas(pMemoryAddress, lpFileData); //复制dll数据，并对齐每个段
		//重定位信息
		if (m_pNTHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress > 0
			&& m_pNTHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size > 0)
		{
			DoRelocation(pMemoryAddress);
		}
		//填充引入地址表
		if (!FillRavAddress(pMemoryAddress)) //修正引入地址表失败
		{
			VirtualFree(pMemoryAddress, 0, MEM_RELEASE);
			return FALSE;
		}
		//修改页属性。应该根据每个页的属性单独设置其对应内存页的属性。这里简化一下。
		//统一设置成一个属性PAGE_EXECUTE_READWRITE
		unsigned long unOld;

		VirtualProtect(pMemoryAddress, nImageSize, PAGE_EXECUTE_READWRITE, &unOld);
	}
	//修正基地址
	m_pNTHeader->OptionalHeader.ImageBase = (DWORD)pMemoryAddress;
	//接下来要调用一下dll的入口函数，做初始化工作。
	m_pDllMain = (ProcDllMain)(m_pNTHeader->OptionalHeader.AddressOfEntryPoint + (DWORD)pMemoryAddress);
	BOOL InitResult = m_pDllMain((HINSTANCE)pMemoryAddress, DLL_PROCESS_ATTACH, 0);

	if (!InitResult) //初始化失败
	{
		m_pDllMain((HINSTANCE)pMemoryAddress, DLL_PROCESS_DETACH, 0);
		VirtualFree(pMemoryAddress, 0, MEM_RELEASE);
		m_pDllMain = NULL;
		return FALSE;
	}

	m_bIsLoadOk = TRUE;
	m_pImageBase = (DWORD)pMemoryAddress;
	return TRUE;
}

//MemGetProcAddress函数从dll中获取指定函数的地址
//返回值： 成功返回函数地址 , 失败返回NULL
//lpProcName: 要查找函数的名字或者序号
FARPROC CMemLoadDll::MemGetProcAddress(LPCSTR lpProcName)
{
	if (m_pNTHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress == 0 ||
		m_pNTHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].Size == 0)
	{
		return NULL;
	}
	if (!m_bIsLoadOk)
	{
		return NULL;
	}

	DWORD dwOffsetStart = m_pNTHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
	DWORD dwSize = m_pNTHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].Size;

	PIMAGE_EXPORT_DIRECTORY pExport = (PIMAGE_EXPORT_DIRECTORY)((DWORD)m_pImageBase + m_pNTHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);
	int iBase = pExport->Base;
	int iNumberOfFunctions = pExport->NumberOfFunctions;
	int iNumberOfNames = pExport->NumberOfNames; //<= iNumberOfFunctions
	LPDWORD pAddressOfFunctions = (LPDWORD)(pExport->AddressOfFunctions + m_pImageBase);
	LPWORD  pAddressOfOrdinals = (LPWORD)(pExport->AddressOfNameOrdinals + m_pImageBase);
	LPDWORD pAddressOfNames = (LPDWORD)(pExport->AddressOfNames + m_pImageBase);

	int iOrdinal = -1;

	if (((DWORD)lpProcName & 0xFFFF0000) == 0) //IT IS A ORDINAL!
	{
		iOrdinal = (DWORD)lpProcName & 0x0000FFFF - iBase;
	}
	else  //use name
	{
		int iFound = -1;

		for (int i = 0; i < iNumberOfNames; i++)
		{
			char* pName = (char*)(pAddressOfNames[i] + m_pImageBase);
			if (strcmp(pName, lpProcName) == 0)
			{
				iFound = i;
				break;
			}
		}
		if (iFound >= 0)
		{
			iOrdinal = (int)(pAddressOfOrdinals[iFound]);
		}
	}

	if (iOrdinal < 0 || iOrdinal >= iNumberOfFunctions)
	{
		return NULL;
	}
	else
	{
		DWORD pFunctionOffset = pAddressOfFunctions[iOrdinal];

		if (pFunctionOffset > dwOffsetStart && pFunctionOffset < (dwOffsetStart + dwSize))//maybe Export Forwarding
		{
			return NULL;
		}
		else
		{
			return (FARPROC)(pFunctionOffset + m_pImageBase);
		}
	}

}

// 重定向PE用到的地址
void CMemLoadDll::DoRelocation(void *pNewBase)
{
	/* 重定位表的结构：
	// DWORD sectionAddress, DWORD size (包括本节需要重定位的数据)
	// 例如 1000节需要修正5个重定位数据的话，重定位表的数据是
	// 00 10 00 00   14 00 00 00      xxxx xxxx xxxx xxxx xxxx 0000
	// -----------   -----------      ----
	// 给出节的偏移  总尺寸=8+6*2     需要修正的地址           用于对齐4字节
	// 重定位表是若干个相连，如果address 和 size都是0 表示结束
	// 需要修正的地址是12位的，高4位是形态字，intel cpu下是3
	 */
	 //假设NewBase是0x600000,而文件中设置的缺省ImageBase是0x400000,则修正偏移量就是0x200000
	DWORD dwDelta = (DWORD)pNewBase - m_pNTHeader->OptionalHeader.ImageBase;

	//注意重定位表的位置可能和硬盘文件中的偏移地址不同，应该使用加载后的地址
	PIMAGE_BASE_RELOCATION pLoc = (PIMAGE_BASE_RELOCATION)((unsigned long)pNewBase
		+ m_pNTHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress);

	while ((pLoc->VirtualAddress + pLoc->SizeOfBlock) != 0) //开始扫描重定位表
	{
		WORD *pLocData = (WORD *)((int)pLoc + sizeof(IMAGE_BASE_RELOCATION));
		//计算本节需要修正的重定位项（地址）的数目
		int nNumberOfReloc = (pLoc->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);

		for (int i = 0; i < nNumberOfReloc; i++)
		{
			if ((DWORD)(pLocData[i] & 0xF000) == 0x00003000) //这是一个需要修正的地址
			{
				// 举例：
				// pLoc->VirtualAddress = 0x1000;
				// pLocData[i] = 0x313E; 表示本节偏移地址0x13E处需要修正
				// 因此 pAddress = 基地址 + 0x113E
				// 里面的内容是 A1 ( 0c d4 02 10)  汇编代码是： mov eax , [1002d40c]
				// 需要修正1002d40c这个地址
				DWORD * pAddress = (DWORD *)((unsigned long)pNewBase + pLoc->VirtualAddress + (pLocData[i] & 0x0FFF));
				*pAddress += dwDelta;
			}
		}
		//转移到下一个节进行处理
		pLoc = (PIMAGE_BASE_RELOCATION)((DWORD)pLoc + pLoc->SizeOfBlock);
	}
}

//填充引入地址表
BOOL CMemLoadDll::FillRavAddress(void *pImageBase)
{
	// 引入表实际上是一个 IMAGE_IMPORT_DESCRIPTOR 结构数组，全部是0表示结束
	// 数组定义如下：
	//
	   // DWORD   OriginalFirstThunk;         // 0表示结束，否则指向未绑定的IAT结构数组
	   // DWORD   TimeDateStamp;
	   // DWORD   ForwarderChain;             // -1 if no forwarders
	   // DWORD   Name;                       // 给出dll的名字
	   // DWORD   FirstThunk;                 // 指向IAT结构数组的地址(绑定后，这些IAT里面就是实际的函数地址)
	unsigned long nOffset = m_pNTHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;

	if (nOffset == 0)
	{
		return TRUE; //No Import Table
	}

	PIMAGE_IMPORT_DESCRIPTOR pID = (PIMAGE_IMPORT_DESCRIPTOR)((unsigned long)pImageBase + nOffset);

	while (pID->Characteristics != 0)
	{
		PIMAGE_THUNK_DATA pRealIAT = (PIMAGE_THUNK_DATA)((unsigned long)pImageBase + pID->FirstThunk);
		PIMAGE_THUNK_DATA pOriginalIAT = (PIMAGE_THUNK_DATA)((unsigned long)pImageBase + pID->OriginalFirstThunk);
		//获取dll的名字
#define NAME_BUF_SIZE 256

		char szBuf[NAME_BUF_SIZE] = ""; //dll name;
		BYTE* pName = (BYTE*)((unsigned long)pImageBase + pID->Name);
		int i = 0;
		for (i = 0; i < NAME_BUF_SIZE; i++)
		{
			if (pName[i] == 0)
			{
				break;
			}
			szBuf[i] = pName[i];
		}
		if (i >= NAME_BUF_SIZE)
		{
			return FALSE;  // bad dll name
		}
		else
		{
			szBuf[i] = 0;
		}

		HMODULE hDll = GetModuleHandleA(szBuf);

		if (hDll == NULL)
		{
			hDll = LoadLibraryA(szBuf);
			if (!hDll)
			{
				return FALSE; //NOT FOUND DLL
			}
			hDllList.push_back(hDll);
		}
		//获取DLL中每个导出函数的地址，填入IAT
		//每个IAT结构是 ：
		// union { PBYTE  ForwarderString;
			  //   PDWORD Function;
			  //   DWORD Ordinal;
			  //   PIMAGE_IMPORT_BY_NAME  AddressOfData;
		// } u1;
		// 长度是一个DWORD ，正好容纳一个地址。
		for (i = 0; ; i++)
		{
			if (pOriginalIAT[i].u1.Function == 0)
			{
				break;
			}

			FARPROC lpFunction = NULL;

			if (pOriginalIAT[i].u1.Ordinal & IMAGE_ORDINAL_FLAG) //这里的值给出的是导出序号
			{
				lpFunction = GetProcAddress(hDll, (LPCSTR)(pOriginalIAT[i].u1.Ordinal & 0x0000FFFF));
			}
			else //按照名字导入
			{
				//获取此IAT项所描述的函数名称
				PIMAGE_IMPORT_BY_NAME pByName = (PIMAGE_IMPORT_BY_NAME)
					((DWORD)pImageBase + (DWORD)(pOriginalIAT[i].u1.AddressOfData));
				//    if(pByName->Hint !=0)
				//     lpFunction = GetProcAddress(hDll, (LPCSTR)pByName->Hint);
				//    else
				lpFunction = GetProcAddress(hDll, (char *)pByName->Name);
			}
			if (lpFunction != NULL)   //找到了！
			{
				pRealIAT[i].u1.Function = (DWORD)lpFunction;
			}
			else
			{
				return FALSE;
			}
		}

		//move to next
		pID = (PIMAGE_IMPORT_DESCRIPTOR)((DWORD)pID + sizeof(IMAGE_IMPORT_DESCRIPTOR));
	}

	return TRUE;
}

//CheckDataValide函数用于检查缓冲区中的数据是否有效的dll文件
//返回值： 是一个可执行的dll则返回TRUE，否则返回FALSE。
//lpFileData: 存放dll数据的内存缓冲区
//nDataLength: dll文件的长度
BOOL CMemLoadDll::CheckDataValide(void* lpFileData, DWORD nDataLength)
{
	//检查长度
	if (nDataLength < sizeof(IMAGE_DOS_HEADER))
	{
		return FALSE;
	}
	m_pDosHeader = (PIMAGE_DOS_HEADER)lpFileData;  // DOS头
	//检查dos头的标记
	/*
	if (m_pDosHeader->e_magic != IMAGE_DOS_SIGNATURE)
	{
		return FALSE;  //0x5A4D : MZ
	}
	*/

	//检查长度
	if ((DWORD)nDataLength < (m_pDosHeader->e_lfanew + sizeof(IMAGE_NT_HEADERS)))
	{
		return FALSE;
	}
	//取得pe头
	m_pNTHeader = (PIMAGE_NT_HEADERS)((unsigned long)lpFileData + m_pDosHeader->e_lfanew); // PE头
	//检查pe头的合法性
	if (m_pNTHeader->Signature != IMAGE_NT_SIGNATURE)
	{
		return FALSE;  //0x00004550 : PE00
	}
	if ((m_pNTHeader->FileHeader.Characteristics & IMAGE_FILE_DLL) == 0) //0x2000  : File is a DLL
	{
		return FALSE;
	}
	if ((m_pNTHeader->FileHeader.Characteristics & IMAGE_FILE_EXECUTABLE_IMAGE) == 0) //0x0002 : 指出文件可以运行
	{
		return FALSE;
	}
	if (m_pNTHeader->FileHeader.SizeOfOptionalHeader != sizeof(IMAGE_OPTIONAL_HEADER))
	{
		return FALSE;
	}

	//取得节表（段表）
	m_pSectionHeader = (PIMAGE_SECTION_HEADER)((int)m_pNTHeader + sizeof(IMAGE_NT_HEADERS));
	//验证每个节表的空间
	for (int i = 0; i < m_pNTHeader->FileHeader.NumberOfSections; i++)
	{
		if ((m_pSectionHeader[i].PointerToRawData + m_pSectionHeader[i].SizeOfRawData) > (DWORD)nDataLength)
		{
			return FALSE;
		}
	}

	return TRUE;
}

//计算对齐边界
int CMemLoadDll::GetAlignedSize(int nOrigin, int nAlignment)
{
	return (nOrigin + nAlignment - 1) / nAlignment * nAlignment;
}

//计算整个dll映像文件的尺寸
int CMemLoadDll::CalcTotalImageSize()
{
	int nSize;

	if (m_pNTHeader == NULL)
	{
		return 0;
	}

	int nAlign = m_pNTHeader->OptionalHeader.SectionAlignment; //段对齐字节数

	// 计算所有头的尺寸。包括dos, coff, pe头 和 段表的大小
	nSize = GetAlignedSize(m_pNTHeader->OptionalHeader.SizeOfHeaders, nAlign);
	// 计算所有节的大小
	for (int i = 0; i < m_pNTHeader->FileHeader.NumberOfSections; ++i)
	{
		//得到该节的大小
		int nCodeSize = m_pSectionHeader[i].Misc.VirtualSize;
		int nLoadSize = m_pSectionHeader[i].SizeOfRawData;
		int nMaxSize = (nLoadSize > nCodeSize) ? (nLoadSize) : (nCodeSize);
		int nSectionSize = GetAlignedSize(m_pSectionHeader[i].VirtualAddress + nMaxSize, nAlign);

		if (nSize < nSectionSize)
		{
			nSize = nSectionSize;  //Use the Max;
		}
	}

	return nSize;
}

//CopyDllDatas函数将dll数据复制到指定内存区域，并对齐所有节
//pSrc: 存放dll数据的原始缓冲区
//pDest:目标内存地址
void CMemLoadDll::CopyDllDatas(void* pDest, void* pSrc)
{
	// 计算需要复制的PE头+段表字节数
	int  nHeaderSize = m_pNTHeader->OptionalHeader.SizeOfHeaders;
	int  nSectionSize = m_pNTHeader->FileHeader.NumberOfSections * sizeof(IMAGE_SECTION_HEADER);
	int  nMoveSize = nHeaderSize + nSectionSize;
	//复制头和段信息
	memcpy(pDest, pSrc, nMoveSize);

	//复制每个节
	for (int i = 0; i < m_pNTHeader->FileHeader.NumberOfSections; ++i)
	{
		if (m_pSectionHeader[i].VirtualAddress == 0 || m_pSectionHeader[i].SizeOfRawData == 0)
		{
			continue;
		}
		// 定位该节在内存中的位置
		void *pSectionAddress = (void *)((unsigned long)pDest + m_pSectionHeader[i].VirtualAddress);
		// 复制段数据到虚拟内存
		memmove((void *)pSectionAddress,
			(void *)((DWORD)pSrc + m_pSectionHeader[i].PointerToRawData), m_pSectionHeader[i].SizeOfRawData);
	}

	//修正指针，指向新分配的内存
	//新的dos头
	m_pDosHeader = (PIMAGE_DOS_HEADER)pDest;
	//新的pe头地址
	m_pNTHeader = (PIMAGE_NT_HEADERS)((int)pDest + (m_pDosHeader->e_lfanew));
	//新的节表地址
	m_pSectionHeader = (PIMAGE_SECTION_HEADER)((int)m_pNTHeader + sizeof(IMAGE_NT_HEADERS));
}