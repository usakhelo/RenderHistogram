//**************************************************************************/
//
//**************************************************************************/
// DESCRIPTION:
// AUTHOR: 
//***************************************************************************/

#include "coronaAutoExposure.h"
#include "WindowsMessageFilter.h"
#include "notify.h"
#include "modstack.h"

#define CoronaAutoExposure_CLASS_ID	Class_ID(0x7354e284, 0x6556a2a9)

static CoronaAutoExposure theCoronaAutoExposure;

class CoronaAutoExposureClassDesc : public ClassDesc2 
{
public:
	int IsPublic() 							{ return TRUE; }
	void* Create(BOOL loading = FALSE) 		{ return &theCoronaAutoExposure; }
	const TCHAR * ClassName() 				{ return GetString(IDS_CLASS_NAME); }
	SClass_ID SuperClassID() 				{ return UTILITY_CLASS_ID; }
	Class_ID ClassID() 						{ return CoronaAutoExposure_CLASS_ID; }
	const TCHAR* Category() 				{ return GetString(IDS_CATEGORY); }

	const TCHAR* InternalName() 			{ return _T("CoronaAutoExposure"); }
	HINSTANCE HInstance() 					{ return hInstance; }
};

static CoronaAutoExposureClassDesc CoronaAutoExposureDesc;

CoronaAutoExposure::CoronaAutoExposure()
	: hPanel(nullptr)
	, iu(nullptr)
{
	isAnimRange = true;
	fromFrame = toFrame = 0;
	fromCalcFrame = toCalcFrame = 0;
	minBrVal = maxBrVal = currBrVal = 0.0f;
}

CoronaAutoExposure::~CoronaAutoExposure()
{
}

ClassDesc2* GetCoronaAutoExposureDesc() {	return &CoronaAutoExposureDesc; }

void CoronaAutoExposure::BeginEditParams(Interface* ip,IUtil* iu) 
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

void CoronaAutoExposure::EndEditParams(Interface* ip,IUtil*)
{
	this->iu = nullptr;
	ip->DeleteRollupPage(hPanel);
	hPanel = nullptr;
}

void CoronaAutoExposure::Init(HWND hWnd/*handle*/)
{
	theCoronaAutoExposure.fromFrameSpn = GetISpinner(GetDlgItem(hWnd, IDC_SPIN_FROM));
	theCoronaAutoExposure.fromFrameSpn->SetScale(1.0f);
	theCoronaAutoExposure.fromFrameSpn->SetLimits(-1000000, 1000000);
	theCoronaAutoExposure.fromFrameSpn->LinkToEdit(GetDlgItem(hWnd, IDC_EDIT_FROM), EDITTYPE_INT);
	theCoronaAutoExposure.fromFrameSpn->SetResetValue(0);

	theCoronaAutoExposure.toFrameSpn = GetISpinner(GetDlgItem(hWnd, IDC_SPIN_TO));
	theCoronaAutoExposure.toFrameSpn->SetScale(1.0f);
	theCoronaAutoExposure.toFrameSpn->SetLimits(-1000000, 1000000);
	theCoronaAutoExposure.toFrameSpn->LinkToEdit(GetDlgItem(hWnd, IDC_EDIT_TO), EDITTYPE_INT);
	theCoronaAutoExposure.toFrameSpn->SetResetValue(0);

	theCoronaAutoExposure.targetBrightnessSpn = GetISpinner(GetDlgItem(hWnd, IDC_SPIN_RATIO));
	theCoronaAutoExposure.targetBrightnessSpn->SetScale(1.0f);
	theCoronaAutoExposure.targetBrightnessSpn->SetLimits(-1000.0f, 1000.0f);
	theCoronaAutoExposure.targetBrightnessSpn->LinkToEdit(GetDlgItem(hWnd, IDC_EDIT_RATIO), EDITTYPE_FLOAT);
	theCoronaAutoExposure.targetBrightnessSpn->SetResetValue(1.0f);
	theCoronaAutoExposure.targetBrightnessSpn->SetValue(1.0f, FALSE);
	CheckDlgButton(hWnd,IDC_R_ACTIVETIME,TRUE);
	UpdateUI(hWnd);
}

void CoronaAutoExposure::Destroy(HWND /*handle*/)
{
}

