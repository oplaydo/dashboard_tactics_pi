/***************************************************************************
* $Id: tactics_pi.cpp, v1.0 2016/06/07 tomBigSpeedy Exp $
*
* Project:  OpenCPN
* Purpose:  tactics Plugin
* Author:   Thomas Rauch
*       (Inspired by original work from Jean-Eudes Onfray)
***************************************************************************
*   Copyright (C) 2010 by David S. Register                               *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
*   This program is distributed in the hope that it will be useful,       *
*   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
*   GNU General Public License for more details.                          *
*                                                                         *
*   You should have received a copy of the GNU General Public License     *
*   along with this program; if not, write to the                         *
*   Free Software Foundation, Inc.,                                       *
*   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,  USA.         *
***************************************************************************
*/

#include "tactics_pi.h"

extern int g_iDashSpeedMax;
extern int g_iDashCOGDamp;
extern int g_iDashSpeedUnit;
extern int g_iDashSOGDamp;
extern int g_iDashDepthUnit;
extern int g_iDashDistanceUnit;  //0="Nautical miles", 1="Statute miles", 2="Kilometers", 3="Meters"
extern int g_iDashWindSpeedUnit; //0="Kts", 1="mph", 2="km/h", 3="m/s"
extern int g_iUTCOffset;

bool g_bTacticsImportChecked;
double g_dalphaDeltCoG;
double g_dalphaLaylinedDampFactor;
double g_dLeewayFactor;
double g_dfixedLeeway;
double g_dalpha_currdir;
int g_iMinLaylineWidth;
int g_iMaxLaylineWidth;
double g_dLaylineLengthonChart;
Polar* BoatPolar;
bool g_bDisplayCurrentOnChart;
wxString g_path_to_PolarFile;
PlugIn_Route *m_pRoute = NULL;
PlugIn_Waypoint *m_pMark = NULL;
double g_dmark_lat = NAN;
double g_dmark_lon = NAN;
double g_dcur_lat = NAN;
double g_dcur_lon = NAN;
double g_dheel[6][5];
bool g_bUseHeelSensor;
bool g_bUseFixedLeeway;
bool g_bManHeelInput;
bool g_bCorrectSTWwithLeeway;  //if true STW is corrected with Leeway (in case Leeway is available)
bool g_bCorrectAWwithHeel;    //if true, AWS/AWA will be corrected with Heel-Angle
bool g_bForceTrueWindCalculation;    //if true, NMEA Data for TWS,TWA,TWD is not used, but the plugin calculated data is used
bool g_bUseSOGforTWCalc; //if true, use SOG instead of STW to calculate TWS,TWA,TWD
bool g_bShowWindbarbOnChart;
bool g_bShowPolarOnChart;
bool g_bPersistentChartPolarAnimation; // If true, continue timer based functions to animate performance on the chart
bool g_bExpPerfData01;
bool g_bExpPerfData02;
bool g_bExpPerfData03;
bool g_bExpPerfData04;
bool g_bExpPerfData05;
bool g_bNKE_TrueWindTableBug;//variable for NKE TrueWindTable-Bugfix
wxString g_sCMGSynonym, g_sVMGSynonym;
wxString tactics_pi::get_sCMGSynonym(void) {return g_sCMGSynonym;};
wxString tactics_pi::get_sVMGSynonym(void) {return g_sVMGSynonym;};


//---------------------------------------------------------------------------------------------------------
//
//          Tactics Performance instruments and functions for Dashboard plug-in
//
//---------------------------------------------------------------------------------------------------------

tactics_pi::tactics_pi( void )
{
    m_hostplugin = NULL;
    m_hostplugin_pconfig = NULL;
    m_bNKE_TrueWindTableBug = false;
    m_VWR_AWA = 10;
	alpha_currspd = 0.2;  //smoothing constant for current speed
	alpha_CogHdt = 0.1; // smoothing constant for diff. btw. Cog & Hdt
	m_ExpSmoothCurrSpd = NAN;
	m_ExpSmoothCurrDir = NAN;
	m_ExpSmoothSog = NAN;
	m_ExpSmoothSinCurrDir = NAN;
	m_ExpSmoothCosCurrDir = NAN;
	m_ExpSmoothSinCog = NAN;
	m_ExpSmoothCosCog = NAN;
	m_CurrentDirection = NAN;
	m_LaylineSmoothedCog = NAN;
	m_LaylineDegRange = 0;
	mSinCurrDir = new DoubleExpSmooth(g_dalpha_currdir);
	mCosCurrDir = new DoubleExpSmooth(g_dalpha_currdir);
	mExpSmoothCurrSpd = new ExpSmooth(alpha_currspd);
	mExpSmoothSog = new DoubleExpSmooth(0.4);
    mExpSmSinCog = new DoubleExpSmooth(
        g_dalphaLaylinedDampFactor);//prev. ExpSmooth(...
    mExpSmCosCog = new DoubleExpSmooth(
        g_dalphaLaylinedDampFactor);//prev. ExpSmooth(...
    m_ExpSmoothDegRange = 0;
	mExpSmDegRange = new ExpSmooth(g_dalphaDeltCoG);
	mExpSmDegRange->SetInitVal(g_iMinLaylineWidth);
	mExpSmDiffCogHdt = new ExpSmooth(alpha_CogHdt);
	mExpSmDiffCogHdt->SetInitVal(0);
	m_bShowPolarOnChart = false;
	m_bShowWindbarbOnChart = false;
	m_bDisplayCurrentOnChart = false;
	m_LeewayOK = false;
	mHdt = NAN;
	mStW = NAN;
	mTWA = NAN;
	mTWD = NAN;
	mTWS = NAN;
	m_calcTWA = NAN;
	m_calcTWD = NAN;
	m_calcTWS = NAN;
	mSOG = NAN;
	mCOG = NAN;
	mlat = NAN;
	mlon = NAN;
	mheel = NAN;
	mLeeway = NAN;
	mPolarTargetSpeed = NAN;
	mBRG = NAN;
	mVMGGain = mCMGGain = mVMGoptAngle = mCMGoptAngle = 0.0;
	mPredictedCoG = NAN;
	for (int i = 0; i < COGRANGE; i++) m_COGRange[i] = NAN;

	m_bTrueWind_available = false;

     b_tactics_dc_message_shown = false;

}

tactics_pi::~tactics_pi( void )
{
    return;
}

int tactics_pi::Init( opencpn_plugin *hostplugin, wxFileConfig *pConf )
{
    m_hostplugin = hostplugin;
    m_hostplugin_pconfig = pConf;
    m_hostplugin_config_path = pConf->GetPath();
    mBRG_Watchdog = 2;
    mTWS_Watchdog = 5;
    mTWD_Watchdog = 5;
    mAWS_Watchdog = 2;

    this->LoadConfig();
    this->ApplyConfig();

	// Context menue for making marks    
	m_pmenu = new wxMenu();
	// this is a dummy menu required by Windows as parent to item created
	wxMenuItem *pmi = new wxMenuItem(m_pmenu, -1, _T("Set Tactics Mark "));
	int miid = AddCanvasContextMenuItem(pmi, m_hostplugin);
	SetCanvasContextMenuItemViz(miid, true);

	return (WANTS_CURSOR_LATLON |
		WANTS_TOOLBAR_CALLBACK |
		INSTALLS_TOOLBAR_TOOL |
		WANTS_PREFERENCES |
		WANTS_CONFIG |
		WANTS_NMEA_SENTENCES |
		WANTS_NMEA_EVENTS |
		USES_AUI_MANAGER |
		WANTS_PLUGIN_MESSAGING |
		WANTS_OPENGL_OVERLAY_CALLBACK |
		WANTS_OVERLAY_CALLBACK
		);
}

bool tactics_pi::DeInit()
{
	this->SaveConfig();

	return true;
}
void tactics_pi::Notify()
{
    mBRG_Watchdog--;
    if (mBRG_Watchdog <= 0) {
      SendSentenceToAllInstruments(OCPN_DBP_STC_BRG, NAN, _T("\u00B0"));
    }
    mTWS_Watchdog--;
    if (mTWS_Watchdog <= 0) {
      mTWS = NAN;
      SendSentenceToAllInstruments(OCPN_DBP_STC_TWS, NAN, _T(""));
    }
    mTWD_Watchdog--;
    if (mTWD_Watchdog <= 0) {
      mTWD = NAN;
      mTWA = NAN;
      SendSentenceToAllInstruments(OCPN_DBP_STC_TWD, NAN, _T("\u00B0"));
      SendSentenceToAllInstruments(OCPN_DBP_STC_TWA, NAN, _T("\u00B0"));
    }
    mAWS_Watchdog--;
    if (mAWS_Watchdog <= 0) {
      SendSentenceToAllInstruments(OCPN_DBP_STC_AWS, NAN, _T(""));
    }

    this->ExportPerformanceData();

}

