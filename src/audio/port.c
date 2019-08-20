/*
 * Copyright (C) 2018-2019 Alexandros Theodotou <alex at zrythm dot org>
 *
 * This file is part of Zrythm
 *
 * Zrythm is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Zrythm is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with Zrythm.  If not, see <https://www.gnu.org/licenses/>.
 */

/** \file
 * Implementation of Port API.
 * Ports are passed when processing audio signals. They carry buffers
 * with data */

#define M_PIF 3.14159265358979323846f

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "project.h"
#include "audio/channel.h"
#ifdef HAVE_JACK
#include "audio/engine_jack.h"
#endif
#include "audio/midi.h"
#include "audio/mixer.h"
#include "audio/modulator.h"
#include "audio/pan.h"
#include "audio/passthrough_processor.h"
#include "audio/port.h"
#include "plugins/plugin.h"
#include "utils/arrays.h"
#include "utils/objects.h"

#include <gtk/gtk.h>

#define SLEEPTIME_USEC 60

/**
 * This function finds the Ports corresponding to
 * the PortIdentifiers for srcs and dests.
 *
 * Should be called after the ports are loaded from
 * yml.
 */
void
port_init_loaded (Port * this)
{
#define SET_FIELDS_FROM_ID(id,port) \
  switch (id->owner_type) \
    { \
    case PORT_OWNER_TYPE_PLUGIN: \
      port->track = \
        TRACKLIST->tracks[id->track_pos]; \
      g_warn_if_fail (port->track); \
      port->plugin = \
        port->track->channel->plugins[ \
          id->plugin_slot]; \
      g_warn_if_fail (port->plugin); \
      break; \
    case PORT_OWNER_TYPE_TRACK: \
      port->track = \
        TRACKLIST->tracks[id->track_pos]; \
      g_warn_if_fail (port->track); \
      break; \
    default: \
      break; \
    }

  /* find plugin and track */
  SET_FIELDS_FROM_ID ((&this->identifier),this);


  PortIdentifier * id;
  for (int i = 0; i < this->num_srcs; i++)
    {
      id = &this->src_ids[i];
      this->srcs[i] = port_find_from_identifier (id);
      g_warn_if_fail (this->srcs[i]);
    }
  for (int i = 0; i < this->num_dests; i++)
    {
      id = &this->dest_ids[i];
      this->dests[i] =
        port_find_from_identifier (id);
      g_warn_if_fail (this->dests[i]);
    }
}

/**
 * Finds the Port corresponding to the identifier.
 *
 * @param id The PortIdentifier to use for
 *   searching.
 */
