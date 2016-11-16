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
#include "notify.h"
#include "modstack.h"
#include <vector>
#include <algorithm>

#define renderHistogram_CLASS_ID	Class_ID(0x7354e284, 0x6556a2a9)


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

static DWORD WINAPI fn(LPVOID arg)
{
	return(0);
}

void renderHistogram::ApplyModifier()
{
	if (ip->GetSelNodeCount() > 1 || ip->GetSelNodeCount() < 1)
	{
		TSTR title = GetString(IDS_CLASS_NAME);
		TSTR message = GetString(IDS_SELECTIONCOUNT_ERROR);
		MessageBox(hPanel, message, title, MB_ICONEXCLAMATION);
		return;
	}

	INode *node;
	node = ip->GetSelNode(0);

	Object *obj = node->GetObjectRef();

	if (!obj)
	{
		TSTR title = GetString(IDS_CLASS_NAME);
		TSTR message = GetString(IDS_SELECTION_ERROR);
		MessageBox(hPanel, message, title, MB_ICONEXCLAMATION);
		return;
	}

	if (obj->SuperClassID() != CAMERA_CLASS_ID)
	{
		TSTR title = GetString(IDS_CLASS_NAME);
		TSTR message = GetString(IDS_SELECTIONTYPE_ERROR);
		MessageBox(hPanel, message, title, MB_ICONEXCLAMATION);
		return;
	}

	IDerivedObject *dobj = CreateDerivedObject(obj);

	Modifier *coronaCameraMod = (Modifier *)CreateInstance(
		OSM_CLASS_ID, Class_ID((ulong)-1551416164, (ulong)502132111));

	IParamBlock2* coronaModPBlock =((Animatable*)coronaCameraMod)->GetParamBlock(0);
	assert(coronaModPBlock);

	//add animated parameter in the same range
	TimeValue startFrame, endFrame;
	int duration;
	int delta = GetTicksPerFrame();
	if (isAnimRange) {
		Interval frames = ip->GetAnimRange();
		startFrame = frames.Start();
		endFrame = frames.End();
		duration = (int)frames.Duration();
	} else {
		startFrame = fromFrame * GetTicksPerFrame();
		endFrame = toFrame * GetTicksPerFrame();
		duration = endFrame - startFrame + 1;
	}
	brightnessArray.empty();
	brightnessArray.reserve(duration / GetTicksPerFrame());

	BOOL enableEV = true;

	ParamBlockDesc2* pbdesc = coronaModPBlock->GetDesc();
	int param_index = pbdesc->NameToIndex(_T("simpleExposure"));
	const ParamDef* param_def = pbdesc->GetParamDefByIndex(param_index);

	Control *controller = (Control *) CreateInstance(CTRL_FLOAT_CLASS_ID,Class_ID(LININTERP_FLOAT_CLASS_ID,0)); 
	coronaModPBlock->SetControllerByID(param_def->ID, 0, controller, true);
	coronaModPBlock->SetValueByName<BOOL>( _T("overrideSimpleExposure"), enableEV, 0);

	SuspendAnimate();
	AnimateOn();
	for (int frame = startFrame; frame <= endFrame; frame += delta)
	{
		float EV = 1.5f;
		//controller->SetValue(frame, &EV, 1);
		coronaModPBlock->SetValue(param_def->ID, frame, EV);
		//coronaModPBlock->SetValueByName<float>( _T("simpleExposure"), EV, frame);
	}
	ResumeAnimate();

	dobj->AddModifier(coronaCameraMod);

	node->SetObjectRef(dobj);
}

void renderHistogram::TestFunc()
{
	Renderer *renderer = ip->GetCurrentRenderer(false);
	int numBlks = renderer->NumParamBlocks();
	DebugPrint(_M("renderer->NumParamBlocks %d\r\n"), numBlks);
	for (int i=0; i < numBlks; i++) {
		auto pBlock = renderer->GetParamBlock(i);
		DebugPrint(_M("paramBlock Name %s\r\n"), pBlock->GetLocalName());
		int numParams = pBlock->NumParams();
		DebugPrint(_M("var->NumParams() %d\r\n"), numParams);
		Interval inf; // = ip->GetAnimRange();
		inf.SetInfinite();
		auto pDesc = pBlock->GetDesc();
		DebugPrint(_M("pDesc %s\r\n"), pDesc->int_name);
		int descCount = pDesc->Count();
		DebugPrint(_M("descCount %d\r\n"), descCount);
		for(int j=0; j<descCount; j++) {
			auto pId = pBlock->IndextoID(j);
			auto& pDef1 = pBlock->GetParamDef(pId);
			DebugPrint(_M("pDef name %s\r\n"), pDef1.int_name);
			DebugPrint(_M("pDef type %d\r\n"), pDef1.type);
			switch (pDef1.type) {
			case TYPE_FLOAT: 
				DebugPrint(_M("pDef value %f\r\n"), pBlock->GetFloat(pId, ip->GetTime(), inf));
				break;
			case TYPE_BOOL:
			case TYPE_INT:
				DebugPrint(_M("pDef value %d\r\n"), pBlock->GetInt(pId, ip->GetTime(), inf));
				break;
			case TYPE_STRING:
				DebugPrint(_M("pDef value %s\r\n"), pBlock->GetStr(pId, ip->GetTime(), inf));
				break;
			}
		}
		auto classDesc = pBlock->GetDesc()->cd;
		int numInt = classDesc->NumInterfaces();
		DebugPrint(_M("classDesc->NumInterfaces() %d\r\n"), numInt);
		for(int j=0; j<numInt; j++) {
			auto intface = classDesc->GetInterfaceAt(j);
			auto fpdesc = intface->GetDesc();
			DebugPrint(_M("fpInterface %s\r\n"), fpdesc->internal_name);
			DebugPrint(_M("numFunctions %d\r\n"), fpdesc->functions.Count());
			for(int k=0; k<fpdesc->functions.Count(); k++) {
				auto fnc = fpdesc->functions[k];
				DebugPrint(_M("function %s\r\n"), fnc->internal_name);
			}
		}
	}
}