bool tactics_pi::LoadConfig()
{
    wxFileConfig *pConf = (wxFileConfig *) m_hostplugin_pconfig;
	if (!pConf)
        return false;

    this->m_this_config_path =
        this->m_hostplugin_config_path + _T("/Tactics");
    pConf->SetPath( this->m_this_config_path );
    if (!this->LoadConfig_CheckTacticsPlugin( pConf )) {
        /* If not imported from the tactics_pi
         standalone Tactics Plugin, we must have our own
         settings in ini-file by now: */
        pConf->SetPath( this->m_this_config_path );
        bool bUserDecision = g_bTacticsImportChecked;
        this->LoadTacticsPluginBasePart( pConf );
        pConf->SetPath( this->m_this_config_path + _T("/Performance"));
        this->LoadTacticsPluginPerformancePart( pConf );
        g_bTacticsImportChecked = bUserDecision;
    } // then load from this plugin's sub-group
    else {
        bool bUserDecision = g_bTacticsImportChecked;
        this->ImportStandaloneTacticsSettings ( pConf );
        g_bTacticsImportChecked = bUserDecision;
    } // else load from the tactics_pi plugin's group
    BoatPolar = NULL;
    /* Unlike the tactics_pi plugin, Dashboard  not absolutely require
       to have polar-file - it may be that the user is not
       interested in performance part. Yet. We can ask that later. */
    if (g_path_to_PolarFile != _T("NULL")) {
        BoatPolar = new Polar(this);
        BoatPolar->loadPolar(g_path_to_PolarFile);
    }
    
    return true;
}
/*
 Check and swap to original TacticsPlugin group if the user wish to import from there, return false if no import
*/
bool tactics_pi::LoadConfig_CheckTacticsPlugin( wxFileConfig *pConf )
{
    bool bCheckIt = false;
    pConf->SetPath( this->m_this_config_path );
   if (!pConf->Exists(_T("TacticsImportChecked"))) {
        g_bTacticsImportChecked = false;
        bCheckIt = true;
    } // then this must be the first run...
    else {
        pConf->Read(_T("TacticsImportChecked"), &g_bTacticsImportChecked, false);
        if (!g_bTacticsImportChecked)
            bCheckIt = true;
    } /* else check is done only once, normally, unless
         TacticsImportChecked is altered manually to false */
    if (!bCheckIt)
        return false;
    if (!this->StandaloneTacticsSettingsExists ( pConf )) 
        return false;
    wxString message(
        _("Import existing Tactics plugin settings into Dashboard's integrated Tactics settings? (Cancel=later)"));
    wxMessageDialog *dlg = new wxMessageDialog(
        GetOCPNCanvasWindow(), message, _T("Dashboard configuration choice"), wxYES_NO|wxCANCEL);
    int choice = dlg->ShowModal();
    if ( choice == wxID_YES ) {
        
        g_bTacticsImportChecked = true;
        return true;
    } // then import
    else {
        if (choice == wxID_NO ) { 
            g_bTacticsImportChecked = true;
            return false;
        } // then do not import, attempt to import from local
        else {
            g_bTacticsImportChecked = false;
            return false;
        } // else not sure (cancel): not now, but will ask again
    } // else no or cancel
}
void tactics_pi::ImportStandaloneTacticsSettings ( wxFileConfig *pConf )
{
    // Here we suppose that both tactics_pi and this Dashboard implementation are identical
    pConf->SetPath( "/PlugIns/Tactics" );
    this->LoadTacticsPluginBasePart( pConf );
    pConf->SetPath( "/PlugIns/Tactics/Performance" );
    this->LoadTacticsPluginPerformancePart( pConf );
}

bool tactics_pi::StandaloneTacticsSettingsExists ( wxFileConfig *pConf )
{
    pConf->SetPath( "/PlugIns/Tactics/Performance" );
    if (pConf->Exists(_T("PolarFile")))
        return true;
    return false;
}

/*
  Load tactics_pi settings from the Tactics base group,
  underneath the group given in pConf object (you must set it).
*/
void tactics_pi::LoadTacticsPluginBasePart ( wxFileConfig *pConf )
{
    pConf->Read(_T("CurrentDampingFactor"), &g_dalpha_currdir, 0.008);
    pConf->Read(_T("LaylineDampingFactor"), &g_dalphaLaylinedDampFactor, 0.15);
    pConf->Read(_T("LaylineLenghtonChart"), &g_dLaylineLengthonChart, 10.0);
    pConf->Read(_T("MinLaylineWidth"), &g_iMinLaylineWidth, 4);
    pConf->Read(_T("MaxLaylineWidth"), &g_iMaxLaylineWidth, 30);
    pConf->Read(_T("LaylineWidthDampingFactor"), &g_dalphaDeltCoG, 0.25);
    pConf->Read(_T("ShowCurrentOnChart"), &g_bDisplayCurrentOnChart, false);
    m_bDisplayCurrentOnChart = g_bDisplayCurrentOnChart;
    pConf->Read(_T("CMGSynonym"), &g_sCMGSynonym, _T("CMG"));
    pConf->Read(_T("VMGSynonym"), &g_sVMGSynonym, _T("VMG"));
    pConf->Read(_T("TacticsImportChecked"), &g_bTacticsImportChecked, false);
}
/*
  Import tactics_pi settings from the Tactics Performance Group,
  underneath the group given in pConf object (you must set it)
*/
void tactics_pi::LoadTacticsPluginPerformancePart ( wxFileConfig *pConf )
{
    pConf->Read(_T("PolarFile"), &g_path_to_PolarFile, _T("NULL"));
    pConf->Read(_T("BoatLeewayFactor"), &g_dLeewayFactor, 10);
    pConf->Read(_T("fixedLeeway"), &g_dfixedLeeway, 30);
    pConf->Read(_T("UseHeelSensor"), &g_bUseHeelSensor, true);
    pConf->Read(_T("UseFixedLeeway"), &g_bUseFixedLeeway, false);
    pConf->Read(_T("UseManHeelInput"), &g_bManHeelInput, false);
    pConf->Read(_T("Heel_5kn_45Degree"), &g_dheel[1][1], 5);
    pConf->Read(_T("Heel_5kn_90Degree"), &g_dheel[1][2], 8);
    pConf->Read(_T("Heel_5kn_135Degree"), &g_dheel[1][3], 5);
    pConf->Read(_T("Heel_10kn_45Degree"), &g_dheel[2][1], 8);
    pConf->Read(_T("Heel_10kn_90Degree"), &g_dheel[2][2], 10);
    pConf->Read(_T("Heel_10kn_135Degree"), &g_dheel[2][3], 11);
    pConf->Read(_T("Heel_15kn_45Degree"), &g_dheel[3][1], 25);
    pConf->Read(_T("Heel_15kn_90Degree"), &g_dheel[3][2], 20);
    pConf->Read(_T("Heel_15kn_135Degree"), &g_dheel[3][3], 13);
    pConf->Read(_T("Heel_20kn_45Degree"), &g_dheel[4][1], 20);
    pConf->Read(_T("Heel_20kn_90Degree"), &g_dheel[4][2], 16);
    pConf->Read(_T("Heel_20kn_135Degree"), &g_dheel[4][3], 15);
    pConf->Read(_T("Heel_25kn_45Degree"), &g_dheel[5][1], 25);
    pConf->Read(_T("Heel_25kn_90Degree"), &g_dheel[5][2], 20);
    pConf->Read(_T("Heel_25kn_135Degree"), &g_dheel[5][3], 20);
    pConf->Read(_T("UseManHeelInput"), &g_bManHeelInput, false);
    pConf->Read(_T("CorrectSTWwithLeeway"), &g_bCorrectSTWwithLeeway, false);  //if true, STW is corrected with Leeway (in case Leeway is available)
    pConf->Read(_T("CorrectAWwithHeel"), &g_bCorrectAWwithHeel, false);    //if true, AWS/AWA are corrected with Heel-Angle
    pConf->Read(_T("ForceTrueWindCalculation"), &g_bForceTrueWindCalculation, false);    //if true, NMEA Data for TWS,TWA,TWD is not used, but the plugin calculated data is used
    pConf->Read(_T("ShowWindbarbOnChart"), &g_bShowWindbarbOnChart, false);
    m_bShowWindbarbOnChart = g_bShowWindbarbOnChart;
    pConf->Read(_T("ShowPolarOnChart"), &g_bShowPolarOnChart, false);
    m_bShowPolarOnChart = g_bShowPolarOnChart;
    pConf->Read(_T("UseSOGforTWCalc"), &g_bUseSOGforTWCalc, false);
    pConf->Read(_T("ExpPolarSpeed"), &g_bExpPerfData01, false);
    pConf->Read(_T("ExpCourseOtherTack"), &g_bExpPerfData02, false);
    pConf->Read(_T("ExpTargetVMG"), &g_bExpPerfData03, false);
    pConf->Read(_T("ExpVMG_CMG_Diff_Gain"), &g_bExpPerfData04, false);
    pConf->Read(_T("ExpCurrent"), &g_bExpPerfData05, false);
    pConf->Read(_T("NKE_TrueWindTableBug"), &g_bNKE_TrueWindTableBug, false);
    m_bNKE_TrueWindTableBug = g_bNKE_TrueWindTableBug;
}
void tactics_pi::ApplyConfig(void)
{
    return;
}

bool tactics_pi::SaveConfig()
{
    wxFileConfig *pConf = (wxFileConfig *) m_hostplugin_pconfig;
    if (!pConf)
        return false;
    pConf->SetPath( this->m_this_config_path );
    SaveTacticsPluginBasePart ( pConf );
    pConf->SetPath( this->m_this_config_path + _T("/Performance"));
    SaveTacticsPluginPerformancePart ( pConf );
    return true;
}
/*
  Save tactics_pi settings into the Tactics base group,
  underneath the group given in pConf object (you have to set it).
*/
void tactics_pi::SaveTacticsPluginBasePart ( wxFileConfig *pConf )
{
    pConf->Write(_T("CurrentDampingFactor"), g_dalpha_currdir);
    pConf->Write(_T("LaylineDampingFactor"), g_dalphaLaylinedDampFactor);
    pConf->Write(_T("LaylineLenghtonChart"), g_dLaylineLengthonChart);
    pConf->Write(_T("MinLaylineWidth"), g_iMinLaylineWidth);
    pConf->Write(_T("MaxLaylineWidth"), g_iMaxLaylineWidth);
    pConf->Write(_T("LaylineWidthDampingFactor"), g_dalphaDeltCoG);
    pConf->Write(_T("ShowCurrentOnChart"), g_bDisplayCurrentOnChart);
    pConf->Write(_T("CMGSynonym"), g_sCMGSynonym);
    pConf->Write(_T("VMGSynonym"), g_sVMGSynonym);
    pConf->Write(_T("TacticsImportChecked"), g_bTacticsImportChecked);
}
/*
  Save tactics_pi settings into the Tactics Performance Group,
  underneath the group given in pConf object (you have to set it).
*/
void tactics_pi::SaveTacticsPluginPerformancePart ( wxFileConfig *pConf )
{
    pConf->Write(_T("PolarFile"), g_path_to_PolarFile);
    pConf->Write(_T("BoatLeewayFactor"), g_dLeewayFactor);
    pConf->Write(_T("fixedLeeway"), g_dfixedLeeway);
    pConf->Write(_T("UseHeelSensor"), g_bUseHeelSensor);
    pConf->Write(_T("UseFixedLeeway"), g_bUseFixedLeeway);
    pConf->Write(_T("UseManHeelInput"), g_bManHeelInput);
    pConf->Write(_T("CorrectSTWwithLeeway"), g_bCorrectSTWwithLeeway);
    pConf->Write(_T("CorrectAWwithHeel"), g_bCorrectAWwithHeel);
    pConf->Write(_T("ForceTrueWindCalculation"), g_bForceTrueWindCalculation);
    pConf->Write(_T("UseSOGforTWCalc"), g_bUseSOGforTWCalc);
    pConf->Write(_T("ShowWindbarbOnChart"), g_bShowWindbarbOnChart);
    pConf->Write(_T("ShowPolarOnChart"), g_bShowPolarOnChart);
    pConf->Write(_T("Heel_5kn_45Degree"), g_dheel[1][1]);
    pConf->Write(_T("Heel_5kn_90Degree"), g_dheel[1][2]);
    pConf->Write(_T("Heel_5kn_135Degree"), g_dheel[1][3]);
    pConf->Write(_T("Heel_10kn_45Degree"), g_dheel[2][1]);
    pConf->Write(_T("Heel_10kn_90Degree"), g_dheel[2][2]);
    pConf->Write(_T("Heel_10kn_135Degree"), g_dheel[2][3]);
    pConf->Write(_T("Heel_15kn_45Degree"), g_dheel[3][1]);
    pConf->Write(_T("Heel_15kn_90Degree"), g_dheel[3][2]);
    pConf->Write(_T("Heel_15kn_135Degree"), g_dheel[3][3]);
    pConf->Write(_T("Heel_20kn_45Degree"), g_dheel[4][1]);
    pConf->Write(_T("Heel_20kn_90Degree"), g_dheel[4][2]);
    pConf->Write(_T("Heel_20kn_135Degree"), g_dheel[4][3]);
    pConf->Write(_T("Heel_25kn_45Degree"), g_dheel[5][1]);
    pConf->Write(_T("Heel_25kn_90Degree"), g_dheel[5][2]);
    pConf->Write(_T("Heel_25kn_135Degree"), g_dheel[5][3]);
    pConf->Write(_T("ExpPolarSpeed"), g_bExpPerfData01);
    pConf->Write(_T("ExpCourseOtherTack"), g_bExpPerfData02);
    pConf->Write(_T("ExpTargetVMG"), g_bExpPerfData03);
    pConf->Write(_T("ExpVMG_CMG_Diff_Gain"), g_bExpPerfData04);
    pConf->Write(_T("ExpCurrent"), g_bExpPerfData05);
    pConf->Write(_T("NKE_TrueWindTableBug"), g_bNKE_TrueWindTableBug);
}


