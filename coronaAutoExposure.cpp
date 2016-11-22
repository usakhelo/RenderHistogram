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
#include "GetCOREInterface.h"
#include <maxscript/maxwrapper/bitmaps.h>
#include <maxscript/maxscript.h>

#define CoronaAutoExposure_CLASS_ID	Class_ID(0x7354e284, 0x6556a2a9)


class maxRndProgressCB : public RendProgressCallback2 {
	public:
		void	SetTitle ( const MCHAR *title ) {};
		int		Progress ( int done, int total );
    void  SetStep (int current, int total) {};
		bool	abort;
};

int maxRndProgressCB::Progress( int done, int total ) {
	if (abort)
		return (RENDPROG_ABORT);
	else   
		return (RENDPROG_CONTINUE);
}

static CoronaAutoExposure theCoronaAutoExposure;

class CoronaAutoExposureClassDesc : public ClassDesc2
{
public:
	int IsPublic() { return TRUE; }
	void* Create(BOOL loading = FALSE) { return &theCoronaAutoExposure; }
	const TCHAR * ClassName() { return GetString(IDS_CLASS_NAME); }
	SClass_ID SuperClassID() { return UTILITY_CLASS_ID; }
	Class_ID ClassID() { return CoronaAutoExposure_CLASS_ID; }
	const TCHAR* Category() { return GetString(IDS_CATEGORY); }

	const TCHAR* InternalName() { return _T("CoronaAutoExposure"); }
	HINSTANCE HInstance() { return hInstance; }
};

static CoronaAutoExposureClassDesc CoronaAutoExposureDesc;

CoronaAutoExposure::CoronaAutoExposure()
	: hPanel(nullptr)
	, iu(nullptr)
{
	isAnimRange = true;
	targetBrightness = .5f;
	useSmooth = false;
	fromFrame = toFrame = 0;
	fromCalcFrame = toCalcFrame = 0;
	minBrVal = maxBrVal = currBrVal = 0.0f;
}

CoronaAutoExposure::~CoronaAutoExposure()
{
}

ClassDesc2* GetCoronaAutoExposureDesc() { return &CoronaAutoExposureDesc; }

void CoronaAutoExposure::BeginEditParams(Interface* ip, IUtil* iu)
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

void CoronaAutoExposure::EndEditParams(Interface* ip, IUtil*)
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

	theCoronaAutoExposure.targetBrightnessSpn = GetISpinner(GetDlgItem(hWnd, IDC_SPIN_TARGET));
	theCoronaAutoExposure.targetBrightnessSpn->SetScale(0.01f);
	theCoronaAutoExposure.targetBrightnessSpn->SetLimits(-1000.0f, 1000.0f);
	theCoronaAutoExposure.targetBrightnessSpn->LinkToEdit(GetDlgItem(hWnd, IDC_EDIT_TARGET), EDITTYPE_FLOAT);
	theCoronaAutoExposure.targetBrightnessSpn->SetResetValue(1.0f);

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
		break;

	case WM_DESTROY:
		theCoronaAutoExposure.Destroy(hWnd);
		break;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {

		case IDC_DORENDER:
			theCoronaAutoExposure.ResetCounters();
			theCoronaAutoExposure.UpdateUI(hWnd);
			theCoronaAutoExposure.RenderFrames();
			break;
		case IDC_APPLYCAMERA:
			//theCoronaAutoExposure.TestFunc();
			theCoronaAutoExposure.ApplyModifier();
			break;

		case IDC_R_ACTIVETIME:
			theCoronaAutoExposure.isAnimRange = true;
			theCoronaAutoExposure.UpdateUI(hWnd);
			break;

		case IDC_R_RANGE:
			theCoronaAutoExposure.isAnimRange = false;
			theCoronaAutoExposure.UpdateUI(hWnd);
			break;

		case IDC_C_SMOOTH:
			theCoronaAutoExposure.useSmooth = (bool)IsDlgButtonChecked(hWnd, IDC_C_SMOOTH);
			theCoronaAutoExposure.UpdateUI(hWnd);
			break;
			//useSmooth

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

	case CC_SPINNER_CHANGE:
		switch (LOWORD(wParam)) {
		case IDC_SPIN_FROM:
			theCoronaAutoExposure.fromFrame = theCoronaAutoExposure.fromFrameSpn->GetIVal();
			break;
		case IDC_SPIN_TO:
			theCoronaAutoExposure.toFrame = theCoronaAutoExposure.toFrameSpn->GetIVal();
			break;
		case IDC_SPIN_TARGET:
			theCoronaAutoExposure.targetBrightness = theCoronaAutoExposure.targetBrightnessSpn->GetFVal();
		}

	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_MOUSEMOVE:
		GetCOREInterface()->RollupMouseMessage(hWnd, msg, wParam, lParam);
		break;

	default:
		return 0;
	}
	return 1;
}

