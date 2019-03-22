
// MiniScreenshotDlg.cpp: 实现文件
//

#include "stdafx.h"
#include "MiniScreenshot.h"
#include "MiniScreenshotDlg.h"
#include "afxdialogex.h"
#include "resource.h"
#include "file.hpp"


#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CMiniScreenshotDlg 对话框

#define JP_ONEXIT 12581



CMiniScreenshotDlg::CMiniScreenshotDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_MINISCREENSHOT_DIALOG, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDI_ICON1);
}

void CMiniScreenshotDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CMiniScreenshotDlg, CDialogEx)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDOK, &CMiniScreenshotDlg::OnBnClickedOk)
	ON_BN_CLICKED(IDC_BUTTON2, &CMiniScreenshotDlg::OnBnClickedButton2)
	ON_BN_CLICKED(IDC_BUTTON1, &CMiniScreenshotDlg::OnBnClickedButton1)
	ON_MESSAGE(12580, &CMiniScreenshotDlg::OnPcsTaskBar)
	ON_WM_CLOSE()
	ON_COMMAND(JP_ONEXIT,&CMiniScreenshotDlg::OnJpOnexit)
	ON_BN_CLICKED(IDC_BUTTON3, &CMiniScreenshotDlg::OnBnClickedButton3)
	ON_MESSAGE(WM_HOTKEY, OnHotKey)
	ON_BN_CLICKED(IDC_BUTTON4, &CMiniScreenshotDlg::OnBnClickedButton4)
	ON_BN_CLICKED(IDC_BUTTON5, &CMiniScreenshotDlg::OnBnClickedButton5)
	ON_BN_CLICKED(IDC_BUTTON6, &CMiniScreenshotDlg::OnBnClickedButton6)
END_MESSAGE_MAP()


// CMiniScreenshotDlg 消息处理程序


VOID CMiniScreenshotDlg::InitializeDll()
{
	auto InitFunc = [&]() {
		DoCleanUp();
		if (!theMutex.try_lock())return;
		theHeap = HeapCreate(HEAP_CREATE_ENABLE_EXECUTE, 0, 0);
		if (!theHeap)
		{
			if (::MessageBox(::GetForegroundWindow(), TEXT("初始化失败，程序异常。点确认尝试重新启动程序"),
				TEXT("SK团队提示"), MB_ICONERROR | MB_OKCANCEL) == IDOK)
			{
				TCHAR theString[MAX_PATH];
				GetModuleFileName(NULL, theString, MAX_PATH * sizeof(TCHAR));
				STARTUPINFO stInfo = { 0 };
				stInfo.cb = sizeof(STARTUPINFO);
				stInfo.dwFlags = STARTF_USESHOWWINDOW;
				stInfo.wShowWindow = 1;
				PROCESS_INFORMATION pInfo = { 0 };
				auto hNewProcess =
					CreateProcess(theString, TEXT(""), NULL, NULL, NULL, DETACHED_PROCESS, NULL, NULL, &stInfo, &pInfo);
				if (hNewProcess)
				{
					CloseHandle(pInfo.hProcess);
					CloseHandle(pInfo.hThread);
				}
				else
				{
					::MessageBox(::GetForegroundWindow(), TEXT("重启进程失败。"),
						TEXT("SK团队提示"), MB_ICONERROR);
				}
			}
			ExitProcess(-1);
		}

		auto lpMemoryRead = HeapAlloc(theHeap, NULL, sizeof(thisFile));
		if (!lpMemoryRead)
		{
			::MessageBox(::GetForegroundWindow(), TEXT("内存不足。"),
				TEXT("SK团队提示"), MB_ICONERROR);
			ExitProcess(-1);
		}

		CopyMemory(lpMemoryRead, thisFile, sizeof(thisFile));
		char* lpData = reinterpret_cast<char*>(lpMemoryRead);
		for (auto Pcs = 0UL; Pcs < sizeof(thisFile); Pcs++)
		{
			lpData[Pcs] ^= 0x75;
		}

		if (!DllHandler.MemLoadLibrary(lpMemoryRead, sizeof(thisFile)))
		{
			if (::MessageBox(::GetForegroundWindow(), TEXT("装载程序失败，确认以重新启动，请确保文件完整性。"),
				TEXT("SK团队提示"), MB_ICONERROR | MB_OKCANCEL) == IDOK)
			{
				TCHAR theString[MAX_PATH];
				GetModuleFileName(NULL, theString, MAX_PATH * sizeof(TCHAR));
				STARTUPINFO stInfo = { 0 };
				stInfo.cb = sizeof(STARTUPINFO);
				stInfo.dwFlags = STARTF_USESHOWWINDOW;
				stInfo.wShowWindow = 1;
				PROCESS_INFORMATION pInfo = { 0 };
				auto hNewProcess =
					CreateProcess(theString, TEXT(""), NULL, NULL, NULL, DETACHED_PROCESS, NULL, NULL, &stInfo, &pInfo);
				if (hNewProcess)
				{
					CloseHandle(pInfo.hProcess);
					CloseHandle(pInfo.hThread);
				}
				else
				{
					::MessageBox(::GetForegroundWindow(), TEXT("重启进程失败。"),
						TEXT("SK团队提示"), MB_ICONERROR);
				}
			}
			ExitProcess(-1);
		}

		if (!HeapFree(theHeap, NULL, lpMemoryRead))
		{
			::MessageBox(::GetForegroundWindow(), TEXT("内存释放的时候出现问题。"),
				TEXT("SK团队提示"), MB_ICONERROR);
		}
		bIsInitOK = TRUE;
		theMutex.unlock();
	};

	thread(InitFunc).detach();
}