/*********************************************************************
Taken from cutil
*********************************************************************/
inline int myCCW(wxRealPoint p0, wxRealPoint p1, wxRealPoint p2) {
	double dx1, dx2;
	double dy1, dy2;

	dx1 = p1.x - p0.x; dx2 = p2.x - p0.x;
	dy1 = p1.y - p0.y; dy2 = p2.y - p0.y;

	/* This is basically a slope comparison: we don't do divisions because

	* of divide by zero possibilities with pure horizontal and pure
	* vertical lines.
	*/
	return ((dx1 * dy2 > dy1 * dx2) ? 1 : -1);

}
/*********************************************************************
returns true if we have a line intersection.
Taken from cutil, but with double variables
**********************************************************************/
inline bool IsLineIntersect(wxRealPoint p1, wxRealPoint p2, wxRealPoint p3, wxRealPoint p4)
{
	return (((myCCW(p1, p2, p3) * myCCW(p1, p2, p4)) <= 0)
		&& ((myCCW(p3, p4, p1) * myCCW(p3, p4, p2) <= 0)));

}
/*********************************************************************
calculate Line intersection between 2 lines, each described by 2 points
return lat/lon of intersection point
basic calculation:
int p1[] = { -4,  5, 1 };
int p2[] = { -2, -5, 1 };
int p3[] = { -6,  2, 1 };
int p4[] = {  5,  4, 1 };
int l1[3], l2[3], s[3];
double sch[2];
l1[0] = p1[1] * p2[2] - p1[2] * p2[1];
l1[1] = p1[2] * p2[0] - p1[0] * p2[2];
l1[2] = p1[0] * p2[1] - p1[1] * p2[0];
l2[0] = p3[1] * p4[2] - p3[2] * p4[1];
l2[1] = p3[2] * p4[0] - p3[0] * p4[2];
l2[2] = p3[0] * p4[1] - p3[1] * p4[0];
s[0] = l1[1] * l2[2] - l1[2] * l2[1];
s[1] = l1[2] * l2[0] - l1[0] * l2[2];
s[2] = l1[0] * l2[1] - l1[1] * l2[0];
sch[0] = (double)s[0] / (double)s[2];
sch[1] = (double)s[1] / (double)s[2];
**********************************************************************/
wxRealPoint GetLineIntersection(wxRealPoint line1point1, wxRealPoint line1point2, wxRealPoint line2point1, wxRealPoint line2point2)
{
	wxRealPoint intersect;
	intersect.x = -999.;
	intersect.y = -999.;
	if (IsLineIntersect(line1point1, line1point2, line2point1, line2point2)){
		double line1[3], line2[3], s[3];
		line1[0] = line1point1.y * 1. - 1. * line1point2.y;
		line1[1] = 1. * line1point2.x - line1point1.x * 1.;
		line1[2] = line1point1.x * line1point2.y - line1point1.y * line1point2.x;
		line2[0] = line2point1.y * 1. - 1. * line2point2.y;
		line2[1] = 1. * line2point2.x - line2point1.x * 1.;
		line2[2] = line2point1.x * line2point2.y - line2point1.y * line2point2.x;
		s[0] = line1[1] * line2[2] - line1[2] * line2[1];
		s[1] = line1[2] * line2[0] - line1[0] * line2[2];
		s[2] = line1[0] * line2[1] - line1[1] * line2[0];
		intersect.x = s[0] / s[2];
		intersect.y = s[1] / s[2];
	}
	return intersect;
}
/**********************************************************************
Function calculates the time to sail for a given distance, TWA and TWS,
based on the polar data
***********************************************************************/
double CalcPolarTimeToMark(double distance, double twa, double tws)
{
	double pspd = BoatPolar->GetPolarSpeed(twa, tws);
	return distance / pspd;
}
/**********************************************************************
Function returns the (smaller) TWA of a given TWD and Course.
Used for Target-CMG calculation.
It covers the 359 - 0 degree problem
e.g. : TWD = 350, ctm = 10; the TWA is returned as 20 degrees
(and not 340 if we'd do a simple TWD - ctm)
***********************************************************************/
double getMarkTWA(double twd, double ctm)
{
	double val, twa;
	if (twd > 180)
	{
		val = twd - 180;
		if (ctm < val)
			twa = 360 - twd + ctm;
		else
			twa = twd > ctm ? twd - ctm : ctm - twd;
	}
	else
	{
		val = twd + 180;
		if (ctm > val)
			twa = 360 - ctm + twd;
		else
			twa = twd > ctm ? twd - ctm : ctm - twd;
	}
	return twa;
}
/**********************************************************************
Function returns the (smaller) degree range of 2 angular values
on the compass rose (without sign)
It covers the 359 - 0 degree problem
e.g. : max = 350, min = 10; the rage is returned as 20 degrees
(and not 340 if we'd do a simple max - min)
**********************************************************************/
double getDegRange(double max, double min)
{
	double val, range;
	if (max > 180)
	{
		val = max - 180;
		if (min < val)
			range = 360 - max + min;
		else
			range = max > min ? max - min : min - max;
	}
	else
	{
		val = max + 180;
		if (min > val)
			range = 360 - min + max;
		else
			range = max > min ? max - min : min - max;
	}
	return range;
}
/**********************************************************************
Function returns the (smaller) signed degree range of 2 angular values
on the compass rose (clockwise is +)
It covers the 359 - 0 degree problem
e.g. : fromAngle = 350, toAngle = 10; the range is returned as +20 degrees
(and not 340 if we'd do a simple fromAngle - toAngle)
***********************************************************************/
double getSignedDegRange(double fromAngle, double toAngle)
{
	double val, range;
	if (fromAngle > 180)
	{
		val = fromAngle - 180;
		if (toAngle < val)
			range = 360 - fromAngle + toAngle;
		else
			range = toAngle - fromAngle;
	}
	else
	{
		val = fromAngle + 180;
		if (toAngle > val)
			range = -(360 - toAngle + fromAngle);
		else
			range = toAngle - fromAngle;
	}
	return range;
}


/**********************************************************************
Draw the OpenGL overlay
Called by Plugin Manager on main system process cycle
**********************************************************************/
bool tactics_pi::RenderGLOverlay(wxGLContext *pcontext, PlugIn_ViewPort *vp)
{
	b_tactics_dc_message_shown = false; // show message box if RenderOverlay() is called again
	if (m_bLaylinesIsVisible || m_bDisplayCurrentOnChart || m_bShowWindbarbOnChart || m_bShowPolarOnChart){
		glPushAttrib(GL_COLOR_BUFFER_BIT | GL_LINE_BIT | GL_ENABLE_BIT | GL_POLYGON_BIT | GL_HINT_BIT);
		glEnable(GL_LINE_SMOOTH);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
		glPushMatrix();
		this->DoRenderLaylineGLOverlay(pcontext, vp);
		this->DoRenderCurrentGLOverlay(pcontext, vp);
		glPopMatrix();
		glPopAttrib();
	}
	return true;
}

bool tactics_pi::RenderOverlay(wxDC &dc, PlugIn_ViewPort *vp)
{
	if (b_tactics_dc_message_shown == false) {
        b_tactics_dc_message_shown = true;
        
		wxString message(_("You have to turn on OpenGL to use chart overlay "));
		wxMessageDialog dlg(GetOCPNCanvasWindow(), message, _T("dashboard_tactics_pi message"), wxOK);
		dlg.ShowModal();
	}
	return false;
}

