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

#include "Midi/ControllerEvent.h"
#include "Midi/Sequence.h"
#include "Midi/Track.h"

#include "irrXML/irrXML.h"

using namespace AriaMaestosa;

// ----------------------------------------------------------------------------------------------------------

ControllerEvent::ControllerEvent(unsigned short controller, int tick, wxFloat64 value)
{
    m_controller = controller;
    m_tick       = tick;
    m_value      = value;
}

// ----------------------------------------------------------------------------------------------------------

void ControllerEvent::setTick(int i)
{
    m_tick = i;
}

// ----------------------------------------------------------------------------------------------------------

void ControllerEvent::setValue(wxFloat64 value)
{
    m_value = value;
}

// ----------------------------------------------------------------------------------------------------------
// ----------------------------------------    SERIALIZATION    ---------------------------------------------
// ----------------------------------------------------------------------------------------------------------
#if 0
#pragma mark -
#pragma mark Serialization
#endif

void ControllerEvent::saveToFile(wxFileOutputStream& fileout)
{

    writeData( wxT("  <controlevent type=\"") + to_wxString(m_controller)           , fileout );
    writeData( wxT("\" tick=\"")              + to_wxString(m_tick)                 , fileout );
    writeData( wxT("\" value=\"")             + to_wxString(m_value) + wxT("\"/>\n"), fileout );

}

// ----------------------------------------------------------------------------------------------------------

bool ControllerEvent::readFromFile(irr::io::IrrXMLReader* xml)
{
    // ---- read "type"
    const char* type = xml->getAttributeValue("type");
    if (type != NULL)
    {
        m_controller = atoi(type);
    }
    else
    {
        m_controller = 0;
        std::cout << "Missing info from file: controller type" << std::endl;
        return false;
    }

    // ---- read "tick"
    const char* tick_c = xml->getAttributeValue("tick");
    if (tick_c != NULL)
    {
        m_tick = atoi(tick_c);
    }
    else
    {
        m_tick = 0;
        std::cout << "Missing info from file: controller tick" << std::endl;
        return false;
    }

    // ---- read "value"
    const char* value_c = xml->getAttributeValue("value");
    if (value_c != NULL)
    {
        m_value = atof(value_c);
    }
    else
    {
        m_value = 0;
        std::cout << "Missing info from file: controller value" << std::endl;
        return false;
    }

    return true;
}

// ----------------------------------------------------------------------------------------------------------

void TextEvent::saveToFile(wxFileOutputStream& fileout)
{
    writeData( wxT("  <controlevent type=\"") + to_wxString(m_controller) , fileout );
    writeData( wxT("\" tick=\"")              + to_wxString(m_tick)       , fileout );
    
    wxString val = m_text.getModel()->getValue();
    val.Replace( wxT("\r\n"), wxT("\n") );
    val.Replace( wxT("\n"), wxT("&#xD;") );
    val.Replace( wxT("\r"), wxT("&#xD;") );
    writeData( wxString(wxT("\" value=\""))   + val + wxT("\"/>\n")       , fileout );
}

// ----------------------------------------------------------------------------------------------------------

bool TextEvent::readFromFile(irr::io::IrrXMLReader* xml)
{
    // ---- read "type"
    const char* type = xml->getAttributeValue("type");
    if (type != NULL)
    {
        m_controller = atoi(type);
    }
    else
    {
        m_controller = 0;
        std::cout << "Missing info from file: text event type" << std::endl;
        return false;
    }
    
    // ---- read "tick"
    const char* tick_c = xml->getAttributeValue("tick");
    if (tick_c != NULL)
    {
        m_tick = atoi(tick_c);
    }
    else
    {
        m_tick = 0;
        std::cout << "Missing info from file: text event tick" << std::endl;
        return false;
    }
    
    // ---- read "value"
    const char* value_c = xml->getAttributeValue("value");
    if (value_c != NULL)
    {
        m_text.getModel()->setValue(wxString(value_c, wxConvUTF8));
    }
    else
    {
        std::cout << "Missing info from file: text event value" << std::endl;
        return false;
    }
    
    return true;
}

// ----------------------------------------------------------------------------------------------------------