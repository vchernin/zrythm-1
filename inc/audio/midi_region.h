/*
 * audio/midi_region.h - MIDI region
 *
 * Copyright (C) 2019 Alexandros Theodotou
 *
 * This file is part of Zrythm
 *
 * Zrythm is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Zrythm is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Zrythm.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef __AUDIO_MIDI_REGION_H__
#define __AUDIO_MIDI_REGION_H__

typedef struct Track Track;
typedef struct Position Position;
typedef struct MidiRegion MidiRegion;
typedef struct MidiNote MidiNote;

MidiRegion *
midi_region_new (Track *    track,
                 Position * start_pos,
                 Position * end_pos);

/**
 * Deep clones the midi region.
 */
MidiRegion *
midi_region_clone (MidiRegion * src);

/**
 * Creates region (used when loading projects).
 */
MidiRegion *
midi_region_get_or_create_blank (int id);

/**
 * Adds midi note to region
 */
void
midi_region_add_midi_note (MidiRegion * region,
                      MidiNote * midi_note);

/**
 * Returns the midi note with the given pitch from the
 * unended notes.
 *
 * Used when recording.
 */
MidiNote *
midi_region_find_unended_note (MidiRegion * self,
                               int          pitch);
/**
 * Returns the midi note at given position with the given
 * pitch.
 *
 * Used when recording.
 */
//MidiNote *
//midi_region_get_midi_note_at (
  //MidiRegion * self,
  //Position *   pos,
  //int          pitch);

#endif // __AUDIO_MIDI_REGION_H__