Port *
port_find_from_identifier (
  PortIdentifier * id)
{
  Track * tr;
  Channel * ch;
  Plugin * pl;
  switch (id->owner_type)
    {
    case PORT_OWNER_TYPE_BACKEND:
      switch (id->type)
        {
        case TYPE_EVENT:
          if (id->flow == FLOW_OUTPUT)
            { /* TODO */ }
          else if (id->flow == FLOW_INPUT)
            {
              if (id->flags & PORT_FLAG_MANUAL_PRESS)
                return
                  AUDIO_ENGINE->
                    midi_editor_manual_press;
              else
                return AUDIO_ENGINE->midi_in;
            }
          break;
        case TYPE_AUDIO:
          if (id->flow == FLOW_OUTPUT)
            {
              if (id->flags & PORT_FLAG_STEREO_L)
                return AUDIO_ENGINE->stereo_out->l;
              else if (id->flags &
                         PORT_FLAG_STEREO_R)
                return AUDIO_ENGINE->stereo_out->r;
            }
          else if (id->flow == FLOW_INPUT)
            {
              if (id->flags & PORT_FLAG_STEREO_L)
                return AUDIO_ENGINE->stereo_in->l;
              else if (id->flags &
                         PORT_FLAG_STEREO_R)
                return AUDIO_ENGINE->stereo_in->r;
            }
          break;
        default:
          break;
        }
      break;
    case PORT_OWNER_TYPE_PLUGIN:
      tr =
        TRACKLIST->tracks[id->track_pos];
      g_warn_if_fail (tr);
      pl =
        tr->channel->plugins[id->plugin_slot];
      g_warn_if_fail (pl);
      switch (id->flow)
        {
        case FLOW_INPUT:
          return
            pl->in_ports[id->port_index];
          break;
        case FLOW_OUTPUT:
          return
            pl->out_ports[id->port_index];
          break;
        case FLOW_UNKNOWN:
          return
            pl->unknown_ports[id->port_index];
          break;
        }
      break;
    case PORT_OWNER_TYPE_TRACK:
      tr = TRACKLIST->tracks[id->track_pos];
      g_warn_if_fail (tr);
      ch = tr->channel;
      g_warn_if_fail (ch);
      switch (id->type)
        {
        case TYPE_EVENT:
          if (id->flow == FLOW_OUTPUT)
            { /* TODO */ }
          else if (id->flow == FLOW_INPUT)
            {
              if (id->flags & PORT_FLAG_PIANO_ROLL)
                return ch->piano_roll;
              else
                return ch->midi_in;
            }
          break;
        case TYPE_AUDIO:
          if (id->flow == FLOW_OUTPUT)
            {
              if (id->flags & PORT_FLAG_STEREO_L)
                return ch->stereo_out->l;
              else if (id->flags &
                         PORT_FLAG_STEREO_R)
                return ch->stereo_out->r;
            }
          else if (id->flow == FLOW_INPUT)
            {
              if (id->flags & PORT_FLAG_STEREO_L)
                return ch->stereo_in->l;
              else if (id->flags &
                         PORT_FLAG_STEREO_R)
                return ch->stereo_in->r;
            }
          break;
        default:
          break;
        }
      break;
    case PORT_OWNER_TYPE_FADER:
      tr = TRACKLIST->tracks[id->track_pos];
      g_warn_if_fail (tr);
      ch = tr->channel;
      g_warn_if_fail (ch);
      if (id->flow == FLOW_OUTPUT)
        {
          if (id->flags & PORT_FLAG_STEREO_L)
            return ch->fader.stereo_out->l;
          else if (id->flags &
                     PORT_FLAG_STEREO_R)
            return ch->fader.stereo_out->r;
        }
      else if (id->flow == FLOW_INPUT)
        {
          if (id->flags & PORT_FLAG_STEREO_L)
            return ch->fader.stereo_in->r;
          else if (id->flags &
                     PORT_FLAG_STEREO_R)
            return ch->fader.stereo_in->r;
        }
      break;
    case PORT_OWNER_TYPE_PREFADER:
      tr = TRACKLIST->tracks[id->track_pos];
      g_warn_if_fail (tr);
      ch = tr->channel;
      g_warn_if_fail (ch);
      if (id->flow == FLOW_OUTPUT)
        {
          if (id->flags & PORT_FLAG_STEREO_L)
            return ch->prefader.stereo_out->l;
          else if (id->flags &
                     PORT_FLAG_STEREO_R)
            return ch->prefader.stereo_out->r;
        }
      else if (id->flow == FLOW_INPUT)
        {
          if (id->flags & PORT_FLAG_STEREO_L)
            return ch->prefader.stereo_in->r;
          else if (id->flags &
                     PORT_FLAG_STEREO_R)
            return ch->prefader.stereo_in->r;
        }
      break;
    case PORT_OWNER_TYPE_SAMPLE_PROCESSOR:
      if (id->flags & PORT_FLAG_STEREO_L)
        return SAMPLE_PROCESSOR->stereo_out->l;
      else if (id->flags &
                 PORT_FLAG_STEREO_R)
        return SAMPLE_PROCESSOR->stereo_out->r;
    }

  g_return_val_if_reached (NULL);
}

void
stereo_ports_init_loaded (StereoPorts * sp)
{
  port_init_loaded (sp->l);
  port_init_loaded (sp->r);
}

/**
 * Creates port.
 *
 * Sets id and updates appropriate counters.
 */
