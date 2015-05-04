/*
 * Copyright (C) 2015 - Martin Jaros <xjaros32@stud.feec.vutbr.cz>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <alsa/asoundlib.h>
#include <opus/opus.h>

#include "debug.h"
#include "media.h"

#define CAPTURE_CHANNELS    1
#define CAPTURE_RATE        48000
#define CAPTURE_LATENCY     40000

#define PLAYBACK_CHANNELS   2
#define PLAYBACK_RATE       48000
#define PLAYBACK_LATENCY    40000

struct media
{
    int timerfd;
    snd_pcm_t *playback, *capture;
    OpusEncoder *encoder;
    OpusDecoder *decoder;
};

size_t media_sizeof() { return sizeof(struct media) + opus_encoder_get_size(1) + opus_decoder_get_size(2); }

void media_init(struct media *media, const char *capture, const char *playback)
{
    if(snd_pcm_open(&media->playback, capture, SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK) ||
       snd_pcm_set_params(media->playback, SND_PCM_FORMAT_S16_LE, SND_PCM_ACCESS_RW_INTERLEAVED, PLAYBACK_CHANNELS, PLAYBACK_RATE, 1, PLAYBACK_LATENCY))
    { ERROR("Cannot open playback device"); abort(); }

    if(snd_pcm_open(&media->capture, playback, SND_PCM_STREAM_CAPTURE, SND_PCM_NONBLOCK) ||
       snd_pcm_set_params(media->capture, SND_PCM_FORMAT_S16_LE, SND_PCM_ACCESS_RW_INTERLEAVED, CAPTURE_CHANNELS, CAPTURE_RATE, 1, CAPTURE_LATENCY))
    { ERROR("Cannot open capture device"); abort(); }

    media->encoder = (OpusEncoder*)((void*)media + sizeof(struct media));
    int res = opus_encoder_init(media->encoder, CAPTURE_RATE, CAPTURE_CHANNELS, OPUS_APPLICATION_VOIP); assert(res == OPUS_OK);

    media->decoder = (OpusDecoder*)((void*)media + sizeof(struct media) + opus_encoder_get_size(1));
    res = opus_decoder_init(media->decoder, PLAYBACK_RATE, PLAYBACK_CHANNELS); assert(res == OPUS_OK);
}

void media_start(struct media *media)
{
    opus_decoder_ctl(media->decoder, OPUS_RESET_STATE);
    if(snd_pcm_prepare(media->playback) || snd_pcm_prepare(media->capture)) { ERROR("Cannot start device"); abort(); }
}

void media_stop(struct media *media)
{
    if(snd_pcm_drop(media->playback) || snd_pcm_drop(media->capture)) { ERROR("Cannot stop device"); abort(); }
}

size_t media_pull(struct media *media, uint8_t *buffer, size_t len)
{
    const int nsamples = CAPTURE_RATE / 50;
    int16_t samples[CAPTURE_CHANNELS * nsamples];
    int res = snd_pcm_readi(media->capture, samples, nsamples);
    if(res < 0)
    {
        WARN("%s", snd_strerror(res));
        res = snd_pcm_prepare(media->capture);
        if(res != 0) { ERROR("Cannot recover device"); abort(); }
    }
    DEBUG("Read %d/%d frames", res, nsamples);

    memset(samples + res, 0, 2 * (nsamples - res));
    res = opus_encode(media->encoder, samples, nsamples, buffer, len); assert(res > 0);
    return res;
}

void media_push(struct media *media, uint8_t *buffer, size_t len)
{
    if(buffer == NULL) { opus_decoder_ctl(media->decoder, OPUS_RESET_STATE); return; }
    int nsamples = opus_packet_get_samples_per_frame(buffer, PLAYBACK_RATE);
    if(nsamples < 0) { WARN("Invalid packet"); return; }

    int16_t samples[PLAYBACK_CHANNELS * nsamples];
    int res = opus_decode(media->decoder, buffer, len, samples, nsamples, 0);
    if(res == OPUS_INVALID_PACKET) { WARN("Invalid packet"); return; }; assert(res == nsamples);

    res = snd_pcm_writei(media->playback, samples, nsamples);
    if(res < 0)
    {
        WARN("%s", snd_strerror(res));
        res = snd_pcm_prepare(media->playback);
        if(res != 0) { ERROR("Cannot recover device"); abort(); }
    }
    DEBUG("Written %d/%d frames", res, nsamples);
}
