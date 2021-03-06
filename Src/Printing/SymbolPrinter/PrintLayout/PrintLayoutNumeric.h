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

#ifndef __PrintLayoutNumeric_H__
#define __PrintLayoutNumeric_H__


namespace AriaMaestosa
{
    class LayoutLine;
    class LayoutPage;
    
    /**
     * This is the "numeric" counterpart for PrintLayoutAbstract.
     * Where PrintLayoutAbstract layed out things in abstract ways, without taking care
     * of actual measurements in units of things, this class takes the output of
     * of PrintLayoutAbstract and turns them into hard printing coordinates.
     */
    class PrintLayoutNumeric
    {
        /** Internal method called by 'setLineCoordsAndDivideItsSpace'. Takes care of setting the coords
          * in the passed 'LineTrackRef'. Called once per track. */
        void placeTrackWithinCoords(const int trackID, LayoutLine& line,
                                    int x0, const int y0, const int x1, const int y1);
        
        /** Internal method called by 'setLineCoordsAndDivideItsSpace'. Takes care of setting the coords
          * of each layourt element of the line. Called once for the whole line. */
        void placeElementsWithinCoords(LayoutLine& line, int x0, const int x1);
        
        /**
         * Internal method called by 'placeLinesInPage'.
         * A single line of measures might contain more than one track, stacked in a vertical fashion.
         * This method receives the space that was allocated for the full line as argument;
         * it first sets the coord of this line. Then it splits the available vertical space between
         * the various tracks that form it, and set the coords of the LineTrackRef referring to each
         * track in this line.
         */
        void setLineCoordsAndDivideItsSpace(LayoutLine& line, const int x0, const int y0,
                                            const int x1, const int y1);
        
    public:
        
        PrintLayoutNumeric();
        
        
        /**
         * Main method to use this class. Given a LayoutPage, calculates the actual print coordinates of the
         * lines and elements within this page. The output is achieved by setting previously not set values
         * in the layout classes (FIXME(DESIGN): required call sequence exposes implementation details)
         *
         * @param page                Page for which to calculate and set absolute print coord
         * @param notation_area_y0    Y coordinate at which the actual notation can start being drawn (excluding header)
         * @param notation_area_h     Height in print units of the area that is for notation
         *                            (without the header/footer, etc.)
         * @param x0                  The minimum x coordinate at which printing can occur
         * @param x1                  The maximum x coordinate at which printing can occur
         */
        void placeLinesInPage(LayoutPage& page,
                              float notation_area_y0, const float notation_area_h,
                              const int pageHeight, const int x0, const int x1);
    };
    
}
#endif
