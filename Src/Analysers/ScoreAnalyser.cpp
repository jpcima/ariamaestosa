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

#include "Editors/ScoreEditor.h"
#include "Analysers/ScoreAnalyser.h"
#include "Midi/MeasureData.h"
#include "AriaCore.h"
#include <cmath>
#include <math.h>

namespace AriaMaestosa
{

/*
 * This class receives a range of IDs of notes that are candidates for beaming. Its job is to decide
 * how to beam the notes in order to get maximal results, as well as changing the NoteRenderInfo objects
 * accordingly so that the render is correct.
 */
class BeamGroup
{
    int first_id, last_id;
    int min_level, mid_level, max_level;
    ScoreAnalyser* analyser;

public:
    BeamGroup(ScoreAnalyser* analyser, const int first_id, const int last_id)
    {
        BeamGroup::first_id = first_id;
        BeamGroup::last_id = last_id;
        BeamGroup::analyser = analyser;
        BeamGroup::min_level = 999;
        BeamGroup::max_level = -999;
    }
    void calculateLevel(std::vector<NoteRenderInfo>& noteRenderInfo, ScoreMidiConverter* converter)
    {
        for(int i=first_id;i<=last_id; i++)
        {
            if (noteRenderInfo[i].chord)
            {
                if (noteRenderInfo[i].min_chord_level < min_level) min_level = noteRenderInfo[i].min_chord_level;
                if (noteRenderInfo[i].max_chord_level > max_level) max_level = noteRenderInfo[i].max_chord_level;
            }
            else
            {
                const int level = noteRenderInfo[i].level;
                if (level < min_level) min_level = level;
                if (level > max_level) max_level = level;
            }
        }

        // if nothing found (most likely meaning we only have one triplet note alone) use values from the first
        if (min_level == 999)  min_level = noteRenderInfo[first_id].level;
        if (max_level == -999) max_level = noteRenderInfo[first_id].level;

        mid_level = (int)round( (min_level + max_level)/2.0 );
    }