/********************************************************************
Draw the OpenGL Layline overlay
*********************************************************************/
void tactics_pi::DoRenderCurrentGLOverlay(wxGLContext *pcontext, PlugIn_ViewPort *vp)
{
	if (m_bDisplayCurrentOnChart && !std::isnan(mlat) && !std::isnan(mlon) && !std::isnan(m_CurrentDirection)) {
		//draw the current on the chart here
		/*
         *           0
         *          /\
         *         /  \
         *        /    \
         *     6 /_  X _\ 1
         *        5|  |2
         *         |__|
         *        4    3
         */
		wxPoint boat;
		GetCanvasPixLL(vp, &boat, mlat, mlon);

		double m_radius = 100;
		wxRealPoint currpoints[7];
		double currval = m_CurrentDirection;
		double rotate = vp->rotation;
		double currvalue = ((currval - 90.) * M_PI / 180) + rotate;

		currpoints[0].x = boat.x + (m_radius * .40 * cos(currvalue));
		currpoints[0].y = boat.y + (m_radius * .40 * sin(currvalue));
		currpoints[1].x = boat.x + (m_radius * .18 * cos(currvalue + 1.5));
		currpoints[1].y = boat.y + (m_radius * .18 * sin(currvalue + 1.5));
		currpoints[2].x = boat.x + (m_radius * .10 * cos(currvalue + 1.5));
		currpoints[2].y = boat.y + (m_radius * .10 * sin(currvalue + 1.5));

		currpoints[3].x = boat.x + (m_radius * .3 * cos(currvalue + 2.8));
		currpoints[3].y = boat.y + (m_radius * .3 * sin(currvalue + 2.8));
		currpoints[4].x = boat.x + (m_radius * .3 * cos(currvalue - 2.8));
		currpoints[4].y = boat.y + (m_radius * .3 * sin(currvalue - 2.8));

		currpoints[5].x = boat.x + (m_radius * .10 * cos(currvalue - 1.5));
		currpoints[5].y = boat.y + (m_radius * .10 * sin(currvalue - 1.5));
		currpoints[6].x = boat.x + (m_radius * .18 * cos(currvalue - 1.5));
		currpoints[6].y = boat.y + (m_radius * .18 * sin(currvalue - 1.5));
        //below 0.2 knots the current generally gets inaccurate, as the error level gets too high.
        // as a hint, fade the current transparency below 0.25 knots...
        //        int curr_trans = 164;
        //        if (m_ExpSmoothCurrSpd <= 0.20) {
        //          fading : 0.2 * 5 = 1 --> we set full transp. >=0.2
        //          curr_trans = m_ExpSmoothCurrSpd * 5 * curr_trans;
        //          if (curr_trans < 50) curr_trans = 50;  //lower limit
        //        }
        //        GLubyte red(7), green(107), blue(183), alpha(curr_trans);
        //        glColor4ub(7, 107, 183, curr_trans);                 	// red, green, blue,  alpha

		GLubyte red(7), green(107), blue(183), alpha(164);
		glColor4ub(7, 107, 183, 164); // red, green, blue,  alpha
        //        glLineWidth(2);
        //		glBegin(GL_POLYGON | GL_LINES);
        glBegin(GL_POLYGON);
        glVertex2d(currpoints[0].x, currpoints[0].y);
		glVertex2d(currpoints[1].x, currpoints[1].y);
		glVertex2d(currpoints[2].x, currpoints[2].y);
		glVertex2d(currpoints[3].x, currpoints[3].y);
		glVertex2d(currpoints[4].x, currpoints[4].y);
		glVertex2d(currpoints[5].x, currpoints[5].y);
		glVertex2d(currpoints[6].x, currpoints[6].y);
		glEnd();
	}
}

