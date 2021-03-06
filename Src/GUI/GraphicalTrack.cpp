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


#include <iostream>
#include <wx/numdlg.h>
#include <wx/wfstream.h>
#include <wx/textdlg.h>

#include "Utils.h"
#include "GUI/GraphicalTrack.h"

#include "AriaCore.h"
#include "Actions/SetAccidentalSign.h"
#include "Editors/KeyboardEditor.h"
#include "Editors/Editor.h"
#include "Editors/GuitarEditor.h"
#include "Editors/DrumEditor.h"
#include "Editors/RelativeXCoord.h"
#include "Editors/ControllerEditor.h"
#include "Editors/ScoreEditor.h"
#include "GUI/ImageProvider.h"
#include "GUI/MainFrame.h"
#include "GUI/MainPane.h"
#include "IO/IOUtils.h"
#include "Midi/DrumChoice.h"
#include "Midi/InstrumentChoice.h"
#include "Midi/MeasureData.h"
#include "Midi/Sequence.h"
#include "Midi/Track.h"
#include "Midi/Players/PlatformMidiManager.h"
#include "Pickers/DrumPicker.h"
#include "Pickers/InstrumentPicker.h"
#include "Pickers/MagneticGridPicker.h"
#include "Pickers/VolumeSlider.h"
#include "PreferencesData.h"
#include "Renderers/Drawable.h"
#include "Renderers/ImageBase.h"
#include "Renderers/RenderAPI.h"

#include "irrXML/irrXML.h"

using namespace AriaMaestosa;

const int THUMB_SIZE_ABOVE = 3;
const int THUMB_SIZE_BELOW = 1;
const int TRACK_VOLUME_LIMIT_1 = 33;
const int TRACK_VOLUME_LIMIT_2 = 66;

namespace AriaMaestosa
{
    class AriaWidget
    {
    protected:
        int m_x, m_y;
        int m_width;
        bool m_hidden;
        wxString m_tooltip;

    public:
        LEAK_CHECK();
        
        AriaWidget(int width)
        {
            m_x = 0;
            m_y = 0;
            m_width = width;
            m_hidden = false;
        }
        
        void setTooltip(wxString tooltip) { m_tooltip = tooltip; }
        const wxString getTooltip() const { return m_tooltip; }
        
        
        virtual const ptr_vector<BitmapButton, HOLD>& getChildren() const
        {        
            static ptr_vector<BitmapButton, HOLD> empty_children_vector;
            return empty_children_vector;
        }
        
        int getX() const { return m_x; }
        int getY() const { return m_y; }
        int getWidth() const { return m_width; }
        
        bool isHidden() const { return m_hidden; }
        void show(bool shown){ m_hidden = not shown; }
        
        // don't call this, let WidgetLayoutManager do it
        void setX(const int x){ m_x = x; }
        void setY(const int y){ m_y = y; }
        
        bool clickIsOnThisWidget(const int mx, const int my) const
        {
            return (not m_hidden) and ( mx > m_x and my > m_y and mx < m_x + m_width and my < m_y + 30);
        }
        
        virtual void render(){}
        virtual ~AriaWidget(){}
    };
    
    // --------------------------------------------------------------------------------------------------
    
    class BlankField : public AriaWidget
    {
    public:
        BlankField(int width) : AriaWidget(width){}
        virtual ~BlankField(){}
        
        void render()
        {
            if (m_hidden) return;
            comboBorderDrawable->move(m_x, m_y + 7);
            comboBorderDrawable->setFlip(false, false);
            comboBorderDrawable->render();
            
            comboBodyDrawable->move(m_x + 14, m_y + 7);
            comboBodyDrawable->scale((m_width - 28)/4.0 , 1);
            comboBodyDrawable->render();
            
            comboBorderDrawable->move(m_x + m_width - 14, m_y + 7 );
            comboBorderDrawable->setFlip(true,false);
            comboBorderDrawable->render();
        }
		
		int getUsableWidth() const
		{
			// some of the corner is usable for contents too so don't count  fully
			return m_width - 1.5f*comboBorderDrawable->getImageWidth();
		}
    };
    
    // --------------------------------------------------------------------------------------------------
    
    class ComboBox : public AriaWidget
    {
    public:
        ComboBox(int width) : AriaWidget(width){}
        virtual ~ComboBox(){}
        
        void render()
        {
            if (m_hidden) return;
            comboBorderDrawable->move(m_x, m_y + 7);
            comboBorderDrawable->setFlip(false, false);
            comboBorderDrawable->render();
            
            comboBodyDrawable->move(m_x + 14, m_y + 7);
            comboBodyDrawable->scale((m_width - 28 - 18)/4.0, 1);
            comboBodyDrawable->render();
            
            comboSelectDrawable->move(m_x + m_width - 14 - 18, m_y + 7);
            comboSelectDrawable->render();
        }
    };
    
    // --------------------------------------------------------------------------------------------------
    
    class BitmapButton : public AriaWidget
    {
        int m_y_offset;
        bool m_enabled;
        bool m_center_x;
        AriaRender::ImageState m_state;
        
    public:
        Drawable* m_drawable;
        
        BitmapButton(int width, int y_offset, Drawable* drawable, bool centerX=false) : AriaWidget(width)
        {
            m_drawable = drawable;
            m_y_offset = y_offset;
            m_enabled = true;

            m_center_x = centerX;
            m_state = AriaRender::STATE_NORMAL;
        }
        virtual ~BitmapButton(){}
        
        BitmapButton* setImageState(AriaRender::ImageState state)
        {
            m_state = state;
            return this;
        }
        
        void render()
        {
            if (m_hidden) return;
            
            if (m_state != AriaRender::STATE_NORMAL)
            {
                AriaRender::setImageState(m_state);
            }
            else if (not m_enabled)
            {
                AriaRender::setImageState(AriaRender::STATE_DISABLED);
            }
            
            if (m_center_x and m_drawable->getImageWidth() < m_width)
            {
                const int ajust = (m_width - m_drawable->getImageWidth())/2;
                m_drawable->move(m_x + m_drawable->getHotspotX() + ajust, m_y + m_y_offset);
            }
            else
            {
                m_drawable->move(m_x + m_drawable->getHotspotX(), m_y + m_y_offset);
            }
            
            m_drawable->render();
        }
        
        void enable(const bool enabled)
        {
            m_enabled = enabled;
        }
    };
    
    // --------------------------------------------------------------------------------------------------
    
    template<typename PARENT>
    class ToolBar : public PARENT
    {
        ptr_vector<BitmapButton, HOLD> m_contents;
        std::vector<int> m_margin;
        
    public:
        ToolBar() : PARENT(22)
        {
        }
        void addItem(BitmapButton* btn, int margin_after)
        {
            m_contents.push_back(btn);
            m_margin.push_back(margin_after);
        }
        
        virtual const ptr_vector<BitmapButton, HOLD>& getChildren() const { return m_contents; }
        
        void layout()
        {
            if (PARENT::m_hidden) return;
            
            PARENT::m_width = 22;
            int currentX = PARENT::m_x + 11;
            
            const int amount = m_contents.size();
            for (int n=0; n<amount; n++)
            {
                m_contents[n].setX(currentX);
                m_contents[n].setY(PARENT::m_y);
                
                currentX        += m_contents[n].getWidth() + m_margin[n];
                PARENT::m_width += m_contents[n].getWidth() + m_margin[n];
            }
        }
        
        BitmapButton& getItem(const int item)
        {
            return m_contents[item];
        }
        
        void render()
        {
            if (PARENT::m_hidden) return;
            
            // render background
            PARENT::render();
            
            // render buttons
            const int amount = m_contents.size();
            
            for (int n=0; n<amount; n++)
            {
                m_contents[n].render();
            }
            AriaRender::setImageState(AriaRender::STATE_NORMAL);
        }
    };
    
    // --------------------------------------------------------------------------------------------------
    
    class WidgetLayoutManager
    {
        ptr_vector<AriaWidget, HOLD> m_widgets_left;
        ptr_vector<AriaWidget, HOLD> m_widgets_right;
        
    public:
        LEAK_CHECK();
        
        WidgetLayoutManager()
        {
        }
        
        const ptr_vector<AriaWidget, HOLD>& getLeftWidgets() const { return m_widgets_left; }
        const ptr_vector<AriaWidget, HOLD>& getRightWidgets() const { return m_widgets_right; }
        
        void addFromLeft(AriaWidget* w)
        {
            m_widgets_left.push_back(w);
        }
        void addFromRight(AriaWidget* w)
        {
            m_widgets_right.push_back(w);
        }
        void layout(const int x_origin, const int y_origin)
        {
            const int lamount = m_widgets_left.size();
            int lx = x_origin;
            for (int n=0; n<lamount; n++)
            {
                m_widgets_left[n].setX(lx);
                m_widgets_left[n].setY(y_origin);
                lx += m_widgets_left[n].getWidth();
            }
            
            const int ramount = m_widgets_right.size();
            int rx = Display::getWidth() - 17;
            
            for (int n=0; n<ramount; n++)
            {
                rx -= m_widgets_right[n].getWidth();
                m_widgets_right[n].setX(rx);
                m_widgets_right[n].setY(y_origin);
            }
        }
        
        void renderAll(bool focus)
        {
            AriaRender::images();
            
            const int lamount = m_widgets_left.size();
            for (int n=0; n<lamount; n++)
            {
                if (not focus) AriaRender::setImageState(AriaRender::STATE_NO_FOCUS);
                else           AriaRender::setImageState(AriaRender::STATE_NORMAL);
                
                m_widgets_left.get(n)->render();
            }
            
            const int ramount = m_widgets_right.size();
            for (int n=0; n<ramount; n++)
            {
                if (not focus) AriaRender::setImageState(AriaRender::STATE_NO_FOCUS);
                else           AriaRender::setImageState(AriaRender::STATE_NORMAL);
                
                m_widgets_right.get(n)->render();
            }
        }
    };
}

#if 0
#pragma mark -
#pragma mark GraphicalTrack (ctor & dtor)
#endif
    
// ----------------------------------------------------------------------------------------------------------

/** This is the height *in addition to* the border, which is of a static size */
const int EXPANDED_BAR_HEIGHT = 20;
const int COLLAPSED_BAR_HEIGHT = 5;

const int EDITOR_ICON_SIZE = 30;
const int TRACK_MIN_SIZE = 35;

