//**************************************************************************/
//
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
#include "custcont.h"
#include "icolorman.h"
#include <INodeValidity.h>
//===========================================================================
//Camera Picking
//===========================================================================
BOOL CamPickNodeCallback::Filter(INode *node)
{
	ObjectState os = node->EvalWorldState(theCoronaAutoExposure.ip->GetTime());
	if (os.obj->SuperClassID()==CAMERA_CLASS_ID) 
		return TRUE;
	else 
		return FALSE;
}

BOOL PickCameraMode::HitTest(IObjParam *ip, HWND hWnd, ViewExp * vpt, IPoint2 m, int flags) {


	if ( ! vpt || ! vpt->IsAlive() )
	{
		DbgAssert(!_T("Invalid viewport!"));
		return FALSE;
	}

	return ip->PickNode(hWnd,m,&thePickFilt) ? TRUE : FALSE;
}

BOOL PickCameraMode::Pick(IObjParam *ip, ViewExp *vpt) {

	if ( ! vpt || ! vpt->IsAlive() )
	{
		DbgAssert(!_T("Invalid viewport!"));
		return FALSE;
	}

	INode *node = vpt->GetClosestHit();
	if (node) {
		ObjectState os = node->EvalWorldState(theCoronaAutoExposure.ip->GetTime());
		if (os.obj->SuperClassID() == CAMERA_CLASS_ID) {
			theCoronaAutoExposure.cameraNodeBtn->SetCheck(FALSE);
			theCoronaAutoExposure.SetCam(node);
			return TRUE;
		}
	}
	return FALSE;
}

int maxRndProgressCB::Progress( int done, int total ) {
	if (abort)
		return (RENDPROG_ABORT);
	else   
		return (RENDPROG_CONTINUE);
}

//===========================================================================
//Utility Methods
//===========================================================================

CoronaAutoExposure::CoronaAutoExposure()
	: hPanel(nullptr)
	, iu(nullptr)
{
	isAnimRange = true;
	targetBrightness = .5f;
	useSmooth = false;
	fromFrame = toFrame = 0;
	everyNth = 1;
	fromCalcFrame = toCalcFrame = 0;
	minBrVal = maxBrVal = currBrVal = 0.0f;
	passLimit = 50;
	camNode = nullptr;
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
	GetCOREInterface()->ClearPickMode();
}

void CoronaAutoExposure::Init(HWND hWnd/*handle*/)
{
	int numPoints;

	fromFrameSpn = GetISpinner(GetDlgItem(hWnd, IDC_SPIN_FROM));
	fromFrameSpn->SetScale(1.0f);
	fromFrameSpn->SetLimits(-1000000, 1000000);
	fromFrameSpn->LinkToEdit(GetDlgItem(hWnd, IDC_EDIT_FROM), EDITTYPE_INT);
	fromFrameSpn->SetResetValue(0);

	toFrameSpn = GetISpinner(GetDlgItem(hWnd, IDC_SPIN_TO));
	toFrameSpn->SetScale(1.0f);
	toFrameSpn->SetLimits(-1000000, 1000000);
	toFrameSpn->LinkToEdit(GetDlgItem(hWnd, IDC_EDIT_TO), EDITTYPE_INT);
	toFrameSpn->SetResetValue(0);

	targetBrightnessSpn = GetISpinner(GetDlgItem(hWnd, IDC_SPIN_TARGET));
	targetBrightnessSpn->SetScale(0.01f);
	targetBrightnessSpn->SetLimits(-1000.0f, 1000.0f);
	targetBrightnessSpn->LinkToEdit(GetDlgItem(hWnd, IDC_EDIT_TARGET), EDITTYPE_FLOAT);
	targetBrightnessSpn->SetResetValue(1.0f);

	everyNthSpn = GetISpinner(GetDlgItem(hWnd, IDC_SPIN_NTH));
	everyNthSpn->SetScale(1);
	everyNthSpn->SetLimits(0, 1000);
	everyNthSpn->LinkToEdit(GetDlgItem(hWnd, IDC_EDIT_NTH), EDITTYPE_INT);
	everyNthSpn->SetResetValue(1);

	passLimitSpn = GetISpinner(GetDlgItem(hWnd, IDC_SPIN_PASS));
	passLimitSpn->SetScale(1);
	passLimitSpn->SetLimits(0, 100000);
	passLimitSpn->LinkToEdit(GetDlgItem(hWnd, IDC_EDIT_PASS), EDITTYPE_INT);
	passLimitSpn->SetResetValue(1);

	cameraNodeBtn = GetICustButton(GetDlgItem(hWnd, IDC_CAMERA_NODE));
	cameraNodeBtn->SetType(CBT_CHECK);
	cameraNodeBtn->SetHighlightColor(GREEN_WASH);
	cameraNodeBtn->SetCheck(FALSE);

	RegisterNotification(PreOpen, this, NOTIFY_FILE_PRE_OPEN);
	RegisterNotification(PreDeleteNode, this, NOTIFY_SCENE_PRE_DELETED_NODE);
	
	if (camNode != nullptr) {
		INodeValidity *nodeValidity = static_cast<INodeValidity*>(camNode->GetInterface(NODEVALIDITY_INTERFACE));
		if (nodeValidity == nullptr) {   //seems like prevents crash after scene reset
			camNode = nullptr;
		}
	}

	if (camNode != nullptr && camNode->GetObjectRef() == nullptr)
		camNode = nullptr;

	UpdateUI(hWnd);
}

