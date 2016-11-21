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
#include "WindowsMessageFilter.h"

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

	bool CheckWindowsMessages(HWND hWnd);
	void RenderFrames();

private:

	static INT_PTR CALLBACK DlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

	ISpinnerControl*  fromFrame;
	ISpinnerControl*  toFrame;
	ISpinnerControl*  ratioVal;

	HWND   hPanel;
	IUtil* iu;
	Interface *ip;
};

class maxRndProgressCB;
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
	this->ip = ip;
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

void renderHistogram::Init(HWND hWnd/*handle*/)
{
	CheckDlgButton(hWnd,IDC_R_ACTIVETIME,TRUE);
}

void renderHistogram::Destroy(HWND /*handle*/)
{

}

bool renderHistogram::CheckWindowsMessages(HWND hWnd)
{
	MaxSDK::WindowsMessageFilter messageFilter;
  messageFilter.AddUnfilteredWindow(hWnd);
  messageFilter.RunNonBlockingMessageLoop();
  return !messageFilter.Aborted();
}

void renderHistogram::RenderFrames()
{
	//open renderer
	//remember current parameters
	//set draft quality parameters
	//hook the postframe callback
	//hook the afterrender callback (which removes all the hooks)
	//start rendering
	//Interface *ip = GetCOREInterface();
	ViewParams vp;
	INode *cam = ip->GetViewExp(NULL).GetViewCamera();
	
	Bitmap *bm = NULL;
	if (!bm) {
		BitmapInfo bi;
		bi.SetWidth(320);
		bi.SetHeight(240);
		bi.SetType(BMM_FLOAT_RGBA_32);
		bi.SetFlags(MAP_HAS_ALPHA);
		bi.SetAspect(1.0f);
		bm = TheManager->Create(&bi);
		}
   bm->Display(_T("AutoExposure"), BMM_RND);

	Interval frames = ip->GetAnimRange();
	TimeValue startFrame = frames.Start();
	TimeValue endFrame = frames.End();
	int delta = GetTicksPerFrame();
	int res = ip->OpenCurRenderer(cam, NULL, RENDTYPE_NORMAL); //, 320, 240); 
	for (int frame = startFrame; frame <= endFrame; frame += delta)
	{
		ip->CurRendererRenderFrame(frame, bm);
		if (!CheckWindowsMessages(ip->GetMAXHWnd()))
			break;
	}
	ip->CloseCurRenderer();
	bm->DeleteThis();
}

INT_PTR CALLBACK renderHistogram::DlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg) 
	{
	case WM_INITDIALOG:
		renderHistogram::GetInstance()->Init(hWnd);
		GetInstance()->fromFrame = GetISpinner(GetDlgItem(hWnd, IDC_SPIN_FROM));
		GetInstance()->fromFrame->SetScale(1.0f);
		GetInstance()->fromFrame->SetLimits(-1000000, 1000000);
		GetInstance()->fromFrame->LinkToEdit(GetDlgItem(hWnd, IDC_EDIT_FROM), EDITTYPE_INT);
		GetInstance()->fromFrame->SetResetValue(0);

		GetInstance()->toFrame = GetISpinner(GetDlgItem(hWnd, IDC_SPIN_TO));
		GetInstance()->toFrame->SetScale(1.0f);
		GetInstance()->toFrame->SetLimits(-1000000, 1000000);
		GetInstance()->toFrame->LinkToEdit(GetDlgItem(hWnd, IDC_EDIT_TO), EDITTYPE_INT);
		GetInstance()->toFrame->SetResetValue(0);

		GetInstance()->ratioVal = GetISpinner(GetDlgItem(hWnd, IDC_SPIN_RATIO));
		GetInstance()->ratioVal->SetScale(1.0f);
		GetInstance()->ratioVal->SetLimits(-1000.0f, 1000.0f);
		GetInstance()->ratioVal->LinkToEdit(GetDlgItem(hWnd, IDC_EDIT_RATIO), EDITTYPE_FLOAT);
		GetInstance()->ratioVal->SetResetValue(1.0f);
		GetInstance()->ratioVal->SetValue(1.0f, FALSE);
		break;

	case WM_DESTROY:
		renderHistogram::GetInstance()->Destroy(hWnd);
		break;

	case WM_COMMAND:
#pragma message(TODO("React to the user interface commands."))
		switch (LOWORD(wParam)) {

		case IDC_DORENDER:
			GetInstance()->RenderFrames();
			break;
		case IDC_APPLYCAMERA:
			//check if camera is selected
			//check if average values are calculated
			//create modifier
			//create animated paramater
			//apply to camera
			break;
		case IDC_CLOSE:
			EndDialog(hWnd, FALSE);
			ReleaseISpinner(GetInstance()->fromFrame);
			ReleaseISpinner(GetInstance()->toFrame);
			ReleaseISpinner(GetInstance()->ratioVal);
			if (GetInstance()->iu)
				GetInstance()->iu->CloseUtility();
			break;
		}
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