GraphicalTrack::GraphicalTrack(Track* track, GraphicalSequence* seq, MagneticGrid* magneticGrid) :
    m_instrument_name( m_instrument_string = new Model<wxString>(wxT("")), false ),
    m_name_renderer(track->getNameModel(), false)
{
    m_keyboard_editor     = NULL;
    m_guitar_editor       = NULL;
    m_drum_editor         = NULL;
    m_controller_editor   = NULL;
    m_score_editor        = NULL;
    m_resizing_subeditor  = NULL;
    
    m_gsequence = seq;
    m_track = track;
    m_focused_editor = KEYBOARD;
    
    m_name_renderer.setMaxWidth(120);
    m_name_renderer.setFont( getTrackNameFont() );
    
    
    track->setListener(this);
    track->setInstrumentListener(this);
    track->setDrumListener(this);
    
    ASSERT(track);
    
    m_grid = new MagneticGridPicker(this, magneticGrid);
    
    m_last_mouse_y = 0;
    
    m_collapsed       = false;
    m_dragging_resize = false;
    m_docked          = false;
    
    m_height = 128;
    
    // create widgets
    m_components = new WidgetLayoutManager();
    
    m_collapse_button = new BitmapButton(26, 15, collapseDrawable);
    m_components->addFromLeft(m_collapse_button);
    
    m_volume_button = new BitmapButton(32, 10, volumeDrawable);
    m_volume_button->setTooltip( wxString(_("Track volume")) );
    m_components->addFromLeft(m_volume_button);
    
    m_mute_button = new BitmapButton(24, 16, muteDrawable);
    m_mute_button->setTooltip( wxString(_("Mute")) );
    m_components->addFromLeft(m_mute_button);
    
    m_solo_button = new BitmapButton(24, 16, soloDrawable);
    m_solo_button->setTooltip( wxString(_("Solo")) );
    m_components->addFromLeft(m_solo_button);
    
    m_dock_toolbar = new ToolBar<BlankField>();
    BitmapButton* maximize;
    m_dock_toolbar->addItem( maximize = new BitmapButton( 16, 14, maximizeTrackDrawable, false), 0 );
    maximize->setTooltip(wxString(_("Maximize track")));
    
    BitmapButton* dock;
    m_dock_toolbar->addItem( dock = new BitmapButton( 16, 14, dockTrackDrawable, false), 0 );
    dock->setTooltip(wxString(_("Dock track")));
    m_components->addFromLeft(m_dock_toolbar);
    m_dock_toolbar->layout();
    
    m_track_name = new BlankField(175);
    m_components->addFromLeft(m_track_name);
    
    m_grid_combo = new ToolBar<ComboBox>();
    m_grid_combo->addItem( new BitmapButton( 16, 14, mgrid_1, true ), 0 );
    m_grid_combo->addItem( new BitmapButton( 16, 14, mgrid_2, true ), 0 );
    m_grid_combo->addItem( new BitmapButton( 16, 14, mgrid_4, true ), 0 );
    m_grid_combo->addItem( new BitmapButton( 16, 14, mgrid_8, true ), 0 );
    m_grid_combo->addItem( new BitmapButton( 16, 14, mgrid_16, true ), 0 );
    m_grid_combo->addItem( new BitmapButton( 16, 14, mgrid_32, true ), 10 );
    m_grid_combo->addItem( new BitmapButton( 16, 14, mgrid_triplet, true ), 0 );
    m_grid_combo->addItem( new BitmapButton( 16, 14, mgrid_dotted, true ), 25 );

    m_components->addFromLeft(m_grid_combo);
    m_grid_combo->layout();
    
    m_score_button = new BitmapButton(32, 7, score_view);
    m_components->addFromLeft(m_score_button);
    m_score_button->setTooltip(wxString(_("Score Editor")));
    
    m_piano_button = new BitmapButton(32, 7, keyboard_view);
    m_components->addFromLeft(m_piano_button);
    m_piano_button->setTooltip(wxString(_("Keyboard Editor")));
    
    m_tab_button = new BitmapButton(32, 7, guitar_view);
    m_components->addFromLeft(m_tab_button);
    m_tab_button->setTooltip(wxString(_("Tablature Editor")));
    
    m_drum_button = new BitmapButton(38, 7, drum_view);
    m_components->addFromLeft(m_drum_button);
    m_drum_button->setTooltip(wxString(_("Drum Editor")));
    
    m_ctrl_button = new BitmapButton(32, 7, controller_view);
    m_components->addFromLeft(m_ctrl_button);
    m_ctrl_button->setTooltip(wxString(_("Controller Editor")));
    
    m_sharp_flat_picker = new ToolBar<BlankField>();
    m_sharp_flat_picker->addItem( (new BitmapButton( 14, 21, sharpSign,   true ))->setImageState(AriaRender::STATE_NOTE), 6 );
    m_sharp_flat_picker->addItem( (new BitmapButton( 14, 24, flatSign,    true ))->setImageState(AriaRender::STATE_NOTE), 6 );
    m_sharp_flat_picker->addItem( (new BitmapButton( 14, 21, naturalSign, true ))->setImageState(AriaRender::STATE_NOTE), 0 );
    m_components->addFromLeft(m_sharp_flat_picker);
    
    m_instrument_field = new BlankField(195);
    m_components->addFromRight(m_instrument_field);
	m_instrument_name.setMaxWidth( m_instrument_field->getUsableWidth() );
    
    m_channel_field = new BlankField(28);
    m_components->addFromRight(m_channel_field);
    
    if (m_track->isNotationTypeEnabled(DRUM))
    {
        m_instrument_string->setValue(DrumChoice::getDrumkitName( m_track->getDrumKit() ));
    }
    else
    {
        m_instrument_string->setValue(getInstrumentName(m_track->getInstrument()));
    }
    
    m_instrument_name.setFont( getInstrumentNameFont() );
}

// ----------------------------------------------------------------------------------------------------------
    
GraphicalTrack::~GraphicalTrack()
{
}

// ----------------------------------------------------------------------------------------------------------
    
void GraphicalTrack::createEditors()
{
    ASSERT(m_all_editors.size() == 0); // function to be called once per object only
    
    m_keyboard_editor = new KeyboardEditor(this);
    m_keyboard_editor->setRelativeHeight(1.0f);
    m_all_editors.push_back(m_keyboard_editor);
    
    m_guitar_editor = new GuitarEditor(this);
    m_guitar_editor->setRelativeHeight(1.0f);
    m_all_editors.push_back(m_guitar_editor);
    
    m_drum_editor = new DrumEditor(this);
    m_drum_editor->setRelativeHeight(1.0f);
    m_all_editors.push_back(m_drum_editor);
    
    m_controller_editor = new ControllerEditor(this);
    m_controller_editor->setRelativeHeight(1.0f);
    m_all_editors.push_back(m_controller_editor);
    
    m_score_editor = new ScoreEditor(this);
    m_score_editor->setRelativeHeight(1.0f);
    m_all_editors.push_back(m_score_editor);
}

// ----------------------------------------------------------------------------------------------------------
    
#if 0
#pragma mark -
#pragma mark Events
#endif

bool GraphicalTrack::mouseWheelMoved(int mx, int my, int value)
{
    if (my > m_from_y and my < m_to_y)
    {
        Editor* ed = getEditorAt(my);
        if (ed != NULL)
        {
            ed->scroll( value / 75.0 );
            Display::render();
        }
        return false; // event belongs to this track and was processed, stop searching
    }
    else
    {
        return true; // event does not belong to this track, continue searching
    }
}

// ----------------------------------------------------------------------------------------------------------

bool GraphicalTrack::handleEditorChanges(int x, BitmapButton* button, Editor* editor, NotationType type)
{
    if (x > button->getX() and x < button->getX() + EDITOR_ICON_SIZE)
    {
        if (not Display::isSelectMorePressed())
        {
            if (type != SCORE)      m_track->setNotationType(SCORE, false);
            if (type != GUITAR)     m_track->setNotationType(GUITAR, false);
            if (type != KEYBOARD)   m_track->setNotationType(KEYBOARD, false);
            if (type != DRUM)       m_track->setNotationType(DRUM, false);
            if (type != CONTROLLER) m_track->setNotationType(CONTROLLER, false);
            
            m_track->setNotationType(type, true);
            evenlyDistributeSpace();
        }
        else
        {
            if (not m_track->isNotationTypeEnabled(type))
            {
                const int DEFAULT_SIZE = 150;
                if (not m_gsequence->isTrackMaximized()) m_height += DEFAULT_SIZE;
                const float relativeHeight = float(DEFAULT_SIZE)/float(m_height);
                
                //int count = m_track->getEnabledEditorCount();
                //const float subtractToOthers = relativeHeight / float(count);
                
                for (int n=0; n<NOTATION_TYPE_COUNT; n++)
                {
                    if (n == type)
                    {
                        getEditorFor( (NotationType)n )->setRelativeHeight( relativeHeight );
                    }
                    else if (m_track->isNotationTypeEnabled( (NotationType)n ))
                    {
                        Editor* otherEditor = getEditorFor( (NotationType)n );
                        float curr = otherEditor->getRelativeHeight();
                        otherEditor->setRelativeHeight( curr - curr*relativeHeight );
                    }
                    
                }
            }
            else
            {
                // can't disable the last shown ditor
                if (m_track->getEnabledEditorCount() <= 1) return true;
                
                if (not m_gsequence->isTrackMaximized()) m_height -= m_height*editor->getRelativeHeight();
                float toRemove = getEditorFor( type )->getRelativeHeight();
                
                for (int n=0; n<NOTATION_TYPE_COUNT; n++)
                {
                    if (n != type and m_track->isNotationTypeEnabled( (NotationType)n ))
                    {
                        Editor* otherEditor = getEditorFor( (NotationType)n );
                        float curr = otherEditor->getRelativeHeight();
                        otherEditor->setRelativeHeight( curr/(1.0f - toRemove) );
                    }
                }
            }
            
            m_track->setNotationType(type, not m_track->isNotationTypeEnabled(type));
            DisplayFrame::updateVerticalScrollbar();
        }
        return true;
    }
    return false;
}


wxString GraphicalTrack::getInstrumentName(int instId)
{
    return InstrumentChoice::getInstrumentName(instId);
}



// ----------------------------------------------------------------------------------------------------------