    void doBeam(std::vector<NoteRenderInfo>& noteRenderInfo, ScoreEditor* editor)
    {
        assert( editor != NULL );
        
        if (last_id == first_id) return; // note alone, no beaming to perform

        ScoreMidiConverter* converter = editor->getScoreMidiConverter();
        //const int y_step = editor->getYStep();

        // check for number of "beamable" notes and split if current amount is not acceptable with the current time sig
        // only considering note 0 to get the measure should be fine since Aria only beams within the same measure
        const int num = getMeasureData()->getTimeSigNumerator( noteRenderInfo[0].measureBegin );
        const int denom = getMeasureData()->getTimeSigDenominator( noteRenderInfo[0].measureBegin );
        const int flag_amount = noteRenderInfo[first_id].flag_amount;

        int max_amount_of_notes_beamed_toghether = 4;

        // FIXME - not always right
        if (num == 3 and denom == 4) max_amount_of_notes_beamed_toghether = 2 * (int)(std::pow(2.0,flag_amount-1));
        else if ((num == 6 and denom == 4) or (num == 6 and denom == 8)) max_amount_of_notes_beamed_toghether = 3 * (int)(std::pow(2.0,flag_amount-1));
        else max_amount_of_notes_beamed_toghether = num * (int)(std::pow(2.0,flag_amount-1));
        if (noteRenderInfo[first_id].triplet) max_amount_of_notes_beamed_toghether=3;

        const int beamable_note_amount = last_id - first_id + 1;

        // if max_amount_of_notes_beamed_toghether is an even number, don't accept an odd number of grouped notes
        const int base_unit = (max_amount_of_notes_beamed_toghether % 2 == 0 ? 2 : 1);
        if (beamable_note_amount <= max_amount_of_notes_beamed_toghether and beamable_note_amount % base_unit != 0)
        {
            max_amount_of_notes_beamed_toghether = base_unit;
        }

        if (beamable_note_amount > max_amount_of_notes_beamed_toghether)
        {
            // amount is not acceptable, split

            // try to find where beamed groups of such notes usually start and end in the measure
            // this is where splitting should be performed
            const int group_len = noteRenderInfo[first_id].tick_length * max_amount_of_notes_beamed_toghether;
            const int first_tick_in_measure = getMeasureData()->firstTickInMeasure( getMeasureData()->measureAtTick(noteRenderInfo[first_id].tick) );

            int split_at_id = -1;
            for(int n=first_id+1; n<=last_id; n++)
            {
                if ( (noteRenderInfo[n].tick - first_tick_in_measure) % group_len == 0 )
                {
                    split_at_id = n;
                    break;
                }
            }

            if (split_at_id == -1)
            {
                // dumb split
                BeamGroup first_half(analyser, first_id, first_id + max_amount_of_notes_beamed_toghether - 1);
                BeamGroup second_half(analyser, first_id + max_amount_of_notes_beamed_toghether, last_id);
                first_half.doBeam(noteRenderInfo, editor);
                second_half.doBeam(noteRenderInfo, editor);
            }
            else
            {
                BeamGroup first_half(analyser, first_id, split_at_id - 1);
                BeamGroup second_half(analyser, split_at_id, last_id);
                first_half.doBeam(noteRenderInfo, editor);
                second_half.doBeam(noteRenderInfo, editor);
            }

            return;
        }

        calculateLevel(noteRenderInfo, converter);

        noteRenderInfo[first_id].beam_show_above = (mid_level < analyser->stemPivot ? false : true);
        noteRenderInfo[first_id].beam = true;

        for(int j=first_id; j<=last_id; j++)
        {
            // give correct stem orientation (up or down)
            noteRenderInfo[j].stem_type = ( noteRenderInfo[first_id].beam_show_above ?  STEM_UP : STEM_DOWN );

            // reset any already set stem location, since we'll need to totally redo them for the beam
            noteRenderInfo[j].stem_y_level = -1;
        }

        // set initial beam info in note
        noteRenderInfo[first_id].beam_to_tick  = noteRenderInfo[last_id].tick;
        noteRenderInfo[first_id].beam_to_sign  = noteRenderInfo[last_id].sign;
        noteRenderInfo[first_id].beam_to_level = analyser->getStemTo(noteRenderInfo[last_id]);
        noteRenderInfo[first_id].stem_y_level  = analyser->getStemTo(noteRenderInfo[first_id]);

        // check if the stem is too inclined, fix it if necessary
        const float height_diff = fabsf( noteRenderInfo[first_id].beam_to_level - noteRenderInfo[first_id].stem_y_level );

        if ( height_diff > 3 )
        {
            const float height_shift = height_diff - 3;
            const bool end_on_higher_level = (noteRenderInfo[first_id].beam_to_level > noteRenderInfo[first_id].stem_y_level );
            if (noteRenderInfo[first_id].beam_show_above)
            {
                if (end_on_higher_level) noteRenderInfo[first_id].beam_to_level -= height_shift;
                else noteRenderInfo[first_id].stem_y_level -= height_shift;
            }
            else
            {
                if (end_on_higher_level) noteRenderInfo[first_id].stem_y_level += height_shift;
                else noteRenderInfo[first_id].beam_to_level += height_shift;
            }
        }

        // fix all note stems so they all point in the same direction and have the correct height
        while (true)
        {
            const int from_tick = noteRenderInfo[first_id].tick;
            const float from_level = analyser->getStemTo(noteRenderInfo[first_id]);
            const int to_tick = noteRenderInfo[first_id].beam_to_tick;
            const float to_level = noteRenderInfo[first_id].beam_to_level;

            bool need_to_start_again = false;
            for (int j=first_id; j<=last_id; j++)
            {
                // give correct stem height (so it doesn't end above or below beam line)
                // rel_pos will be 0 for first note of a beamed serie, and 1 for the last one
                const float rel_pos = (float)(noteRenderInfo[j].tick - from_tick) / (float)(to_tick - from_tick);
                if (j != first_id)
                {
                    noteRenderInfo[j].stem_y_level = (float)from_level + (float)(to_level - from_level) * rel_pos;
                }

                // check if stem is long enough and on right side of beam
                // here the distinction between base level and stem origin is tricky but necessary
                // to preporly deal with chords. In a chord, when we check if the stem is long enough,
                // we don't want to check the entire stem, only the part coming out of the top/bottom note
                const float stemheight = fabsf(noteRenderInfo[j].stem_y_level - noteRenderInfo[j].getBaseLevel());
                const bool too_short = stemheight < analyser->min_stem_height;

                const float diff = noteRenderInfo[j].stem_y_level - noteRenderInfo[j].getBaseLevel();
                const bool on_wrong_side_of_beam =
                    (noteRenderInfo[first_id].beam_show_above and diff>0) or
                    ((not noteRenderInfo[first_id].beam_show_above) and diff<0);

                if ( too_short or on_wrong_side_of_beam )
                {
                    // we've got a problem here. this stem is too short and will look weird
                    // we'll adjust the height of the beam and try again
                    // (the order of the tests here is important; if the beam is on the wrong
                    // side of the beam AND too short, only the first should should happen)
                    float beam_shift = 0;
                    if (on_wrong_side_of_beam) beam_shift = analyser->min_stem_height + fabsf(diff);
                    else if (too_short) beam_shift = analyser->min_stem_height - stemheight;

                    if (noteRenderInfo[first_id].beam_show_above)
                    {
                        noteRenderInfo[first_id].beam_to_level -= beam_shift;
                        noteRenderInfo[first_id].stem_y_level  -= beam_shift;
                    }
                    else
                    {
                        noteRenderInfo[first_id].beam_to_level += beam_shift;
                        noteRenderInfo[first_id].stem_y_level  += beam_shift;
                    }
                    need_to_start_again = true;
                    break;
                }

                if (j != first_id) noteRenderInfo[j].flag_amount = 0;
            }
            if (not need_to_start_again) break; // we're done, no need to loop again
        }
    }
};

}

