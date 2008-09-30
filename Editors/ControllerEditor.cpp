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


#include "AriaCore.h"

#include "Actions/EditAction.h"
#include "Actions/AddControlEvent.h"
#include "Actions/AddControllerSlide.h"

#include "Editors/ControllerEditor.h"
#include "Midi/Sequence.h"
#include "Midi/Track.h"
#include "Pickers/TuningPicker.h"
#include "Images/ImageProvider.h"
#include "Midi/MeasureData.h"
#include "GUI/RenderUtils.h"

#include <string>
#include "Pickers/ControllerChoice.h"

#include "Editors/RelativeXCoord.h"

namespace AriaMaestosa {
	
ControllerEditor::ControllerEditor(Track* track) : Editor(track)
{
    
    mouse_is_in_editor=false;
    
    selection_begin = -1;
    selection_end = -1;
    
	hasBeenResizing = false;
	
    controllerChoice = new ControllerChoice(track->graphics);
}

int ControllerEditor::getCurrentControllerType()
{
	return controllerChoice->getControllerID();
}

void ControllerEditor::render()
{
	render( RelativeXCoord_empty(), -1, RelativeXCoord_empty(), -1, true );
}

void ControllerEditor::renderEvents()
{
    const int area_from_y = getEditorYStart()+7;
	const int area_to_y = getYEnd()-15;
	const float y_zoom = (float)( area_to_y-area_from_y ) / 127.0;
    
    AriaRender::color(0, 0.4, 1);
    AriaRender::lineWidth(3);
    
    ControllerEvent* tmp;
    int previous_location=-1, previous_value=-1;
    
    const int currentController = controllerChoice->getControllerID();
    
    const int eventAmount = track->getControllerEventAmount( currentController );
    const int scroll = sequence->getXScrollInPixels();
    
    
    int eventsOfThisType=0;
    for(int n=0; n<eventAmount; n++)
	{
        tmp = track->getControllerEvent(n, currentController);
        if(tmp->getController() != currentController) continue; // only draw events of this controller
        eventsOfThisType++;
        
        const int xloc = tmp->getPositionInPixels();
        const unsigned short value = tmp->getValue();
        
        if(previous_location - scroll > getXEnd()) // if events are no more visible, stop drawing
            return;
        
        if(xloc - scroll > getEditorsXStart())
		{
            if(previous_location!=-1 and previous_value!=-1)
			{
                AriaRender::line(previous_location - scroll, area_from_y+previous_value*y_zoom,
                                 xloc - scroll, area_from_y+previous_value*y_zoom);
            }
        }
        
        previous_location = xloc;
        previous_value = value;
    }// next
	
	// draw horizontal line drom last event to end of visible area
    if(eventsOfThisType>0)
	{
        AriaRender::line(previous_location - scroll, area_from_y+previous_value*y_zoom,
                         getXEnd(), area_from_y+previous_value*y_zoom);
    }
    
}

void ControllerEditor::render(RelativeXCoord mousex_current, int mousey_current,
							  RelativeXCoord mousex_initial, int mousey_initial, bool focus)
{
    AriaRender::beginScissors(10, getEditorYStart(), width - 15, 20+height);
    
    // -------------------------------- background ----------------------------
	
	const int area_from_y = getEditorYStart()+7;
	const int area_to_y = getYEnd()-15;

    AriaRender::primitives();
    
    AriaRender::color(0.9, 0.9, 0.9);
    AriaRender::rect(0, 0, getXEnd(), area_from_y);
    AriaRender::rect(0, area_to_y,getXEnd(), getYEnd());

    AriaRender::color(1,1,1);
    AriaRender::rect(0, area_from_y, getXEnd(), area_to_y);

    // -------------------------- selection ------------------------
    
    if(selection_begin != -1 and focus)
	{
        
        RelativeXCoord selectX1(selection_begin, MIDI);
        RelativeXCoord selectX2(selection_end, MIDI);
        
        AriaRender::color(0.8, 0.9, 1);
        AriaRender::rect(selectX1.getRelativeTo(WINDOW) , area_from_y,
                         selectX2.getRelativeTo(WINDOW) , area_to_y);
        
    }

    // ----------------------------------- middle line -----------------------
    AriaRender::color(0.9, 0.9, 0.9);
	
	// tempo
	if(controllerChoice->getControllerID()==201)
	{

		// top value is 500, bottom value is 0, find where to put middle value for it to be main tempo
		const int liney = (int)(
								area_to_y - (area_to_y-area_from_y)*( (sequence->getTempo()-20) / 380.0)
								);
        AriaRender::line(0, liney, getXEnd(), liney);
	}
	else
		// all others
	{
        AriaRender::line(0, (area_from_y+area_to_y)/2, getXEnd(), (area_from_y+area_to_y)/2);
    }

    drawVerticalMeasureLines(area_from_y, area_to_y);
    
	// ------------------------ min/max, on/off, left/right, etc. -------------------
    AriaRender::color(0.5, 0.5, 0.5);
    
	wxString top_name=controllerChoice->getTopLabel();
    AriaRender::small_text( top_name.mb_str(),  getEditorsXStart()+5 , area_from_y + 10);
	
	wxString bottom_name=controllerChoice->getBottomLabel();
    AriaRender::small_text( bottom_name.mb_str(),  getEditorsXStart()+5 , area_to_y - 5);
    
	// -------------------------- draw controller events ---------------------
    renderEvents();
		
	// ----------------------- add controller events (preview) -------------------
	if(track->graphics->dragging_resize) hasBeenResizing=true;
	
    if(mouse_is_in_editor and selection_begin == -1)
	{

        AriaRender::lineWidth(3);
        AriaRender::color(0, 0.4, 1);
        
        if(mousey_initial>=area_from_y and mousey_initial<=area_to_y and !hasBeenResizing)
		{

			// if out of bounds
            if(mousey_current<area_from_y) mousey_current=area_from_y;
            if(mousey_current>area_to_y) mousey_current=area_to_y;
            
			int tick1 = snapMidiTickToGrid( mousex_initial.getRelativeTo(MIDI)/* + sequence->getXScrollInMidiTicks()*/);
			int tick2 = snapMidiTickToGrid( mousex_current.getRelativeTo(MIDI)/* + sequence->getXScrollInMidiTicks()*/);
											
			if(tick2 < 0) tick2 = 0;
            if(tick1 < 0) tick1 = 0; 
			
            AriaRender::line(( tick1 - sequence->getXScrollInMidiTicks())
                             *sequence->getZoom()+getEditorsXStart(), mousey_initial,
                             ( tick2 - sequence->getXScrollInMidiTicks())
                             *sequence->getZoom()+getEditorsXStart(), mousey_current);
        }
    }
    AriaRender::lineWidth(1);

    // -----------------------------------------------------------------
    // left part with names
    // -----------------------------------------------------------------
    
    // grey background
    if(!focus) AriaRender::color(0.4, 0.4, 0.4);
    else AriaRender::color(0.8, 0.8, 0.8);
    
    AriaRender::rect( 0, getEditorYStart(),
                      getEditorsXStart(), getYEnd());
    
    // controller name
    AriaRender::color(0,0,0);

    char* controllerName=controllerChoice->getControllerName();
    AriaRender::small_text_newline_between_words(controllerName,18, getEditorYStart()+15);

    AriaRender::images();
    AriaRender::endScissors();
    
}

void ControllerEditor::mouseDown(RelativeXCoord x, const int y)
{
	
	hasBeenResizing = false;
	
	
    // prepare coords
	selection_begin = -1;
	selection_end = -1;
	
    // check if user is dragging on this track
    mouse_is_in_editor=false;
    if(y<getYEnd()-15 and y>getEditorYStart() and
	   x.getRelativeTo(WINDOW) < getWidth() - 24 and
	   x.getRelativeTo(EDITOR)>-1)
		mouse_is_in_editor=true;  
	
	// check whether we're selecting
	selecting = false;
	if(mouse_is_in_editor and Display::isSelectMorePressed())
		selecting = true;
	
    if( x.getRelativeTo(WINDOW)<getEditorsXStart() and y>getEditorYStart() and !track->graphics->collapsed )
        Display::popupMenu(controllerChoice,x.getRelativeTo(WINDOW),y+15);

}

void ControllerEditor::mouseDrag(RelativeXCoord mousex_current, const int mousey_current,
								 RelativeXCoord mousex_initial, const int mousey_initial)
{
	
    if( mouse_is_in_editor and selecting /*Display::isSelectMorePressed()*/ )
	{
        
        // ------------------------ select ---------------------
        selection_begin = snapMidiTickToGrid( /*sequence->getXScrollInMidiTicks() +*/ mousex_initial.getRelativeTo(MIDI) );
        selection_end   = snapMidiTickToGrid( /*sequence->getXScrollInMidiTicks() +*/ mousex_current.getRelativeTo(MIDI) );
        
    }
	
}

void ControllerEditor::mouseUp(RelativeXCoord mousex_current, int mousey_current,
							   RelativeXCoord mousex_initial, int mousey_initial)
{
	
    if(mouse_is_in_editor)
	{
        
        if( selecting /*Display::isSelectMorePressed()*/ )
		{
            
            // ------------------------ select ---------------------
            
            selection_begin = snapMidiTickToGrid(/*sequence->getXScrollInMidiTicks() +*/ mousex_initial.getRelativeTo(MIDI) );
            selection_end   = snapMidiTickToGrid( /*sequence->getXScrollInMidiTicks() +*/ mousex_current.getRelativeTo(MIDI) );
            
			// no selection
            if(selection_begin == selection_end)
			{
                selection_begin = -1;
                selection_end = -1;
            }
            selecting = false;
        }
		else
		{
			
            selection_begin = -1;
            selection_end = -1;
            
			const int area_from_y = getEditorYStart()+7;
			const int area_to_y = getYEnd()-15;
			const float y_zoom = (float)( area_to_y-area_from_y ) / 127.0;
			
            // ------------------------ add controller events ---------------------
            
            // if mouse is out of bounds
			if(mousex_initial.getRelativeTo(WINDOW) < getEditorsXStart()) return;
			
			
            if(mousey_initial<area_from_y) return;
            if(mousey_initial>area_to_y) return;
			if(track->graphics->dragging_resize or hasBeenResizing) return;
            if(mousey_current<area_from_y) mousey_current=area_from_y;
            if(mousey_current>area_to_y) mousey_current=area_to_y;
            
			int tick1=snapMidiTickToGrid( mousex_initial.getRelativeTo(MIDI) /*+ sequence->getXScrollInMidiTicks()*/ );
            int tick2=snapMidiTickToGrid( mousex_current.getRelativeTo(MIDI) /*+ sequence->getXScrollInMidiTicks()*/ );
			
            if(tick2 < 0) tick2 = 0;
            if(tick1 < 0) tick1 = 0; 
			

            if(tick1 == tick2)
                track->action( new Action::AddControlEvent(tick1,
									 (int)((mousey_initial-area_from_y)/y_zoom),
									 controllerChoice->getControllerID()) );
            else
			{
                if(tick1<tick2) track->action( new Action::AddControllerSlide(tick1,
																		 (int)( (mousey_initial-area_from_y)/y_zoom ),
																		 tick2,
																		 (int)( (mousey_current-area_from_y)/y_zoom ),
																		 controllerChoice->getControllerID()) );
                else track->action( new Action::AddControllerSlide(tick2,
															   (int)( (mousey_current-area_from_y)/y_zoom ),
															   tick1,
															   (int)( (mousey_initial-area_from_y)/y_zoom ),
															   controllerChoice->getControllerID()) );
			}// end if tick1==tick2
            
        }// end if meta down
    }
    
    mouse_is_in_editor=false;

	
    render();
    
}

void ControllerEditor::rightClick(RelativeXCoord x, const int y)
{
        
}

void ControllerEditor::mouseExited(RelativeXCoord mousex_current, int mousey_current,
								   RelativeXCoord mousex_initial, int mousey_initial)
{
	// if mouse leaves the frame, it has the same effect as if it was released (terminate drag, terminate selection, etc.)
	this->mouseUp(mousex_current, mousey_current, mousex_initial, mousey_initial);
	Display::render();
}

int ControllerEditor::getYScrollInPixels()
{
    /*Unused*/
    return -1;
}

ControllerEditor::~ControllerEditor()
{
}

int ControllerEditor::getSelectionBegin()
{
    return selection_begin;
}

int ControllerEditor::getSelectionEnd()
{
    return selection_end;    
}

void ControllerEditor::selectAll( bool selected )
{
    
    // Select none
    if(!selected)
	{
        selection_begin = -1;
        selection_end = -1;
    }
	else
		// Select all 
	{  

        selection_begin = 0;
        //selection_end = sequence->getMeasureAmount() * sequence->ticksPerMeasure();
        selection_end = getMeasureData()->getTotalTickAmount();
    }
    
}

}