bool GraphicalTrack::processMouseDown(RelativeXCoord mousex, int mousey)
{
    m_dragging_resize = false;
    
    m_last_mouse_y = mousey;
    
    if (mousey > m_from_y and mousey < m_to_y)
    {
        m_gsequence->getModel()->setCurrentTrack( m_track );

        if (not m_collapsed)
        {
            // resize drag
            if (mousey > m_to_y - 10 and mousey < m_to_y and not m_gsequence->isTrackMaximized())
            {
                m_dragging_resize = true;
                
                return false;
            }
            
            // if track is not collapsed, let the editor handle the mouse event too
            Editor* ed = getEditorAt(mousey, &m_next_to_resizing_subeditor);
            if (ed != NULL)
            {
                if (mousey >= ed->getYEnd() - THUMB_SIZE_ABOVE)
                {
                    m_resizing_subeditor = ed;
                }
                //else if (mousey <= ed->getTrackYStart() + THUMB_SIZE_BELOW)
                //{
                //    m_resizing_subeditor = ed;
                //}
                else
                {
                    ed->mouseDown(mousex, mousey);
                }
            }
        }

        if (not ImageProvider::imagesLoaded()) return true;
        
        const int winX = mousex.getRelativeTo(WINDOW);
        
        // collapse
        if (m_collapse_button->clickIsOnThisWidget(winX, mousey))
        {
            m_collapsed = not m_collapsed;
            DisplayFrame::updateVerticalScrollbar();
        }

        // maximize button
        if (m_dock_toolbar->getItem(0).clickIsOnThisWidget(winX, mousey))
        {
            Sequence* seq = m_gsequence->getModel();

            if (not m_gsequence->isTrackMaximized())
            {
                // switch on maximize mode
                const int track_amount = seq->getTrackAmount();
                for (int n=0; n<track_amount; n++)
                {
                    Track* track = seq->getTrack(n);
                    GraphicalTrack* gtrack = m_gsequence->getGraphicsFor(track);
                    if (gtrack != this)
                    {
                        gtrack->dock();
                        m_gsequence->setDockVisible(true);
                    }
                }
                for (int n=0; n<track_amount; n++)
                {
                    Track* track = seq->getTrack(n);
                    GraphicalTrack* gtrack = m_gsequence->getGraphicsFor(track);
                    if (gtrack == this)
                    {
                        maximizeHeight();
                        seq->setCurrentTrack(track);
                        break;
                    }
                }
                m_gsequence->setYScroll(0);
                DisplayFrame::updateVerticalScrollbar();
                m_gsequence->setTrackMaximized(true);
            }
            else
            {
                // switch off maximize mode.
                const int track_amount = seq->getTrackAmount();
                for (int n=0; n<track_amount; n++)
                {
                    Track* track = seq->getTrack(n);
                    GraphicalTrack* gtrack = m_gsequence->getGraphicsFor(track);
                    
                    if (gtrack->isDocked()) gtrack->dock(false);
                    
                    gtrack->maximizeHeight(false);
                }
                DisplayFrame::updateVerticalScrollbar();
                m_gsequence->setTrackMaximized(false);
            }
        }
        // dock button
        else if ( m_dock_toolbar->getItem(1).clickIsOnThisWidget(winX, mousey) )
        {
            // This button is disabled in maximized mode
            if (not m_gsequence->isTrackMaximized())
            {
                dock();
                DisplayFrame::updateVerticalScrollbar();
            }
        }
        
        
        // volume
        if (m_volume_button->clickIsOnThisWidget(winX, mousey))
        {
            int screen_x, screen_y;
            
            Display::clientToScreen(mousex.getRelativeTo(WINDOW),mousey, &screen_x, &screen_y);
            showVolumeSlider(screen_x, screen_y, m_track);
        }

        // mute
        if (m_mute_button->clickIsOnThisWidget(winX, mousey))
        {
            m_track->toggleMuted();
            DisplayFrame::updateVerticalScrollbar();
        }
        
        // solo
        if (m_solo_button->clickIsOnThisWidget(winX, mousey))
        {
            m_track->toggleSoloed();
            DisplayFrame::updateVerticalScrollbar();
        }

        // track name
        if (m_track_name->clickIsOnThisWidget(winX, mousey))
        {
            wxString msg = wxGetTextFromUser(_("Choose a new track title."), wxT("Aria Maestosa"),
                                             m_track->getName() );
            if (msg.Length() > 0) m_track->setName( msg );
            Display::render();
        }

        // grid
        if (m_grid_combo->clickIsOnThisWidget(winX, mousey))
        {
            wxCommandEvent fake_event;

            if ( m_grid_combo->getItem(0).clickIsOnThisWidget(winX, mousey) )
                m_grid->grid1selected(fake_event);
            else if ( m_grid_combo->getItem(1).clickIsOnThisWidget(winX, mousey) )
                m_grid->grid2selected(fake_event);
            else if ( m_grid_combo->getItem(2).clickIsOnThisWidget(winX, mousey) )
                m_grid->grid4selected(fake_event);
            else if ( m_grid_combo->getItem(3).clickIsOnThisWidget(winX, mousey) )
                m_grid->grid8selected(fake_event);
            else if ( m_grid_combo->getItem(4).clickIsOnThisWidget(winX, mousey) )
                m_grid->grid16selected(fake_event);
            else if ( m_grid_combo->getItem(5).clickIsOnThisWidget(winX, mousey) )
                m_grid->grid32selected(fake_event);
            else if ( m_grid_combo->getItem(6).clickIsOnThisWidget(winX, mousey) )
                m_grid->toggleTriplet();
            else if ( m_grid_combo->getItem(7).clickIsOnThisWidget(winX, mousey) )
                m_grid->toggleDotted();
            else if ( winX > m_grid_combo->getItem(7).getX() + 16)
            {
                m_grid->syncWithModel();
                Display::popupMenu(m_grid, m_grid_combo->getX() + 5, m_from_y + 30);
            }
        }


        // instrument
        if (m_instrument_field->clickIsOnThisWidget(winX, mousey))
        {
            if (m_track->isNotationTypeEnabled(DRUM))
            {
                Core::getDrumPicker()->setModel(m_track->getDrumkitModel());
                Display::popupMenu((wxMenu*)(Core::getDrumPicker()), Display::getWidth() - 175, m_from_y + 30);
            }
            else
            {
                Core::getInstrumentPicker()->setModel(m_track->getInstrumentModel());
                Display::popupMenu((wxMenu*)(Core::getInstrumentPicker()),
                                   Display::getWidth() - 175, m_from_y + 30);
            }
        }

        // channel
        if (m_gsequence->getModel()->getChannelManagementType() == CHANNEL_MANUAL)
        {

            if (m_channel_field->clickIsOnThisWidget(winX, mousey))
            {
                const int channel = wxGetNumberFromUser( _("Enter the ID of the channel this track should play in"),
                                                         wxT(""),
                                                         _("Channel choice"),
                                                         m_track->getChannel(),
                                                         0,
                                                         15 );
                if (channel >= 0 and channel <= 15)
                {
                    m_track->setChannel(channel);
                    Display::render();
                }
            }
        }


        if (mousey > m_from_y + 10 and mousey < m_from_y + 10 + EDITOR_ICON_SIZE)
        {
            // FIXME: setting drums to channel 9 will probably fail if you're trying to enable multiple editors
            
            // modes
            if (handleEditorChanges(winX, m_score_button, m_score_editor, SCORE))
            {
            }
            else if (handleEditorChanges(winX, m_piano_button, m_keyboard_editor, KEYBOARD))
            {
            }
            else if (handleEditorChanges(winX, m_tab_button, m_guitar_editor, GUITAR))
            {
            }
            else if (handleEditorChanges(winX, m_drum_button, m_drum_editor, DRUM))
            {
                // in midi, drums go to channel 9 (10 if you start counting from one)
                if (m_track->isNotationTypeEnabled(DRUM) and
                    m_gsequence->getModel()->getChannelManagementType() == CHANNEL_MANUAL)
                {
                    m_track->setChannel(9);
                }
            }
            else if (winX > m_ctrl_button->getX() and winX < m_ctrl_button->getX() + EDITOR_ICON_SIZE)
            {
                if (not m_track->isNotationTypeEnabled(CONTROLLER))
                {
                    if (not m_gsequence->isTrackMaximized()) m_height += 150;
                    m_track->setNotationType(CONTROLLER, true);
                }
                else
                {
                    if (not m_gsequence->isTrackMaximized())
                    {
                        m_height -= m_height*m_controller_editor->getRelativeHeight();
                    }
                    m_track->setNotationType(CONTROLLER, false);
                }
                
                DisplayFrame::updateVerticalScrollbar();
                evenlyDistributeSpace();
            }
            
            // in midi, drums go to channel 9. So, if we exit drums, change channel so that it's not 9 anymore.
            if (not m_track->isNotationTypeEnabled(DRUM) and
                m_gsequence->getModel()->getChannelManagementType() == CHANNEL_MANUAL and
                m_track->getChannel() == 9)
            {
                // FIXME: ensure all channels have the same instrument
                m_track->setChannel(0);
            }
        }
        
        if (m_track->isNotationTypeEnabled(SCORE) and mousey > m_from_y + 15 and mousey < m_from_y + 30)
        {
            // sharp/flat signs
            if ( m_sharp_flat_picker->getItem(0).clickIsOnThisWidget(winX, mousey) )
            {
                m_track->action( new Action::SetAccidentalSign(SHARP) );
            }
            else if ( m_sharp_flat_picker->getItem(1).clickIsOnThisWidget(winX, mousey) )
            {
                m_track->action( new Action::SetAccidentalSign(FLAT) );
            }
            else if ( m_sharp_flat_picker->getItem(2).clickIsOnThisWidget(winX, mousey) )
            {
                m_track->action( new Action::SetAccidentalSign(NATURAL) );
            }
        }

        return false;
    }
    else
    {
        return true;
    }
}

// ----------------------------------------------------------------------------------------------------------

bool GraphicalTrack::processRightMouseClick(RelativeXCoord x, int y)
{
    if (y > m_from_y and y < m_to_y)
    {
        Editor* ed = getEditorAt(y);
        if (ed != NULL) ed->rightClick(x,y);
        return false;
    }
    else
    {
        return true;
    }

}

// ----------------------------------------------------------------------------------------------------------

