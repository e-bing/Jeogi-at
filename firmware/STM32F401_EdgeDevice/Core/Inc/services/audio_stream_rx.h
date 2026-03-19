/*
 * audio_stream_rx.h
 *
 *  Created on: 2026. 3. 19.
 *      Author: 2-21
 */

#ifndef INC_SERVICES_AUDIO_STREAM_RX_H_
#define INC_SERVICES_AUDIO_STREAM_RX_H_

#include <stdint.h>

void AudioStreamRx_Init(void);
void AudioStreamRx_Start(void);
void AudioStreamRx_Reset(void);

uint32_t AudioStreamRx_Available(void);
uint32_t AudioStreamRx_Read(uint8_t *dst, uint32_t len);

#endif /* INC_SERVICES_AUDIO_STREAM_RX_H_ */