/*********************************************************************
Draw the OpenGL Layline overlay
**********************************************************************/
void tactics_pi::DoRenderLaylineGLOverlay(wxGLContext *pcontext, PlugIn_ViewPort *vp)
{
	wxPoint  mark_center;
	wxPoint boat;
	if (!std::isnan(mlat) && !std::isnan(mlon)) {
		GetCanvasPixLL(vp, &vpoints[0], mlat, mlon);
		boat = vpoints[0];
		/*********************************************************************
		Draw wind barb on boat position
***********************************************************************/
        //mTWD=NAN caught in subroutines
		DrawWindBarb(boat, vp);
		DrawPolar(vp, boat, mTWD);
	}
	//wxString GUID = _T("TacticsWP");
	if (!GetSingleWaypoint(_T("TacticsWP"), m_pMark)) m_pMark = NULL;
	if (m_pMark){
		/*********************************************************************
		Draw wind barb on mark position
        **********************************************************************/
		GetCanvasPixLL(vp, &mark_center, m_pMark->m_lat, m_pMark->m_lon);
		DrawWindBarb(mark_center, vp);
		/*********************************************************************
		Draw direct line from boat to mark as soon as mark is dropped. Reduces problems to find it ...
**********************************************************************/
		glColor4ub(255, 128, 0, 168); //orange
		glLineWidth(2);
		glBegin(GL_LINES);
		glVertex2d(boat.x, boat.y);
		glVertex2d(mark_center.x, mark_center.y);
		glEnd();

	}

	if (m_bLaylinesIsVisible){
        if (!std::isnan(mlat) && !std::isnan(mlon) && !std::isnan(mCOG) && !std::isnan(mHdt) && !std::isnan(mStW) && !std::isnan(mTWS) && !std::isnan(mTWA)) {
			if (std::isnan(m_LaylineSmoothedCog)) m_LaylineSmoothedCog = mCOG;
            if (std::isnan(mLeeway)) mLeeway = 0.0;
			/**********************************************************************
			Draw the boat laylines, independent from the "Temp. Tactics WP"
			The first (foreward) layline is on the COG pointer
***********************************************************************/
			wxString curTack = mAWAUnit;
			wxString targetTack = _T("");
			//it shows '�L'= wind from left = port tack or '�R'=wind from right = starboard tack
			//we're on port tack, so vertical layline is red
			if (curTack == _T("\u00B0L")) {
				GLubyte red(204), green(41), blue(41), alpha(128);
				glColor4ub(204, 41, 41, 128);                 	// red, green, blue,  alpha
				targetTack = _T("R");
			}
			else if (curTack == _T("\u00B0R"))  {// we're on starboard tack, so vertical layline is green
				GLubyte red(0), green(200), blue(0), alpha(128);
				glColor4ub(0, 200, 0, 128);                 	// red, green, blue,  alpha
				targetTack = _T("L");
			}
			double tmplat1, tmplon1, tmplat2, tmplon2;
			PositionBearingDistanceMercator_Plugin(mlat, mlon, m_LaylineSmoothedCog - m_ExpSmoothDegRange / 2., g_dLaylineLengthonChart, &tmplat1, &tmplon1);
			GetCanvasPixLL(vp, &vpoints[1], tmplat1, tmplon1);
			PositionBearingDistanceMercator_Plugin(mlat, mlon, m_LaylineSmoothedCog + m_ExpSmoothDegRange / 2., g_dLaylineLengthonChart, &tmplat2, &tmplon2);
			GetCanvasPixLL(vp, &vpoints[2], tmplat2, tmplon2);
			glBegin(GL_TRIANGLES);
			glVertex2d(vpoints[0].x, vpoints[0].y);
			glVertex2d(vpoints[1].x, vpoints[1].y);
			glVertex2d(vpoints[2].x, vpoints[2].y);
			glEnd();
			/*********************************************************************
			Calculate and draw  the second boat layline (for other tack)
			---------------------------------------------------------
			Approach : we're drawing the first layline on COG, but TWA is based on boat heading (Hdt).
			To calculate the layline of the other tack, sum up
			diff_btw_Cog_and_Hdt (now we're on Hdt)
			+ 2 x TWA
			+ Leeway
			---------
			= predictedHdt
			+ current_angle
			======================
			= newCog (on other tack)
			Calculation of (sea) current angle :
			1. from actual pos. calculate the endpoint of predictedHdt (out: predictedLatHdt, predictedLonHdt),
			assuming same StW on other tack
			2. at that point apply current : startpoint predictedLatHdt, predictedLonHdt + bearing + speed; out : predictedLatCog, predictedLonCog
			3. calculate angle (and speed) from curr pos to predictedLatCog, predictedLonCog; out : newCog + newSOG
************************************************************************/
			double  diffCogHdt;
			double tws_kts = fromUsrSpeed_Plugin(mTWS, g_iDashWindSpeedUnit);
			double stw_kts = fromUsrSpeed_Plugin(mStW, g_iDashSpeedUnit);
			double currspd_kts = std::isnan(m_ExpSmoothCurrSpd) ? 0.0 : fromUsrSpeed_Plugin(m_ExpSmoothCurrSpd, g_iDashSpeedUnit);
			double currdir = std::isnan(m_CurrentDirection) ? 0.0 : m_CurrentDirection;
			diffCogHdt = getDegRange(mCOG, mHdt);
			mExpSmDiffCogHdt->SetAlpha(alpha_CogHdt);
			m_ExpSmoothDiffCogHdt = mExpSmDiffCogHdt->GetSmoothVal((diffCogHdt < 0 ? -diffCogHdt : diffCogHdt));
			if (targetTack == _T("R")){ // currently wind is from port ...now
				mPredictedHdG = m_LaylineSmoothedCog - m_ExpSmoothDiffCogHdt - 2 * mTWA - fabs(mLeeway); //Leeway is signed 
				GLubyte red(0), green(200), blue(0), alpha(128);
				glColor4ub(0, 200, 0, 128);                 	// red, green, blue,  alpha
			}
			else if (targetTack == _T("L")){ //currently wind from starboard
				mPredictedHdG = m_LaylineSmoothedCog + m_ExpSmoothDiffCogHdt + 2 * mTWA + fabs(mLeeway); //Leeway is signed 
				GLubyte red(204), green(41), blue(41), alpha(128);
				glColor4ub(204, 41, 41, 128);                 	// red, green, blue,  alpha
			}
			else {
				mPredictedHdG = (mTWA < 10) ? 180 : 0;
			}
			if (mPredictedHdG < 0) mPredictedHdG += 360;
			if (mPredictedHdG >= 360) mPredictedHdG -= 360;
			double predictedLatHdt, predictedLonHdt, predictedLatCog, predictedLonCog;
			double  predictedSog;
			//apply current on predicted Heading
			PositionBearingDistanceMercator_Plugin(mlat, mlon, mPredictedHdG, stw_kts, &predictedLatHdt, &predictedLonHdt);
			PositionBearingDistanceMercator_Plugin(predictedLatHdt, predictedLonHdt, currdir, currspd_kts, &predictedLatCog, &predictedLonCog);
			DistanceBearingMercator_Plugin(predictedLatCog, predictedLonCog, mlat, mlon, &mPredictedCoG, &predictedSog);

			tackpoints[0] = vpoints[0];
			double tmplat3, tmplon3, tmplat4, tmplon4;
			PositionBearingDistanceMercator_Plugin(mlat, mlon, mPredictedCoG - m_ExpSmoothDegRange / 2., g_dLaylineLengthonChart, &tmplat3, &tmplon3);
			GetCanvasPixLL(vp, &tackpoints[1], tmplat3, tmplon3);
			PositionBearingDistanceMercator_Plugin(mlat, mlon, mPredictedCoG + m_ExpSmoothDegRange / 2., g_dLaylineLengthonChart, &tmplat4, &tmplon4);
			GetCanvasPixLL(vp, &tackpoints[2], tmplat4, tmplon4);
			glBegin(GL_TRIANGLES);
			glVertex2d(tackpoints[0].x, tackpoints[0].y);
			glVertex2d(tackpoints[1].x, tackpoints[1].y);
			glVertex2d(tackpoints[2].x, tackpoints[2].y);
			glEnd();

            //            wxLogMessage("mlat=%f, mlon=%f,currspd=%f,predictedCoG=%f, mTWA=%f,mLeeway=%f, g_iDashSpeedUnit=%d", mlat, mlon, currspd_kts, mPredictedCoG, mTWA, mLeeway,g_iDashSpeedUnit);
            //wxLogMessage("tackpoints[0].x=%d, tackpoints[0].y=%d,tackpoints[1].x=%d, tackpoints[1].y=%d,tackpoints[2].x=%d, tackpoints[2].y=%d", tackpoints[0].x, tackpoints[0].y, tackpoints[1].x, tackpoints[1].y, tackpoints[2].x, tackpoints[2].y);
            //wxString GUID = _T("TacticsWP");

			//if (!GetSingleWaypoint(_T("TacticsWP"), m_pMark)) m_pMark = NULL;
			if (m_pMark)
			{
/*********************************************************************
				Draw the laylines btw. mark and boat with max 1 tack
				Additionally calculate if sailing the directline to
                 mark is faster.
				This direct line calculation is based on the theoretical
                  polardata for the TWA when on 'course to mark',
				TWS and 'distance to mark'
				Idea:
				* we draw the VMG laylines on mark and boat position
				* currently (for simplicity) I'm drawing/calculating
                 the laylines up- AND downwind ! Room for improvement ...
				* per layline pair ..
				* check, if they intersect, if yes
				* calculate time to sail
				* calculate time to sail on direct line, based on
                   polar data
				* either draw layline or direct line
***********************************************************************/
				//calculate Course to Mark = CTM
				double CTM, DistToMark, directLineTimeToMark, directLineTWA;
				DistanceBearingMercator_Plugin(m_pMark->m_lat, m_pMark->m_lon, mlat, mlon, &CTM, &DistToMark);
				//calc time-to-mark on direct line, versus opt. TWA and intersection
				directLineTWA = getMarkTWA(mTWD, CTM);
				directLineTimeToMark = CalcPolarTimeToMark(DistToMark, directLineTWA, tws_kts);
				if (std::isnan(directLineTimeToMark)) directLineTimeToMark = 99999;
				//use target VMG calculation for laylines-to-mark
				TargetxMG tvmg = BoatPolar->Calc_TargetVMG(directLineTWA, tws_kts); // directLineTWA <= 90� --> upwind, >90 --> downwind
				//optional : use target CMG calculation for laylines-to-mark
				//TargetxMG tvmg = BoatPolar->Calc_TargetCMG(mTWS,mTWD,CTM); // directLineTWA <= 90� --> upwind, >90 --> downwind
				double sigCTM_TWA = getSignedDegRange(CTM, mTWD);
				double cur_tacklinedir, target_tacklinedir;
				if (!std::isnan(tvmg.TargetAngle))
				{
					if (curTack == _T("\u00B0L")){
						cur_tacklinedir = mTWD - tvmg.TargetAngle - fabs(mLeeway);  //- m_ExpSmoothDiffCogHdt
						target_tacklinedir = mTWD + tvmg.TargetAngle + fabs(mLeeway);//+ m_ExpSmoothDiffCogHdt
					}
					else{
						cur_tacklinedir = mTWD + tvmg.TargetAngle + fabs(mLeeway);//+ m_ExpSmoothDiffCogHdt
						target_tacklinedir = mTWD - tvmg.TargetAngle - fabs(mLeeway);//- m_ExpSmoothDiffCogHdt
					}
					while (cur_tacklinedir < 0) cur_tacklinedir += 360;
					while (cur_tacklinedir > 359) cur_tacklinedir -= 360;
					while (target_tacklinedir < 0) target_tacklinedir += 360;
					while (target_tacklinedir > 359) target_tacklinedir -= 360;
					double lat, lon, curlat, curlon, act_sog;
					wxRealPoint m_end, m_end2, c_end, c_end2;
					//apply current on foreward layline
					PositionBearingDistanceMercator_Plugin(mlat, mlon, cur_tacklinedir, stw_kts, &lat, &lon);
					PositionBearingDistanceMercator_Plugin(lat, lon, currdir, currspd_kts, &curlat, &curlon);
					DistanceBearingMercator_Plugin(curlat, curlon, mlat, mlon, &cur_tacklinedir, &act_sog);
					//cur_tacklinedir=local_bearing(curlat, curlon, mlat, mlon);
					//apply current on mark layline
					PositionBearingDistanceMercator_Plugin(mlat, mlon, target_tacklinedir, stw_kts, &lat, &lon);
					PositionBearingDistanceMercator_Plugin(lat, lon, currdir, currspd_kts, &curlat, &curlon);
					DistanceBearingMercator_Plugin(curlat, curlon, mlat, mlon, &target_tacklinedir, &act_sog);
					//target_tacklinedir=local_bearing(curlat, curlon, mlat, mlon);
					double cur_tacklinedir2 = cur_tacklinedir > 180 ? cur_tacklinedir - 180 : cur_tacklinedir + 180;

					//Mark : get an end of the current dir
					PositionBearingDistanceMercator_Plugin(m_pMark->m_lat, m_pMark->m_lon, cur_tacklinedir, DistToMark * 2, &m_end.y, &m_end.x);
					//Mark : get the second end of the same line on the opposite direction
					PositionBearingDistanceMercator_Plugin(m_pMark->m_lat, m_pMark->m_lon, cur_tacklinedir2, DistToMark * 2, &m_end2.y, &m_end2.x);

					double boat_fwTVMGDir = target_tacklinedir > 180 ? target_tacklinedir - 180 : target_tacklinedir + 180;
					//Boat : get an end of the predicted layline
					PositionBearingDistanceMercator_Plugin(mlat, mlon, boat_fwTVMGDir, DistToMark * 2, &c_end.y, &c_end.x);
					//Boat : get the second end of the same line on the opposite direction
					PositionBearingDistanceMercator_Plugin(mlat, mlon, target_tacklinedir, DistToMark * 2, &c_end2.y, &c_end2.x);

					// see if we have an intersection of the 2 laylines
					wxRealPoint intersection_pos;
					intersection_pos = GetLineIntersection(c_end, c_end2, m_end, m_end2);

					if (intersection_pos.x > 0 && intersection_pos.y > 0) {
						//calc time-to-mark on direct line, versus opt. TWA and intersection
						// now calc time-to-mark via intersection
						double TimeToMarkwithIntersect, tempCTM, DistToMarkwInt, dist1, dist2;
						//distance btw. Boat and intersection
						DistanceBearingMercator_Plugin(mlat, mlon, intersection_pos.y, intersection_pos.x, &tempCTM, &dist1);
						//dist1 = local_distance(mlat, mlon, intersection_pos.y, intersection_pos.x);

						//distance btw. Intersection - Mark
						DistanceBearingMercator_Plugin(intersection_pos.y, intersection_pos.x, m_pMark->m_lat, m_pMark->m_lon, &tempCTM, &dist2);
						//dist2 = local_distance(intersection_pos.y, intersection_pos.x, m_pMark->m_lat, m_pMark->m_lon);

						//Total distance as sum of dist1 + dist2
						//Note : current is NOT yet taken into account here !
						DistToMarkwInt = dist1 + dist2;
						TimeToMarkwithIntersect = CalcPolarTimeToMark(DistToMarkwInt, tvmg.TargetAngle, tws_kts);
						if (std::isnan(TimeToMarkwithIntersect))TimeToMarkwithIntersect = 99999;
						if (TimeToMarkwithIntersect > 0 && directLineTimeToMark > 0){
							//only draw the laylines with intersection, if they are faster than the direct course
							if (TimeToMarkwithIntersect < directLineTimeToMark){
								if (curTack == _T("\u00B0L"))
									glColor4ub(255, 0, 0, 255);
								else
									glColor4ub(0, 200, 0, 255);
								glLineWidth(2);
								wxPoint inter;
								GetCanvasPixLL(vp, &inter, intersection_pos.y, intersection_pos.x);
								glBegin(GL_LINES); // intersect from forward layline --> target VMG --> mark
								glVertex2d(boat.x, boat.y); // from boat with target VMG-Angle sailing forward  to intersection
								glVertex2d(inter.x, inter.y);
								if (curTack == _T("\u00B0L"))
									glColor4ub(0, 200, 0, 255);
								else
									glColor4ub(255, 0, 0, 255);
								glVertex2d(inter.x, inter.y); // from intersection with target VMG-Angle to mark
								glVertex2d(mark_center.x, mark_center.y);
								glEnd();
							}
							else{ // otherwise highlight the direct line
								if (sigCTM_TWA < 0)
									glColor4ub(255, 0, 0, 255);
								else
									glColor4ub(0, 200, 0, 255);
								glLineWidth(2);
								glBegin(GL_LINES);
								glVertex2d(boat.x, boat.y);
								glVertex2d(mark_center.x, mark_center.y);
								glEnd();
							}
						}
					}
					else { //no intersection at all
						if (directLineTimeToMark < 99999.){ //but direct line may be valid
							if (sigCTM_TWA <0)
								glColor4ub(255, 0, 0, 255);
							else
								glColor4ub(0, 200, 0, 255);
							glLineWidth(2);
							glBegin(GL_LINES);
							glVertex2d(boat.x, boat.y);
							glVertex2d(mark_center.x, mark_center.y);
							glEnd();
						}
						else { //no intersection and no valid direct line
							// convert from coordinates to screen values
							wxPoint cogend, mark_end;
							GetCanvasPixLL(vp, &mark_end, m_end.y, m_end.x);
							GetCanvasPixLL(vp, &cogend, c_end.y, c_end.x);
							if (curTack == _T("\u00B0L"))glColor4ub(255, 0, 0, 255);
							else  glColor4ub(0, 200, 0, 255);
							glLineWidth(2);
							glLineStipple(4, 0xAAAA);
							glEnable(GL_LINE_STIPPLE);
							glBegin(GL_LINES); // intersect from forward layline --> target VMG --> mark
							glVertex2d(boat.x, boat.y); // from boat with target VMG-Angle sailing forward  to intersection
							glVertex2d(cogend.x, cogend.y);
							if (curTack == _T("\u00B0L"))glColor4ub(0, 200, 0, 255);
							else  glColor4ub(255, 0, 0, 255);
							glVertex2d(mark_end.x, mark_end.y); // from intersection with target VMG-Angle to mark
							glVertex2d(mark_center.x, mark_center.y);
							glEnd();
							glDisable(GL_LINE_STIPPLE);
						}
					}
				}
				while (target_tacklinedir < 0) target_tacklinedir += 360;
				while (target_tacklinedir > 359) target_tacklinedir -= 360;
				double target_tacklinedir2 = target_tacklinedir > 180 ? target_tacklinedir - 180 : target_tacklinedir + 180;
				wxRealPoint pm_end, pm_end2, pc_end, pc_end2;

				//Mark : get an end of the predicted layline
				PositionBearingDistanceMercator_Plugin(m_pMark->m_lat, m_pMark->m_lon, target_tacklinedir, DistToMark * 2, &pm_end.y, &pm_end.x);
				//Mark : get the second end of the same predicted layline on the opposite direction
				PositionBearingDistanceMercator_Plugin(m_pMark->m_lat, m_pMark->m_lon, target_tacklinedir2, DistToMark * 2, &pm_end2.y, &pm_end2.x);
				//PositionBearingDistanceMercator_Plugin(mlat, mlon, predictedCoG, g_dLaylineLengthonChart, &tmplat1, &tmplon1);
				double boat_tckTVMGDir = cur_tacklinedir > 180 ? cur_tacklinedir - 180 : cur_tacklinedir + 180;
				//Boat : get an end of the predicted layline
				PositionBearingDistanceMercator_Plugin(mlat, mlon, boat_tckTVMGDir, DistToMark * 2, &pc_end.y, &pc_end.x);
				//Boat : get the second end of the same predicted layline on the opposite direction
				PositionBearingDistanceMercator_Plugin(mlat, mlon, cur_tacklinedir, DistToMark * 2, &pc_end2.y, &pc_end2.x);

				// see if we have an intersection of the 2 laylines
				wxRealPoint pIntersection_pos;
				pIntersection_pos = GetLineIntersection(pc_end, pc_end2, pm_end, pm_end2);

				if (pIntersection_pos.x > 0 && pIntersection_pos.y > 0){
					//calc time-to-mark on direct line, versus opt. TWA and intersection
					// now calc time-to-mark via intersection
					double TimeToMarkwInt, tempCTM, DistToMarkwInt, dist1, dist2;
					//distance btw. boat and intersection
					DistanceBearingMercator_Plugin(mlat, mlon, pIntersection_pos.y, pIntersection_pos.x, &tempCTM, &dist1);
					//dist1 = local_distance(mlat, mlon, pIntersection_pos.y, pIntersection_pos.x);

					//distance btw. Intersection - Mark
					DistanceBearingMercator_Plugin(pIntersection_pos.y, pIntersection_pos.x, m_pMark->m_lat, m_pMark->m_lon, &tempCTM, &dist2);
					//dist2 = local_distance(pIntersection_pos.y, pIntersection_pos.x, m_pMark->m_lat, m_pMark->m_lon);
					//Total distance as sum of dist1 + dist2
					DistToMarkwInt = dist1 + dist2;
					TimeToMarkwInt = CalcPolarTimeToMark(DistToMarkwInt, tvmg.TargetAngle, tws_kts);
					if (std::isnan(TimeToMarkwInt))TimeToMarkwInt = 99999;
					if (TimeToMarkwInt > 0 && directLineTimeToMark > 0){
						//only draw the laylines with intersection, if they are faster than the direct course
						if (TimeToMarkwInt < directLineTimeToMark){
							if (curTack == _T("\u00B0L"))
								glColor4ub(0, 200, 0, 255);
							else
								glColor4ub(255, 0, 0, 255);
							glLineWidth(2);
							wxPoint pinter;
							GetCanvasPixLL(vp, &pinter, pIntersection_pos.y, pIntersection_pos.x);

							glBegin(GL_LINES); // intersect from target layline --> target other tack VMG --> mark
							glVertex2d(boat.x, boat.y);   //from boat to intersection with Target VMG-Angle, but sailing on other tack
							glVertex2d(pinter.x, pinter.y);
							if (curTack == _T("\u00B0L"))
								glColor4ub(255, 0, 0, 255);
							else
								glColor4ub(0, 200, 0, 255);
							glVertex2d(pinter.x, pinter.y);//from intersection to mark with Target VMG-Angle, but sailing on other tack
							glVertex2d(mark_center.x, mark_center.y);
							glEnd();
						}
						else { // otherwise highlight the direct line 
							if (sigCTM_TWA <0)
								glColor4ub(255, 0, 0, 255);
							else
								glColor4ub(0, 200, 0, 255);
							glLineWidth(2);
							glBegin(GL_LINES);
							glVertex2d(boat.x, boat.y);
							glVertex2d(mark_center.x, mark_center.y);
							glEnd();
						}
					}
				}
				else { //no intersection ...
					if (directLineTimeToMark < 99999.){ //but direct line may be valid
						if (sigCTM_TWA <0)
							glColor4ub(255, 0, 0, 255);
						else
							glColor4ub(0, 200, 0, 255);
						glLineWidth(2);
						glBegin(GL_LINES);
						glVertex2d(boat.x, boat.y);
						glVertex2d(mark_center.x, mark_center.y);
						glEnd();
					}
					else{
						wxPoint pcogend, pmarkend;
						GetCanvasPixLL(vp, &pmarkend, pm_end.y, pm_end.x);
						GetCanvasPixLL(vp, &pcogend, pc_end.y, pc_end.x);

						if (curTack == _T("\u00B0L"))glColor4ub(0, 200, 0, 255);
						else  glColor4ub(255, 0, 0, 255);
						glLineWidth(2);
						glLineStipple(4, 0xAAAA);
						glEnable(GL_LINE_STIPPLE);
						glBegin(GL_LINES); // intersect from target layline --> target other tack VMG --> mark
						glVertex2d(boat.x, boat.y);   //from boat to intersection with Target VMG-Angle, but sailing on other tack
						glVertex2d(pcogend.x, pcogend.y);
						if (curTack == _T("\u00B0L"))glColor4ub(255, 0, 0, 255);
						else glColor4ub(0, 200, 0, 255);
						glVertex2d(pmarkend.x, pmarkend.y);//from intersection to mark with Target VMG-Angle, but sailing on other tack
						glVertex2d(mark_center.x, mark_center.y);
						glEnd();
						glDisable(GL_LINE_STIPPLE);
					}
				}
			}
		}
	}

}