void GraphicalTrack::processMouseRelease()
{
    //std::cout << "mouse up GraphicalTrack" << std::endl;

    m_resizing_subeditor = NULL;
    
    if (not m_dragging_resize)
    {
        Editor* ed = getEditorAt(Display::getMouseY_initial());
        if (ed != NULL)
        {
            ed->mouseUp(Display::getMouseX_current(),
                        Display::getMouseY_current(),
                        Display::getMouseX_initial(),
                        Display::getMouseY_initial());
        }
    }
    
    if (m_dragging_resize)
    {
        m_dragging_resize = false;
        DisplayFrame::updateVerticalScrollbar();
    }
}

// ----------------------------------------------------------------------------------------------------------

void GraphicalTrack::processMouseExited(RelativeXCoord x_now, int y_now,
                                        RelativeXCoord x_initial, int y_initial)
{
    m_resizing_subeditor = NULL;
    
    Editor* ed = getEditorAt(y_initial);
    if (ed == NULL) return;
    ed->mouseExited(x_now, y_now, x_initial, y_initial);
}

// ----------------------------------------------------------------------------------------------------------

bool GraphicalTrack::processMouseDrag(RelativeXCoord x, int y)
{
    if (m_resizing_subeditor != NULL)
    {
        int editor_from_y = getEditorFromY();

        float delta = (y - m_last_mouse_y)/float(m_to_y - editor_from_y);
        
        //printf("(y - m_last_mouse_y) = %i / out of = %i // delta = %f\n", (y - m_last_mouse_y), (m_to_y - editor_from_y), delta);
        float newh = m_resizing_subeditor->getRelativeHeight() + delta;
        //printf("    Top editor : %f\n", newh);
        
        
        float new_next_h = -1.0f;
        Editor* next = m_next_to_resizing_subeditor;
        if (next != NULL)
        {
            new_next_h = next->getRelativeHeight() - delta;
        }
        
        if (newh >= 0.1f and newh <= 0.9f and ((new_next_h >= 0.1f and new_next_h <= 0.9f) or next == NULL))
        {
            m_resizing_subeditor->setRelativeHeight( newh );
            
            if (next != NULL)
            {
                //printf("    Bottom editor : %f\n", next->getRelativeHeight() - delta);
                next->setRelativeHeight(new_next_h);
            }
            
            Display::render();
        }
        m_last_mouse_y = y;

        return false;
    }
    
    if ((y > m_from_y and y < m_to_y) or m_dragging_resize)
    {
        // until the end of the method, mousex_current/mousey_current contain the location of the mouse last time
        // this event was thrown in the dragging process. This can be used to determine the movement of the mouse.
        // At the end of the method, mousex_current/mousey_current are set to the current values.

        //int barHeight = EXPANDED_BAR_HEIGHT;
        //if (m_collapsed) barHeight = COLLAPSED_BAR_HEIGHT;

        if (not m_dragging_resize)
        {
            Editor* ed = getEditorAt(Display::getMouseY_initial());
            if (ed != NULL)
            {
                ed->mouseDrag(x, y,
                              Display::getMouseX_initial(),
                              Display::getMouseY_initial());
            }
        }
        
        // resize drag
        if (m_dragging_resize)
        {

            if (m_height == TRACK_MIN_SIZE)
            { 
                // if it has reached minimal size, wait until mouse comes back over before resizing again
                if (y > m_to_y - 15 and y < m_to_y - 5 and (y - m_last_mouse_y) > 0)
                {
                    m_height += (y - m_last_mouse_y);
                }
            }
            else
            {
                // resize the track and check if it's not too small
                m_height += (y - m_last_mouse_y);
                if (m_height < TRACK_MIN_SIZE) m_height = TRACK_MIN_SIZE; // enforce minimum size
            }
            
            DisplayFrame::updateVerticalScrollbar();
        }

        m_last_mouse_y = y;

        return false;
    }
    else
    {
        return true;
    }
}

// ----------------------------------------------------------------------------------------------------------


wxString GraphicalTrack::processMouseMove(RelativeXCoord x, int y)
{
    Editor* ed = getEditorAt(y);
    if (ed != NULL)
    {
        ed->processMouseMove(x, y);
        for (int n=0; n<m_all_editors.size(); n++)
        {
            if (m_all_editors.get(n) != ed)
            {
                m_all_editors[n].processMouseOutsideOfMe();
            }
        }
    }
    else if (not PlatformMidiManager::get()->isPlaying())
    {
        getMainFrame()->setStatusText(wxT(""));
    }
    
    
    // Find if there is a widget under the mouse with a tooltip
    const int winX = x.getRelativeTo(WINDOW);
    
    const ptr_vector<AriaWidget, HOLD>& left = m_components->getLeftWidgets();
    for (int wId = 0; wId < left.size(); wId++)
    {
        if (left[wId].clickIsOnThisWidget(winX, y))
        {
            const ptr_vector<BitmapButton, HOLD>& children = left[wId].getChildren();
            for (int chId = 0; chId < children.size(); chId++)
            {
                if (children[chId].clickIsOnThisWidget(winX, y))
                {
                    return children[chId].getTooltip();
                }
            }
        
            return left[wId].getTooltip();
        }
        
    }
    
    const ptr_vector<AriaWidget, HOLD>& right = m_components->getRightWidgets();
    for (int wId = 0; wId < right.size(); wId++)
    {
        if (right[wId].clickIsOnThisWidget(winX, y))
        {
            const ptr_vector<BitmapButton, HOLD>& children = right[wId].getChildren();
            for (int chId = 0; chId < children.size(); chId++)
            {
                if (children[chId].clickIsOnThisWidget(winX, y))
                {
                    return children[chId].getTooltip();
                }
            }
            
            return right[wId].getTooltip();
        }
    }
    
    return wxString(wxT(""));
}

// ----------------------------------------------------------------------------------------------------------

void GraphicalTrack::processMouseOutsideOfMe()
{
    for (int n=0; n<m_all_editors.size(); n++)
    {
        m_all_editors[n].processMouseOutsideOfMe();
    }
}

// ----------------------------------------------------------------------------------------------------------

void GraphicalTrack::onTrackRemoved(Track* track)
{
    m_keyboard_editor->trackDeleted(track);
    
    // uncomment if these editors get background support too
    // m_guitar_editor->trackDelete(track);
    // m_drum_editor->trackDelete(track);
    // m_controller_editor->trackDelete(track);
    // m_score_editor->trackDelete(track);
}

// ----------------------------------------------------------------------------------------------------------

void GraphicalTrack::onKeyChange(const int symbolAmount, const KeyType type)
{
    const int count = m_all_editors.size();
    for (int n=0; n<count; n++)
    {
        m_all_editors[n].onKeyChange(symbolAmount, type);
    }
}

// ----------------------------------------------------------------------------------------------------------

void GraphicalTrack::onDrumkitChanged(const int newInstrument)
{
    m_instrument_string->setValue(DrumChoice::getDrumkitName( newInstrument ));
}

// ----------------------------------------------------------------------------------------------------------

void GraphicalTrack::onInstrumentChanged(const int newInstrument)
{
    m_instrument_string->setValue(getInstrumentName(newInstrument));
}

// ----------------------------------------------------------------------------------------------------------
#if 0
#pragma mark -
#pragma mark Getters/Setters
#endif

void GraphicalTrack::setCollapsed(const bool collapsed)
{
    m_collapsed = collapsed;
}

// ----------------------------------------------------------------------------------------------------------

void GraphicalTrack::setHeight(const int height)
{
    m_height = height;
}

// ----------------------------------------------------------------------------------------------------------

void GraphicalTrack::maximizeHeight(bool maximize)
{
    if (maximize)
    {
        setCollapsed(false);
                
        const bool exp = m_gsequence->getModel()->getMeasureData()->isExpandedMode();
        setHeight(Display::getHeight() - m_gsequence->getDockHeight() - MEASURE_BAR_Y -
                  EXPANDED_BAR_HEIGHT - BORDER_SIZE - 30 -
                  (exp ? EXPANDED_MEASURE_BAR_H : MEASURE_BAR_H)  );
    }
    else
    {
        if (m_height > 200) m_height = 200;
    }
}

// ----------------------------------------------------------------------------------------------------------

void GraphicalTrack::dock(const bool setDocked)
{
    if (setDocked)
    {
        m_docked = true;
        m_gsequence->addToDock( this );
    }
    else
    {
        m_docked = false;
        m_gsequence->removeFromDock( this );
    }
}

// ----------------------------------------------------------------------------------------------------------

int GraphicalTrack::getTotalHeight() const
{

    if (m_docked) return 0;

    // FIXME: remove hardcoded numbers
    if (m_collapsed)
    {
        return 45; // COLLAPSED_BAR_HEIGHT
    }
    else
    {
        return EXPANDED_BAR_HEIGHT + 50 + m_height;
    }

}

// ----------------------------------------------------------------------------------------------------------
/*
Editor* GraphicalTrack::getCurrentEditor()
{
    switch (m_track->getNotationType())
    {
        case KEYBOARD:      return m_keyboard_editor;
        case GUITAR:        return m_guitar_editor;
        case DRUM:          return m_drum_editor;
        case CONTROLLER:    return m_controller_editor;
        case SCORE:         return m_score_editor;
        default :
            std::cerr << "[GraphicalTrack] getCurrentEditor : unknown mode!" << std::endl;
            ASSERT(false);
            return NULL; // shut up warnings
    }
}
*/
// ----------------------------------------------------------------------------------------------------------

