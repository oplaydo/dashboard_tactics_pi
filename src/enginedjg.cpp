/******************************************************************************
* $Id: enginedjg.cpp, v1.0 2019/11/30 VaderDarth Exp $
*
* Project:  OpenCPN
* Purpose:  dahboard_tactics_pi plug-in
* Author:   Jean-Eudes Onfray
* 
***************************************************************************
*   Copyright (C) 2010 by David S. Register   *
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
*   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,  USA.             *
***************************************************************************
*/

// For compilers that support precompilation, includes "wx/wx.h".
#include <wx/wxprec.h>

// for all others, include the necessary headers (this file is usually all you
// need because it includes almost all "standard" wxWidgets headers)
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <wx/version.h>

#include <algorithm>
#include <functional>

#include "dashboard_pi.h"
#include "enginedjg.h"
#include "plugin_ids.h"

// --- the following is probably needed only for demonstration and testing! ---
extern int GetRandomNumber(int, int);


wxBEGIN_EVENT_TABLE (DashboardInstrument_EngineDJG, InstruJS)
   EVT_TIMER (myID_TICK_ENGINEDJG, DashboardInstrument_EngineDJG::OnThreadTimerTick)
   EVT_CLOSE (DashboardInstrument_EngineDJG::OnClose)
wxEND_EVENT_TABLE ()
//************************************************************************************************************************
// Numerical instrument for engine monitoring data
//************************************************************************************************************************

DashboardInstrument_EngineDJG::DashboardInstrument_EngineDJG(
    TacticsWindow *pparent, wxWindowID id, sigPathLangVector *sigPaths,
    wxString ids, PI_ColorScheme cs, wxString format ) :
    InstruJS ( pparent, id, ids, cs )
{
    m_pparent = pparent;
    previousTimestamp = 0LL; // dashboard instru base class
    m_orient = wxVERTICAL;
    m_pSigPathLangVector = sigPaths;
    m_threadRunning = false;
    m_pconfig = GetOCPNConfigObject();

    if ( !LoadConfig() )
        return;

    m_pThreadEngineDJGTimer = new wxTimer( this, myID_TICK_ENGINEDJG );
    m_pThreadEngineDJGTimer->Start(100, wxTIMER_CONTINUOUS);
}
DashboardInstrument_EngineDJG::~DashboardInstrument_EngineDJG(void)
{
    this->m_pThreadEngineDJGTimer->Stop();
    delete this->m_pThreadEngineDJGTimer;
    SaveConfig();
    return;
}
void DashboardInstrument_EngineDJG::OnClose( wxCloseEvent &event )
{
    this->m_pThreadEngineDJGTimer->Stop();
    this->stopScript(); // base class implements, we are first to be called
    event.Skip(); // Destroy() must be called
}

void DashboardInstrument_EngineDJG::derivedTimeoutEvent()
{
    m_data = L"0.0";
    derived2TimeoutEvent();
}

void DashboardInstrument_EngineDJG::SetData(
    unsigned long long st, double data, wxString unit, long long timestamp)
{
    return; // this derived class gets its data from the multiplexer through a callback PushData()
}