/**********************************************************************
Draw the OpenGL Polar on the ships position overlay
Polar is normalized (always same size)
What should be drawn:
* the actual polar curve for the actual TWS
* 0/360� point (directly upwind)
* the rest of the polar currently in 2� steps
***********************************************************************/
#define STEPS  180 //72

void tactics_pi::DrawPolar(PlugIn_ViewPort *vp, wxPoint pp, double PolarAngle)
{
	if (m_bShowPolarOnChart && !std::isnan(mTWS) && !std::isnan(mTWD) && !std::isnan(mBRG)){
		glColor4ub(0, 0, 255, 192);	// red, green, blue,  alpha (byte values)
		double polval[STEPS];
		double max = 0;
		double rotate = vp->rotation;
		int i;
		if (mTWS > 0){
			TargetxMG vmg_up = BoatPolar->GetTargetVMGUpwind(mTWS);
			TargetxMG vmg_dn = BoatPolar->GetTargetVMGDownwind(mTWS);
			TargetxMG CmGMax, CmGMin;
			BoatPolar->Calc_TargetCMG2(mTWS, mTWD, mBRG, &CmGMax, &CmGMin);  //CmGMax = the higher value, CmGMin the lower cmg value

			for (i = 0; i < STEPS / 2; i++){ //0...179
				polval[i] = BoatPolar->GetPolarSpeed(i * 2 + 1, mTWS); //polar data is 1...180 !!! i*2 : draw in 2� steps
				polval[STEPS - 1 - i] = polval[i];
				//if (std::isnan(polval[i])) polval[i] = polval[STEPS-1 - i] = 0.0;
				if (polval[i]>max) max = polval[i];
			}
			wxPoint currpoints[STEPS];
			double rad, anglevalue;
			for (i = 0; i < STEPS; i++){
				anglevalue = deg2rad(PolarAngle + i * 2) + deg2rad(0. - ANGLE_OFFSET); //i*2 : draw in 2� steps
				rad = 81 * polval[i] / max;
				currpoints[i].x = pp.x + (rad * cos(anglevalue));
				currpoints[i].y = pp.y + (rad * sin(anglevalue));
			}
			glLineWidth(1);
			glBegin(GL_LINES);

			if (std::isnan(polval[0])){ //always draw the 0� point (directly upwind)
				currpoints[0].x = pp.x;
				currpoints[0].y = pp.y;
			}
			glVertex2d(currpoints[0].x, currpoints[0].y);

			for (i = 1; i < STEPS; i++){
				if (!std::isnan(polval[i])){  //only draw, if we have a real data value (NAN is init status, w/o data)
					glVertex2d(currpoints[i].x, currpoints[i].y);
					glVertex2d(currpoints[i].x, currpoints[i].y);
				}
			}
			glVertex2d(currpoints[0].x, currpoints[0].y); //close the curve

			//dc->DrawPolygon(STEPS, currpoints, 0, 0);
			glEnd();
			//draw Target-VMG Angles now
			if (!std::isnan(vmg_up.TargetAngle)){
				rad = 81 * BoatPolar->GetPolarSpeed(vmg_up.TargetAngle, mTWS) / max;
				DrawTargetAngle(vp, pp, PolarAngle + vmg_up.TargetAngle, _T("BLUE3"), 1, rad);
				DrawTargetAngle(vp, pp, PolarAngle - vmg_up.TargetAngle, _T("BLUE3"), 1, rad);
			}
			if (!std::isnan(vmg_dn.TargetAngle)){
				rad = 81 * BoatPolar->GetPolarSpeed(vmg_dn.TargetAngle, mTWS) / max;
				DrawTargetAngle(vp, pp, PolarAngle + vmg_dn.TargetAngle, _T("BLUE3"), 1, rad);
				DrawTargetAngle(vp, pp, PolarAngle - vmg_dn.TargetAngle, _T("BLUE3"), 1, rad);
			}
			if (!std::isnan(CmGMax.TargetAngle)){
				rad = 81 * BoatPolar->GetPolarSpeed(CmGMax.TargetAngle, mTWS) / max;
				DrawTargetAngle(vp, pp, PolarAngle + CmGMax.TargetAngle, _T("URED"), 2, rad);
			}
			if (!std::isnan(CmGMin.TargetAngle)){
				rad = 81 * BoatPolar->GetPolarSpeed(CmGMin.TargetAngle, mTWS) / max;
				DrawTargetAngle(vp, pp, PolarAngle + CmGMin.TargetAngle, _T("URED"), 1, rad);
			}
			//Hdt line
			if (!std::isnan(mHdt)){
				wxPoint hdt;
				anglevalue = deg2rad(mHdt) + deg2rad(0. - ANGLE_OFFSET) + rotate;
				rad = 81 * 1.1;
				hdt.x = pp.x + (rad * cos(anglevalue));
				hdt.y = pp.y + (rad * sin(anglevalue));
				glColor4ub(0, 0, 255, 255);	// red, green, blue,  alpha (byte values)
				glLineWidth(3);
				glBegin(GL_LINES);
				glVertex2d(pp.x, pp.y);
				glVertex2d(hdt.x, hdt.y);
				glEnd();
			}
		}
	}
}
/***********************************************************************
Draw pointers for the optimum target VMG- and CMG Angle (if bearing is available)
************************************************************************/
void tactics_pi::DrawTargetAngle(PlugIn_ViewPort *vp, wxPoint pp, double Angle, wxString color, int size, double rad){
	//  if (TargetAngle > 0){
	double rotate = vp->rotation;
	//    double value = deg2rad(PolarAngle + TargetAngle) + deg2rad(0 - ANGLE_OFFSET) + rotate;
	//    double value1 = deg2rad(PolarAngle + 5 + TargetAngle) + deg2rad(0 - ANGLE_OFFSET) + rotate;
	//    double value2 = deg2rad(PolarAngle - 5 + TargetAngle) + deg2rad(0 - ANGLE_OFFSET) + rotate;
	double sizefactor, widthfactor;
	if (size == 1) {
		sizefactor = 1.05;
		widthfactor = 1.05;
	}
	else{
		sizefactor = 1.12;
		widthfactor = 1.4;
	}
	double value = deg2rad(Angle) + deg2rad(0 - ANGLE_OFFSET) + rotate;
	double value1 = deg2rad(Angle + 5 * widthfactor) + deg2rad(0 - ANGLE_OFFSET) + rotate;
	double value2 = deg2rad(Angle - 5 * widthfactor) + deg2rad(0 - ANGLE_OFFSET) + rotate;

	/*
     *           0
     *          /\
     *         /  \
     *        /    \
     *     2 /_ __ _\ 1
     *
     *           X
     */
	wxPoint points[4];
	points[0].x = pp.x + (rad * 0.95 * cos(value));
	points[0].y = pp.y + (rad * 0.95 * sin(value));
	points[1].x = pp.x + (rad * 1.15 * sizefactor * cos(value1));
	points[1].y = pp.y + (rad * 1.15 * sizefactor * sin(value1));
	points[2].x = pp.x + (rad * 1.15 * sizefactor * cos(value2));
	points[2].y = pp.y + (rad * 1.15 * sizefactor * sin(value2));
	/*    points[1].x = pp.x + (rad * 1.15 * cos(value1));
          points[1].y = pp.y + (rad * 1.15 * sin(value1));
          points[2].x = pp.x + (rad * 1.15 * cos(value2));
          points[2].y = pp.y + (rad * 1.15 * sin(value2));*/
	if (color == _T("BLUE3")) glColor4ub(0, 0, 255, 128);
	else if (color == _T("URED")) glColor4ub(255, 0, 0, 128);
	else glColor4ub(255, 128, 0, 168);

	glLineWidth(1);
	glBegin(GL_TRIANGLES);
	glVertex2d(points[0].x, points[0].y);
	glVertex2d(points[1].x, points[1].y);
	glVertex2d(points[2].x, points[2].y);
	glEnd();

	//}
	//  }
}
/***********************************************************************
Toggle Layline Render overlay
************************************************************************/
void tactics_pi::ToggleLaylineRender(wxWindow* parent)
{
	m_bLaylinesIsVisible = m_bLaylinesIsVisible ? false : true;
}
void tactics_pi::ToggleCurrentRender(wxWindow* parent)
{
	m_bDisplayCurrentOnChart = m_bDisplayCurrentOnChart ? false : true;
}
void tactics_pi::TogglePolarRender(wxWindow* parent)
{
	m_bShowPolarOnChart = m_bShowPolarOnChart ? false : true;
}
void tactics_pi::ToggleWindbarbRender(wxWindow* parent)
{
	m_bShowWindbarbOnChart = m_bShowWindbarbOnChart ? false : true;
}