Port *
port_new (char * label)
{
  Port * port = calloc (1, sizeof (Port));

  port->identifier.plugin_slot = -1;
  port->identifier.track_pos = -1;

  port->num_dests = 0;
  port->buf =
    calloc (AUDIO_ENGINE->block_length,
            sizeof (float));
  port->identifier.flow = FLOW_UNKNOWN;
  port->identifier.label = g_strdup (label);

  g_message ("[port_new] Creating port %s",
             port->identifier.label);
  return port;
}

/**
 * Creates port.
 */
Port *
port_new_with_type (PortType     type,
                    PortFlow     flow,
                    char         * label)
{
  Port * port = port_new (label);

  port->identifier.type = type;
  if (port->identifier.type == TYPE_EVENT)
    port->midi_events = midi_events_new (port);
  port->identifier.flow = flow;

  return port;
}

/**
 * Creates stereo ports.
 */
StereoPorts *
stereo_ports_new (Port * l, Port * r)
{
  StereoPorts * sp =
    calloc (1, sizeof (StereoPorts));
  sp->l = l;
  sp->r = r;

  return sp;
}

/**
 * Gathers all ports in the project and puts them
 * in the given array and size.
 */
void
port_get_all (
  Port ** ports,
  int *   size)
{
  *size = 0;

#define _ADD(port) \
  array_append ( \
    ports, (*size), \
    port)

  _ADD (AUDIO_ENGINE->stereo_in->l);
  _ADD (AUDIO_ENGINE->stereo_in->r);
  _ADD (AUDIO_ENGINE->stereo_out->l);
  _ADD (AUDIO_ENGINE->stereo_out->r);
  _ADD (AUDIO_ENGINE->midi_in);
  _ADD (AUDIO_ENGINE->midi_out);
  _ADD (AUDIO_ENGINE->midi_editor_manual_press);
  _ADD (SAMPLE_PROCESSOR->stereo_out->l);
  _ADD (SAMPLE_PROCESSOR->stereo_out->r);

  Plugin * pl;
  Channel * ch;
  Track * tr;
  int i, j, k;
  for (i = 0; i < TRACKLIST->num_tracks; i++)
    {
      tr = TRACKLIST->tracks[i];
      if (!tr->channel)
        continue;
      ch = tr->channel;

      /* add channel ports */
      _ADD (ch->stereo_in->l);
      _ADD (ch->stereo_in->r);
      _ADD (ch->stereo_out->l);
      _ADD (ch->stereo_out->r);
      _ADD (ch->piano_roll);
      _ADD (ch->midi_in);
      _ADD (ch->midi_out);

      /* add fader ports */
      _ADD (ch->fader.stereo_in->l);
      _ADD (ch->fader.stereo_in->r);
      _ADD (ch->fader.stereo_out->l);
      _ADD (ch->fader.stereo_out->r);

      /* add prefader ports */
      _ADD (ch->prefader.stereo_in->l);
      _ADD (ch->prefader.stereo_in->r);
      _ADD (ch->prefader.stereo_out->l);
      _ADD (ch->prefader.stereo_out->r);

#define ADD_PLUGIN_PORTS \
          if (!pl) \
            continue; \
 \
          for (k = 0; k < pl->num_in_ports; k++) \
            _ADD (pl->in_ports[k]); \
          for (k = 0; k < pl->num_out_ports; k++) \
            _ADD (pl->out_ports[k])

      /* add plugin ports */
      for (j = 0; j < STRIP_SIZE; j++)
        {
          pl = tr->channel->plugins[j];

          ADD_PLUGIN_PORTS;
        }
      for (j = 0; j < tr->num_modulators; j++)
        {
          pl = tr->modulators[j]->plugin;

          ADD_PLUGIN_PORTS;
        }
#undef ADD_PLUGIN_PORTS
    }
}

/**
 * Creates port and adds given data to it.
 */
Port *
port_new_with_data (PortInternalType internal_type, ///< the internal data format
                    PortType     type,
                    PortFlow     flow,
                    char         * label,
                    void         * data)   ///< the data
{
  Port * port = port_new_with_type (type, flow, label);

  /** TODO semaphore **/
  port->data = data;
  port->internal_type = internal_type;

  /** TODO end semaphore */

  return port;
}

/**
 * Sets the owner plugin & its slot.
 */