void DashboardInstrument_EngineDJG::OnThreadTimerTick( wxTimerEvent &event )
{
    /*
    if ( m_threadRunning ) {
        if ( m_path.IsEmpty() ) {
              We will emulate in this event the right click event for a selection a signal path from a list,
              given by the hosting application in the constructor (see below how to use). We'll
              make a simple random simulator (not to implement any GUI features for now)
            wxString sTestingOnly[3];
            sTestingOnly[0] = L"propulsion.port.revolutions";
            sTestingOnly[1] = L"propulsion.port.oilPressure";
            sTestingOnly[2] = L"propulsion.port.temperature";
            // m_path = sTestingOnly[ GetRandomNumber(0,2) ]; // let Mme Fortuna to be the user, for testing!!!!!!!
            // m_path = sTestingOnly[1]; // Let's test with the oil pressure!
            // Subscribe to the signal path data with this object's method to call back
            // m_pushHere = std::bind(&DashboardInstrument_EngineDJG::PushData,
            //                       this, _1, _2, _3 );
            // m_pushHereUUID = m_pparent->subscribeTo ( m_path, m_pushHere );
        }
        if ( instrIsReadyForConfig() && !instrIsRunning() ) {
            setNewConfig ( m_path );
            restartInstrument();
        }
    }
    else {
    */
    if ( !m_threadRunning ) {
        wxSize thisSize = wxControl::GetSize();
        wxSize thisFrameInitSize = GetSize( m_orient, thisSize );
        SetInitialSize ( thisFrameInitSize );
        wxSize webViewInitSize = thisFrameInitSize;
        this->loadHTML( m_fullPathHTML, webViewInitSize );
        m_pThreadEngineDJGTimer->Stop();
        m_pThreadEngineDJGTimer->Start(1000, wxTIMER_CONTINUOUS);
        m_threadRunning = true;
    } // else thread is not running (no JS instrument created in this frame)


        /*
          Get the titles, descriptions and user's language for his selection from the hosting application
        */
        // sigPathLangVector::iterator it = std::find_if(
        //     m_pSigPathLangVector->begin(), m_pSigPathLangVector->end(),
        //     [this](const sigPathLangTuple& e){return std::get<0>(e) == m_path;});
        // if ( it != m_pSigPathLangVector->end() ) {
        //     sigPathLangTuple sigPathWithLangFeatures = *it;
        //     // the window title is changed in the base class, see instrument.h
        //     m_title = std::get<1>(sigPathWithLangFeatures);
        //     // Subscribe to the signal path data with this object's method to call back
        //     m_pushHere = std::bind(&DashboardInstrument_EngineDJG::PushData,
        //                            this, _1, _2, _3 );
        //     m_pushHereUUID = m_pparent->subscribeTo ( m_path, m_pushHere );
        // } // then found user selection from the available signal paths for subsribtion

    // } // then not subscribed to any path yet
}

wxSize DashboardInstrument_EngineDJG::GetSize( int orient, wxSize hint )
{
    int x,y;
    m_orient = orient;
    if( m_orient == wxHORIZONTAL ) {
        x = ENGINEDJG_H_MIN_WIDTH;
        y = wxMax( hint.y, ENGINEDJG_H_MIN_HEIGHT );
    }
    else {
        x = wxMax( hint.x, ENGINEDJG_V_MIN_WIDTH );
        y = ENGINEDJG_V_MIN_HEIGHT;
    }
    return wxSize( x, y );
}

bool DashboardInstrument_EngineDJG::LoadConfig()
{
    wxFileConfig *pConf = m_pconfig;
    
    if (!pConf)
        return false;
    
    // Make a proposal for the defaul path _and_ the protocool, which use can then override in the file:
    wxString sFullPathHTML = "file://"; // preferably 'http://' or 'https://' but this is easier for people to start with
    sFullPathHTML += *GetpSharedDataLocation(); // provide by the plug-in API
    wxString s = wxFileName::GetPathSeparator();
    sFullPathHTML +=
        _T("plugins")  + s + _T("dashboard_tactics_pi") + s + _T("data")+ s +
        _T("instrujs") + s + _T("enginedjg") + s + _T("index.html");

    pConf->SetPath(_T("/PlugIns/Dashboard/WebView/EngineDJG/"));
    pConf->Read(_T("instrujsURL"), &m_fullPathHTML, sFullPathHTML );
    
    
    return true;
}

void DashboardInstrument_EngineDJG::SaveConfig()
{
    wxFileConfig *pConf = m_pconfig;
    
    if (!pConf)
        return;

    pConf->SetPath(_T("/PlugIns/Dashboard/WebView/EngineDJG/"));
    pConf->Write(_T("instrujsURL"), m_fullPathHTML );
    
    return;
}

