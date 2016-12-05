#pragma once
//**************************************************************************/
// 
// DESCRIPTION: Includes for Plugins
// AUTHOR: 
//***************************************************************************/

#include "3dsmaxsdk_preinclude.h"
#include "resource.h"
#include <istdplug.h>
#include <iparamb2.h>
#include <iparamm2.h>
#include <maxtypes.h>
#include <mouseman.h>
#include <utilapi.h>



extern TCHAR *GetString(int id);
extern HINSTANCE hInstance;

#define CoronaAutoExposure_CLASS_ID	Class_ID(0x7354e284, 0x6556a2a9)

class CoronaAutoExposure : public UtilityObj 
{
    friend class ObjPick;

public:

	CoronaAutoExposure();
	virtual ~CoronaAutoExposure();

	void DeleteThis() { }

	void BeginEditParams(Interface *ip,IUtil *iu);
	void EndEditParams(Interface *ip,IUtil *iu);

	void Init(HWND hWnd);
	void Destroy(HWND hWnd);

	bool CheckWindowsMessages(HWND);
	void TestFunc();
	void ApplyModifier();
	void SmoothCurve(MaxSDK::Array<float>&);
	void RenderFrames();
	float CalculateMeanBrightness(Bitmap*);
	FPValue GetCoronaBuffer(Renderer*);
	void ResetCounters();
	void UpdateUI(HWND);

private:
	static INT_PTR CALLBACK DlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

	ISpinnerControl*  fromFrameSpn;
	ISpinnerControl*	passLimitSpn;
	ISpinnerControl*  toFrameSpn;
	ISpinnerControl*  everyNthSpn;
	ISpinnerControl*  targetBrightnessSpn;

	ICustButton*	cameraNodeBtn;

	bool isAnimRange;
	int fromFrame, toFrame, everyNth;
	int fromCalcFrame, toCalcFrame;
	float minBrVal, maxBrVal, currBrVal;
	float targetBrightness;
	bool useSmooth;
	int passLimit;

	HWND   hPanel;
	IUtil* iu;
	Interface *ip;
	MaxSDK::Array<float> brightArray;
};

static CoronaAutoExposure theCoronaAutoExposure;

class maxRndProgressCB : public RendProgressCallback2 {
public:
	void	SetTitle(const MCHAR *title) {};
	int		Progress(int done, int total);
	void  SetStep(int current, int total) {};
	bool	abort;
};

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

static CommandMode*		lastMode = NULL;

static void SetPickMode(PickModeCallback* p, int w = 0) {
    if (!p) {
        GetCOREInterface()->PushCommandMode(lastMode);
        lastMode = NULL;
        GetCOREInterface()->ClearPickMode();
    } else {
        lastMode = GetCOREInterface()->GetCommandMode();
        GetCOREInterface()->SetPickMode(p);
    }
}

class ObjPick : public PickModeCallback {

		CoronaAutoExposure *parent;

public:		
    BOOL HitTest(IObjParam *ip,HWND hWnd,ViewExp *vpt,IPoint2 m,int flags);
    BOOL Pick(IObjParam *ip,ViewExp *vpt);	

    void EnterMode(IObjParam *ip);
    void ExitMode(IObjParam *ip);

    void SetParentObj(CoronaAutoExposure *obj) { parent = obj; }
};

static  ObjPick  thePick;