/**********************************************************************
 **********************************************************************/
bool tactics_pi::GetLaylineVisibility(wxWindow* parent)
{
	return m_bLaylinesIsVisible;
}
bool tactics_pi::GetCurrentVisibility(wxWindow* parent)
{
	return m_bDisplayCurrentOnChart;
}
bool tactics_pi::GetWindbarbVisibility(wxWindow* parent)
{
	return m_bShowWindbarbOnChart;
}
bool tactics_pi::GetPolarVisibility(wxWindow* parent)
{
	return m_bShowPolarOnChart;
}

/*******************************************************************
 *******************************************************************/
void tactics_pi::CalculateLaylineDegreeRange(void)
{
	//calculate degree-range for laylines
	//do some exponential smoothing on degree range of COGs and  COG itself
    if (!std::isnan(mCOG)){
        if (mCOG != m_COGRange[0]){
            if (std::isnan(m_ExpSmoothSinCog)) m_ExpSmoothSinCog = 0;
            if (std::isnan(m_ExpSmoothCosCog)) m_ExpSmoothCosCog = 0;


            double mincog = 360, maxcog = 0;
            for (int i = 0; i < COGRANGE; i++){
                if (!std::isnan(m_COGRange[i])){
                    mincog = wxMin(mincog, m_COGRange[i]);
                    maxcog = wxMax(maxcog, m_COGRange[i]);
                }
            }
            m_LaylineDegRange = getDegRange(maxcog, mincog);
            for (int i = 0; i < COGRANGE - 1; i++) m_COGRange[i + 1] = m_COGRange[i];
            m_COGRange[0] = mCOG;
            if (m_LaylineDegRange < g_iMinLaylineWidth){
                m_LaylineDegRange = g_iMinLaylineWidth;
            }
            else if (m_LaylineDegRange > g_iMaxLaylineWidth){
                m_LaylineDegRange = g_iMaxLaylineWidth;
            }

            //shifting
            double rad = (90 - mCOG)*M_PI / 180.;
            mExpSmSinCog->SetAlpha(g_dalphaLaylinedDampFactor);
            mExpSmCosCog->SetAlpha(g_dalphaLaylinedDampFactor);
            m_ExpSmoothSinCog = mExpSmSinCog->GetSmoothVal(sin(rad));
            m_ExpSmoothCosCog = mExpSmCosCog->GetSmoothVal(cos(rad));

            m_LaylineSmoothedCog = (int)(90. - (atan2(m_ExpSmoothSinCog, m_ExpSmoothCosCog)*180. / M_PI) + 360.) % 360;


            mExpSmDegRange->SetAlpha(g_dalphaDeltCoG);
            m_ExpSmoothDegRange = mExpSmDegRange->GetSmoothVal(m_LaylineDegRange);
        }
    }
}


/**********************************************************************
Draw the OpenGL Windbarb on the ships position overlay
Basics taken from tackandlay_pi and adopted
***********************************************************************/
void tactics_pi::DrawWindBarb(wxPoint pp, PlugIn_ViewPort *vp)
{
    if (m_bShowWindbarbOnChart && !std::isnan(mTWD) && !std::isnan(mTWS)){
        if (mTWD >= 0 && mTWD < 360){
            glColor4ub(0, 0, 255, 192);	// red, green, blue,  alpha (byte values)
            double rad_angle;
            double shaft_x, shaft_y;
            double barb_0_x, barb_0_y, barb_1_x, barb_1_y;
            double barb_2_x, barb_2_y;
            double barb_length_0_x, barb_length_0_y, barb_length_1_x, barb_length_1_y;
            double barb_length_2_x, barb_length_2_y;
            double barb_3_x, barb_3_y, barb_4_x, barb_4_y, barb_length_3_x, barb_length_3_y, barb_length_4_x, barb_length_4_y;
            double tws_kts = fromUsrSpeed_Plugin(mTWS, g_iDashWindSpeedUnit);

            double barb_length[50] = {
                0, 0, 0, 0, 0,    //  0 knots
                0, 0, 0, 0, 5,    //  5 knots
                0, 0, 0, 0, 10,    // 10 knots
                0, 0, 0, 5, 10,    // 15 knots
                0, 0, 0, 10, 10,   // 20 knots
                0, 0, 5, 10, 10,   // 25 knots
                0, 0, 10, 10, 10,  // 30 knots
                0, 5, 10, 10, 10,  // 35 knots
                0, 10, 10, 10, 10,  // 40 knots
                5, 10, 10, 10, 10  // 45 knots
            };

            int p = 0;
            if (tws_kts < 3.)
                p = 0;
            else if (tws_kts >= 3. && tws_kts < 8.)
                p = 1;
            else if (tws_kts >= 8. && tws_kts < 13.)
                p = 2;
            else if (tws_kts >= 13. && tws_kts < 18.)
                p = 3;
            else if (tws_kts >= 18. && tws_kts < 23.)
                p = 4;
            else if (tws_kts >= 23. && tws_kts < 28.)
                p = 5;
            else if (tws_kts >= 28. && tws_kts < 33.)
                p = 6;
            else if (tws_kts >= 33. && tws_kts < 38.)
                p = 7;
            else if (tws_kts >= 38. && tws_kts < 43.)
                p = 8;
            else if (tws_kts >= 43. && tws_kts < 48.)
                p = 9;
            else if (tws_kts >= 48.)
                p = 9;
            //wxLogMessage("mTWS=%.2f --> p=%d", mTWS, p);
            p = 5 * p;

            double rotate = vp->rotation;
            rad_angle = ((mTWD - 90.) * M_PI / 180) + rotate;

            shaft_x = cos(rad_angle) * 90;
            shaft_y = sin(rad_angle) * 90;

            barb_0_x = pp.x + .6 * shaft_x;
            barb_0_y = (pp.y + .6 * shaft_y);
            barb_1_x = pp.x + .7 * shaft_x;
            barb_1_y = (pp.y + .7 * shaft_y);
            barb_2_x = pp.x + .8 * shaft_x;
            barb_2_y = (pp.y + .8 * shaft_y);
            barb_3_x = pp.x + .9 * shaft_x;
            barb_3_y = (pp.y + .9 * shaft_y);
            barb_4_x = pp.x + shaft_x;
            barb_4_y = (pp.y + shaft_y);

            barb_length_0_x = cos(rad_angle + M_PI / 4) * barb_length[p] * 3;
            barb_length_0_y = sin(rad_angle + M_PI / 4) * barb_length[p] * 3;
            barb_length_1_x = cos(rad_angle + M_PI / 4) * barb_length[p + 1] * 3;
            barb_length_1_y = sin(rad_angle + M_PI / 4) * barb_length[p + 1] * 3;
            barb_length_2_x = cos(rad_angle + M_PI / 4) * barb_length[p + 2] * 3;
            barb_length_2_y = sin(rad_angle + M_PI / 4) * barb_length[p + 2] * 3;
            barb_length_3_x = cos(rad_angle + M_PI / 4) * barb_length[p + 3] * 3;
            barb_length_3_y = sin(rad_angle + M_PI / 4) * barb_length[p + 3] * 3;
            barb_length_4_x = cos(rad_angle + M_PI / 4) * barb_length[p + 4] * 3;
            barb_length_4_y = sin(rad_angle + M_PI / 4) * barb_length[p + 4] * 3;

            glLineWidth(2);
            glBegin(GL_LINES);
            glVertex2d(pp.x, pp.y);
            glVertex2d(pp.x + shaft_x, pp.y + shaft_y);
            glVertex2d(barb_0_x, barb_0_y);
            glVertex2d(barb_0_x + barb_length_0_x, barb_0_y + barb_length_0_y);
            glVertex2d(barb_1_x, barb_1_y);
            glVertex2d(barb_1_x + barb_length_1_x, barb_1_y + barb_length_1_y);
            glVertex2d(barb_2_x, barb_2_y);
            glVertex2d(barb_2_x + barb_length_2_x, barb_2_y + barb_length_2_y);
            glVertex2d(barb_3_x, barb_3_y);
            glVertex2d(barb_3_x + barb_length_3_x, barb_3_y + barb_length_3_y);
            glVertex2d(barb_4_x, barb_4_y);
            glVertex2d(barb_4_x + barb_length_4_x, barb_4_y + barb_length_4_y);
            glEnd();
        }
    }
}