void
port_set_owner_plugin (
  Port *   port,
  Plugin * pl)
{
  g_warn_if_fail (port && pl);

  port->plugin = pl;
  port->track = pl->track;
  port->identifier.track_pos = pl->track->pos;
  port->identifier.plugin_slot = pl->slot;
  port->identifier.owner_type =
    PORT_OWNER_TYPE_PLUGIN;
}

/**
 * Sets the owner sample processor.
 */
void
port_set_owner_sample_processor (
  Port *   port,
  SampleProcessor * sample_processor)
{
  port->sample_processor = sample_processor;
  port->identifier.owner_type =
    PORT_OWNER_TYPE_SAMPLE_PROCESSOR;
}

/**
 * Sets the owner track & its ID.
 */
void
port_set_owner_track (
  Port *    port,
  Track *   track)
{
  g_warn_if_fail (port && track);

  port->track = track;
  port->identifier.track_pos = track->pos;
  port->identifier.owner_type =
    PORT_OWNER_TYPE_TRACK;
}

/**
 * Sets the owner fader & its ID.
 */
void
port_set_owner_fader (
  Port *    port,
  Fader *   fader)
{
  g_warn_if_fail (port && fader);

  port->track = fader->channel->track;
  port->identifier.track_pos =
    fader->channel->track->pos;
  port->identifier.owner_type =
    PORT_OWNER_TYPE_FADER;
}

/**
 * Sets the owner fader & its ID.
 */
void
port_set_owner_prefader (
  Port *                 port,
  PassthroughProcessor * fader)
{
  g_warn_if_fail (port && fader);

  port->track = fader->channel->track;
  port->identifier.track_pos =
    fader->channel->track->pos;
  port->identifier.owner_type =
    PORT_OWNER_TYPE_PREFADER;
}

/**
 * Returns whether the Port's can be connected (if
 * the connection will be valid and won't break the
 * acyclicity of the graph).
 */
int
ports_can_be_connected (
  const Port * src,
  const Port *dest)
{
  Graph * graph =
    graph_new (&MIXER->router, src, dest);
  if (graph)
    {
      graph_free (graph);
      return 1;
    }
  else
    return 0;
}

/**
 * Connets src to dest.
 *
 * @param locked Lock the connection or not.
 */
int
port_connect (
  Port * src,
  Port * dest,
  const int locked)
{
  g_warn_if_fail (src != NULL);
  g_warn_if_fail (dest != NULL);
  port_disconnect (src, dest);
  if ((src->identifier.type !=
       dest->identifier.type) &&
      !(src->identifier.type == TYPE_CV &&
        dest->identifier.type == TYPE_CONTROL))
    {
      g_warning ("Cannot connect ports, incompatible types");
      return -1;
    }
  src->dests[src->num_dests] = dest;
  port_identifier_copy (
    &dest->identifier,
    &src->dest_ids[src->num_dests]);
  src->multipliers[src->num_dests] = 1.f;
  src->dest_locked[src->num_dests] = locked;
  src->dest_enabled[src->num_dests] = 1;
  src->num_dests++;
  dest->srcs[dest->num_srcs] = src;
  port_identifier_copy (
    &src->identifier,
    &dest->src_ids[dest->num_srcs]);
  dest->num_srcs++;

  /* set base value if cv -> control */
  if (src->identifier.type == TYPE_CV &&
      dest->identifier.type == TYPE_CONTROL)
    {
      if (dest->internal_type == INTERNAL_LV2_PORT)
        {
          dest->base_value =
            dest->lv2_port->control;
        }
      /*dest->has_modulators = 1;*/
    }

  g_message ("Connected port (%s) to (%s)",
             src->identifier.label,
             dest->identifier.label);
  return 0;
}

/**
 * Disconnects src from dest.
 */