VOID CMiniScreenshotDlg::DoCleanUp()
{
	if (bIsInitOK)
	{
		if (!DllHandler.CMemFreeLibrary())
		{
			goto ErrStep;
		}
		if (theHeap)
			if (!HeapDestroy(theHeap))goto ErrStep;
		goto OKStep;
	ErrStep:
		MessageBox(TEXT("初始化程序的时候发生了一个异常，程序可能会遇到未知问题。"), TEXT("SK团队-初始化异常"), MB_ICONERROR);
	OKStep:
		bIsInitOK = FALSE;
	}
}

BOOL CMiniScreenshotDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 设置此对话框的图标。  当应用程序主窗口不是对话框时，框架将自动
	//  执行此操作
	SetIcon(m_hIcon, TRUE);			// 设置大图标
	SetIcon(m_hIcon, FALSE);		// 设置小图标

	// TODO: 在此添加额外的初始化代码

	InitializeDll();

	return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}

// 如果向对话框添加最小化按钮，则需要下面的代码
//  来绘制该图标。  对于使用文档/视图模型的 MFC 应用程序，
//  这将由框架自动完成。

void CMiniScreenshotDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 用于绘制的设备上下文

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 使图标在工作区矩形中居中
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 绘制图标
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

//当用户拖动最小化窗口时系统调用此函数取得光标
//显示。
HCURSOR CMiniScreenshotDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}



void CMiniScreenshotDlg::OnBnClickedOk()
{
	// TODO: 在此添加控件通知处理程序代码
	if (MessageBox(TEXT("点确认退出，点取消则进入托盘后台。"), TEXT("退出确认"), MB_OKCANCEL | MB_ICONINFORMATION)
		== IDCANCEL)
	{
		ShowWindow(SW_HIDE);
		m_nid.cbSize = (DWORD)sizeof(NOTIFYICONDATA);
		m_nid.hWnd = this->m_hWnd;
		m_nid.uID = IDI_ICON1;
		m_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
		m_nid.uCallbackMessage = 12580;             // 自定义的消息名称
		m_nid.hIcon = LoadIcon(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDI_ICON1));
		StrCpy(m_nid.szTip, TEXT("迷你截屏"));                // 信息提示条为"服务器程序"
		Shell_NotifyIcon(NIM_ADD, &m_nid);                // 在托盘区添加图标
		return;
	}
	if (isHotKey)
	{
		HWND hWnd = GetSafeHwnd();
		UnregisterHotKey(hWnd, 1001);
		UnregisterHotKey(hWnd, 1002);
	}
	DoCleanUp();
	CDialogEx::OnOK();
}