using namespace AriaMaestosa;

// -----------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------

#if 0
#pragma mark -
#pragma mark NoteRenderInfo
#endif

NoteRenderInfo::NoteRenderInfo(int tick, int level, int tick_length, PitchSign sign, const bool selected, int pitch)
{
    // what we know before render pass 1
    NoteRenderInfo::selected = selected;
    NoteRenderInfo::tick = tick;
    NoteRenderInfo::tick_length = tick_length;
    NoteRenderInfo::sign = sign;
    NoteRenderInfo::level = level;
    NoteRenderInfo::pitch = pitch;

    // what we will know after render pass 1
    //unknown_duration = false;
    instant_hit=false;
    triplet = false;
    dotted = false;
    flag_amount = 0;
    y = -1;
    tied_with_tick = -1;
    tie_up = false;
    stem_type = STEM_NONE;

    draw_stem = true;

    triplet_show_above = false;
    triplet_arc_tick_start = -1;
    triplet_arc_tick_end = -1;
    draw_triplet_sign = false;

    beam_show_above = false;
    beam_to_tick = -1;
    beam_to_level = -1;
    beam = false;

    chord = false;
    stem_y_level = -1;
    min_chord_level=-1;
    max_chord_level=-1;

    // measure where the note begins and ends
    measureBegin = getMeasureData()->measureAtTick(tick);
    measureEnd = getMeasureData()->measureAtTick(tick + tick_length - 1);
}

// -----------------------------------------------------------------------------------------------------------

void NoteRenderInfo::tieWith(NoteRenderInfo& renderInfo)
{
    tied_with_tick = renderInfo.tick;

    if (stem_type == STEM_NONE) tie_up = renderInfo.stem_type;
    else tie_up = stem_type;
}

// -----------------------------------------------------------------------------------------------------------

void NoteRenderInfo::tieWith(const int tick)
{
    tied_with_tick = tick;

}

// -----------------------------------------------------------------------------------------------------------

int NoteRenderInfo::getTiedToTick()
{
    return tied_with_tick;
}

// -----------------------------------------------------------------------------------------------------------

void NoteRenderInfo::setTieUp(const bool up)
{
    tie_up = up;
}

// -----------------------------------------------------------------------------------------------------------

bool NoteRenderInfo::isTieUp()
{
    return (stem_type == STEM_NONE ? tie_up : stem_type != STEM_UP);
}

// -----------------------------------------------------------------------------------------------------------

void NoteRenderInfo::setTriplet()
{
    triplet = true;
    draw_triplet_sign = true;
}

// -----------------------------------------------------------------------------------------------------------

int NoteRenderInfo::getBaseLevel()
{
    if (chord) return (stem_type == STEM_UP ? min_chord_level : max_chord_level);
    else return level;
}

// -----------------------------------------------------------------------------------------------------------

int NoteRenderInfo::getStemOriginLevel()
{
    if (chord) return (stem_type == STEM_UP ? max_chord_level : min_chord_level);
    else return level;
}

// -----------------------------------------------------------------------------------------------------------

void NoteRenderInfo::setY(const int newY)
{
    y = newY;
}

// -----------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------

#if 0
#pragma mark -
#pragma mark Score Analyser (public)
#endif

ScoreAnalyser::ScoreAnalyser(Editor* parent, int stemPivot)
{
    ScoreAnalyser::editor = parent;
    ScoreAnalyser::stemPivot = stemPivot;

    stem_height = 5.2;
    min_stem_height = 4.5;
}

// -----------------------------------------------------------------------------------------------------------

void ScoreAnalyser::addToVector( NoteRenderInfo& renderInfo )
{
    addToVector( renderInfo, false );
}

