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


#ifndef _print_layout_h_
#define _print_layout_h_

#include <vector>
#include <map>
#include "ptr_vector.h"
#include "wx/wx.h"
#include "Printing/LayoutTree.h"

namespace AriaMaestosa
{
    class Track;
    class AriaPrintable;

    static const int max_line_width_in_units = 60;
    static const int maxLinesInPage = 10;

int getRepetitionMinimalLength();
void setRepetitionMinimalLength(const int newvalue);

    /**
      * For non-linear printing.
      * Each tick where there is a note/silence/something else is marked with this structure.
      * The 'proportion' argument allows giving more space for a specific tick, e.g. if there's
      * something that takes more space to draw there.
      */
    struct TickPosInfo
    {
        float relativePosition;
        int proportion;
        
        TickPosInfo()
        {
            TickPosInfo::relativePosition = -1; // will be set later
            TickPosInfo::proportion = 1;
        }
        
        TickPosInfo(int proportion)
        {
            TickPosInfo::relativePosition = -1; // will be set later
            TickPosInfo::proportion = proportion;
        }
    };

class PrintLayoutManager
{
    AriaPrintable* parent;
    
    // referencing vectors from AriaPrintable
    ptr_vector<LayoutLine>& layoutLines;
    std::vector<LayoutPage>& layoutPages;
    ptr_vector<MeasureToExport>& measures;
    
    std::vector<LayoutElement> layoutElements;
    
    void layInLinesAndPages();
    
    void calculateRelativeLengths();
    
    void createLayoutElements(bool checkRepetitions_bool);
    void generateMeasures(ptr_vector<Track, REF>& tracks);
    
    void findSimilarMeasures();
    
    void divideLineAmongTracks(LayoutLine& line, const int x0, const int y0, const int x1, const int y1,
                       int margin_below, int margin_above);
    
public:
    PrintLayoutManager(AriaPrintable* parent,
                       ptr_vector<LayoutLine>& layoutLines  /* out */,
                       std::vector<LayoutPage>& layoutPages  /* out */,
                       ptr_vector<MeasureToExport>& mesaures /* out */);
    
    void placeTracksInPage(LayoutPage& page, const int text_height, const float track_area_height, const int level_y_amount,
                         const int pageHeight, const int x0, const int y0, const int x1);
    
    void calculateLayoutElements(ptr_vector<Track, REF>& track, const bool checkRepetitions_bool);
};
    
}

#endif