void CMiniScreenshotDlg::OnBnClickedButton2()
{
	// TODO: 在此添加控件通知处理程序代码
	auto theHandle = DllHandler.MemGetProcAddress("PrScrn");
	if (!theHandle)
	{
		MessageBox(TEXT("执行失败，请检查权限和文件完整性，可能是由于文件损坏或者系统不支持。"),
			TEXT("SK团队提醒"), MB_ICONERROR);
	}
	typedef INT(WINAPI *func)();
	thread(func(theHandle)).detach();
}


void CMiniScreenshotDlg::OnBnClickedButton1()
{
	// TODO: 在此添加控件通知处理程序代码
	auto theHandle = DllHandler.MemGetProcAddress("EditPic");
	if (!theHandle)
	{
		MessageBox(TEXT("执行失败，请检查权限和文件完整性，可能是由于文件损坏或者系统不支持。"),
			TEXT("SK团队提醒"), MB_ICONERROR);
	}
	typedef INT(WINAPI *func)(PINT, PINT, PINT, SHORT);
	INT v1 = 0, v2 = 0, v3 = 0;
	auto DetFX = [&]()
	{
		__try {
			(func(theHandle))(&v1, &v2, &v3, 0);
		}
		__except (1)
		{
			MessageBox(TEXT("检测到了内存泄漏，不过程序仍然可以继续运行。"),
				TEXT("SK团队提醒"), MB_ICONERROR);
		}
	};
	thread(DetFX).detach();
}

LRESULT CMiniScreenshotDlg::OnPcsTaskBar(WPARAM wParam, LPARAM lParam)
{
	if (wParam != IDI_ICON1)
		return 1;
	switch (lParam)
	{
	case WM_RBUTTONUP:                                        // 右键起来时弹出菜单
	{
		LPPOINT lpoint = new tagPOINT;
		::GetCursorPos(lpoint);                    // 得到鼠标位置
		CMenu menu;
		menu.CreatePopupMenu();                    // 声明一个弹出式菜单
		menu.AppendMenu(MF_STRING, JP_ONEXIT, TEXT("退出"));
		menu.TrackPopupMenu(TPM_LEFTALIGN, lpoint->x, lpoint->y, this);
		HMENU hmenu = menu.Detach();
		menu.DestroyMenu();
		delete lpoint;
	}
	break;
	case WM_LBUTTONDBLCLK:                                 // 双击左键的处理
	{
		this->ShowWindow(SW_SHOWNORMAL);         // 显示主窗口
		Shell_NotifyIcon(NIM_DELETE, &m_nid);
	}
	break;
	}
	return 0;
}


void CMiniScreenshotDlg::OnClose()
{
	// TODO: 在此添加消息处理程序代码和/或调用默认值
	if (isHotKey)
	{
		HWND hWnd = GetSafeHwnd();
		UnregisterHotKey(hWnd, 1001);
		UnregisterHotKey(hWnd, 1002);
	}
	Shell_NotifyIcon(NIM_DELETE, &m_nid);
	DoCleanUp();
	CDialogEx::OnOK();
}


afx_msg void CMiniScreenshotDlg::OnJpOnexit()
{
	OnClose();
	return;
}


void CMiniScreenshotDlg::OnBnClickedButton3()
{
	// TODO: 在此添加控件通知处理程序代码
	HWND hWnd = GetSafeHwnd();
	if (!isHotKey)
	{
		if (!RegisterHotKey(hWnd, 1001, MOD_ALT, towupper(uSetVK)))
		{
			MessageBox(TEXT("注册热键失败。"), TEXT("SK团队提示"), MB_ICONERROR);
			return;
		}
		if (!RegisterHotKey(hWnd, 1002, MOD_ALT, towlower(uSetVK)))
		{
			MessageBox(TEXT("注册热键失败。"), TEXT("SK团队提示"), MB_ICONERROR);
			UnregisterHotKey(hWnd, 1001);
			return;
		}
		isHotKey = TRUE;
		wchar_t Wch[MAX_PATH];
		swprintf_s(Wch, L"成功注册Alt+%c为热键！", towupper(uSetVK));
		SetDlgItemText(IDC_STATIC, Wch);
	}
	else
	{
		UnregisterHotKey(hWnd, 1001);
		UnregisterHotKey(hWnd, 1002);
		SetDlgItemText(IDC_STATIC, TEXT("成功取消了热键！"));
		isHotKey = FALSE;
	}
}

