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

#ifndef _sequence_
#define _sequence_

#include <wx/string.h>

class wxFileOutputStream;
// forward
namespace irr { namespace io {
    class IXMLBase;
    template<class char_type, class super_class> class IIrrXMLReader;
    typedef IIrrXMLReader<char, IXMLBase> IrrXMLReader; } }

#include "AriaCore.h"
//#include "Editors/RelativeXCoord.h"
#include "Midi/Track.h"
#include "ptr_vector.h"
#include "Utils.h"

#include <math.h> // for 'round'
#include <string>


namespace AriaMaestosa
{

    class ControllerEvent;
    class MeasureBar;
    class IMeasureDataListener;

    const int DEFAULT_SONG_LENGTH = 12;
    
    namespace Action { class AddControllerSlide; }
    
    /**
      * @brief Interface for listeners that are to be notified of playback start/end
      */
    class IPlaybackModeListener
    {
    public:
        virtual ~IPlaybackModeListener() { }
        
        virtual void onEnterPlaybackMode() = 0;
        virtual void onLeavePlaybackMode() = 0;
    };
    
    /**
      * @brief Interface for listeners that are to be notified of changes to the event (undo) stack
      */
    class IActionStackListener
    {
    public:
        virtual ~IActionStackListener() {}
        
        virtual void onActionStackChanged() = 0;
    };
    
    class ISequenceDataListener
    {
    public:
        virtual ~ISequenceDataListener() {}
        
        virtual void onSequenceDataChanged() = 0;
    };
    
    /**
      * @brief This is a midi Sequence, or a "file".
      *
      * Each tab in the tab bar represents one Sequence instance.
      * It contains general information and a list of tracks.
      *
      * @ingroup midi
      */
    class Sequence
    {

        int m_tempo;
        int beatResolution;

        wxString m_copyright;
        wxString internal_sequenceName;

        int currentTrack;

        ptr_vector<Track> tracks;

        ChannelManagementType channelManagement;

        ptr_vector<Action::EditAction> undoStack;

        IPlaybackModeListener* m_playback_listener;
        
        IActionStackListener* m_action_stack_listener;
        
        ISequenceDataListener* m_seq_data_listener;
        
        /** Whether a metronome should be heard during playback */
        bool m_play_with_metronome;
        
        void copy();
        
        // TODO: get rif of friendship?
        friend class GraphicalSequence;
        friend class Action::AddControllerSlide;
        friend class Track;
        
        // FIXME(DESIGN): not sure "follow playback" belongs here
        /** set this flag true to follow playback */
        bool m_follow_playback;
        
        AriaRenderString       m_sequence_filename;
        OwnerPtr<MeasureData>  m_measure_data;
        
        ptr_vector<ControllerEvent> m_tempo_events;
        
        /** this object is to be modified by MainFrame, to remember where to save this sequence */
        wxString m_filepath;
        
        /** if no scrolling is done, this value will be used to determine where to place notes */
        int m_notes_shift_when_no_scrolling;
        
     public:

        LEAK_CHECK();

        //FIXME: remove read-write public members!
        /** 
          * set to true when importing - indicates the sequence will have frequent changes and not compute
          * too much until it's over
          */
        bool importing;

        /**
          * @brief Sequence constructor
          * @note  The listeners are optional, pass NULL where you don't need to be notified
          */
        Sequence(IPlaybackModeListener* playbackListener, IActionStackListener* actionStackListener,
                 ISequenceDataListener* sequenceDataListener, IMeasureDataListener* measureListener,
                 bool addDefautTrack);
        ~Sequence();
        
        /**
         * @brief perform an action that affects multiple tracks
         *
         * This is the method called for performing any action that can be undone.
         * A EditAction object is used to describe the task, and it also knows how to revert it.
         * The EditAction objects are kept in a stack in Sequence in order to offer multiple undo levels.
         *
         * Sequence::action does actions that affect all tracks. Also see Track::action.
         */
        void action( Action::MultiTrackAction* action );

        /** @brief you do not need to call this yourself, Track::action and Sequence::action do. */
        void addToUndoStack( Action::EditAction* action );
        
        /** @brief undo the Action at the top of the undo stack */
        void undo();
        
        /** @return the name of the Action at the top of the undo stack */
        wxString getTopActionName() const;
        
        /** @brief forbid undo, by dropping all undo information kept in memory. */
        void clearUndoStack();
        
        /** @return is there something to undo? */
        bool somethingToUndo();

        wxString suggestFileName() const;
        wxString suggestTitle() const;
        
        void spacePressed();
        
        /** @return the number of tracks in this sequence */
        int getTrackAmount() const { return tracks.size(); }
        
        /** @return the ID of the currently selected track */
        int getCurrentTrackID() const { return currentTrack; }
        
        Track* getTrack(int ID)  { return tracks.get(ID); }
        Track* getCurrentTrack() { return tracks.get(currentTrack); }
        void setCurrentTrackID(int ID);
        void setCurrentTrack(Track* track);
        