float renderHistogram::CalculateMeanBrightness(Bitmap *bm)
{
	if (!bm)
		return 0.0;

	const double rY = 0.212655;
	const double gY = 0.715158;
	const double bY = 0.072187;
	float sumLum = 0.0;
	float result = 0.0;

	int biWidth = bm->Width();
	int biHeight = bm->Height();

	BMM_Color_fl c1;

	for (int i = 0; i < biWidth; i++)
	{
		for (int j = 0; j < biHeight; j++)
		{
			bm->GetPixels(i,j,1,&c1);
			float lum = rY*c1.r + gY*c1.g + bY*c1.b;
			sumLum += lum;
		}
	}

	result = sumLum / (biWidth * biHeight);

	return result;
}

void renderHistogram::RenderFrames()
{
	ViewParams vp;
	INode *cam = ip->GetViewExp(NULL).GetViewCamera();
	if (!cam)
	{
		TSTR title = GetString(IDS_CLASS_NAME);
		TSTR message = GetString(IDS_CAM_ERROR);
		MessageBox(hPanel, message, title, MB_ICONEXCLAMATION);
		return;
	}
#pragma message(TODO("check for selected camera object too"))

	Renderer* currRenderer = ip->GetCurrentRenderer(false);
	MSTR rendName;
	currRenderer->GetClassName(rendName);
	if (!rendName.StartsWith(_T("corona"), false))
	{
		TSTR title = GetString(IDS_CLASS_NAME);
		TSTR message = GetString(IDS_REND_ERROR);
		MessageBox(hPanel, message, title, MB_ICONEXCLAMATION);
		return;
	}

	int curWidth = ip->GetRendWidth();
	int curHeight = ip->GetRendHeight();
	float aspect = (float)curWidth / curHeight;

	Bitmap *bm = NULL;
	if (!bm) {
		BitmapInfo bi;
		bi.SetWidth(320);
		bi.SetHeight((int)(320 / aspect));
		bi.SetType(BMM_FLOAT_RGBA_32);
		bi.SetFlags(MAP_HAS_ALPHA);
		bi.SetAspect(1.0f);
		bm = TheManager->Create(&bi);
	}
	//bm->Display(_T("AutoExposure"), BMM_RND);

	TimeValue startFrame, endFrame;
	int duration;
	int delta = GetTicksPerFrame();
	if (isAnimRange) {
		Interval frames = ip->GetAnimRange();
		startFrame = frames.Start();
		endFrame = frames.End();
		duration = (int)frames.Duration();
	} else {
		startFrame = fromFrame * GetTicksPerFrame();
		endFrame = toFrame * GetTicksPerFrame();
		duration = endFrame - startFrame + 1;
	}
	brightnessArray.empty();
	brightnessArray.reserve(duration / GetTicksPerFrame());

	Renderer *renderer = ip->GetCurrentRenderer(false);
	auto pBlock = renderer->GetParamBlock(0);
	BOOL mbCam, mbGeo, dofUse;
	bool result;
	Interval inf;
	inf.SetInfinite();
	result = pBlock->GetValueByName<BOOL>(_T("mb.useCamera"), ip->GetTime(), mbCam, inf);
	result = pBlock->GetValueByName<BOOL>(_T("mb.useGeometry"), ip->GetTime(), mbGeo, inf);
	result = pBlock->GetValueByName<BOOL>(_T("dof.use"), ip->GetTime(), dofUse, inf);

	BOOL off = false;
	result = pBlock->SetValueByName<BOOL>(_T("mb.useCamera"), off, ip->GetTime());
	result = pBlock->SetValueByName<BOOL>(_T("mb.useGeometry"), off, ip->GetTime());
	result = pBlock->SetValueByName<BOOL>(_T("dof.use"), off, ip->GetTime());

	LPVOID arg = nullptr;
	ip->ProgressStart(_M("Calculating Average Brightness"), TRUE, fn, arg);
	int res = ip->OpenCurRenderer(cam, NULL, RENDTYPE_NORMAL);
	for (int frame = startFrame; frame <= endFrame; frame += delta)
	{
		ip->ProgressUpdate((int)((float)frame/duration * 100.0f));
		ip->CurRendererRenderFrame(frame, bm);
		brightnessArray.push_back(CalculateMeanBrightness(bm));
		if (ip->GetCancel()) {
			int retval = MessageBox(ip->GetMAXHWnd(), _M("Really Cancel?"), _M("Question"), MB_ICONQUESTION | MB_YESNO);
			if (retval == IDYES)
				break;
			else if (retval == IDNO)
				ip->SetCancel(FALSE);
		}

		if (!CheckWindowsMessages(ip->GetMAXHWnd()))
			break;
	}
	ip->CloseCurRenderer();
	ip->ProgressEnd();

	result = pBlock->SetValueByName<BOOL>(_T("mb.useCamera"), mbCam, ip->GetTime());
	result = pBlock->SetValueByName<BOOL>(_T("mb.useGeometry"), mbGeo, ip->GetTime());
	result = pBlock->SetValueByName<BOOL>(_T("dof.use"), dofUse, ip->GetTime());

	bm->DeleteThis();
}

