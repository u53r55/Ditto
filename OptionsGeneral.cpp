// OptionsGeneral.cpp : implementation file
//

#include "stdafx.h"
#include "CP_Main.h"
#include "OptionsGeneral.h"
#include "InternetUpdate.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// COptionsGeneral property page

IMPLEMENT_DYNCREATE(COptionsGeneral, CPropertyPage)

COptionsGeneral::COptionsGeneral() : CPropertyPage(COptionsGeneral::IDD)
{
	//{{AFX_DATA_INIT(COptionsGeneral)
	//}}AFX_DATA_INIT
}

COptionsGeneral::~COptionsGeneral()
{
}

void COptionsGeneral::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(COptionsGeneral)
	DDX_Control(pDX, IDC_GET_PATH, m_btGetPath);
	DDX_Control(pDX, IDC_PATH, m_ePath);
	DDX_Control(pDX, IDC_SET_DB_PATH, m_btSetDatabasePath);
	DDX_Control(pDX, IDC_CHECK_UPDATES, m_btCheckForUpdates);
	DDX_Control(pDX, IDC_COMPACT_REPAIR, m_btCompactAndRepair);
	DDX_Control(pDX, IDC_EXPIRE_AFTER, m_eExpireAfter);
	DDX_Control(pDX, IDC_MAX_SAVED_COPIES, m_eMaxSavedCopies);
	DDX_Control(pDX, IDC_MAXIMUM, m_btMaximumCheck);
	DDX_Control(pDX, IDC_EXPIRE, m_btExpire);
	DDX_Control(pDX, IDC_DISPLAY_IN_SYSTEMTRAY, m_btShowIconInSysTray);
	DDX_Control(pDX, IDC_START_ON_STARTUP, m_btRunOnStartup);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(COptionsGeneral, CPropertyPage)
	//{{AFX_MSG_MAP(COptionsGeneral)
	ON_BN_CLICKED(IDC_BT_COMPACT_AND_REPAIR, OnBtCompactAndRepair)
	ON_BN_CLICKED(IDC_CHECK_FOR_UPDATES, OnCheckForUpdates)
	ON_BN_CLICKED(IDC_SET_DB_PATH, OnSetDbPath)
	ON_BN_CLICKED(IDC_GET_PATH, OnGetPath)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// COptionsGeneral message handlers

BOOL COptionsGeneral::OnInitDialog() 
{
	CPropertyPage::OnInitDialog();
	
	m_pParent = (COptionsSheet *)GetParent();

	m_btRunOnStartup.SetCheck(CGetSetOptions::GetRunOnStartUp());
	m_btShowIconInSysTray.SetCheck(CGetSetOptions::GetShowIconInSysTray());
	m_btMaximumCheck.SetCheck(CGetSetOptions::GetCheckForMaxEntries());
	m_btExpire.SetCheck(CGetSetOptions::GetCheckForExpiredEntries());
	m_btCompactAndRepair.SetCheck(CGetSetOptions::GetCompactAndRepairOnExit());
	m_btCheckForUpdates.SetCheck(CGetSetOptions::GetCheckForUpdates());
	
	m_eExpireAfter.SetNumber(CGetSetOptions::GetExpiredEntries());
	m_eMaxSavedCopies.SetNumber(CGetSetOptions::GetMaxEntries());

	CString csPath = CGetSetOptions::GetDBPath(FALSE);
	if(csPath.IsEmpty())
	{
		m_ePath.EnableWindow(FALSE);
		m_btGetPath.EnableWindow(FALSE);
	}
	else
	{
		m_btSetDatabasePath.SetCheck(BST_CHECKED);
		m_ePath.SetWindowText(csPath);
	}

	return TRUE;
}

BOOL COptionsGeneral::OnApply()
{
	UpdateData();

	::SendMessage(theApp.m_MainhWnd, WM_SHOW_TRAY_ICON, m_btShowIconInSysTray.GetCheck(), 0);

	CGetSetOptions::SetShowIconInSysTray(m_btShowIconInSysTray.GetCheck());
	CGetSetOptions::SetRunOnStartUp(m_btRunOnStartup.GetCheck());
	CGetSetOptions::SetCheckForMaxEntries(m_btMaximumCheck.GetCheck());
	CGetSetOptions::SetCheckForExpiredEntries(m_btExpire.GetCheck());
	CGetSetOptions::SetCompactAndRepairOnExit(m_btCompactAndRepair.GetCheck());
	CGetSetOptions::SetCheckForUpdates(m_btCheckForUpdates.GetCheck());
	
	CGetSetOptions::SetMaxEntries(m_eMaxSavedCopies.GetNumber());
	CGetSetOptions::SetExpiredEntries(m_eExpireAfter.GetNumber());

	if(m_btSetDatabasePath.GetCheck() == BST_CHECKED)
	{
		CString csPath;
		m_ePath.GetWindowText(csPath);

		if(csPath.IsEmpty() == FALSE)
		{
			if(ValidDB(csPath) == FALSE)
			{
				MessageBox("Invalid Database", "Ditto", MB_OK);
				m_ePath.SetFocus();
				return FALSE;
			}			
		}	
		
		if(csPath != CGetSetOptions::GetDBPath(FALSE))
		{
			CGetSetOptions::SetDBPath(csPath);
			theApp.CloseDB();
		}
	}
	
	return CPropertyPage::OnApply();
}

BOOL COptionsGeneral::OnSetActive() 
{	
	return CPropertyPage::OnSetActive();
}
void COptionsGeneral::OnBtCompactAndRepair() 
{
	CompactDatabase();
	RepairDatabase();
}

void COptionsGeneral::OnCheckForUpdates() 
{
	CInternetUpdate update;

	if(update.CheckForUpdate(m_hWnd, FALSE, TRUE))
	{
		::PostMessage(theApp.m_MainhWnd, WM_CLOSE_APP, 0, 0);
		m_pParent->EndDialog(-1);
	}
}

void COptionsGeneral::OnSetDbPath() 
{
	if(m_btSetDatabasePath.GetCheck() == BST_CHECKED)
	{
		m_ePath.EnableWindow(TRUE);
		m_btGetPath.EnableWindow(TRUE);
	}
	else
	{
		m_ePath.EnableWindow(FALSE);
		m_btGetPath.EnableWindow(FALSE);
	}	
}

void COptionsGeneral::OnGetPath() 
{
	OPENFILENAME	FileName;

	char			szFileName[400];
	char			szDir[400];

	memset(&FileName, 0, sizeof(FileName));
	memset(szFileName, 0, sizeof(szFileName));
	memset(&szDir, 0, sizeof(szDir));

	FileName.lStructSize = sizeof(FileName);

	
	FileName.lpstrTitle = "Open Database";
	FileName.Flags = OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT|OFN_PATHMUSTEXIST;
	FileName.nMaxFile = 400;
	FileName.lpstrFile = szFileName;
	FileName.lpstrInitialDir = szDir;
	FileName.lpstrFilter = "Database Files (.MDB)\0*.mdb";
	FileName.lpstrDefExt = "mdb";

	if(GetOpenFileName(&FileName) == 0)
		return;

	CString	csPath(FileName.lpstrFile);

	if(ValidDB(csPath) == FALSE)
	{
		MessageBox("Invalid Database", "Ditto", MB_OK);
		m_ePath.SetFocus();
	}
	else
		m_ePath.SetWindowText(csPath);	
}
