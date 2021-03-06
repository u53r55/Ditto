#include "stdafx.h"
#include "CP_Main.h"
#include "OleClipSource.h"
#include "shared/TextConvert.h"
#include "CF_HDropAggregator.h"
#include "CF_UnicodeTextAggregator.h"
#include "CF_TextAggregator.h"
#include "richtextaggregator.h"
#include "htmlformataggregator.h"

/*------------------------------------------------------------------*\
COleClipSource
\*------------------------------------------------------------------*/
//IMPLEMENT_DYNAMIC(COleClipSource, COleDataSource)
COleClipSource::COleClipSource()
{
	m_bLoadedFormats = false;
	m_bOnlyPaste_CF_TEXT = false;
	m_pCustomPasteFormats = NULL;
}

COleClipSource::~COleClipSource()
{
	
}

BOOL COleClipSource::DoDelayRender()
{
	CClipTypes types;
	m_ClipIDs.GetTypes(types);
	
	INT_PTR count = types.GetSize();
	for(int i=0; i < count; i++)
	{
		DelayRenderData(types[i]);
	}

	return count > 0;
}

#include "Client.h"

BOOL COleClipSource::DoImmediateRender()
{
	if(m_bLoadedFormats)
		return TRUE;

	m_bLoadedFormats = true;

	if(m_pCustomPasteFormats != NULL)
	{
		return PutFormatOnClipboard(m_pCustomPasteFormats) > 0;
	}
	
	INT_PTR count = m_ClipIDs.GetSize();
	if(count <= 0)
		return 0;

	BOOL bProcessedMult = FALSE;

	if(count > 1)
	{
		CStringA SepA = CTextConvert::ConvertToChar(g_Opt.GetMultiPasteSeparator());
		CCF_TextAggregator CFText(SepA);
		if(m_ClipIDs.AggregateData(CFText, CF_TEXT, g_Opt.m_bMultiPasteReverse && g_Opt.m_bHistoryStartTop))
		{
			CacheGlobalData(CF_TEXT, CFText.GetHGlobal());
			bProcessedMult = TRUE;
		}

		CStringW SepW = CTextConvert::ConvertToUnicode(g_Opt.GetMultiPasteSeparator());
		CCF_UnicodeTextAggregator CFUnicodeText(SepW);
		if(m_ClipIDs.AggregateData(CFUnicodeText, CF_UNICODETEXT, g_Opt.m_bMultiPasteReverse && g_Opt.m_bHistoryStartTop))
		{
			CacheGlobalData(CF_UNICODETEXT, CFUnicodeText.GetHGlobal());
			bProcessedMult = TRUE;
		}

		if(m_bOnlyPaste_CF_TEXT == false)
		{
			CCF_HDropAggregator HDrop;
			if(m_ClipIDs.AggregateData(HDrop, CF_HDROP, g_Opt.m_bMultiPasteReverse && g_Opt.m_bHistoryStartTop))
			{
				CacheGlobalData(CF_HDROP, HDrop.GetHGlobal());
				bProcessedMult = TRUE;
			}

			CRichTextAggregator RichText(SepA);
			if(m_ClipIDs.AggregateData(RichText, theApp.m_RTFFormat, g_Opt.m_bMultiPasteReverse && g_Opt.m_bHistoryStartTop))
			{
				CacheGlobalData(theApp.m_RTFFormat, RichText.GetHGlobal());
				bProcessedMult = TRUE;
			}

			CHTMLFormatAggregator Html(SepA);
			if(m_ClipIDs.AggregateData(Html, theApp.m_HTML_Format, g_Opt.m_bMultiPasteReverse && g_Opt.m_bHistoryStartTop))
			{
				CacheGlobalData(theApp.m_HTML_Format, Html.GetHGlobal());
				bProcessedMult = TRUE;
			}
		}
	}

	if(count >= 1 && bProcessedMult == FALSE)
	{
		CClip clip;
		CClipFormats formats;

		clip.LoadFormats(m_ClipIDs[0], m_bOnlyPaste_CF_TEXT);
		
		return PutFormatOnClipboard(&clip.m_Formats) > 0;
	}		

	return bProcessedMult;
}