// -----------------------------------------------------------------------------------------------------------

void ScoreAnalyser::addToVector( NoteRenderInfo& renderInfo, const bool recursion )
{
    // check if note lasts more than one measure. If so we need to divide it in 2.
    if (renderInfo.measureEnd > renderInfo.measureBegin) // note in longer than mesaure, need to divide it in 2
    {
        const int firstEnd = getMeasureData()->lastTickInMeasure(renderInfo.measureBegin);
        const int firstLength = firstEnd - renderInfo.tick;
        const int secondLength = renderInfo.tick_length - firstLength;
        
        // split the note in two, and collect resulting notes in a vector.
        // then we can iterate through that vector and tie all notes together
        // (remember, note may be split in more than 2 if one of the 2 initial halves has a rare length)
        
        int initial_id = -1;
        if (!recursion) initial_id = noteRenderInfo.size();
        
        if (aboutEqual(firstLength, 0)) return;
        if (aboutEqual(secondLength, 0)) return;
        
        NoteRenderInfo part1(renderInfo.tick, renderInfo.level, firstLength, renderInfo.sign, renderInfo.selected, renderInfo.pitch);
        addToVector(part1, true);
        NoteRenderInfo part2(getMeasureData()->firstTickInMeasure(renderInfo.measureBegin+1),
                             renderInfo.level, secondLength, renderInfo.sign, renderInfo.selected, renderInfo.pitch);
        addToVector(part2, true);
        
        if (!recursion)
        {
            // done splitting, now iterate through all notes that
            // were added in this recusrion and tie them
            const int amount = noteRenderInfo.size();
            for(int i=initial_id+1; i<amount; i++)
                noteRenderInfo[i].tieWith(noteRenderInfo[i-1]);
        }
        
        return;
    }
    
    // find how to draw notes. how many flags, dotted, triplet, etc.
    // if note duration is unknown it will be split
    const float relativeLength = renderInfo.tick_length / (float)(getMeasureData()->beatLengthInTicks()*4);
    
    renderInfo.stem_type = (renderInfo.level >= stemPivot ? STEM_UP : STEM_DOWN);
    if (relativeLength>=1) renderInfo.stem_type=STEM_NONE; // whole notes have no stem
    renderInfo.hollow_head = false;
    
    const int beat = getMeasureData()->beatLengthInTicks();
    const int tick_in_measure_start = renderInfo.tick - getMeasureData()->firstTickInMeasure( renderInfo.measureBegin );
    const int remaining = beat - (tick_in_measure_start % beat);
    const bool starts_on_beat = aboutEqual(remaining,0) or aboutEqual(remaining,beat);
    
    if ( aboutEqual(relativeLength, 1.0) ){ renderInfo.hollow_head = true; renderInfo.stem_type=STEM_NONE; }
    else if ( aboutEqual(relativeLength, 1.0/2.0) ){ renderInfo.hollow_head = true; } // 1/2
    else if ( aboutEqual(relativeLength, 1.0/3.0) ){ renderInfo.setTriplet(); renderInfo.hollow_head = true; } // triplet 1/2
    else if ( aboutEqual(relativeLength, 1.0/4.0) ); // 1/4
    else if ( aboutEqual(relativeLength, 1.0/8.0) ) renderInfo.flag_amount = 1; // 1/8
    else if ( aboutEqual(relativeLength, 1.0/6.0) ){ renderInfo.setTriplet(); } // triplet 1/4
    else if ( aboutEqual(relativeLength, 1.0/16.0) ) renderInfo.flag_amount = 2; // 1/16
    else if ( aboutEqual(relativeLength, 1.0/12.0) ){ renderInfo.setTriplet(); renderInfo.flag_amount = 1; } // triplet 1/8
    else if ( aboutEqual(relativeLength, 1.0/32.0) ) renderInfo.flag_amount = 3; // 1/32
    else if ( aboutEqual(relativeLength, 1.0/24.0) ) { renderInfo.setTriplet(); renderInfo.flag_amount = 2; } // triplet 1/16
    else if ( aboutEqual(relativeLength, 3.0/4.0) and starts_on_beat){ renderInfo.dotted = true; renderInfo.hollow_head=true; } // dotted 1/2
    else if ( aboutEqual(relativeLength, 3.0/8.0) and starts_on_beat ) renderInfo.dotted = true; // dotted 1/4
    else if ( aboutEqual(relativeLength, 3.0/2.0) and starts_on_beat ){ renderInfo.dotted = true; renderInfo.hollow_head=true; } // dotted whole
    else if ( relativeLength < 1.0/32.0 )
    {
        renderInfo.instant_hit = true;
    }
    else
    { // note is of unknown duration. split it in a serie of tied notes.
        
        
        // how long is the first note after the split?
        int firstLength_tick;
        
        // start by reaching the next beat if not already done
        if (!starts_on_beat and !aboutEqual(remaining, renderInfo.tick_length))
        {
            firstLength_tick = remaining;
        }
        else
        {
            // use division to split note
            float closestShorterDuration = 1;
            while(closestShorterDuration >= relativeLength) closestShorterDuration /= 2.0;
            
            firstLength_tick = closestShorterDuration*(float)(getMeasureData()->beatLengthInTicks()*4);
        }
        
        const int secondBeginning_tick = renderInfo.tick + firstLength_tick;
        //RelativeXCoord secondBeginningRel(secondBeginning_tick, MIDI);
        
        int initial_id = -1;
        
        if (!recursion)
        {
            initial_id = noteRenderInfo.size();
        }
        
        NoteRenderInfo part1(renderInfo.tick, renderInfo.level, firstLength_tick, renderInfo.sign, renderInfo.selected, renderInfo.pitch);
        addToVector(part1, true);
        NoteRenderInfo part2(secondBeginning_tick, renderInfo.level,
                             renderInfo.tick_length-firstLength_tick, renderInfo.sign, renderInfo.selected, renderInfo.pitch);
        addToVector(part2, true);
        
        if (!recursion)
        {
            // done splitting, now iterate through all notes that
            // were added in this recusrion and tie them
            const int amount = noteRenderInfo.size();
            for(int i=initial_id+1; i<amount; i++)
            {
                noteRenderInfo[i].tieWith(noteRenderInfo[i-1]);
            }
        }
        
        return;
    }
    
    if (renderInfo.triplet)
    {
        renderInfo.triplet_arc_tick_start = renderInfo.tick;
        renderInfo.triplet_arc_level = renderInfo.level;
    }
    
    assertExpr(renderInfo.level,>,-1);
    noteRenderInfo.push_back(renderInfo);
}