/*********************************************************************
NMEA $PNKEP (NKE style) performance data
**********************************************************************/
void tactics_pi::ExportPerformanceData(void)
{
	//PolarTargetSpeed
	if (g_bExpPerfData01 && !std::isnan(mPolarTargetSpeed)){
		createPNKEP_NMEA(1, mPolarTargetSpeed, mPolarTargetSpeed  * 1.852, 0, 0);
	}
	//todo : extract mPredictedCoG calculation from layline.calc and add to CalculatePerformanceData
	if (g_bExpPerfData02 && !std::isnan(mPredictedCoG)){
		createPNKEP_NMEA(2, mPredictedCoG, 0, 0, 0); // course (CoG) on other tack
	}
	//Target VMG angle, act. VMG % upwind, act. VMG % downwind
	if (g_bExpPerfData03 && !std::isnan(tvmg.TargetAngle) && tvmg.TargetSpeed > 0){
		createPNKEP_NMEA(3, tvmg.TargetAngle, mPercentTargetVMGupwind, mPercentTargetVMGdownwind, 0);
	}
	//Gain VMG de 0 � 999%, Angle pour optimiser le VMG de 0 � 359�,Gain CMG de 0 � 999%,Angle pour optimiser le CMG de 0 � 359�
	if (g_bExpPerfData04)
		createPNKEP_NMEA(4, mCMGoptAngle, mCMGGain, mVMGoptAngle, mVMGGain);
	//current direction, current speed kts, current speed in km/h,
	if (g_bExpPerfData05 && !std::isnan(m_CurrentDirection) && !std::isnan(m_ExpSmoothCurrSpd)){
		createPNKEP_NMEA(5, m_CurrentDirection, m_ExpSmoothCurrSpd, m_ExpSmoothCurrSpd  * 1.852, 0);
	}
}

void tactics_pi::createPNKEP_NMEA(int sentence, double data1, double data2, double data3, double data4)
{
	wxString nmeastr = "";
	switch (sentence)
	{
	case 0:
		//strcpy(nmeastr, "$PNKEPA,");
		break;
	case 1:
		nmeastr = _T("$PNKEP,01,") + wxString::Format("%.2f,N,", data1) + wxString::Format("%.2f,K", data2);
		break;
	case 2:
		/*course on next tack(code PNKEP02)
		$PNKEP, 02, x.x*hh<CR><LF>
		\ Cap sur bord Oppos� / prochain bord de 0 � 359�*/
		nmeastr = _T("$PNKEP,02,") + wxString::Format("%.1f", data1);
		break;
	case 3:
		/*    $PNKEP, 03, x.x, x.x, x.x*hh<CR><LF>
		|    |     \ performance downwind from 0 to 99 %
		|     \ performance upwind from 0 to 99 %
		\ opt.VMG angle  0 � 359�  */
		nmeastr = _T("$PNKEP,03,") + wxString::Format("%.1f,", data1) + wxString::Format("%.1f,", data2) + wxString::Format("%.1f", data3);
		break;
	case 4:
		/*Calculates the gain for VMG & CMG and stores it in the variables
		mVMGGain, mCMGGain,mVMGoptAngle,mCMGoptAngle
		Gain is the percentage btw. the current boat speed mStW value and Target-VMG/CMG
		Question : shouldn't we compare act.VMG with Target-VMG ? To be investigated ...
		$PNKEP, 04, x.x, x.x, x.x, x.x*hh<CR><LF>
		|    |    |    \ Gain VMG de 0 � 999 %
		|    |     \ Angle pour optimiser le VMG de 0 � 359�
		|    \ Gain CMG de 0 � 999 %
		\ Angle pour optimiser le CMG de 0 � 359�*/
		nmeastr = _T("$PNKEP,04,") + wxString::Format("%.1f,", data1) + wxString::Format("%.1f,", data2) + wxString::Format("%.1f,", data3) + wxString::Format("%.1f", data4);
		break;
	case 5:
		nmeastr = _T("$PNKEP,05,") + wxString::Format("%.1f,", data1) + wxString::Format("%.2f,N,", data2) + wxString::Format("%.2f,K", data3);
		break;
	default:
		nmeastr = _T("");
		break;
	}
	if (nmeastr != "")
		SendNMEASentence(nmeastr);
}
/***********************************************************************
Put the created NMEA record ot O's NMEA data stream
Taken from nmeaconverter_pi.
All credits to Pavel !
***********************************************************************/
void tactics_pi::SendNMEASentence(wxString sentence)
{
	wxString Checksum = ComputeChecksum(sentence);
	sentence = sentence.Append(wxT("*"));
	sentence = sentence.Append(Checksum);
	sentence = sentence.Append(wxT("\r\n"));
	//wxLogMessage(sentence);
	PushNMEABuffer(sentence);
}
/**********************************************************************
Calculate the checksum of the created NMEA record.
Taken from nmeaconverter_pi.
All credits to Pavel !
***********************************************************************/
wxString tactics_pi::ComputeChecksum(wxString sentence)
{
	unsigned char calculated_checksum = 0;
	for (wxString::const_iterator i = sentence.begin() + 1; i != sentence.end() && *i != '*'; ++i)
		calculated_checksum ^= static_cast<unsigned char> (*i);

	return(wxString::Format(wxT("%02X"), calculated_checksum));
}

/**********************************************************************
NKE has a bug in its TWS calculation with activated(!) "True Wind Tables"
in combination of Multigraphic instrument and HR wind sensor. This almost
duplicates the TWS values delivered in MWD & VWT and destroys the Wind
history view, showing weird peaks. It is particularly annoying when @anchor
and trying to record windspeeds ...
It seems to happen only when 
* VWR sentence --> AWA changes from "Left" to "Right" through 0� AWA
* with low AWA values like 0...4 degrees
NMEA stream looks like this:
                 $IIMWD,,,349,M,6.6,N,3.4,M*29                 6.6N TWS
                 $IIVWT, 3, R, 7.1, N, 3.7, M, 13.1, K * 63
                 $IIVWR, 0, R, 7.9, N, 4.1, M, 14.6, K * 6F    it seems to begin with 0�AWA ...
                 $IIMWD, , , 346, M, 15.3, N, 7.9, M * 18      jump from 6.6 to 15.3 TWS ...
                 $IIVWT, 7, R, 15.3, N, 7.9, M, 28.3, K * 56   VWT also has 15.3 TWS
                 $IIVWR, 1, L, 8.6, N, 4.4, M, 15.9, K * 7B     1�AWA
                 $IIMWD, , , 345, M, 15.8, N, 8.1, M * 17       still 15.8 TWS
                 $IIVWT, 1, L, 8.6, N, 4.4, M, 15.9, K * 7D     VWT now back to 8.6 TWS
                 $IIVWR, 0, R, 9.0, N, 4.6, M, 16.7, K * 6C     passing 0�AWA again
                 $IIMWD, , , 335, M, 17.6, N, 9.1, M * 1D       jump to 17.6 TWS
                 $IIVWT, 8, R, 17.6, N, 9.1, M, 32.6, K * 56    still 17.6 TWS 
                 $IIVWR, 4, R, 9.1, N, 4.7, M, 16.9, K * 66     VWR increasing to 4�AWA...
                 $IIMWD, , , 335, M, 9.0, N, 4.6, M*2E          and back to normal, 9 TWS
Trying to catch this here, return true if detected and if the check
is explictly requested in the ini-file. The implementation of
the SetNMEASentence in the child class can then simply drop this data.
***********************************************************************/
bool tactics_pi::SetNMEASentenceMWD_NKEbug(double SentenceWindSpeedKnots)
{
    if (!m_bNKE_TrueWindTableBug)
        return false;
    if (std::isnan(SentenceWindSpeedKnots))
        return false;
    if (std::isnan(mTWS))
        return false;
    if ((m_VWR_AWA < 8.0) && (SentenceWindSpeedKnots > mTWS*1.7)){
        /* wxLogMessage(
            "MWD-Sentence, MWD-Spd=%f,mTWS=%f,VWR_AWA=%f,NKE_BUG=%d",
            SentenceWindSpeedKnots, mTWS, m_VWR_AWA, m_bNKE_TrueWindTableBug); */
        return true;
    } // then conditions explained in the above description have been met
    return false;
}