void CoronaAutoExposure::Destroy(HWND /*handle*/)
{
	UnRegisterNotification(PreOpen, this, NOTIFY_FILE_PRE_OPEN);
	UnRegisterNotification(PreDeleteNode, this, NOTIFY_SCENE_PRE_DELETED_NODE);

	SetCam(nullptr);
}

void CoronaAutoExposure::PreOpen(void* param, NotifyInfo* info) {
	CoronaAutoExposure* utlityObj = (CoronaAutoExposure*)param;
	if (utlityObj == NULL)
		return;

	utlityObj->SetCam(nullptr);
}

void CoronaAutoExposure::PreDeleteNode(void* param, NotifyInfo* arg) {
	CoronaAutoExposure* utlityObj = (CoronaAutoExposure*)param;
	if (utlityObj == NULL)
		return;

	INode* deletedNode = (INode*)arg->callParam;
	if (deletedNode == utlityObj->camNode) {
		utlityObj->SetCam(nullptr);
	}
}

void CoronaAutoExposure::SetCam(INode *node) {
	camNode = node;
	WStr label;
	label = node != nullptr ? node->GetName() : _M("Select Camera");
  cameraNodeBtn->SetText(label);
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
			theCoronaAutoExposure.RenderFrames();
			break;
		case IDC_APPLYCAMERA:
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
			ReleaseISpinner(theCoronaAutoExposure.passLimitSpn);
			ReleaseISpinner(theCoronaAutoExposure.fromFrameSpn);
			ReleaseISpinner(theCoronaAutoExposure.toFrameSpn);
			ReleaseISpinner(theCoronaAutoExposure.targetBrightnessSpn);
			ReleaseISpinner(theCoronaAutoExposure.everyNthSpn);
			ReleaseICustButton(theCoronaAutoExposure.cameraNodeBtn);

			if (theCoronaAutoExposure.iu)
				theCoronaAutoExposure.iu->CloseUtility();
			break;

		case IDC_CAMERA_NODE:
			GetCOREInterface()->SetPickMode(&thePick);
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
			break;
		case IDC_SPIN_PASS:
			theCoronaAutoExposure.passLimit = theCoronaAutoExposure.passLimitSpn->GetIVal();
			break;
		case IDC_SPIN_NTH: {
			int tempNth = theCoronaAutoExposure.everyNthSpn->GetIVal();
			if (tempNth < 1) {
				tempNth = 1;
				theCoronaAutoExposure.everyNthSpn->SetValue(tempNth, false);
			}
			theCoronaAutoExposure.everyNth = tempNth;
			break;
											 }
		default:
			break;
		}
		break;

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

	str.printf(_T("%s"), camNode != nullptr ? camNode->GetName() : _M("Select Camera"));
  cameraNodeBtn->SetText(str);

	CheckDlgButton(hWnd, IDC_R_ACTIVETIME, isAnimRange);
	CheckDlgButton(hWnd, IDC_R_RANGE, !isAnimRange);

	EnableWindow(GetDlgItem(hWnd, IDC_EDIT_FROM), !isAnimRange);
	EnableWindow(GetDlgItem(hWnd, IDC_EDIT_TO), !isAnimRange);
	EnableWindow(GetDlgItem(hWnd, IDC_SPIN_FROM), !isAnimRange);
	EnableWindow(GetDlgItem(hWnd, IDC_SPIN_TO), !isAnimRange);

	CheckDlgButton(hWnd, IDC_C_SMOOTH, useSmooth);

	everyNthSpn->SetValue(everyNth, false);
	passLimitSpn->SetValue(passLimit, false);
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

//===========================================================================
//Core Methods
//===========================================================================