// -----------------------------------------------------------------------------------------------------------

void ScoreAnalyser::setStemPivot(const int level)
{
    stemPivot = level;
}

// -----------------------------------------------------------------------------------------------------------

void ScoreAnalyser::clearAndPrepare()
{
    noteRenderInfo.clear();
}

// -----------------------------------------------------------------------------------------------------------

float ScoreAnalyser::getStemTo(NoteRenderInfo& note)
{
    if      (note.stem_y_level != -1)     return note.stem_y_level;
    else if (note.stem_type == STEM_UP)   return note.getStemOriginLevel() - stem_height;
    else if (note.stem_type == STEM_DOWN) return note.getStemOriginLevel() + stem_height;
    else { assert(false); return -1; }
}
    
// -----------------------------------------------------------------------------------------------------------

void ScoreAnalyser::analyseNoteInfo()
{
    putInTimeOrder();
    findAndMergeChords();
    processTriplets();
    processNoteBeam();
    
}

// -----------------------------------------------------------------------------------------------------------

ScoreAnalyser* ScoreAnalyser::getSubset(const int fromTick, const int toTick)
{
    ScoreAnalyser* out = new ScoreAnalyser(*this);
    
    //std::cout << "getting subset out of " << (int)(out->noteRenderInfo.size()) << " elements\n";
    
    for (int n=0; n<(int)out->noteRenderInfo.size(); n++)
    {
        if (out->noteRenderInfo[n].tick < fromTick or out->noteRenderInfo[n].tick >= toTick)
        {
            out->noteRenderInfo.erase(out->noteRenderInfo.begin() +  n);
            n--;
            if (n<-1) n=-1;
        }
    }
    
    return out;
}

// -----------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------

#if 0
#pragma mark -
#pragma mark Score Analyser (private)
#endif

// -----------------------------------------------------------------------------------------------------------

void ScoreAnalyser::putInTimeOrder()
{

    const int visibleNoteAmount = noteRenderInfo.size();
#ifdef _MORE_DEBUG_CHECKS
    int iteration = 0;
#endif
    for(int i=1; i<visibleNoteAmount; i++)
    {
#ifdef _MORE_DEBUG_CHECKS
        iteration++;
        assertExpr(iteration,<,100000);
#endif

        // put in time order
        // making sure notes without stem come before notes with a stem
        if ( noteRenderInfo[i].tick < noteRenderInfo[i-1].tick or
           ( noteRenderInfo[i].tick == noteRenderInfo[i-1].tick and noteRenderInfo[i-1].stem_type != STEM_NONE and noteRenderInfo[i].stem_type == STEM_NONE)
           )
        {
            NoteRenderInfo tmp = noteRenderInfo[i-1];
            noteRenderInfo[i-1] = noteRenderInfo[i];
            noteRenderInfo[i] = tmp;
            i -= 2; if (i<0) i=0;
        }
    }

}