INT_PTR CALLBACK CoronaAutoExposure::DlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg) 
	{
	case WM_INITDIALOG:
		theCoronaAutoExposure.Init(hWnd);

		DebugPrint(_M("WM_INITDIALOG %d\r\n"), hWnd);

		break;

	case WM_DESTROY:
		theCoronaAutoExposure.Destroy(hWnd);
		break;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {

		case IDC_DORENDER:
			theCoronaAutoExposure.isAnimRange = IsDlgButtonChecked(hWnd, IDC_R_ACTIVETIME) == BST_CHECKED;
			theCoronaAutoExposure.fromFrame = theCoronaAutoExposure.fromFrameSpn->GetIVal();
			theCoronaAutoExposure.toFrame = theCoronaAutoExposure.toFrameSpn->GetIVal();
			theCoronaAutoExposure.RenderFrames();
			break;
		case IDC_APPLYCAMERA:
			//theCoronaAutoExposure.TestFunc();
			theCoronaAutoExposure.ApplyModifier();
			break;

		case IDC_R_ACTIVETIME:
			theCoronaAutoExposure.EnableRangeCtrls(hWnd, false);
			break;

		case IDC_R_RANGE:
			theCoronaAutoExposure.EnableRangeCtrls(hWnd, true);
			break;

		case IDC_CLOSE:
			EndDialog(hWnd, FALSE);
			ReleaseISpinner(theCoronaAutoExposure.fromFrameSpn);
			ReleaseISpinner(theCoronaAutoExposure.toFrameSpn);
			ReleaseISpinner(theCoronaAutoExposure.targetBrightnessSpn);
			if (theCoronaAutoExposure.iu)
				theCoronaAutoExposure.iu->CloseUtility();
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

bool CoronaAutoExposure::CheckWindowsMessages(HWND hWnd)
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

void CoronaAutoExposure::ApplyModifier()
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

	//add animated parameter in the same frame range
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

	//calculate EV params
	float targetBrightness = targetBrightnessSpn->GetFVal();
	MaxSDK::Array<float> evArray(brightnessArray2.length());
	for(auto i=0; i < brightnessArray2.length(); i++) {
		float ratio = targetBrightness / brightnessArray2[i];
		float ev = log(ratio) / log(2);
		evArray[i] = ev;
	}

	BOOL enableEV = true;

	ParamBlockDesc2* pbdesc = coronaModPBlock->GetDesc();
	int param_index = pbdesc->NameToIndex(_T("simpleExposure"));
	const ParamDef* param_def = pbdesc->GetParamDefByIndex(param_index);

	Control *controller = (Control *) CreateInstance(CTRL_FLOAT_CLASS_ID,Class_ID(LININTERP_FLOAT_CLASS_ID,0)); 
	coronaModPBlock->SetControllerByID(param_def->ID, 0, controller, true);
	coronaModPBlock->SetValueByName<BOOL>( _T("overrideSimpleExposure"), enableEV, 0);

	SuspendAnimate();
	AnimateOn();
	for (int frame = startFrame, i=0; frame <= endFrame; frame += delta, i++)
	{
		if (i < evArray.length())
			coronaModPBlock->SetValue(param_def->ID, frame, evArray[i]);
	}
	ResumeAnimate();

	dobj->AddModifier(coronaCameraMod);

	node->SetObjectRef(dobj);
}

void CoronaAutoExposure::TestFunc()
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

float CoronaAutoExposure::CalculateMeanBrightness(Bitmap *bm)
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

void CoronaAutoExposure::RenderFrames()
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
	brightnessArray2.removeAll();
	brightnessArray2.reserve(duration / GetTicksPerFrame());

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
	for (int frame = startFrame; frame <= endFrame; frame += delta)	{
		ip->ProgressUpdate((int)((float)frame/duration * 100.0f));
		ip->CurRendererRenderFrame(frame, bm);

		WStr str; 
		str.printf(_T("Lowest Value: %f"), .5f);
		SetDlgItemText(hPanel, IDC_LOW_BRIGHT, str.ToMCHAR());

		currBrVal = CalculateMeanBrightness(bm);
		brightnessArray2.append(currBrVal);

		if (frame == startFrame) {
			maxBrVal = minBrVal = currBrVal;
		}

		if (currBrVal > maxBrVal)
			maxBrVal = currBrVal;

		if (currBrVal < minBrVal)
			minBrVal = currBrVal;

		fromCalcFrame = (int)((float)startFrame/delta);
		toCalcFrame = (int)((float)frame/delta);

		UpdateUI(hPanel);

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

void CoronaAutoExposure::UpdateUI(HWND hWnd) {

	WStr str; 
	str.printf(_T("Current: %f"), currBrVal);
	SetDlgItemText(hWnd, IDC_CUR_BRIGHT, str.ToMCHAR());
	str.printf(_T("Lowest: %f"), minBrVal);
	SetDlgItemText(hWnd, IDC_LOW_BRIGHT, str.ToMCHAR());
	str.printf(_T("Highest: %f"), maxBrVal);
	SetDlgItemText(hWnd, IDC_HI_BRIGHT, str.ToMCHAR());

	str.printf(_T("%d"), fromCalcFrame);
	SetDlgItemText(hWnd, IDC_FRAME_FROM, str.ToMCHAR());
	str.printf(_T("%d"), toCalcFrame);
	SetDlgItemText(hWnd, IDC_FRAME_TO, str.ToMCHAR());

	EnableWindow(GetDlgItem(hWnd, IDC_EDIT_FROM), !isAnimRange);
	EnableWindow(GetDlgItem(hWnd, IDC_EDIT_TO), !isAnimRange);
	EnableWindow(GetDlgItem(hWnd, IDC_SPIN_FROM), !isAnimRange);
	EnableWindow(GetDlgItem(hWnd, IDC_SPIN_TO), !isAnimRange);

	fromFrameSpn->SetValue(fromFrame, false);
	toFrameSpn->SetValue(toFrame, false);
}

void CoronaAutoExposure::EnableRangeCtrls(HWND hWnd, bool state) {
	EnableWindow(GetDlgItem(hWnd, IDC_EDIT_FROM), state);
	EnableWindow(GetDlgItem(hWnd, IDC_EDIT_TO), state);
	EnableWindow(GetDlgItem(hWnd, IDC_SPIN_FROM), state);
	EnableWindow(GetDlgItem(hWnd, IDC_SPIN_TO), state);
}