void CoronaAutoExposure::ApplyModifier()
{
	if (camNode == nullptr) {
		TSTR title = GetString(IDS_CLASS_NAME);
		TSTR message = GetString(IDS_SELECTION_ERROR);
		MessageBox(hPanel, message, title, MB_ICONEXCLAMATION);
		return;
	}

	Object *obj = camNode->GetObjectRef();
	Modifier *coronaCameraMod = nullptr;
	IDerivedObject *dobj = nullptr;

	if (!obj)
	{
		TSTR title = GetString(IDS_CLASS_NAME);
		TSTR message = GetString(IDS_SELECTION_ERROR);
		MessageBox(hPanel, message, title, MB_ICONEXCLAMATION);
		return;
	}

	if (obj->SuperClassID() == GEN_DERIVOB_CLASS_ID)
	{
		dobj = static_cast<IDerivedObject*>(obj);

		auto rootobj = dobj->GetObjRef();
		if (rootobj->SuperClassID() == CAMERA_CLASS_ID) {
			obj = rootobj;
		} else {
			TSTR title = GetString(IDS_CLASS_NAME);
			TSTR message = GetString(IDS_SELECTIONTYPE_ERROR);
			MessageBox(hPanel, message, title, MB_ICONEXCLAMATION);
			return;
		}

		if (dobj->NumModifiers() > 0) {
			Modifier* modobj = dobj->GetModifier(0); //upper modifier
			if (modobj->ClassID() == Class_ID((ulong)2743551132, (ulong)502132111))
				coronaCameraMod = modobj;
		}
	}

	if (obj->SuperClassID() != CAMERA_CLASS_ID)
	{
		TSTR title = GetString(IDS_CLASS_NAME);
		TSTR message = GetString(IDS_SELECTIONTYPE_ERROR);
		MessageBox(hPanel, message, title, MB_ICONEXCLAMATION);
		return;
	}

	if (brightArray.length() == 0)
	{
		TSTR title = GetString(IDS_CLASS_NAME);
		TSTR message = GetString(IDS_EMPTY_ARRAY_ERR);
		MessageBox(hPanel, message, title, MB_ICONEXCLAMATION);
		return;
	}

	//add animated parameter in the same frame range
	TimeValue startFrame, endFrame;
	int duration;
	int delta = GetTicksPerFrame() * everyNth;
	if (isAnimRange) {
		Interval frames = ip->GetAnimRange();
		startFrame = frames.Start();
		endFrame = frames.End();
	}
	else {
		startFrame = fromFrame * GetTicksPerFrame();
		endFrame = toFrame * GetTicksPerFrame();
	}

	//calculate EV params
	MaxSDK::Array<float> evArray(brightArray.length());
	for (auto i = 0; i < brightArray.length(); i++) {
		float ratio = targetBrightness / brightArray[i];
		DebugPrint(_T("brighness: %f\r\n"), brightArray[i]);
		float ev = log(ratio) / log(2);
		evArray[i] = ev;
	}

	if (useSmooth)
		SmoothCurve(evArray);

	if (dobj == nullptr)
		dobj = CreateDerivedObject(obj);

	bool newmod = false;
	if (coronaCameraMod == nullptr) {
		coronaCameraMod = (Modifier *)CreateInstance(OSM_CLASS_ID, Class_ID((ulong)2743551132, (ulong)502132111));
		newmod = true;
	}

	if (!coronaCameraMod->IsEnabled())
		coronaCameraMod->EnableMod();

	IParamBlock2* coronaModPBlock = ((Animatable*)coronaCameraMod)->GetParamBlock(0);
	assert(coronaModPBlock);

	BOOL enableEV = true;

	ParamBlockDesc2* pbdesc = coronaModPBlock->GetDesc();
	int param_index = pbdesc->NameToIndex(_T("simpleExposure"));
	const ParamDef* param_def = pbdesc->GetParamDefByIndex(param_index);

	Control *controller = coronaModPBlock->GetControllerByID(param_def->ID, 0);
	if ( controller != nullptr ) {
		controller->DeleteThis();
	}

	int interp = everyNth > 3 ? HYBRIDINTERP_FLOAT_CLASS_ID : LININTERP_FLOAT_CLASS_ID;
	controller = (Control *)CreateInstance(CTRL_FLOAT_CLASS_ID, Class_ID(interp, 0));
	coronaModPBlock->SetControllerByID(param_def->ID, 0, controller, true);
	coronaModPBlock->SetValueByName<BOOL>(_T("overrideSimpleExposure"), enableEV, 0);

	SuspendAnimate();
	AnimateOn();
	for (int frame = startFrame, i = 0; frame <= endFrame; frame += delta, i++)
	{
		if (i < evArray.length()) {
			coronaModPBlock->SetValue(param_def->ID, frame, evArray[i]);
			DebugPrint(_T("EV: %f\r\n"), evArray[i]);
		}
	}
	ResumeAnimate();

	if(newmod) {
		dobj->AddModifier(coronaCameraMod);
		camNode->SetObjectRef(dobj);
	}
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
			//if (i == 1)
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

	result = numOfPx / sumLum;
	//DebugPrint(_M("harmonic: %f\r\n"), numOfPx / sumLum);
	//DebugPrint(_M("arithmetic: %f\r\n"), sumLum2 / numOfPx);
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

	if (brightArray.length() > 0) {
		TSTR title = GetString(IDS_CLASS_NAME);
		TSTR message = GetString(IDS_RENDER_OVERWRITE);
		int result = MessageBox(hPanel, message, title, MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON1);
		if (result == IDNO)
			return;
	}

	camNode = cam;
#pragma message(TODO("check for selected camera object too"))

	ResetCounters();
	UpdateUI(hPanel);

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

	TimeValue startFrame, endFrame;
	int duration;
	int delta = everyNth * GetTicksPerFrame();

	if (isAnimRange) {
		Interval frames = ip->GetAnimRange();
		startFrame = frames.Start();
		endFrame = frames.End();
		duration = (int)frames.Duration();
	}
	else {
		startFrame = fromFrame * GetTicksPerFrame();
		endFrame = toFrame * GetTicksPerFrame();
		duration = endFrame - startFrame + GetTicksPerFrame();
	}

	brightArray.removeAll();
	brightArray.reserve(duration / delta);
	duration = duration / everyNth;

	Renderer *renderer = ip->GetCurrentRenderer(false);
	auto pBlock = renderer->GetParamBlock(0);
	BOOL mbCam, mbGeo, dofUse;
	int passes;
	bool result;
	Interval inf;
	inf.SetInfinite();
	result = pBlock->GetValueByName<BOOL>(_T("mb.useCamera"), ip->GetTime(), mbCam, inf);
	result = pBlock->GetValueByName<BOOL>(_T("mb.useGeometry"), ip->GetTime(), mbGeo, inf);
	result = pBlock->GetValueByName<BOOL>(_T("dof.use"), ip->GetTime(), dofUse, inf);
	result = pBlock->GetValueByName<int>(_T("progressive.passLimit"), ip->GetTime(), passes, inf);

	BOOL off = false;
	int passL = passLimit;
	result = pBlock->SetValueByName<BOOL>(_T("mb.useCamera"), off, ip->GetTime());
	result = pBlock->SetValueByName<BOOL>(_T("mb.useGeometry"), off, ip->GetTime());
	result = pBlock->SetValueByName<BOOL>(_T("dof.use"), off, ip->GetTime());
	result = pBlock->SetValueByName<int>(_T("progressive.passLimit"), passL, ip->GetTime());

	maxRndProgressCB rndCB;
	rndCB.abort = false;

	LPVOID arg = nullptr;

	ip->ProgressStart(_M("Calculating Average Brightness"), TRUE, fn, arg);
	ip->OpenCurRenderer(cam, NULL, RENDTYPE_NORMAL);

	int progress = 0;
	for (int frame = startFrame; frame <= endFrame; frame += delta) {

		ip->ProgressUpdate((int)((float)progress / duration * 100.0f));
		progress += delta / everyNth;

		ip->CurRendererRenderFrame(frame, bm, &rndCB);

		FPValue fValue = GetCoronaBuffer(renderer);
		if (is_bitmap(fValue.v)) {
			MAXBitMap *mbm = (MAXBitMap*)fValue.v;
			if (mbm->bm != nullptr) {
				currBrVal = CalculateMeanBrightness(mbm->bm);
				fValue.Free();
			}
		}

		brightArray.append(currBrVal);

		if (frame == startFrame) {
			maxBrVal = minBrVal = currBrVal;
		}

		if (currBrVal > maxBrVal)
			maxBrVal = currBrVal;

		if (currBrVal < minBrVal)
			minBrVal = currBrVal;

		fromCalcFrame = (int)((float)startFrame / delta * everyNth);
		toCalcFrame = (int)((float)frame / delta * everyNth);

		UpdateUI(hPanel);

		if (ip->GetCancel()) {
			int retval = MessageBox(ip->GetMAXHWnd(), _M("Really Cancel?"), _M("Question"), MB_ICONQUESTION | MB_YESNO);
			if (retval == IDYES) {
				rndCB.abort = true;
				break;
			}
			else if (retval == IDNO) {
				rndCB.abort = false;
				ip->SetCancel(FALSE);
			}
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