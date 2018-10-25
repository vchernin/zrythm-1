/*
 * audio/automation_track.h - Automation track
 *
 * Copyright (C) 2018 Alexandros Theodotou
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

#ifndef __AUDIO_AUTOMATION_TRACK_H__
#define __AUDIO_AUTOMATION_TRACK_H__

#include "audio/position.h"
#include "audio/automation_point.h"

#define MAX_AUTOMATION_POINTS 1200

typedef struct Port Port;
typedef struct AutomationTrackWidget AutomationTrackWidget;
typedef struct Track Track;
typedef struct Automatable Automatable;

typedef struct AutomationTrack
{
  Automatable *             automatable; ///< the automatable
  AutomationTrackWidget *   widget;
  Track *                   track; ///< owner track
  AutomationPoint           automation_points[MAX_AUTOMATION_POINTS];
                                      ///< automation points
  int                       num_automation_points;
} AutomationTrack;

AutomationTrack *
automation_track_new(Track *        track,
                     Automatable *  automatable);

#endif // __AUDIO_AUTOMATION_TRACK_H__
