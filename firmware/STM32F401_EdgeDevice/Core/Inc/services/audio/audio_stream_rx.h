/*
 * audio_stream_rx.h
 *
 *  Created on: 2026. 3. 19.
 *      Author: 2-21
 */

#ifndef SERVICES_AUDIO_AUDIO_STREAM_RX_H
#define SERVICES_AUDIO_AUDIO_STREAM_RX_H

#include <stdint.h>

void AudioStreamRx_Init(void);
void AudioStreamRx_Reset(void);
void AudioStreamRx_Start(void);
void AudioStreamRx_Process(void);

uint32_t AudioStreamRx_Available(void);
uint32_t AudioStreamRx_Read(uint8_t *dst, uint32_t len);

#endif /* SERVICES_AUDIO_AUDIO_STREAM_RX_H */
