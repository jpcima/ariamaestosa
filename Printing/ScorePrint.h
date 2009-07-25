/*
 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License along
 with this program; if not, write to the Free Software Foundation, Inc.,
 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */


#ifndef _score_out_
#define _score_out_

#include <wx/file.h>
#include "Config.h"
#include "Printing/PrintingBase.h"
#include "Printing/PrintLayout.h"
#include "Editors/ScoreEditor.h"

namespace AriaMaestosa
{
    class ScoreAnalyser;
    
class ScorePrintable : public EditorPrintable
{
    void gatherScoreInfo(LayoutLine& line);
    //void gatherNotesAndBasicSetup(LayoutLine& line);
    
    // should eventually replace at least part of the two above
    void generateScoreInfo();
    
    void analyseAndDrawScore(bool f_clef, ScoreAnalyser& analyser, LayoutLine& line, wxDC& dc,
                   const int extra_lines_above, const int extra_lines_under,
                   const int x0, const int y0, const int x1, const int y1, bool show_measure_number);
    
    bool g_clef, f_clef;
    int middle_c_level;
    OwnerPtr<ScoreAnalyser> g_clef_analyser;
    OwnerPtr<ScoreAnalyser> f_clef_analyser;

public:
    ScorePrintable(Track* track_arg);
    virtual ~ScorePrintable();

    void addUsedTicks(const MeasureTrackReference& trackRef, std::map< int /* tick */, float /* position */ >&);
    
    void earlySetup();

    void drawLine(LayoutLine& line, wxDC& dc);
    int calculateHeight(LayoutLine& line);
    };

}

#endif
