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

#include "Actions/ScaleTrack.h"
#include "Actions/ScaleSong.h"
#include "Actions/EditAction.h"
#include "Midi/Track.h"
#include "Midi/Sequence.h"

#include <wx/intl.h>

using namespace AriaMaestosa::Action;


ScaleSong::ScaleSong(float factor, int relative_to) :
    //I18N: (undoable) action name
    MultiTrackAction( _("scale song") )
{
    m_factor = factor;
    m_relative_to = relative_to;
}

ScaleSong::~ScaleSong()
{
}

void ScaleSong::perform()
{
    const int trackAmount = m_sequence->getTrackAmount();
    for (int t=0; t<trackAmount; t++)
    {
        Action::ScaleTrack* action = new Action::ScaleTrack(m_factor, m_relative_to, false);
        
        action->setParentTrack(m_sequence->getTrack(t), m_visitor->getNewTrackVisitor(t));
        action->perform();
        actions.push_back(action);
    }
}

void ScaleSong::undo()
{
    const int amount = actions.size();
    for (int a=0; a<amount; a++)
    {
        actions[a].undo();
    }
}