void GraphicalTrack::onNotationTypeChange()
{    
    if (m_track->isNotationTypeEnabled(DRUM))
    {
        m_instrument_string->setValue(DrumChoice::getDrumkitName( m_track->getDrumKit() ));
    }
    else
    {
        // only call 'set' if the string really changed, the OpenGL implementation of 'set' involves
        // RTT and is quite expensive.
        wxString name = getInstrumentName(m_track->getInstrument());
        if (m_instrument_string->getValue() != name)
        {
            m_instrument_string->setValue(name);
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------

int GraphicalTrack::getNoteStartInPixels(const int id) const
{
    return (int)( (float)m_track->getNoteStartInMidiTicks(id) * m_gsequence->getZoom() );
}

// ---------------------------------------------------------------------------------------------------------------

int GraphicalTrack::getNoteEndInPixels(const int id) const
{
    return (int)( (float)m_track->getNoteEndInMidiTicks(id) * m_gsequence->getZoom() );
}

// ---------------------------------------------------------------------------------------------------------------

void GraphicalTrack::selectNote(const int id, const bool selected, bool ignoreModifiers)
{    
    ASSERT(id != SELECTED_NOTES); // not supported in this function
    
    bool done_for_controller = false;

    // ---- select/deselect all notes
    if (id == ALL_NOTES)
    {
        // if this is a 'select none' command, unselect any selected measures in the top bar
        if (not selected) getSequence()->getMeasureBar()->unselect();
        
        Editor* ed = getFocusedEditor();
        if (ed != NULL and ed->getNotationType() == CONTROLLER)
        {
            // FIXME(DESIGN): controller editor must be handled differently (special case)
            m_controller_editor->selectAll( selected );
            done_for_controller = true;
        }
    }
    
    if (not done_for_controller)
    {
        m_track->selectNote(id, selected, ignoreModifiers);
    }
}

// ----------------------------------------------------------------------------------------------------------

Editor* GraphicalTrack::getEditorAt(const int y, Editor** next)
{
    if (next != NULL) *next = NULL;
    
    if (m_track->isNotationTypeEnabled(SCORE) and y >= m_score_editor->getTrackYStart() and
        y <= m_score_editor->getYEnd())
    {
        m_focused_editor = SCORE;
        if (next != NULL)
        {
            if      (m_track->isNotationTypeEnabled(KEYBOARD))   *next = m_keyboard_editor;
            else if (m_track->isNotationTypeEnabled(GUITAR))     *next = m_guitar_editor;
            else if (m_track->isNotationTypeEnabled(DRUM))       *next = m_drum_editor;
            else if (m_track->isNotationTypeEnabled(CONTROLLER)) *next = m_controller_editor;
        }
        return m_score_editor;
    }
    
    if (m_track->isNotationTypeEnabled(GUITAR) and y >= m_guitar_editor->getTrackYStart() and
        y <= m_guitar_editor->getYEnd())
    {
        m_focused_editor = GUITAR;
        if (next != NULL)
        {
            if      (m_track->isNotationTypeEnabled(KEYBOARD))   *next = m_keyboard_editor;
            else if (m_track->isNotationTypeEnabled(DRUM))       *next = m_drum_editor;
            else if (m_track->isNotationTypeEnabled(CONTROLLER)) *next = m_controller_editor;
        }
        return m_guitar_editor;
    }

    if (m_track->isNotationTypeEnabled(KEYBOARD) and y >= m_keyboard_editor->getTrackYStart()and
        y <= m_keyboard_editor->getYEnd())
    {
        m_focused_editor = KEYBOARD;
        if (next != NULL)
        {
            if      (m_track->isNotationTypeEnabled(DRUM))       *next = m_drum_editor;
            else if (m_track->isNotationTypeEnabled(CONTROLLER)) *next = m_controller_editor;
        }
        return m_keyboard_editor;
    }
    
    if (m_track->isNotationTypeEnabled(DRUM) and y >= m_drum_editor->getTrackYStart() and
        y <= m_drum_editor->getYEnd())
    {
        m_focused_editor = DRUM;
        if (next != NULL)
        {
            if (m_track->isNotationTypeEnabled(CONTROLLER)) *next = m_controller_editor;
        }
        return m_drum_editor;
    }
    
    if (m_track->isNotationTypeEnabled(CONTROLLER) and y >= m_controller_editor->getTrackYStart() and
        y <= m_controller_editor->getYEnd())
    {
        m_focused_editor = CONTROLLER;
        if (next != NULL) *next = NULL;
        return m_controller_editor;
    }
    
    return NULL;
}

// ----------------------------------------------------------------------------------------------------------

Editor* GraphicalTrack::getFocusedEditor()
{
    if (m_focused_editor == KEYBOARD and m_track->isNotationTypeEnabled(KEYBOARD))
    {
        return m_keyboard_editor;
    }
    if (m_focused_editor == GUITAR and m_track->isNotationTypeEnabled(GUITAR))
    {
        return m_guitar_editor;
    }
    if (m_focused_editor == DRUM and m_track->isNotationTypeEnabled(DRUM))
    {
        return m_drum_editor;
    }
    if (m_focused_editor == SCORE and m_track->isNotationTypeEnabled(SCORE))
    {
        return m_score_editor;
    }
    if (m_focused_editor == CONTROLLER and m_track->isNotationTypeEnabled(CONTROLLER))
    {
        return m_controller_editor;
    }
    
    // Focused editor not found!! Pick the firsdt we find
    if (m_track->isNotationTypeEnabled(KEYBOARD))
    {
        m_focused_editor = KEYBOARD;
        return m_keyboard_editor;
    }
    if (m_track->isNotationTypeEnabled(GUITAR))
    {
        m_focused_editor = GUITAR;
        return m_guitar_editor;
    }
    if (m_track->isNotationTypeEnabled(DRUM))
    {
        m_focused_editor = DRUM;
        return m_drum_editor;
    }
    if (m_track->isNotationTypeEnabled(SCORE))
    {
        m_focused_editor = SCORE;
        return m_score_editor;
    }
    if (m_track->isNotationTypeEnabled(CONTROLLER))
    {
        m_focused_editor = CONTROLLER;
        return m_controller_editor;
    }
    
    // WTF??
    ASSERT(false);
    return NULL;
}

// ----------------------------------------------------------------------------------------------------------

int GraphicalTrack::getEditorFromY() const
{
    int editor_from_y = m_from_y + BORDER_SIZE;
    
    if (not m_collapsed)
    {
        editor_from_y += EXPANDED_BAR_HEIGHT;
    }
    
    return editor_from_y;
}

// ----------------------------------------------------------------------------------------------------------

void GraphicalTrack::evenlyDistributeSpace()
{
    int count = 0;
    if (m_track->isNotationTypeEnabled(SCORE))      count++;
    if (m_track->isNotationTypeEnabled(KEYBOARD))   count++;
    if (m_track->isNotationTypeEnabled(GUITAR))     count++;
    if (m_track->isNotationTypeEnabled(DRUM))       count++;
    if (m_track->isNotationTypeEnabled(CONTROLLER)) count++;
    
    if (m_track->isNotationTypeEnabled(SCORE))      m_score_editor->setRelativeHeight(1.0f / count);
    if (m_track->isNotationTypeEnabled(KEYBOARD))   m_keyboard_editor->setRelativeHeight(1.0f / count);
    if (m_track->isNotationTypeEnabled(GUITAR))     m_guitar_editor->setRelativeHeight(1.0f / count);
    if (m_track->isNotationTypeEnabled(DRUM))       m_drum_editor->setRelativeHeight(1.0f / count);
    if (m_track->isNotationTypeEnabled(CONTROLLER)) m_controller_editor->setRelativeHeight(1.0f / count);
}

// ----------------------------------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------------------

#if 0
#pragma mark -
#pragma mark Rendering
#endif

void GraphicalTrack::renderHeader(const int x, const int y, const bool closed, const bool focus)
{
    // mark 'dock' button as disabled when maximize mode is activated
    m_dock_toolbar->getItem(1).setImageState(m_gsequence->isTrackMaximized() ?
                                             AriaRender::STATE_GHOST :
                                             AriaRender::STATE_NORMAL );
    
    const bool channel_mode = (m_gsequence->getModel()->getChannelManagementType() == CHANNEL_MANUAL);
    
    int barHeight = EXPANDED_BAR_HEIGHT;
    if (closed) barHeight = COLLAPSED_BAR_HEIGHT;
    
    if (not focus) AriaRender::setImageState(AriaRender::STATE_NO_FOCUS);
    else           AriaRender::setImageState(AriaRender::STATE_NORMAL);
    
    AriaRender::images();
    
    // --------------------------------------------------
    
    // left border
    borderDrawable->move(x + LEFT_EDGE_X + BORDER_SIZE, y + BORDER_SIZE);
    borderDrawable->setFlip(false, true);
    borderDrawable->rotate(90);
    borderDrawable->scale(1, barHeight /*number of pixels high*/ /20.0 );
    borderDrawable->render();
    
    // right border
    borderDrawable->move(x + Display::getWidth() - MARGIN - BORDER_SIZE + 20 /*due to rotation of 90 degrees*/, y + BORDER_SIZE);
    borderDrawable->setFlip(false, false);
    borderDrawable->rotate(90);
    borderDrawable->scale(1, barHeight /*number of pixels high*/ /20.0 );
    borderDrawable->render();
    
    // top left corner
    cornerDrawable->move(x + LEFT_EDGE_X,y);
    cornerDrawable->setFlip(false, false);
    cornerDrawable->render();
    
    // top border
    borderDrawable->move(x + LEFT_EDGE_X + BORDER_SIZE, y);
    borderDrawable->setFlip(false, false);
    borderDrawable->rotate(0);
    borderDrawable->scale((Display::getWidth() - MARGIN - BORDER_SIZE*2)/20.0, 1 );
    borderDrawable->render();
    
    // top right corner
    cornerDrawable->move(x+Display::getWidth() - MARGIN - BORDER_SIZE, y);
    cornerDrawable->setFlip(true, false);
    cornerDrawable->render();
    
    // --------------------------------------------------
    
    // center
    AriaRender::primitives();
    
    if (not focus) AriaRender::color(0.31/2, 0.31/2, 0.31/2);
    else           AriaRender::color(0.31, 0.31, 0.31);
    
    // FIXME: don't hardcode numbers
    AriaRender::rect(x + LEFT_EDGE_X + BORDER_SIZE, y + BORDER_SIZE, x + Display::getWidth() - MARGIN - BORDER_SIZE, y + BORDER_SIZE + barHeight);
    
    
    // --------------------------------------------------
    
    if (closed)
    {
        AriaRender::images();
        
        if (not focus) AriaRender::setImageState(AriaRender::STATE_NO_FOCUS);
        else           AriaRender::setImageState(AriaRender::STATE_NORMAL);
        
        // bottom left corner
        cornerDrawable->move(x + LEFT_EDGE_X, y + BORDER_SIZE + barHeight);
        cornerDrawable->setFlip(false, true);
        cornerDrawable->render();
        
        // bottom border
        borderDrawable->move(x + LEFT_EDGE_X + BORDER_SIZE, y + BORDER_SIZE + barHeight);
        borderDrawable->setFlip(false, true);
        borderDrawable->rotate(0);
        borderDrawable->scale((Display::getWidth() - MARGIN - BORDER_SIZE*2)/20.0, 1 );
        borderDrawable->render();
        
        // bottom right corner
        cornerDrawable->move(x+ Display::getWidth() - MARGIN - BORDER_SIZE, y + BORDER_SIZE + barHeight);
        cornerDrawable->setFlip(true, true);
        cornerDrawable->render();
        
        AriaRender::setImageState(AriaRender::STATE_NORMAL);
    }
    else
    {
        // white area
        AriaRender::primitives();
        
        if (m_track->isPlayed())
        {
            AriaRender::color(1, 1, 1);
        }
        else
        {
            AriaRender::color(0.9, 0.9, 0.9);
        }
        
        AriaRender::rect(x + LEFT_EDGE_X,
                         y + barHeight + BORDER_SIZE,
                         x + Display::getWidth() - MARGIN,
                         y + barHeight + BORDER_SIZE + 20 + m_height);
    }//end if
    
    // ------------------ prepare to draw components ------------------
    if (m_collapsed) m_collapse_button->m_drawable->setImage( expandImg );
    else             m_collapse_button->m_drawable->setImage( collapseImg );
    
    if (m_track->isMuted()) m_mute_button->m_drawable->setImage( muteOnImg );
    else                    m_mute_button->m_drawable->setImage( muteOffImg );
    
    if (m_track->isSoloed()) m_solo_button->m_drawable->setImage( soloOnImg );
    else                     m_solo_button->m_drawable->setImage( soloOffImg );
    
    
    int trackVolume = m_track->getVolume();
    Image* volumeImg;
    if (trackVolume<=TRACK_VOLUME_LIMIT_1)
    {
        volumeImg = volumeLowImg;
    }
    else if (trackVolume>TRACK_VOLUME_LIMIT_1 && trackVolume<=TRACK_VOLUME_LIMIT_2)
    {
        volumeImg = volumeMediumImg;
    }
    else if (trackVolume>TRACK_VOLUME_LIMIT_2)
    {
        volumeImg = volumeHighImg;
    }
    m_volume_button->m_drawable->setImage(volumeImg);
    
    m_score_button -> enable( m_track->isNotationTypeEnabled(SCORE)      and focus );
    m_piano_button -> enable( m_track->isNotationTypeEnabled(KEYBOARD)   and focus );
    m_tab_button   -> enable( m_track->isNotationTypeEnabled(GUITAR)     and focus );
    m_drum_button  -> enable( m_track->isNotationTypeEnabled(DRUM)       and focus );
    m_ctrl_button  -> enable( m_track->isNotationTypeEnabled(CONTROLLER) and focus );
    
    m_sharp_flat_picker->show(m_track->isNotationTypeEnabled(SCORE));
    
    m_channel_field->show(channel_mode);
    
    // ------------------ layout and draw components ------------------
    m_components->layout(20, y);
    m_sharp_flat_picker->layout();
    m_grid_combo->layout();
    m_dock_toolbar->layout();
    m_components->renderAll(focus);
    
    //  ------------------ post-drawing  ------------------
    
    // draw track name
    AriaRender::images();
    AriaRender::color(0,0,0);
    m_name_renderer.bind();
    
#ifdef __WXMSW__
    m_name_renderer.render(m_track_name->getX()+11, y+30);
#else
    m_name_renderer.render(m_track_name->getX()+11, y+29);
#endif

    // draw grid label
    int grid_selection_x;
    switch (m_grid->getModel()->getDivider())
    {
        case 1:
            grid_selection_x = mgrid_1->getX();
            break;
        case 2:
        case 3:
            grid_selection_x = mgrid_2->getX();
            break;
        case 4:
        case 6:
            grid_selection_x = mgrid_4->getX();
            break;
        case 8:
        case 12:
            grid_selection_x = mgrid_8->getX();
            break;
        case 16:
        case 24:
            grid_selection_x = mgrid_16->getX();
            break;
        case 32:
        case 48:
            grid_selection_x = mgrid_32->getX();
            break;
        default: // length is chosen from drop-down menu
            grid_selection_x = -1;
    }
    
    AriaRender::primitives();
    AriaRender::color(0,0,0);
    AriaRender::hollow_rect(grid_selection_x, y+15, grid_selection_x+16, y+30);
    if (m_grid->getModel()->isTriplet())
    {
        AriaRender::hollow_rect(mgrid_triplet->getX(),      y + 15,
                                mgrid_triplet->getX() + 16, y + 30);
    }
    if (m_grid->getModel()->isDotted())
    {
        AriaRender::hollow_rect(mgrid_dotted->getX(),      y + 15,
                                mgrid_dotted->getX() + 16, y + 30);
    }
    
    // mark maximize mode as on if relevant
    if (m_gsequence->isTrackMaximized())
    {
        const int rectx = m_dock_toolbar->getItem(0).getX();
        AriaRender::hollow_rect(rectx, y+13, rectx+16, y+29);
    }
    
    // draw instrument name
    AriaRender::images();
    AriaRender::color(0,0,0);
    
    m_instrument_name.bind();
#ifdef __WXMSW__
    m_instrument_name.render(m_instrument_field->getX()+11 ,y+30);
#else
    m_instrument_name.render(m_instrument_field->getX()+11 ,y+29);
#endif
        
    // draw channel number
    if (channel_mode)
    {
        wxString channelName = to_wxString(m_track->getChannel());
        
        AriaRender::color(0,0,0);
        
        const int char_amount_in_channel_name = channelName.size();
        if (char_amount_in_channel_name == 1)
            AriaRender::renderNumber(channelName.mb_str(), m_channel_field->getX()+10, y+28);
        else
            AriaRender::renderNumber(channelName.mb_str(), m_channel_field->getX()+7, y+28);
    }
    
}

// ----------------------------------------------------------------------------------------------------------

int GraphicalTrack::render(const int y, const int currentTick, const bool focus)
{
    
    if (not ImageProvider::imagesLoaded()) return 0;
    
    // docked tracks are not drawn
    if (m_docked)
    {
        m_from_y = -1;
        m_to_y = -1;
        return y;
    }
    
    m_from_y = y;
    
    int editor_from_y = getEditorFromY();
    
    if (m_collapsed)
    {
        m_to_y = m_from_y + BORDER_SIZE + COLLAPSED_BAR_HEIGHT + BORDER_SIZE + MARGIN_Y;
    }
    else
    {
        m_to_y = editor_from_y + m_height + BORDER_SIZE + MARGIN_Y;
    }
    
    // tell the editor(s) about its/their new location
    int count = 0;
    if (m_track->isNotationTypeEnabled(SCORE))      count++;
    if (m_track->isNotationTypeEnabled(KEYBOARD))   count++;
    if (m_track->isNotationTypeEnabled(GUITAR))     count++;
    if (m_track->isNotationTypeEnabled(DRUM))       count++;
    if (m_track->isNotationTypeEnabled(CONTROLLER)) count++;
    
    const int editor_height = (m_to_y - editor_from_y - 5);
    int editor_to_y = editor_from_y; //editor_from_y + editor_height;

    const int original_editor_from_y = editor_from_y;
    
    if (m_track->isNotationTypeEnabled(SCORE))
    {
        int h = m_score_editor->getRelativeHeight()*editor_height;
        editor_to_y += h;
        m_score_editor->updatePosition(editor_from_y, editor_to_y, Display::getWidth(), h);
        editor_from_y = editor_to_y + 1;
    }
    if (m_track->isNotationTypeEnabled(GUITAR))
    {
        int h = m_guitar_editor->getRelativeHeight()*editor_height;
        editor_to_y += h;
        m_guitar_editor->updatePosition(editor_from_y, editor_to_y, Display::getWidth(), h);
        editor_from_y = editor_to_y + 1;
    }
    if (m_track->isNotationTypeEnabled(KEYBOARD))
    {
        int h = m_keyboard_editor->getRelativeHeight()*editor_height;
        editor_to_y += h;
        m_keyboard_editor->updatePosition(editor_from_y, editor_to_y, Display::getWidth(), h);
        editor_from_y = editor_to_y + 1;
    }
    if (m_track->isNotationTypeEnabled(DRUM))
    {
        int h = m_drum_editor->getRelativeHeight()*editor_height;
        editor_to_y += h;
        m_drum_editor->updatePosition(editor_from_y, editor_to_y, Display::getWidth(), h);
        editor_from_y = editor_to_y + 1;
    }
    if (m_track->isNotationTypeEnabled(CONTROLLER))
    {
        int h = m_controller_editor->getRelativeHeight()*editor_height;
        editor_to_y += h;
        m_controller_editor->updatePosition(editor_from_y, editor_to_y, Display::getWidth(), h);
        editor_from_y = editor_to_y + 1;
    }
    
    // don't waste time drawing it if out of bounds
    if (m_to_y < 0) return m_to_y;
    if (m_from_y > Display::getHeight()) return m_to_y;
    
    renderHeader(0, y, m_collapsed, focus);
    
    if (not m_collapsed)
    {
        // --------------------------------------------------
        // render editor(s)
        
        const RelativeXCoord x1 = Display::getMouseX_current();
        const int y1 = Display::getMouseY_current();
        const RelativeXCoord x2 = Display::getMouseX_initial();
        const int y2 = Display::getMouseY_initial();
        
        int rcount = 0;

        if (m_track->isNotationTypeEnabled(SCORE))
        {
            rcount++;
            m_score_editor->render(x1, y1, x2, y2, focus);
            
            if (rcount < count)
            {
                AriaRender::primitives();
                AriaRender::color( 0.5f, 0.5f, 0.5f );
                AriaRender::rect(10, m_score_editor->getYEnd() - THUMB_SIZE_ABOVE,
                                 m_score_editor->getXEnd(), m_score_editor->getYEnd() + THUMB_SIZE_BELOW );
            }
        }
        if (m_track->isNotationTypeEnabled(GUITAR))
        {
            rcount++;
            m_guitar_editor->render(x1, y1, x2, y2, focus);
            
            if (rcount < count)
            {
                AriaRender::primitives();
                AriaRender::color( 0.5f, 0.5f, 0.5f );
                AriaRender::rect(10, m_guitar_editor->getYEnd() - THUMB_SIZE_ABOVE,
                                 m_guitar_editor->getXEnd(), m_guitar_editor->getYEnd() + THUMB_SIZE_BELOW );
            }
        }
        if (m_track->isNotationTypeEnabled(KEYBOARD))
        {
            rcount++;
            m_keyboard_editor->render(x1, y1, x2, y2, focus);
            
            if (rcount < count)
            {
                AriaRender::primitives();
                AriaRender::color( 0.5f, 0.5f, 0.5f );
                AriaRender::rect(10, m_keyboard_editor->getYEnd() - THUMB_SIZE_ABOVE,
                                 m_keyboard_editor->getXEnd(), m_keyboard_editor->getYEnd() + THUMB_SIZE_BELOW );
            }
        }
        if (m_track->isNotationTypeEnabled(DRUM))
        {
            rcount++;
            m_drum_editor->render(x1, y1, x2, y2, focus);
            
            if (rcount < count)
            {
                AriaRender::primitives();
                AriaRender::color( 0.5f, 0.5f, 0.5f );
                AriaRender::rect(10, m_drum_editor->getYEnd() - THUMB_SIZE_ABOVE,
                                 m_drum_editor->getXEnd(), m_drum_editor->getYEnd() + THUMB_SIZE_BELOW );
            }
        }
        if (m_track->isNotationTypeEnabled(CONTROLLER))
        {
            m_controller_editor->render(x1, y1, x2, y2, focus);
        }
        
        
        // --------------------------------------------------
        // render playback progress line
        
        AriaRender::primitives();
        
        if (currentTick != -1 and not Display::leftArrow() and not Display::rightArrow())
        {
            AriaRender::color(0.8, 0, 0);
            
            RelativeXCoord tick(currentTick, MIDI, m_gsequence);
            const int x_coord = tick.getRelativeTo(WINDOW);
            
            AriaRender::lineWidth(1);
            
            AriaRender::line(x_coord, original_editor_from_y,
                             x_coord, m_to_y - 5);
            
        }
        
        // --------------------------------------------------
        // render track borders
        
        AriaRender::images();
        
        
        int barHeight = EXPANDED_BAR_HEIGHT;
        if (m_collapsed) barHeight = COLLAPSED_BAR_HEIGHT;
        
        if (not focus) AriaRender::setImageState(AriaRender::STATE_NO_FOCUS);
        else           AriaRender::setImageState(AriaRender::STATE_NORMAL);
        
        // bottom left corner
        whiteCornerDrawable->move(LEFT_EDGE_X, y + BORDER_SIZE + barHeight + m_height);
        whiteCornerDrawable->setFlip(false, false);
        whiteCornerDrawable->render();
        
        // bottom border
        whiteBorderDrawable->move(LEFT_EDGE_X + BORDER_SIZE, y + BORDER_SIZE + barHeight + m_height);
        whiteBorderDrawable->setFlip(false, false);
        whiteBorderDrawable->rotate(0);
        whiteBorderDrawable->scale((Display::getWidth() - MARGIN - BORDER_SIZE*2)/20.0, 1 );
        whiteBorderDrawable->render();
        
        // bottom right corner
        whiteCornerDrawable->move(Display::getWidth() - MARGIN - BORDER_SIZE,
                                  y + BORDER_SIZE + barHeight + m_height);
        whiteCornerDrawable->setFlip(true, false);
        whiteCornerDrawable->render();
        
        // --------------------------------------------------
        
        // left borderDrawable
        whiteBorderDrawable->move(LEFT_EDGE_X + BORDER_SIZE, y + barHeight + BORDER_SIZE);
        whiteBorderDrawable->setFlip(false, false);
        whiteBorderDrawable->rotate(90);
        whiteBorderDrawable->scale(1, m_height / 20.0 );
        whiteBorderDrawable->render();
        
        // right borderDrawable
        whiteBorderDrawable->move(Display::getWidth() - MARGIN , y + barHeight + BORDER_SIZE);
        whiteBorderDrawable->setFlip(false, true);
        whiteBorderDrawable->rotate(90);
        whiteBorderDrawable->scale(1, m_height / 20.0 );
        whiteBorderDrawable->render();
        
    }
    
    AriaRender::images();
    
    // done
    return m_to_y;
}


// Handles TAB keyboard shortcut 
void GraphicalTrack::switchDivider(int index)
{
    /*
    int divider;
    
    divider = m_grid->getModel()->getDivider();
    
    if (m_grid->getModel()->isTriplet())
    {
        divider -= divider/3;
    }

    if (forward)
    {
        if (divider==128) divider = 1;
        else divider *= 2;
    }
    else
    {
        if (divider==1) divider = 128;
        else divider /= 2;
    }
    */
    
    int divider = 1;
    if (index == 2) divider = 2;
    else if (index == 3) divider = 4;
    else if (index == 4) divider = 8;
    else if (index == 5) divider = 16;
    else if (index == 6) divider = 32;
    else if (index == 7) divider = 64;
    else if (index == 8) divider = 128;
    setDivider(divider);
}


void GraphicalTrack::setDivider(int divider)
{
    wxCommandEvent fake_event;
    
    switch (divider)
    {
        case   1 : m_grid->grid1selected(fake_event); break;
        case   2 : m_grid->grid2selected(fake_event); break;
        case   4 : m_grid->grid4selected(fake_event); break;
        case   8 : m_grid->grid8selected(fake_event); break;
        case  16 : m_grid->grid16selected(fake_event); break;
        case  32 : m_grid->grid32selected(fake_event); break;
        case  64 : m_grid->grid64selected(fake_event); break;
        case 128 : m_grid->grid128selected(fake_event); break;
    }
}


void GraphicalTrack::scrollKeyboardEditorNotesIntoView()
{
    m_keyboard_editor->scrollNotesIntoView();
}


// ----------------------------------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------------------
#if 0
#pragma mark -
#pragma mark Serialization
#endif

void GraphicalTrack::saveToFile(wxFileOutputStream& fileout)
{
    const int octave_shift = m_score_editor->getScoreMidiConverter()->getOctaveShift();

    // TODO: move notation type to "Track"
    writeData(wxString(wxT("  <editors ")) + (m_collapsed ? wxT("collapsed=\"true\" ") : wxT("")) + 
              wxT("height=\"") + to_wxString(m_height) + wxT("\">\n"),
              fileout);
    writeData(wxT("    <score enabled=\"") + wxString(m_track->isNotationTypeEnabled(SCORE) ? wxT("true") : wxT("false")) +
              wxT("\" musical_notation=\"") + (m_score_editor->isMusicalNotationEnabled()?wxT("true"):wxT("false")) +
              wxT("\" linear_notation=\"") + (m_score_editor->isLinearNotationEnabled()?wxT("true"):wxT("false")) +
              wxT("\" g_clef=\"") + (m_score_editor->isGClefEnabled()?wxT("true"):wxT("false")) +
              wxT("\" f_clef=\"") + (m_score_editor->isFClefEnabled()?wxT("true"):wxT("false")) +
              ( octave_shift != 0 ? wxT("\" octave_shift=\"") + to_wxString(octave_shift) : wxT("")) +
              wxT("\" scroll=\"") + to_wxString(m_score_editor->getScrollbarPosition()) +
              (m_track->isNotationTypeEnabled(SCORE) ? 
               wxT("\" proportion=\"") + to_wxString(m_score_editor->getRelativeHeight()) :
               wxT("")) +
              (m_score_editor->isBackgroundTrack() ? 
               wxT("\" background_tracks=\"") + m_score_editor->getBackgroundTracks() :
               wxT(""))+
              wxT("\"/>\n"), fileout);
    writeData(wxT("    <keyboard enabled=\"") + wxString(m_track->isNotationTypeEnabled(KEYBOARD) ? wxT("true") : wxT("false")) +
              wxT("\" scroll=\"") + to_wxString(m_keyboard_editor->getScrollbarPosition()) +
              (m_track->isNotationTypeEnabled(KEYBOARD) ? 
               wxT("\" proportion=\"") + to_wxString(m_keyboard_editor->getRelativeHeight()) :
               wxT("")) +
              (m_keyboard_editor->isBackgroundTrack() ? 
               wxT("\" background_tracks=\"") + m_keyboard_editor->getBackgroundTracks() :
               wxT(""))+
              wxT("\"/>\n"), fileout);
    writeData(wxT("    <guitar enabled=\"") + wxString(m_track->isNotationTypeEnabled(GUITAR) ? wxT("true") : wxT("false")) +
              (m_track->isNotationTypeEnabled(GUITAR) ? 
               wxT("\" proportion=\"") + to_wxString(m_guitar_editor->getRelativeHeight()) :
               wxT("")) +
              (m_guitar_editor->isBackgroundTrack() ? 
               wxT("\" background_tracks=\"") + m_guitar_editor->getBackgroundTracks() :
               wxT(""))+
              wxT("\"/>\n"), fileout);
    writeData(wxT("    <drum enabled=\"") + wxString(m_track->isNotationTypeEnabled(DRUM) ? wxT("true") : wxT("false")) +
              wxT("\" scroll=\"") + to_wxString(m_drum_editor->getScrollbarPosition()) +
              (m_track->isNotationTypeEnabled(DRUM) ? 
               wxT("\" proportion=\"") + to_wxString(m_drum_editor->getRelativeHeight()) :
               wxT("")) +
              (m_drum_editor->isBackgroundTrack() ? 
               wxT("\" background_tracks=\"") + m_drum_editor->getBackgroundTracks() :
               wxT(""))+
              wxT("\"/>\n"), fileout);
    writeData(wxT("    <controller enabled=\"") + wxString(m_track->isNotationTypeEnabled(CONTROLLER) ? wxT("true") : wxT("false")) +
              wxT("\" controller=\"") + to_wxString(m_controller_editor->getCurrentControllerType()) +
              (m_track->isNotationTypeEnabled(CONTROLLER) ? 
               wxT("\" proportion=\"") + to_wxString(m_controller_editor->getRelativeHeight()) :
               wxT("")) +
              (m_controller_editor->isBackgroundTrack() ? 
               wxT("\" background_tracks=\"") + m_controller_editor->getBackgroundTracks() :
               wxT(""))+
              wxT("\"/>\n"), fileout);
    writeData(wxT("  </editors>\n"), fileout );
    
    m_grid->getModel()->saveToFile( fileout );
    //keyboardEditor->instrument->saveToFile(fileout);
    //drumEditor->drumKit->saveToFile(fileout);

    // TODO: move this to 'Track', has nothing to do here in GraphicalTrack
    writeData( wxT("  <instrument id=\"") + to_wxString( m_track->getInstrument() ) + wxT("\"/>\n"), fileout);
    writeData( wxT("  <drumkit id=\"") + to_wxString( m_track->getDrumKit() ) + wxT("\" collapseView=\"") +
               to_wxString(m_drum_editor->showOnlyUsedDrums()) + wxT("\"/>\n"), fileout);
    
    // guitar tuning (FIXME: move this out of here)
    writeData( wxT("  <guitartuning "), fileout);
    GuitarTuning* tuning = m_track->getGuitarTuning();
    
    const int stringCount = tuning->tuning.size();
    for (int n=0; n<stringCount; n++)
    {
        writeData(wxT(" string")+ to_wxString((int)n) + wxT("=\"") +
                  to_wxString((int)tuning->tuning[n]) + wxT("\""), fileout );
    }

    writeData( wxT("/>\n\n"), fileout);

}

// ----------------------------------------------------------------------------------------------------------

bool GraphicalTrack::readFromFile(irr::io::IrrXMLReader* xml)
{
    
    bool missingProportions = false;
    
    // TODO: backwards compatibility, eventually remove the first 'if'
    if (strcmp("editor", xml->getNodeName()) == 0)
    {

        const char* height_c = xml->getAttributeValue("height");
        if (height_c != NULL)
        {
            m_height = atoi( height_c );
        }
        else
        {
            std::cout << "Missing info from file: track height" << std::endl;
            m_height = 200;
        }

        const char* collapsed_c = xml->getAttributeValue("collapsed");
        if (collapsed_c != NULL)
        {
            if (strcmp(collapsed_c, "true") == 0)
            {
                m_collapsed = true;
            }
            else if (strcmp(collapsed_c, "false") == 0)
            {
                m_collapsed = false;
            }
            else
            {
                std::cout << "Unknown keyword for attribute 'collapsed' in track: " << collapsed_c << std::endl;
                m_collapsed = false;
            }

        }
        else
        {
            m_collapsed = false;
        }

        const char* f_clef_c = xml->getAttributeValue("f_clef");
        if (f_clef_c != NULL)
        {
            if (strcmp(f_clef_c, "true") == 0)
            {
                m_score_editor->enableFClef(true);
            }
            else if (strcmp(f_clef_c, "false") == 0)
            {
                m_score_editor->enableFClef(false);
            }
            else
            {
                std::cerr << "[GraphicalTrack] readFromFile() : Unknown keyword for attribute 'f_clef' in track: " << f_clef_c << std::endl;
            }

        }
        const char* octave_shift_c = xml->getAttributeValue("octave_shift");
        if ( octave_shift_c != NULL )
        {
            int new_value = atoi( octave_shift_c );
            if (new_value != 0) m_score_editor->getScoreMidiConverter()->setOctaveShift(new_value);
        }
        
        // compatibility code for older versions of .Aria file format (TODO: eventually remove)
        const char* muted_c = xml->getAttributeValue("muted");
        if (muted_c != NULL)
        {
            if (strcmp(muted_c, "true") == 0)
            {
                m_track->setMuted(true);
            }
            else if (strcmp(muted_c, "false") == 0)
            {
                m_track->setMuted(false);
            }
            else
            {
                std::cerr << "Unknown keyword for attribute 'muted' in track: " << muted_c << std::endl;
            }
            
        }
        evenlyDistributeSpace();
    }
    else if (strcmp("editors", xml->getNodeName()) == 0)
    {
        const char* height_c = xml->getAttributeValue("height");
        if (height_c != NULL)
        {
            m_height = atoi( height_c );
        }
        else
        {
            std::cout << "Missing info from file: track height" << std::endl;
            m_height = 200;
        }
        
        const char* collapsed_c = xml->getAttributeValue("collapsed");
        if (collapsed_c != NULL)
        {
            if (strcmp(collapsed_c, "true") == 0)
            {
                m_collapsed = true;
            }
            else if (strcmp(collapsed_c, "false") == 0)
            {
                m_collapsed = false;
            }
            else
            {
                std::cout << "Unknown keyword for attribute 'collapsed' in track: " << collapsed_c << std::endl;
                m_collapsed = false;
            }
            
        }
        else
        {
            m_collapsed = false;
        }
        
        while (xml != NULL and xml->read())
        {
            switch (xml->getNodeType())
            {
                case irr::io::EXN_TEXT:
                {
                    break;
                }
                case irr::io::EXN_ELEMENT:
                {
                    bool enabled = false;
                    const char* enabled_c = xml->getAttributeValue("enabled");
                    if (enabled_c != NULL)
                    {
                        if (strcmp(enabled_c, "true") == 0)
                        {
                            enabled = true;
                        }
                        else if (strcmp(enabled_c, "false") == 0)
                        {
                            enabled = false;
                        }
                        else
                        {
                            std::cerr << "[GraphicalTrack] Unknown keyword for attribute 'enabled' in editor: " << enabled_c << std::endl;
                        }
                    }
                    
                    
                    float scroll = 0.5f;
                    const char* scroll_c = xml->getAttributeValue("scroll");
                    if (scroll_c != NULL)
                    {
                        scroll = atof(scroll_c);
                    }
                    
                    float proportion = 1.0f;
                    const char* proportion_c = xml->getAttributeValue("proportion");
                    if (proportion_c != NULL)
                    {
                        proportion = atof(proportion_c);
                    }
                    else if (enabled)
                    {
                        missingProportions = true;
                    }
                    
    
                    wxString backgroundTracks = wxString::FromUTF8(xml->getAttributeValue("background_tracks"));
                   
                    if (strcmp("score", xml->getNodeName()) == 0)
                    {
                        m_track->setNotationType(SCORE, enabled);
                        m_score_editor->setScrollbarPosition(scroll);
                        m_score_editor->setBackgroundTracks(backgroundTracks);
                        if (enabled) m_score_editor->setRelativeHeight(proportion);
                        
                        const char* musical_notation_c = xml->getAttributeValue("musical_notation");
                        if (musical_notation_c != NULL)
                        {
                            if (strcmp(musical_notation_c, "true") == 0)
                            {
                                m_score_editor->enableMusicalNotation(true);
                            }
                            else if (strcmp(musical_notation_c, "false") == 0)
                            {
                                m_score_editor->enableMusicalNotation(false);
                            }
                            else
                            {
                                std::cout << "Unknown keyword for attribute 'musical_notation' in track: " << musical_notation_c << std::endl;
                            }
                        }

                        const char* linear_notation_c = xml->getAttributeValue("linear_notation");
                        if (linear_notation_c != NULL)
                        {
                            if (strcmp(linear_notation_c, "true") == 0)
                            {
                                m_score_editor->enableLinearNotation(true);
                            }
                            else if (strcmp(linear_notation_c, "false") == 0)
                            {
                                m_score_editor->enableLinearNotation(false);
                            }
                            else
                            {
                                std::cout << "Unknown keyword for attribute 'linear_notation_c' in track: " << linear_notation_c << std::endl;
                            }
                        }
                        
                        const char* g_clef_c = xml->getAttributeValue("g_clef");
                        if (g_clef_c != NULL)
                        {
                            if (strcmp(g_clef_c, "true") == 0)
                            {
                                m_score_editor->enableGClef(true);
                            }
                            else if (strcmp(g_clef_c, "false") == 0)
                            {
                                m_score_editor->enableGClef(false);
                            }
                            else
                            {
                                std::cout << "Unknown keyword for attribute 'g_clef' in track: " << g_clef_c << std::endl;
                            }
                        }
                        
                        const char* f_clef_c = xml->getAttributeValue("f_clef");
                        if (f_clef_c != NULL)
                        {
                            if (strcmp(f_clef_c, "true") == 0)
                            {
                                m_score_editor->enableFClef(true);
                            }
                            else if (strcmp(f_clef_c, "false") == 0)
                            {
                                m_score_editor->enableFClef(false);
                            }
                            else
                            {
                                std::cerr << "[GraphicalTrack] readFromFile() : Unknown keyword for attribute 'f_clef' in track: " << f_clef_c << std::endl;
                            }
                            
                        }
                        
                        const char* octave_shift_c = xml->getAttributeValue("octave_shift");
                        if ( octave_shift_c != NULL )
                        {
                            int new_value = atoi( octave_shift_c );
                            if (new_value != 0) m_score_editor->getScoreMidiConverter()->setOctaveShift(new_value);
                        }
                    }
                    else if (strcmp("keyboard", xml->getNodeName()) == 0)
                    {
                        m_track->setNotationType(KEYBOARD, enabled);
                        m_keyboard_editor->setScrollbarPosition(scroll);
                        m_keyboard_editor->setBackgroundTracks(backgroundTracks);
                        if (enabled) m_keyboard_editor->setRelativeHeight(proportion);
                    }
                    else if (strcmp("guitar", xml->getNodeName()) == 0)
                    {
                        m_track->setNotationType(GUITAR, enabled);
                        m_guitar_editor->setBackgroundTracks(backgroundTracks);
                        if (enabled) m_guitar_editor->setRelativeHeight(proportion);
                    }
                    else if (strcmp("drum", xml->getNodeName()) == 0)
                    {
                        m_track->setNotationType(DRUM, enabled);
                        m_drum_editor->setBackgroundTracks(backgroundTracks);
                        m_drum_editor->setScrollbarPosition(scroll);
                        if (enabled) m_drum_editor->setRelativeHeight(proportion);
                    }
                    else if (strcmp("controller", xml->getNodeName()) == 0)
                    {
                        m_track->setNotationType(CONTROLLER, enabled);
                        m_controller_editor->setBackgroundTracks(backgroundTracks);
                        if (enabled) m_controller_editor->setRelativeHeight(proportion);
                        
                        const char* id = xml->getAttributeValue("controller");
                        if (id != NULL)
                        {
                            getControllerEditor()->setController(atoi(id));
                        }
                    }
                    else
                    {
                        fprintf(stderr, "[GraphicalTrack] WARNING: Unknown editor type '%s'\n", xml->getNodeName());
                    }
                    break;
                }
                case irr::io::EXN_ELEMENT_END:
                {
                    if (strcmp("editors", xml->getNodeName()) == 0)
                    {
                        if (missingProportions) evenlyDistributeSpace();
                        return true;
                    }
                    break;   
                }
                default:break;
            }//end switch
            
        }
    }

    
    return true;
}

// ----------------------------------------------------------------------------------------------------------