void renderHistogram::EnableRangeCtrls(HWND hWnd, bool state) {
	EnableWindow(GetDlgItem(hWnd, IDC_EDIT_FROM), state);
	EnableWindow(GetDlgItem(hWnd, IDC_EDIT_TO), state);
	EnableWindow(GetDlgItem(hWnd, IDC_SPIN_FROM), state);
	EnableWindow(GetDlgItem(hWnd, IDC_SPIN_TO), state);
}

INT_PTR CALLBACK renderHistogram::DlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	renderHistogram* instance = GetInstance();

	switch (msg) 
	{
	case WM_INITDIALOG:
		instance->Init(hWnd);
		instance->fromFrameSpn = GetISpinner(GetDlgItem(hWnd, IDC_SPIN_FROM));
		instance->fromFrameSpn->SetScale(1.0f);
		instance->fromFrameSpn->SetLimits(-1000000, 1000000);
		instance->fromFrameSpn->LinkToEdit(GetDlgItem(hWnd, IDC_EDIT_FROM), EDITTYPE_INT);
		instance->fromFrameSpn->SetResetValue(0);

		instance->toFrameSpn = GetISpinner(GetDlgItem(hWnd, IDC_SPIN_TO));
		instance->toFrameSpn->SetScale(1.0f);
		instance->toFrameSpn->SetLimits(-1000000, 1000000);
		instance->toFrameSpn->LinkToEdit(GetDlgItem(hWnd, IDC_EDIT_TO), EDITTYPE_INT);
		instance->toFrameSpn->SetResetValue(0);

		instance->EnableRangeCtrls(hWnd, false);
		instance->isAnimRange = true;

		instance->targetBrightnessSpn = GetISpinner(GetDlgItem(hWnd, IDC_SPIN_RATIO));
		instance->targetBrightnessSpn->SetScale(1.0f);
		instance->targetBrightnessSpn->SetLimits(-1000.0f, 1000.0f);
		instance->targetBrightnessSpn->LinkToEdit(GetDlgItem(hWnd, IDC_EDIT_RATIO), EDITTYPE_FLOAT);
		instance->targetBrightnessSpn->SetResetValue(1.0f);
		instance->targetBrightnessSpn->SetValue(1.0f, FALSE);
		break;

	case WM_DESTROY:
		instance->Destroy(hWnd);
		break;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {

		case IDC_DORENDER:
			instance->isAnimRange = IsDlgButtonChecked(hWnd, IDC_R_ACTIVETIME) == BST_CHECKED;
			instance->fromFrame = instance->fromFrameSpn->GetIVal();
			instance->toFrame = instance->toFrameSpn->GetIVal();
			instance->RenderFrames();
			if (instance->brightnessArray.size() > 0)
			{
				std::sort(instance->brightnessArray.begin(), instance->brightnessArray.end());
				WStr str; 
				str.printf(_T("Lowest Value: %f"), instance->brightnessArray.front());
				SetDlgItemText(hWnd, IDC_LOW_BRIGHT, str.ToMCHAR());
				str.printf(_T("Highest Value: %f"), instance->brightnessArray.back());
				SetDlgItemText(hWnd, IDC_HI_BRIGHT, str.ToMCHAR());
			}
			break;
		case IDC_APPLYCAMERA:
			//instance->TestFunc();
			//check if camera is selected (or take rendered camera)
			//check if average values are calculated
			//create modifier
			//create animated paramater
			//apply to camera
			instance->ApplyModifier();
			break;

		case IDC_R_ACTIVETIME:
			instance->EnableRangeCtrls(hWnd, false);
			break;

		case IDC_R_RANGE:
			instance->EnableRangeCtrls(hWnd, true);
			break;

		case IDC_CLOSE:
			EndDialog(hWnd, FALSE);
			ReleaseISpinner(instance->fromFrameSpn);
			ReleaseISpinner(instance->toFrameSpn);
			ReleaseISpinner(instance->targetBrightnessSpn);
			if (instance->iu)
				instance->iu->CloseUtility();
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