int
port_disconnect (Port * src, Port * dest)
{
  if (!src || !dest)
    g_warn_if_reached ();

  int pos = -1;
  /* disconnect dest from src */
  array_delete_return_pos (
    src->dests,
    src->num_dests,
    dest,
    pos);
  if (pos >= 0)
    {
      for (int i = pos;
           i < src->num_dests;
           i++)
        {
          port_identifier_copy (
            &src->dest_ids[i + 1],
            &src->dest_ids[i]);
        }
    }

  /* disconnect src from dest */
  pos = -1;
  array_delete_return_pos (
    dest->srcs,
    dest->num_srcs,
    src,
    pos);
  if (pos >= 0)
    {
      for (int i = pos;
           i < dest->num_srcs;
           i++)
        {
          port_identifier_copy (
            &dest->src_ids[i + 1],
            &dest->src_ids[i]);
        }
    }

  g_message ("Disconnected port (%s) from (%s)",
             src->identifier.label,
             dest->identifier.label);
  return 0;
}

/**
 * Returns if the two ports are connected or not.
 */
int
ports_connected (Port * src, Port * dest)
{
  if (array_contains ((void **)src->dests,
                      src->num_dests,
                      dest))
    return 1;
  return 0;
}

/**
 * Disconnects all srcs and dests from port.
 */
int
port_disconnect_all (Port * port)
{
  g_warn_if_fail (port);

  FOREACH_SRCS (port)
    {
      Port * src = port->srcs[i];
      port_disconnect (src, port);
    }

  FOREACH_DESTS (port)
    {
      Port * dest = port->dests[i];
      port_disconnect (port, dest);
    }

  return 0;
}

/**
 * Removes all the given ports from the project,
 * optionally freeing them.
 */
int
ports_remove (
  Port ** ports,
  int *   num_ports)
{
  int i;
  Port * port;

  /* go through each port */
  for (i = 0; i < *num_ports; i++)
    {
      port = ports[i];

      /* assert no connections */
      g_warn_if_fail (port->num_srcs == 0);
      g_warn_if_fail (port->num_dests == 0);

      free_later (port, port_free);
    }
  * num_ports = 0;

  return i;
}

/**
 * Apply given fader value to port.
 *
 * @param start_frame The start frame offset from
 *   0 in this cycle.
 * @param nframes The number of frames to process.
 */
void
port_apply_fader (
  Port *    port,
  float     amp,
  const int start_frame,
  const int nframes)
{
  for (int i = start_frame;
       i < start_frame + nframes; i++)
    {
      if (port->buf[i] != 0.f)
        port->buf[i] *= amp;
    }
}

void
port_receive_midi_events_from_jack (
  Port *      self,
  int         start_frame,
  int   nframes)
{
  if (self->internal_type !=
        INTERNAL_JACK_PORT ||
      self->identifier.type !=
        TYPE_EVENT)
    return;

  void * port_buf =
    jack_port_get_buffer (
      JACK_PORT_T (self->data), nframes);
  int num_events =
    jack_midi_get_event_count (port_buf);

  jack_midi_event_t jack_ev;
  for(int i = 0; i < num_events; i++)
    {
      jack_midi_event_get (
        &jack_ev, port_buf, i);

      if (jack_ev.time >= start_frame &&
          jack_ev.time < start_frame + nframes)
        {
          midi_events_add_event_from_buf (
            self->midi_events,
            jack_ev.time, jack_ev.buffer,
            jack_ev.size);
        }
    }

  if(self->midi_events->num_events > 0)
    g_message ("JACK MIDI (%s): have %d events",
               self->identifier.label,
               num_events);
}

void
port_receive_audio_data_from_jack (
  Port *      port,
  int         start_frames,
  int   nframes)
{
  if (port->internal_type !=
        INTERNAL_JACK_PORT ||
      port->identifier.type !=
        TYPE_AUDIO)
    return;

  float * in;
  in =
    (float *)
    jack_port_get_buffer (
      JACK_PORT_T (port->data),
      AUDIO_ENGINE->nframes);

  for (int i = start_frames;
       i < start_frames + nframes; i++)
    {
      port->buf[i] +=
        in[i];
    }
}

void
port_send_midi_events_to_jack (
  Port *      port,
  int         start_frames,
  int   nframes)
{
  if (port->internal_type !=
        INTERNAL_JACK_PORT ||
      port->identifier.type !=
        TYPE_EVENT)
    return;

  midi_events_copy_to_jack (
    port->midi_events,
    jack_port_get_buffer (
      JACK_PORT_T (port->data),
      AUDIO_ENGINE->nframes));
}