// -----------------------------------------------------------------------------------------------------------

void ScoreAnalyser::findAndMergeChords()
{
    /*
     * start by merging notes playing at the same time (chords)
     * The for loop iterates through all notes. When we find notes that play at the same time,
     * the while loop starts and iterates until we reached the end of the chord.
     * when we're done with a chord, we "summarize" it in a single NoteRenderInfo object.
     * (at this point, the head of the note has been drawn and what's left to do
     * is draw stems, triplet signs, etc. so at this point a chord of note behaves just
     * like a single note).
     */
    for (int i=0; i<(int)noteRenderInfo.size(); i++)
    {
        int start_tick_of_next_note = -1;
        int first_note_of_chord = -1;
        int min_level = 999, max_level = -999;
        int maxid = i, minid = i; // id of the notes with highest and lowest Y
        int smallest_duration = 99999;
        bool last_of_a_serie = false;
        bool triplet = false;
        int flag_amount = 0;

        // when we have found a note chord, this variable will contain the ID of the
        // first note of that bunch (the ID of the last will be "i" when we get to it)
        first_note_of_chord = i;

        if (noteRenderInfo[i].stem_type == STEM_NONE) continue;

        while (true) // FIXME - it should be checked whether there is a chord BEFORE entering the while loop. same for others
        {
            // find next note's tick if there's one
            if (i+1<(int)noteRenderInfo.size())
            {
                start_tick_of_next_note = noteRenderInfo[i+1].tick;
            }
            else
            {
                start_tick_of_next_note = -1;
            }

            // we've processed all notes, exit the loop
            if (not (i<(int)noteRenderInfo.size())) break;

            // check if we're in a chord (i.e. many notes that play at the same time). also check they have stems :
            // for instance wholes have no stems and thus there is no special processing to do on them.
            if (start_tick_of_next_note != -1 and aboutEqual_tick(start_tick_of_next_note, noteRenderInfo[i].tick) and
                noteRenderInfo[i+1].stem_type != STEM_NONE)
            {
            }
            else
            {
                //after this one, it stops. mark this as the last so it will finalize stuff.
                last_of_a_serie = true;
                if (first_note_of_chord == i){ break; } //not a bunch of concurrent notes, just a note alone
            }

            // gather info on notes of the chord, for instance their y location (level) and their duration
            const int level = noteRenderInfo[i].level;
            if (level < min_level)
            {
                min_level = level;
                minid = i;
            }
            if (level > max_level)
            {
                max_level = level;
                maxid = i;
            }

            const int len = noteRenderInfo[i].tick_length;
            if (len < smallest_duration or smallest_duration == 99999)
            {
                smallest_duration = len;
            }

            if (noteRenderInfo[i].flag_amount > flag_amount)
            {
                flag_amount = noteRenderInfo[i].flag_amount;
            }

            if (noteRenderInfo[i].triplet) triplet = true;

            // remove this note's stem. we only need on stem per chord.
            // the right stem will be set on last iterated note of the chord (see below)
            noteRenderInfo[i].draw_stem = false;

            // the note of this iteration is the end of a chord, so it's time to complete chord information
            if (last_of_a_serie)
            {
                if (maxid == minid) break;
                if (first_note_of_chord == i) break;

                // determine average note level to know if we put stems above or below
                // if nothing found use values from the first
                if (min_level == 999)  min_level = noteRenderInfo[first_note_of_chord].level;
                if (max_level == -999) max_level = noteRenderInfo[first_note_of_chord].level;
                const int mid_level = (int)round( (min_level + max_level)/2.0 );

                const bool stem_up = mid_level >= stemPivot+2;

                /*
                 * decide the one note to keep that will "summarize" all others.
                 * it will be the highest or the lowest, depending on if stem is up or down.
                 * feed this NoteRenderInfo object with the info Aria believes will best summarize
                 * the chord. results may vary if you make very different notes play at the same time.
                 * Making a chord into a single note allows it to enter other steps of analysis,
                 * like beaming.
                 */
                NoteRenderInfo summary = noteRenderInfo[ stem_up ? minid : maxid ];
                summary.chord = true;
                summary.min_chord_level = min_level;
                summary.max_chord_level = max_level;
                summary.stem_y_level = (stem_up ?
                                        noteRenderInfo[minid].getStemOriginLevel() - stem_height :
                                        noteRenderInfo[maxid].getStemOriginLevel() + stem_height);
                summary.flag_amount = flag_amount;
                summary.triplet = triplet;
                summary.draw_stem = true;
                summary.stem_type = (stem_up ? STEM_UP : STEM_DOWN);
                summary.tick_length = smallest_duration;

                summary.tieWith( noteRenderInfo[ !stem_up ? minid : maxid ].getTiedToTick() );
                summary.setTieUp( noteRenderInfo[ !stem_up ? minid : maxid ].isTieUp() );

                noteRenderInfo[i] = summary;

                // now that we summarised concurrent notes into a single one, we can erase the other notes of the chord
                assertExpr(i,<,(int)noteRenderInfo.size());
                noteRenderInfo.erase( noteRenderInfo.begin()+first_note_of_chord, noteRenderInfo.begin()+i );
                i = first_note_of_chord-2;
                if (i<0) i=0;

                break;
            }//end if
            i++;
        }//wend

    }//next

}