        /** 
          * @brief  Adds a new track (below currently selected track) to this sequence.
          *
          * @note   Does NOT add an action in the action/undo stack. So if the user requested to add
          *         a track, use the corresponding Action instead.
          * @return a pointer to the added track
          */
        Track* addTrack();
        
        /** 
          * @brief Adds an existing track to the sequence.
          *
          * @note  Does NOT add an action in the action/undo stack. So if the user requested to add
          *        a track, use the corresponding Action instead.
          */
        void addTrack(Track* track);
        
        /** 
          * @brief  Removes (but does not delete) the currently selected track.
          * @return the removed track
          */
        Track* removeSelectedTrack();
        
        /** Deletes a track
          * @param ID ID of the track to delete (in range [0 .. getTrackAmount()-1])
          */
        void deleteTrack(int ID);
        
        /** 
          * @brief Deletes a track
          * @param track A pointer to the track that must be deleted
          */
        void deleteTrack(Track* track);
        
        /** 
          * @brief       sets the "default" tempo (tempo at start of song)
          * @param tempo the new tempo value
          */
        void  setTempo(int tempo);
        
        /** @return      the "default" tempo (tempo at start of song) */
        int   getTempo() const { return m_tempo; }
        
        /** @return the tempo at any tick (not necessarily a tick where there is a tempo change event) */
        int   getTempoAtTick(const int tick) const;
        
        void  addTempoEvent( ControllerEvent* evt );
        
        int                    getTempoEventAmount() const { return m_tempo_events.size();  }
        const ControllerEvent* getTempoEvent(int id) const { return m_tempo_events.getConst(id); }
        
        void eraseTempoEvent(int id) { m_tempo_events.erase(id); }
        
        void setTempoEventValue(int id, int newValue) { m_tempo_events[id].setValue(newValue); }
        void setTempoEventTick (int id, int newTick)  { m_tempo_events[id].setTick(newTick);  }

        /** The tempo event with the given id will be extraced from this sequence (not deleted); the tempo
            vector will not be packed until you call removeMarkedTempoEvents(). See ptr_vector for more
            info */
        ControllerEvent* extractTempoEvent(int id)
        {
            ControllerEvent* evt = m_tempo_events.get(id);
            m_tempo_events.markToBeRemoved(id);
            return evt;
        }
        void removeMarkedTempoEvents()        { m_tempo_events.removeMarked();      }
        
        /**
          * @brief adds a tempo event.
          * @note  used during importing - then we know events are in time order
          *        so no time is wasted verifying that
          */
        void  addTempoEvent_import( ControllerEvent* evt );

        void  setChannelManagementType(ChannelManagementType m);
        ChannelManagementType getChannelManagementType() const { return channelManagement; }

        /** @brief called reacting to the user selecting "snap notes to grid" from the edit menu */
        void snapNotesToGrid();

        void setCopyright( wxString copyright );
        wxString getCopyright() const { return m_copyright; }
        
        void setInternalName(wxString name);
        wxString getInternalName() const
        {
            return internal_sequenceName;
        }

        void scale(
                   float factor,
                   bool rel_first_note, bool rel_begin, // relative to what (only one must be true)
                   bool affect_selection, bool affect_track, bool affect_song // scale what (only one must be true)
                   );

        void paste();
        void pasteAtMouse();

        void setPlayWithMetronome(const bool enabled) { m_play_with_metronome = enabled; }
        bool playWithMetronome   () const             { return m_play_with_metronome;    }
        
        /**
          * @return Ticks per beat (the number of time units in a quarter note.)
          */
        int ticksPerBeat() const { return beatResolution; }
        
        /**
          * @param res Ticks per beat (the number of time units in a quarter note.)
          */
        void setTicksPerBeat(int res);

        MeasureData* getMeasureData() { return m_measure_data; }
        const MeasureData* getMeasureData() const { return m_measure_data.raw_ptr; }

        void clear() { tracks.clearAndDeleteAll(); }
        
        int  getNoteShiftWhenNoScrolling() const  { return m_notes_shift_when_no_scrolling; }
        void setNoteShiftWhenNoScrolling(int val) { m_notes_shift_when_no_scrolling = val;  }
        int  isFollowPlaybackEnabled    () const  { return m_follow_playback;               }
        void enableFollowPlayback(bool enabled)   { m_follow_playback = enabled;            }
        
        wxString getFilepath()                   { return m_filepath; }
        void     setFilepath(wxString newpath)   { m_filepath = newpath; }
        
        wxString getSequenceFilename()           { return m_sequence_filename; }
        void     setSequenceFilename(wxString a) { m_sequence_filename.set(a); }
        
        // FIXME(DESIGN): remove renderer from here
        AriaRenderString& getNameRenderer() { return m_sequence_filename; }
        
        // ---- serialization
        
        /** Called when saving \<Sequence\> ... \</Sequence\> in .aria file */
        void saveToFile(wxFileOutputStream& fileout);
        
        /** Called when reading \<sequence\> ... \</sequence\> in .aria file */
        bool readFromFile(irr::io::IrrXMLReader* xml, GraphicalSequence* gseq);

    };

}
#endif
