/*
 * audio_out.h
 *
 *  Created on: 2026. 2. 27.
 *      Author: Taewoo Yun
 */

#ifndef DRIVERS_AUDIO_AUDIO_OUT_H
#define DRIVERS_AUDIO_AUDIO_OUT_H

void audio_out_init();
void audio_out_start();
void audio_out_stop();
void audio_out_submit_buffer();
void audio_out_poll_events();

#endif /* DRIVERS_AUDIO_AUDIO_OUT_H */
