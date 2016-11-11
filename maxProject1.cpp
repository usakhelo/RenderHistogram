//**************************************************************************/
// Copyright (c) 1998-2007 Autodesk, Inc.
// All rights reserved.
// 
// These coded instructions, statements, and computer programs contain
// unpublished proprietary information written by Autodesk, Inc., and are
// protected by Federal copyright law. They may not be disclosed to third
// parties or copied or duplicated in any form, in whole or in part, without
// the prior written consent of Autodesk, Inc.
//**************************************************************************/
// DESCRIPTION: Appwizard generated plugin
// AUTHOR: 
//***************************************************************************/

#include "maxProject1.h"

#define renderHistogram_CLASS_ID	Class_ID(0x7354e284, 0x6556a2a9)


class renderHistogram : public UtilityObj 
{
public:
		
	//Constructor/Destructor
	renderHistogram();
	virtual ~renderHistogram();

	void DeleteThis() { }
	
	void BeginEditParams(Interface *ip,IUtil *iu);
	void EndEditParams(Interface *ip,IUtil *iu);

	void Init(HWND hWnd);
	void Destroy(HWND hWnd);
	
	// Singleton access
	static renderHistogram* GetInstance() { 
		static renderHistogram therenderHistogram;
		return &therenderHistogram; 
	}

private:

	static INT_PTR CALLBACK DlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

	HWND   hPanel;
	IUtil* iu;
};


class renderHistogramClassDesc : public ClassDesc2 
{
public:
	int IsPublic() 							{ return TRUE; }
	void* Create(BOOL loading = FALSE) 		{ return renderHistogram::GetInstance(); }
	const TCHAR * ClassName() 				{ return GetString(IDS_CLASS_NAME); }
	SClass_ID SuperClassID() 				{ return UTILITY_CLASS_ID; }
	Class_ID ClassID() 						{ return renderHistogram_CLASS_ID; }
	const TCHAR* Category() 				{ return GetString(IDS_CATEGORY); }

	const TCHAR* InternalName() 			{ return _T("renderHistogram"); }	// returns fixed parsable name (scripter-visible name)
	HINSTANCE HInstance() 					{ return hInstance; }				// returns owning module handle
	

};


ClassDesc2* GetrenderHistogramDesc() { 
	static renderHistogramClassDesc renderHistogramDesc;
	return &renderHistogramDesc; 
}


//--- renderHistogram -------------------------------------------------------
renderHistogram::renderHistogram()
	: hPanel(nullptr)
	, iu(nullptr)
{
	
}

renderHistogram::~renderHistogram()
{

}

void renderHistogram::BeginEditParams(Interface* ip,IUtil* iu) 
{
	this->iu = iu;
	hPanel = ip->AddRollupPage(
		hInstance,
		MAKEINTRESOURCE(IDD_PANEL),
		DlgProc,
		GetString(IDS_PARAMS),
		0);
}
	
void renderHistogram::EndEditParams(Interface* ip,IUtil*)
{
	this->iu = nullptr;
	ip->DeleteRollupPage(hPanel);
	hPanel = nullptr;
}

void renderHistogram::Init(HWND /*handle*/)
{

}

void renderHistogram::Destroy(HWND /*handle*/)
{

}

INT_PTR CALLBACK renderHistogram::DlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg) 
	{
		case WM_INITDIALOG:
			renderHistogram::GetInstance()->Init(hWnd);
			break;

		case WM_DESTROY:
			renderHistogram::GetInstance()->Destroy(hWnd);
			break;

		case WM_COMMAND:
			#pragma message(TODO("React to the user interface commands.  A utility plug-in is controlled by the user from here."))
			break;

		case WM_LBUTTONDOWN:
		case WM_LBUTTONUP:
		case WM_MOUSEMOVE:
			GetCOREInterface()->RollupMouseMessage(hWnd,msg,wParam,lParam);
			break;

		default:
			return 0;
	}
	return 1;
}