// -----------------------------------------------------------------------------------------------------------

const bool VERBOSE_ABOUT_TRIPLETS = false;

void ScoreAnalyser::processTriplets()
{
    const int visibleNoteAmount = noteRenderInfo.size();

    for (int i=0; i<visibleNoteAmount; i++)
    {
        int start_tick_of_next_note = -1;

        bool is_triplet = noteRenderInfo[i].triplet;
        int first_triplet = (is_triplet ? i : -1);
        int min_level = 999;
        int max_level = -999;
        bool last_of_a_serie = false;

        int measure = noteRenderInfo[i].measureBegin;
        int previous_measure = measure;

        if (VERBOSE_ABOUT_TRIPLETS) std::cout << "(3) ---- Measure " << (measure+1) << " ----\n";
        
        // check for consecutive notes
        while (true)
        {
            if (VERBOSE_ABOUT_TRIPLETS) std::cout << "(3) Looking at note #" << i << std::endl;
            
            // ---- search for consecutive notes
            if (i+1<visibleNoteAmount)
            {
                start_tick_of_next_note = noteRenderInfo[i+1].tick;
            }

            if (not (i<visibleNoteAmount)) break;

            // if notes are consecutive
            if (start_tick_of_next_note != -1 and
                aboutEqual_tick(start_tick_of_next_note,
                                noteRenderInfo[i].tick+noteRenderInfo[i].tick_length))
            {
                if (VERBOSE_ABOUT_TRIPLETS) std::cout << "(3)    consecutive\n";
            }
            else
            {
                // notes are no more consecutive. it is likely a special action will be performed at the end of a serie
                last_of_a_serie = true;
                if (VERBOSE_ABOUT_TRIPLETS) std::cout << "(3) } // serie ends here :  NOT (no more) consecutive\n";
            }
            if ((not noteRenderInfo[i+1].triplet) or (i-first_triplet >= 2 and first_triplet != -1) )
            {
                if (VERBOSE_ABOUT_TRIPLETS)
                {
                    std::cout << "(3) } // serie ends here : "
                              << (!noteRenderInfo[i+1].triplet ? "next is no triplet " : "")
                              << ((i-first_triplet>=2 and first_triplet != -1) ? "We've had 3 in a row. " : "")
                              << "\n";
                }
                
                last_of_a_serie = true;
            }

            // do not cross measures
            if (i+1<visibleNoteAmount)
            {
                measure = noteRenderInfo[i+1].measureBegin;
                if (measure != previous_measure)
                {
                    if (VERBOSE_ABOUT_TRIPLETS) std::cout << "(3) } // serie ends here : crossing a measure bar\n";

                    last_of_a_serie = true;
                }
                previous_measure = measure;
            }

            if (noteRenderInfo[i].chord)
            {
                if (noteRenderInfo[i].min_chord_level < min_level) min_level = noteRenderInfo[i].min_chord_level;
                if (noteRenderInfo[i].max_chord_level > max_level) max_level = noteRenderInfo[i].max_chord_level;
            }
            else
            {
                const int level = noteRenderInfo[i].level;
                if (level < min_level) min_level = level;
                if (level > max_level) max_level = level;
            }

            // ---- ... and triplet notes
            is_triplet = noteRenderInfo[i].triplet;
            if (is_triplet and first_triplet==-1)
            {
                if (VERBOSE_ABOUT_TRIPLETS) std::cout << "(3) { // starting triplet serie\n";
                first_triplet = i;
                
                // since it's the first note in this triplet series, it's both the min and max
                const int level = noteRenderInfo[i].level;
                min_level = level;
                max_level = level;
            }

            // this note is a triplet, but not the next, so time to do display the triplets sign
            // also triggered if we've had 3 triplet notes in a row, because triplets come by groups of 3...
            if (last_of_a_serie)
            {
                if (is_triplet)
                {
                    if (VERBOSE_ABOUT_TRIPLETS) std::cout << "(3) == Binding triplet [" << first_triplet << " .. " << i << "] ==\n";
                    
                    // if nothing found (most likely meaning we only have one triplet note alone)
                    // use values from the first then.
                    if (min_level == 999)  min_level = noteRenderInfo[first_triplet].level;
                    if (max_level == -999) max_level = noteRenderInfo[first_triplet].level;

                    int mid_level = (int)round( (min_level + max_level)/2.0 );

                    noteRenderInfo[first_triplet].triplet_show_above = (mid_level < stemPivot);

                    if (i != first_triplet) // if not a triplet note alone, but a 'serie' of triplets
                    {
                        // fix all note stems so they all point in the same direction
                        for (int j=first_triplet; j<=i; j++)
                        {
                            //noteRenderInfo[j].stem_type = ( noteRenderInfo[first_triplet].triplet_show_above ? STEM_DOWN : STEM_UP );
                            noteRenderInfo[j].draw_triplet_sign = false;
                        }
                    }
                    else
                    {
                        // this is either a triplet alone or a chord... just use the orientation that it already has
                        noteRenderInfo[first_triplet].triplet_show_above = (noteRenderInfo[first_triplet].stem_type == STEM_DOWN);
                    }

                    if (noteRenderInfo[first_triplet].triplet_show_above)
                    {
                        noteRenderInfo[first_triplet].triplet_arc_level = min_level;
                    }
                    else
                    {
                        noteRenderInfo[first_triplet].triplet_arc_level = max_level;
                    }

                    noteRenderInfo[first_triplet].draw_triplet_sign = true;
                    // noteRenderInfo[first_triplet].triplet = true;
                    noteRenderInfo[first_triplet].triplet_arc_tick_end = noteRenderInfo[i].tick;
                }

                // reset search for triplets
                first_triplet = -1;
                min_level = 999;
                max_level = -999;
            }

            if (last_of_a_serie) break;
            else i++;

        }//wend

    }//next

}