INT_PTR COleClipSource::PutFormatOnClipboard(CClipFormats *pFormats)
{
	Log(_T("Start of put format on clipboard"));

	CClipFormat* pCF;
	INT_PTR	count = pFormats->GetSize();
	bool bDelayedRenderCF_HDROP = false;
	INT_PTR i = 0;

	//see if the html format is in the list
	//if it is the list we will not paste CF_TEXT
	for(i = 0; i < count; i++)
	{
		pCF = &pFormats->ElementAt(i);

		if(pCF->m_cfType == theApp.m_RemoteCF_HDROP)
		{
			bDelayedRenderCF_HDROP = true;
		}
	}

	for(i = 0; i < count; i++)
	{
		pCF = &pFormats->ElementAt(i);

		if(bDelayedRenderCF_HDROP)
		{
			if(pCF->m_cfType == CF_HDROP)
			{
				LogSendRecieveInfo("Added delayed cf_hdrop to clipboard");
				DelayRenderData(pCF->m_cfType);
			}

			continue;
		}

		Log(StrF(_T("Setting clipboard type: %s to the clipboard"), GetFormatName(pCF->m_cfType)));

		CacheGlobalData(pCF->m_cfType, pCF->m_hgData);
		pCF->m_hgData = 0; // OLE owns it now
	}

	pFormats->RemoveAll();

	m_bLoadedFormats = true;

	Log(_T("End of put format on clipboard"));

	return count;
}  

BOOL COleClipSource::OnRenderGlobalData(LPFORMATETC lpFormatEtc, HGLOBAL* phGlobal)
{
	static bool bInHere = false;

	if(bInHere)
	{
		return FALSE;
	}
	bInHere = true;

	HGLOBAL hData = NULL;

	CClipFormat *pFind = m_DelayRenderedFormats.FindFormat(lpFormatEtc->cfFormat);

	if(pFind)
	{
		if(pFind->m_hgData)
		{
			hData = NewGlobalH(pFind->m_hgData, GlobalSize(pFind->m_hgData));
		}
	}
	else
	{
		LogSendRecieveInfo("Delayed Render, getting data from remote machine");

		CClip clip;

		if(m_ClipIDs.GetCount() > 0)
		{
			clip.LoadFormats(m_ClipIDs[0]);

			CClipFormat *pDittoDelayCF_HDROP = clip.m_Formats.FindFormat(theApp.m_RemoteCF_HDROP);
			CClipFormat *pCF_HDROP = clip.m_Formats.FindFormat(CF_HDROP);

			if(pDittoDelayCF_HDROP && pCF_HDROP)
			{
				CDittoCF_HDROP *pData = (CDittoCF_HDROP*)GlobalLock(pDittoDelayCF_HDROP->m_hgData);
				if(pData)
				{
					CString csComputerName;
					CString csIP;

					CTextConvert::ConvertFromUTF8(pData->m_cIP, csIP);
					CTextConvert::ConvertFromUTF8(pData->m_cComputerName, csComputerName);
					
					GlobalUnlock(pDittoDelayCF_HDROP->m_hgData);

					CClient cl;
					hData = cl.RequestCopiedFiles(*pCF_HDROP, csIP, csComputerName);
				}
			}
			else
			{
				hData = m_ClipIDs.Render(lpFormatEtc->cfFormat);
			}
		}

		//Add to a cache of already rendered data
		//Windows seems to call this function multiple times
		//so only the first time do we need to go get the data
		HGLOBAL hCopy = NULL;
		if(hData)
		{
			hCopy = NewGlobalH(hData, GlobalSize(hData));
		}

		CClipFormat format(lpFormatEtc->cfFormat, hCopy);
		format.m_autoDeleteData = false; //owned by m_DelayRenderedFormats
		m_DelayRenderedFormats.Add(format);
	}

	BOOL bRet = FALSE;
	if(hData)
	{
		// if phGlobal is null, we can just give the allocated mem
		// else, our data must fit within the GlobalSize(*phGlobal)
		if(*phGlobal == 0)
		{
			*phGlobal = hData;
		}
		else
		{
			SIZE_T len = min(::GlobalSize(*phGlobal), ::GlobalSize(hData));
			if(len)
			{
				CopyToGlobalHH(*phGlobal, hData, len);
			}
			::GlobalFree(hData);
		}
		bRet = TRUE;
	}

	bInHere = false;

	return bRet;
}