void CoronaAutoExposure::ResetCounters() {
	fromCalcFrame = toCalcFrame = 0;
	minBrVal = maxBrVal = currBrVal = 0.0f;
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

	CheckDlgButton(hWnd, IDC_R_ACTIVETIME, isAnimRange);
	CheckDlgButton(hWnd, IDC_R_RANGE, !isAnimRange);

	EnableWindow(GetDlgItem(hWnd, IDC_EDIT_FROM), !isAnimRange);
	EnableWindow(GetDlgItem(hWnd, IDC_EDIT_TO), !isAnimRange);
	EnableWindow(GetDlgItem(hWnd, IDC_SPIN_FROM), !isAnimRange);
	EnableWindow(GetDlgItem(hWnd, IDC_SPIN_TO), !isAnimRange);

	CheckDlgButton(hWnd, IDC_C_SMOOTH, useSmooth);

	fromFrameSpn->SetValue(fromFrame, false);
	toFrameSpn->SetValue(toFrame, false);
	targetBrightnessSpn->SetValue(targetBrightness, false);
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

	IParamBlock2* coronaModPBlock = ((Animatable*)coronaCameraMod)->GetParamBlock(0);
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
	}
	else {
		startFrame = fromFrame * GetTicksPerFrame();
		endFrame = toFrame * GetTicksPerFrame();
		duration = endFrame - startFrame + 1;
	}

	//calculate EV params
	MaxSDK::Array<float> evArray(brightnessArray2.length());
	for (auto i = 0; i < brightnessArray2.length(); i++) {
		float ratio = targetBrightness / brightnessArray2[i];
		DebugPrint(_T("brighness: %f\r\n"), brightnessArray2[i]);
		float ev = log(ratio) / log(2);
		evArray[i] = ev;
	}

	if (useSmooth)
		SmoothCurve(evArray);

	BOOL enableEV = true;

	ParamBlockDesc2* pbdesc = coronaModPBlock->GetDesc();
	int param_index = pbdesc->NameToIndex(_T("simpleExposure"));
	const ParamDef* param_def = pbdesc->GetParamDefByIndex(param_index);

	Control *controller = (Control *)CreateInstance(CTRL_FLOAT_CLASS_ID, Class_ID(LININTERP_FLOAT_CLASS_ID, 0));
	coronaModPBlock->SetControllerByID(param_def->ID, 0, controller, true);
	coronaModPBlock->SetValueByName<BOOL>(_T("overrideSimpleExposure"), enableEV, 0);

	SuspendAnimate();
	AnimateOn();
	for (int frame = startFrame, i = 0; frame <= endFrame; frame += delta, i++)
	{
		//TODO: if evArray is empty dont add modifier
		if (i < evArray.length()) {
			coronaModPBlock->SetValue(param_def->ID, frame, evArray[i]);
			DebugPrint(_T("EV: %f\r\n"), evArray[i]);
		}
	}
	ResumeAnimate();

	dobj->AddModifier(coronaCameraMod);

	node->SetObjectRef(dobj);
}