void
port_send_audio_data_to_jack (
  Port *      port,
  int         start_frames,
  int         nframes)
{
  if (port->internal_type !=
        INTERNAL_JACK_PORT ||
      port->identifier.type !=
        TYPE_AUDIO)
    return;

  float * out;
  out =
    (float *)
    jack_port_get_buffer (
      JACK_PORT_T (port->data),
      AUDIO_ENGINE->nframes);

  for (int i = start_frames;
       i < start_frames + nframes; i++)
    {
      out[i] = port->buf[i];
    }
}

/**
 * Sums the inputs coming in from JACK, before the
 * port is processed.
 */
void
port_sum_data_from_jack (
  Port * self,
  const int start_frame,
  const int nframes)
{
  if (self->identifier.owner_type ==
        PORT_OWNER_TYPE_BACKEND ||
      self->internal_type !=
        INTERNAL_JACK_PORT ||
      self->identifier.flow !=
        FLOW_INPUT)
    return;

  /* append events from JACK if any */
  port_receive_midi_events_from_jack (
    self, start_frame, nframes);

  /* audio */
  port_receive_audio_data_from_jack (
    self, start_frame, nframes);
}

/**
 * Sends the port data to JACK, after the port
 * is processed.
 */
void
port_send_data_to_jack (
  Port * self,
  const int start_frame,
  const int nframes)
{
  if (self->identifier.owner_type ==
        PORT_OWNER_TYPE_BACKEND ||
      self->internal_type !=
        INTERNAL_JACK_PORT ||
      self->identifier.flow !=
        FLOW_OUTPUT)
    return;

  /* send midi events */
  port_send_midi_events_to_jack (
    self, start_frame, nframes);

  /* send audio data */
  port_send_audio_data_to_jack (
    self, start_frame, nframes);
}

/**
 * First sets port buf to 0, then sums the given
 * port signal from its inputs.
 *
 * @param start_frame The start frame offset from
 *   0 in this cycle.
 * @param nframes The number of frames to process.
 * @param noroll Clear the port buffer in this
 *   range.
 */
void
port_sum_signal_from_inputs (
  Port *    port,
  const int start_frame,
  const int nframes,
  const int noroll)
{
  Port * src_port;
  int k, l;

  g_warn_if_fail (
    start_frame + nframes <=
    AUDIO_ENGINE->nframes);

  switch (port->identifier.type)
    {
    case TYPE_EVENT:
      if (noroll)
        break;

      port_sum_data_from_jack (
        port, start_frame, nframes);

      for (k = 0; k < port->num_srcs; k++)
        {
          src_port = port->srcs[k];
          g_warn_if_fail (
            src_port->identifier.type ==
              TYPE_EVENT);
          midi_events_append (
            src_port->midi_events,
            port->midi_events, start_frame,
            nframes, 0);
        }

      port_send_data_to_jack (
        port, start_frame, nframes);

      /* send UI notification */
      if (port->midi_events->num_events > 0)
        {
          g_message ("port %s has %d events",
                     port->identifier.label,
                     port->midi_events->num_events);
          /*if (port == AUDIO_ENGINE->midi_in)*/
            /*{*/
              /*AUDIO_ENGINE->trigger_midi_activity = 1;*/
            /*}*/
          if (port->identifier.owner_type ==
                PORT_OWNER_TYPE_TRACK)
            {
              port->track->trigger_midi_activity = 1;
            }
        }
      break;
    case TYPE_AUDIO:
      if (noroll)
        {
          for (l = start_frame; l < nframes; l++)
            {
              port->buf[l] = 0;
            }
          break;
        }

      port_sum_data_from_jack (
        port, start_frame, nframes);

      for (k = 0; k < port->num_srcs; k++)
        {
          src_port = port->srcs[k];

          /* sum the signals */
          /*g_message ("port %s start frame %ld nframes %d",*/
                     /*port->identifier.label,*/
                     /*start_frame, nframes);*/
          for (l = start_frame; l < nframes; l++)
            {
              port->buf[l] += src_port->buf[l];
            }
        }

      port_send_data_to_jack (
        port, start_frame, nframes);
      break;
    case TYPE_CONTROL:
      {
        float maxf, minf, depth_range, val_to_use;
        /* whether this is the first CV processed
         * on this control port */
        int first_cv = 1;
        for (k = 0; k < port->num_srcs; k++)
          {
            src_port = port->srcs[k];

            if (src_port->identifier.type ==
                  TYPE_CV)
              {
                maxf =
                  port->lv2_port->lv2_control->maxf;
                minf =
                  port->lv2_port->lv2_control->minf;
                /*float deff =*/
                  /*port->lv2_port->lv2_control->deff;*/
                /*port->lv2_port->control =*/
                  /*deff + (maxf - deff) **/
                    /*src_port->buf[0];*/
                depth_range =
                  (maxf - minf) / 2.f;

                /* figure out whether to use base
                 * value or the current value */
                if (first_cv)
                  {
                    val_to_use = port->base_value;
                    first_cv = 0;
                  }
                else
                  val_to_use =
                    port->lv2_port->control;

                /* TODO replace 1.f with modulator
                 * depth */
                port->lv2_port->control =
                  CLAMP (
                    val_to_use +
                      depth_range * src_port->buf[0] *
                      src_port->multipliers[
                        port_get_dest_index (
                          src_port, port)],
                    minf, maxf);
              }
          }
      }
      break;
    default:
      break;
    }
}