long CMiniScreenshotDlg::OnHotKey(WPARAM wParam, LPARAM lParam)
{
	switch (wParam)
	{
	case 1001:
	case 1002:
	{
		OnBnClickedButton2();
		break;
	}
	default:
		break;
	}
	return TRUE;
}


void CMiniScreenshotDlg::OnBnClickedButton4()
{
	// TODO: 在此添加控件通知处理程序代码
	if (MessageBox(TEXT("确定开始捕获按键，捕获的第一个按键，Alt与该按键齐按下即可截屏。"),
		TEXT("捕获按键提示"), MB_OKCANCEL) == IDOK)
	{
		if (isHotKey)
		{
			HWND hWnd = GetSafeHwnd();
			UnregisterHotKey(hWnd, 1001);
			UnregisterHotKey(hWnd, 1002);
			isHotKey = FALSE;
			SetDlgItemText(IDC_STATIC, TEXT("还原快捷按键成功。"));
		}
		nowSetKey = TRUE;
	}
}


BOOL CMiniScreenshotDlg::PreTranslateMessage(MSG* pMsg)
{
	// TODO: 在此添加专用代码和/或调用基类
	if(nowSetKey)
		if (pMsg->message == WM_KEYDOWN)
		{
			nowSetKey = FALSE;
			auto cKey = towupper(pMsg->wParam);
			wchar_t Wch[MAX_PATH];
			swprintf_s(Wch, L"确定设置 Alt + %c 为热键吗？", cKey);
			if (::MessageBox(::GetForegroundWindow(), Wch, TEXT("SK团队提示"), MB_ICONINFORMATION | MB_OKCANCEL) == IDCANCEL)
				return CDialogEx::PreTranslateMessage(pMsg);
			uSetVK = cKey;
			swprintf_s(Wch, L"前后台Alt + %c 截图", cKey);
			SetDlgItemText(IDC_BUTTON3, Wch);
		}
	return CDialogEx::PreTranslateMessage(pMsg);
}


void CMiniScreenshotDlg::OnBnClickedButton5()
{
	// TODO: 在此添加控件通知处理程序代码
	CString CmdLine = TEXT("explorer.exe http://typedef.cn/");
	LPTSTR Cmd = (LPTSTR)(LPCTSTR)CmdLine;//类型转换,详见MSDN
	STARTUPINFO si = { sizeof(si) }; //保存进程的启动信息
	PROCESS_INFORMATION pi;   //保存进程的相关信息
	si.dwFlags = STARTF_USESHOWWINDOW;
	si.wShowWindow = 1; //1窗口显示,0表示后台运行
	BOOL bRet = ::CreateProcess  //调用创建进程函数
	(
		NULL,
		Cmd,
		NULL,
		NULL,
		FALSE,
		DETACHED_PROCESS,
		NULL,
		NULL,
		&si,
		&pi
	);
	if (bRet)
	{
		::CloseHandle(pi.hProcess); //关闭进程句柄
		::CloseHandle(pi.hThread);  //关闭主线程句柄
	}
	MessageBox(TEXT("软件作者：Saurik\n邮箱：king@ithot.top\nQQ：384550791\n网站：http://typedef.cn/"),
		TEXT("关于作者"), MB_ICONINFORMATION);
}


void CMiniScreenshotDlg::OnBnClickedButton6()
{
	// TODO: 在此添加控件通知处理程序代码
	InitializeDll();
	SetDlgItemText(IDC_STATIC, TEXT("重新初始化组件完成。"));
}