void CoronaAutoExposure::SmoothCurve(MaxSDK::Array<float>& evArray) {
	if (evArray.length() > 2) {
		for (int i = 1; i < evArray.length() - 1; i++) {

      //geometric mean
      //if (i == 1)
      //  evArray[i] = pow(((evArray[i - 1]) * (evArray[i]) * (evArray[i + 1])), 1/3.0f);
      //else if (evArray.length() > 4 && i < evArray.length() - 2)
      //  evArray[i] = pow(((evArray[i - 2]) * (evArray[i - 1]) * (evArray[i]) * (evArray[i + 1]) * (evArray[i + 2])), 1/5.0f);
      //else if (evArray.length() > 3)
      //  evArray[i] = pow(((evArray[i - 2]) * (evArray[i - 1]) * (evArray[i]) * (evArray[i + 1])), 1/4.0f);

      //arithmetic mean
      if (i == 1)
        evArray[i] = ((evArray[i - 1]) + (evArray[i]) + (evArray[i + 1])) / 3.0f;
      else if (evArray.length() > 4 && i < evArray.length() - 2)
        evArray[i] = ((evArray[i - 2]) + (evArray[i - 1]) + (evArray[i]) + (evArray[i + 1]) + (evArray[i + 2])) / 5.0f;
      else if (evArray.length() > 3)
        evArray[i] = ((evArray[i - 2]) + (evArray[i - 1]) + (evArray[i]) + (evArray[i + 1])) / 4.0f;

			//harmonic mean (doesn't work because of negative-positive values
   //   if (i == 1)
			//	evArray[i] = 3.0f / ((1 / evArray[i - 1]) + (1 / evArray[i]) + (1 / evArray[i + 1]));
			//else if (evArray.length() > 4 && i < evArray.length() - 2)
			//	evArray[i] = 5.0f / ((1 / evArray[i - 2]) + (1 / evArray[i - 1]) + (1 / evArray[i]) + (1 / evArray[i + 1]) + (1 / evArray[i + 2]));
			//else if (evArray.length() > 3)
			//	evArray[i] = 4.0f / ((1 / evArray[i - 2]) + (1 / evArray[i - 1]) + (1 / evArray[i]) + (1 / evArray[i + 1]));
		}
	}
}

void CoronaAutoExposure::TestFunc()
{
	ClassDesc * classDesc = ip->GetDllDir().ClassDir().FindClass(RENDERER_CLASS_ID, Class_ID((ulong)1655201228, (ulong)1379677700));
	FPInterface * intface = classDesc->GetInterfaceAt(1);

	FPValue result, result1, param1, param2, param3;
	FPStatus fnStatus = FPS_FAIL;

	param1.type = (ParamType2)TYPE_INT;
	param1.i = 0;
	param2.type = (ParamType2)TYPE_BOOL;
	param2.i = false;
	param3.type = (ParamType2)TYPE_BOOL;
	param3.i = false;
	FPParams fnParams;
	fnParams.params.append(param1);
	fnParams.params.append(param2);
	fnParams.params.append(param3);

	FunctionID fid = intface->FindFn(_T("getCoronaVersion"));
	if (fid) {
		try {
			fnStatus = intface->Invoke(fid, result);
		}
		catch (...) {
		}
	}

	fid = intface->FindFn(_T("getVfbContent"));
	try {
		fnStatus = intface->Invoke(fid, result, &fnParams);
	}
	catch (...) {
	}

	bool testres = is_bitmap(result.v);
	MAXBitMap *mbm = (MAXBitMap*)result.v;
	BitmapInfo* bi2 = &mbm->bi;
	Bitmap* bm2 = mbm->bm;
}

FPValue CoronaAutoExposure::GetCoronaBuffer(Renderer *renderer) {

	ClassDesc * classDesc = ip->GetDllDir().ClassDir().FindClass(RENDERER_CLASS_ID, Class_ID((ulong)1655201228, (ulong)1379677700));
	FPInterface * intface = classDesc->GetInterfaceAt(1);

	FPValue result, param1, param2, param3;
	FPStatus fnStatus = FPS_FAIL;

	param1.type = (ParamType2)TYPE_INT;
	param1.i = 0;
	param2.type = (ParamType2)TYPE_BOOL;
	param2.i = false;
	param3.type = (ParamType2)TYPE_BOOL;
	param3.i = false;
	FPParams fnParams;
	fnParams.params.append(param1);
	fnParams.params.append(param2);
	fnParams.params.append(param3);

	FunctionID fid = intface->FindFn(_T("getVfbContent"));
	try {
		fnStatus = intface->Invoke(fid, result, &fnParams);
	}
	catch (...) {
	}

	//intface->ReleaseInterface();

	return result;
}