// -----------------------------------------------------------------------------------------------------------

void ScoreAnalyser::processNoteBeam()
{
    const int visibleNoteAmount = noteRenderInfo.size();

    // beaming
    // all beam information is stored in the first note of the serie.
    // all others have their flags removed
    // BeamGroup objects are used to ease beaming

    for(int i=0; i<visibleNoteAmount; i++)
    {
        int start_tick_of_next_note = -1;

        int flag_amount = noteRenderInfo[i].flag_amount;
        int first_of_serie = i;
        bool last_of_a_serie = false;

        int measure = noteRenderInfo[i].measureBegin;
        int previous_measure = measure;

        // check for consecutive notes
        while(true)
        {
            if (i+1<visibleNoteAmount)
            {
                start_tick_of_next_note = noteRenderInfo[i+1].tick;
            }

            if (!(i<visibleNoteAmount)) break;

            // if notes are consecutive and of same length
            if (start_tick_of_next_note != -1 and
               aboutEqual_tick(start_tick_of_next_note, noteRenderInfo[i].tick+noteRenderInfo[i].tick_length) and
               noteRenderInfo[i+1].flag_amount == flag_amount and flag_amount > 0 and
               noteRenderInfo[i+1].triplet == noteRenderInfo[i].triplet);
            else
            {
                //notes are no more consecutive. it is likely a special action will be performed at the end of a serie
                last_of_a_serie = true;
            }

            // do not cross measures
            if (i+1<visibleNoteAmount)
            {
                measure = noteRenderInfo[i+1].measureBegin;
                if (measure != previous_measure) last_of_a_serie = true;
                previous_measure = measure;
            }

            // it's the last of a serie, perform actions
            if ( last_of_a_serie)
            {
                if (i>first_of_serie)
                {
                    BeamGroup beam(this, first_of_serie, i);
                    beam.doBeam(noteRenderInfo, dynamic_cast<ScoreEditor*>(editor));
                }

                // reset
                first_of_serie = -1;
                break;
            }

            i++;

        }//wend

    }//next

}
    
// -----------------------------------------------------------------------------------------------------------