#ifdef HAVE_JACK
/**
 * Sets whether to expose the port to JACk and
 * exposes it or removes it from JACK.
 */
void
port_set_expose_to_jack (
  Port * self,
  int    expose)
{
  enum JackPortFlags flags;
  if (self->identifier.flow == FLOW_INPUT)
    flags = JackPortIsInput;
  else if (self->identifier.flow == FLOW_OUTPUT)
    flags = JackPortIsOutput;
  else
    g_return_if_reached ();

  char * type = NULL;
  if (self->identifier.type == TYPE_AUDIO)
    type = JACK_DEFAULT_AUDIO_TYPE;
  else if (self->identifier.type == TYPE_EVENT)
    type = JACK_DEFAULT_MIDI_TYPE;
  else
    g_return_if_reached ();

  if (expose)
    {
      char * label =
        port_get_full_designation (self);
      self->data =
        (void *) jack_port_register (
          AUDIO_ENGINE->client,
          label,
          type, flags, 0);
      g_free (label);
      self->internal_type = INTERNAL_JACK_PORT;
    }
  else
    {
      jack_port_unregister (
        AUDIO_ENGINE->client,
        JACK_PORT_T (self->data));
    }
}
#endif

/**
 * Returns a full designation of the port in the
 * format "Track/Port" or "Track/Plugin/Port".
 *
 * Must be free'd.
 */
char *
port_get_full_designation (
  Port * self)
{
  PortIdentifier * id = &self->identifier;

  switch (id->owner_type)
    {
    case PORT_OWNER_TYPE_BACKEND:
    case PORT_OWNER_TYPE_SAMPLE_PROCESSOR:
      return g_strdup (id->label);
      break;
    case PORT_OWNER_TYPE_PLUGIN:
      return
        g_strdup_printf (
          "%s/%s/%s",
          self->plugin->track->name,
          self->plugin->descr->name,
          id->label);
      break;
    case PORT_OWNER_TYPE_TRACK:
    case PORT_OWNER_TYPE_PREFADER:
    case PORT_OWNER_TYPE_FADER:
      return
        g_strdup_printf (
          "%s/%s",
          self->track->name,
          id->label);
      break;
    default:
      g_return_val_if_reached (NULL);
    }
}

/**
 * Prints all connections.
 */
