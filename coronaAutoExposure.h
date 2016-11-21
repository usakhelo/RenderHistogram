#pragma once

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
// DESCRIPTION: Includes for Plugins
// AUTHOR: 
//***************************************************************************/

#include "3dsmaxsdk_preinclude.h"
#include "resource.h"
#include <istdplug.h>
#include <iparamb2.h>
#include <iparamm2.h>
#include <maxtypes.h>

#include <utilapi.h>

extern TCHAR *GetString(int id);
extern HINSTANCE hInstance;

class CoronaAutoExposure : public UtilityObj 
{
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
	void RenderFrames();
	float CalculateMeanBrightness(Bitmap*);
	Bitmap* GetCoronaBuffer(Renderer*);
	void EnableRangeCtrls(HWND, bool);
	void UpdateUI(HWND);

private:
	static INT_PTR CALLBACK DlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

	ISpinnerControl*  fromFrameSpn;
	ISpinnerControl*  toFrameSpn;
	ISpinnerControl*  targetBrightnessSpn;

	bool isAnimRange;
	int fromFrame, toFrame;
	int fromCalcFrame, toCalcFrame;
	float minBrVal, maxBrVal, currBrVal;

	HWND   hPanel;
	IUtil* iu;
	Interface *ip;
	MaxSDK::Array<float> brightnessArray2;
};