float CoronaAutoExposure::CalculateMeanBrightness(Bitmap *bm)
{
	if (!bm)
		return 0.0;

	const double rY = 0.212655;
	const double gY = 0.715158;
	const double bY = 0.072187;
  float sumLum = 0.0;
  float sumLum2 = 0.0;
	float result = 0.0;

	int biWidth = bm->Width();
	int biHeight = bm->Height();
  int numOfPx = (biWidth * biHeight);
	BMM_Color_fl c1;

	for (int i = 0; i < biWidth; i++)
	{
		for (int j = 0; j < biHeight; j++)
		{
			bm->GetPixels(i, j, 1, &c1);
			float lum = rY*c1.r + gY*c1.g + bY*c1.b;
      if (lum > 0.0f)
        sumLum += 1.0f / lum;
      else
        sumLum += 0.0f;

      sumLum2 += lum;
		}
	}

	//harmonic mean
	result = numOfPx / sumLum;
  DebugPrint(_M("harmonic: %f\r\n"), result);
  DebugPrint(_M("arithmetic: %f\r\n"), sumLum2 / numOfPx);
	//arithmetic mean
	//result = sumLum / (biWidth * biHeight);

	return result;
}

void CoronaAutoExposure::RenderFrames()
{
	ViewParams vp;
	INode *cam = ip->GetViewExp(NULL).GetViewCamera();
	if (!cam) {
		TSTR title = GetString(IDS_CLASS_NAME);
		TSTR message = GetString(IDS_CAM_ERROR);
		MessageBox(hPanel, message, title, MB_ICONEXCLAMATION);
		return;
	}
#pragma message(TODO("check for selected camera object too"))

	Renderer* currRenderer = ip->GetCurrentRenderer(false);
	MSTR rendName;
	currRenderer->GetClassName(rendName);
	if (!rendName.StartsWith(_T("corona"), false)) {
		TSTR title = GetString(IDS_CLASS_NAME);
		TSTR message = GetString(IDS_REND_ERROR);
		MessageBox(hPanel, message, title, MB_ICONEXCLAMATION);
		return;
	}

	if (fromFrame > toFrame) {
		TSTR title = GetString(IDS_CLASS_NAME);
		TSTR message = GetString(IDS_FRAME_ERROR);
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
		bi.SetType(BMM_FLOAT_RGBA_32); //BMM_FLOAT_RGBA_32 / BMM_TRUE_64
		bi.SetFlags(MAP_HAS_ALPHA);
		bi.SetAspect(1.0f);
		bm = TheManager->Create(&bi);
	}

	TimeValue startFrame, endFrame;
	int duration;
	int delta = GetTicksPerFrame();
	if (isAnimRange) {
		Interval frames = ip->GetAnimRange();
		startFrame = frames.Start();
		endFrame = frames.End();
		duration = (int)frames.Duration();
	}
	else {
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

	maxRndProgressCB rndCB;
  rndCB.abort = false;

	LPVOID arg = nullptr;
	ip->ProgressStart(_M("Calculating Average Brightness"), TRUE, fn, arg);
	int progress = 0;
	for (int frame = startFrame; frame <= endFrame; frame += delta) {

		ip->ProgressUpdate((int)((float)progress / duration * 100.0f));
		progress += delta;

    GetCOREInterface8()->QuickRender(frame, bm);//, &rndCB);

		FPValue fValue = GetCoronaBuffer(renderer);
		if (is_bitmap(fValue.v)) {
			MAXBitMap *mbm = (MAXBitMap*)fValue.v;
			if (mbm->bm != nullptr) {
				currBrVal = CalculateMeanBrightness(mbm->bm);
				fValue.Free();
			}
		}

		brightnessArray2.append(currBrVal);

		if (frame == startFrame) {
			maxBrVal = minBrVal = currBrVal;
		}

		if (currBrVal > maxBrVal)
			maxBrVal = currBrVal;

		if (currBrVal < minBrVal)
			minBrVal = currBrVal;

		fromCalcFrame = (int)((float)startFrame / delta);
		toCalcFrame = (int)((float)frame / delta);

		UpdateUI(hPanel);

		if (ip->GetCancel()) {
      rndCB.abort = ip->GetCancel();
			int retval = MessageBox(ip->GetMAXHWnd(), _M("Really Cancel?"), _M("Question"), MB_ICONQUESTION | MB_YESNO);
			if (retval == IDYES)
				break;
			else if (retval == IDNO)
				ip->SetCancel(FALSE);
		}

		if (!CheckWindowsMessages(ip->GetMAXHWnd()))
			break;
	}
	ip->ProgressEnd();

	result = pBlock->SetValueByName<BOOL>(_T("mb.useCamera"), mbCam, ip->GetTime());
	result = pBlock->SetValueByName<BOOL>(_T("mb.useGeometry"), mbGeo, ip->GetTime());
	result = pBlock->SetValueByName<BOOL>(_T("dof.use"), dofUse, ip->GetTime());

	bm->DeleteThis();
}