void
port_print_connections_all ()
{
  /*Port * src, * dest;*/
  /*int i, j;*/

  /*for (i = 0; i < PROJECT->num_ports; i++)*/
    /*{*/
      /*src = project_get_port (i);*/
      /*if (!src->owner_pl &&*/
          /*!src->owner_tr &&*/
          /*!src->owner_backend)*/
        /*{*/
          /*g_warning ("Port %s has no owner",*/
                     /*src->identifier.label);*/
        /*}*/
      /*for (j = 0; j < src->num_dests; j++)*/
        /*{*/
          /*dest = src->dests[j];*/
          /*g_warn_if_fail (dest);*/
          /*g_message ("%s connected to %s", src->identifier.label, dest->identifier.label);*/
        /*}*/
    /*}*/
}

/**
 * Clears the port buffer.
 */
void
port_clear_buffer (Port * port)
{
  if (port->identifier.type == TYPE_AUDIO &&
        port->buf)
    {
      memset (
        port->buf, 0,
        AUDIO_ENGINE->block_length *
          sizeof (float));
      return;
    }
  if (port->identifier.type == TYPE_EVENT &&
      port->midi_events)
    {
      port->midi_events->num_events = 0;
    }
}

/**
 * Applies the pan to the given L/R ports.
 */
void
port_apply_pan_stereo (Port *       l,
                       Port *       r,
                       float        pan,
                       PanLaw       pan_law,
                       PanAlgorithm pan_algo)
{
  /* FIXME not used */
  g_warn_if_reached ();
  /*int nframes = AUDIO_ENGINE->block_length;*/
  /*int i;*/
  /*switch (pan_algo)*/
    /*{*/
    /*case PAN_ALGORITHM_SINE_LAW:*/
      /*for (i = 0; i < nframes; i++)*/
        /*{*/
          /*if (r->buf[i] != 0.f)*/
            /*r->buf[i] *= sinf (pan * (M_PIF / 2.f));*/
          /*if (l->buf[i] != 0.f)*/
            /*l->buf[i] *= sinf ((1.f - pan) * (M_PIF / 2.f));*/
        /*}*/
      /*break;*/
    /*case PAN_ALGORITHM_SQUARE_ROOT:*/
      /*break;*/
    /*case PAN_ALGORITHM_LINEAR:*/
      /*for (i = 0; i < nframes; i++)*/
        /*{*/
          /*if (r->buf[i] != 0.f)*/
            /*r->buf[i] *= pan;*/
          /*if (l->buf[i] != 0.f)*/
            /*l->buf[i] *= (1.f - pan);*/
        /*}*/
      /*break;*/
    /*}*/
}

/**
 * Applies the pan to the given port.
 *
 * @param start_frame The start frame offset from
 *   0 in this cycle.
 * @param nframes The number of frames to process.
 */
void
port_apply_pan (
  Port *       port,
  float        pan,
  PanLaw       pan_law,
  PanAlgorithm pan_algo,
  const int    start_frame,
  const int    nframes)
{
  int i;
  float calc_r = 0.f, calc_l = 0.f;
  int is_stereo_r =
    port->identifier.flags & PORT_FLAG_STEREO_R;

  switch (pan_algo)
    {
    case PAN_ALGORITHM_SINE_LAW:
      calc_l = sinf ((1.f - pan) * (M_PIF / 2.f));
      calc_r = sinf (pan * (M_PIF / 2.f));
      break;
    case PAN_ALGORITHM_SQUARE_ROOT:
      calc_l = sqrtf (1.f - pan);
      calc_r = sqrtf (pan);
      break;
    case PAN_ALGORITHM_LINEAR:
      calc_l = 1.f - pan;
      calc_r = pan;
      break;
    }

  for (i = start_frame;
       i < start_frame + nframes; i++)
    {
      if (port->buf[i] == 0.f)
        continue;

      if (is_stereo_r)
        {
          port->buf[i] *= calc_r;
          continue;
        }

      port->buf[i] *= calc_l;
    }
}

/**
 * Deletes port, doing required cleanup and updating counters.
 */
void
port_free (Port * port)
{
  /* assert no connections */
  g_warn_if_fail (port->num_srcs == 0);
  g_warn_if_fail (port->num_dests == 0);

  if (port->identifier.label)
    g_free (port->identifier.label);
  if (port->buf)
    free (port->buf);

  free (port);
}
