
// MiniScreenshotDlg.h: 头文件
//

#pragma once



#include "MemLoadDll.h.h"
#include <mutex>

// CMiniScreenshotDlg 对话框
class CMiniScreenshotDlg : public CDialogEx
{
// 构造
public:
	CMiniScreenshotDlg(CWnd* pParent = nullptr);	// 标准构造函数

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_MINISCREENSHOT_DIALOG };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 支持

public:

	CMemLoadDll DllHandler;
	CMemLoadDll YkDllHandler;
	HANDLE theHeap = 0;
	NOTIFYICONDATA m_nid;
	BOOL isHotKey = 0;
	UINT uSetVK = L'q';
	BOOL nowSetKey = FALSE;
	BOOL bIsInitOK = FALSE;
	std::mutex theMutex;
	VOID InitializeDll();
	VOID DoCleanUp();


// 实现
protected:
	HICON m_hIcon;

	// 生成的消息映射函数
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnBnClickedOk();
	afx_msg void OnBnClickedButton2();
	afx_msg void OnBnClickedButton1();
	afx_msg LRESULT OnPcsTaskBar(WPARAM wParam, LPARAM lParam);
	afx_msg void OnClose();
protected:
	afx_msg void OnJpOnexit();
public:
	afx_msg void OnBnClickedButton3();
	afx_msg long OnHotKey(WPARAM wParam, LPARAM lParam);
	afx_msg void OnBnClickedButton4();
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	afx_msg void OnBnClickedButton5();
	afx_msg void OnBnClickedButton6();
};
