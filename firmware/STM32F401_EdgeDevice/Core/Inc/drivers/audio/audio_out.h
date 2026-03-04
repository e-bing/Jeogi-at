/*
 * audio_out.h
 *
 *  Created on: 2026. 2. 27.
 *      Author: Taewoo Yun
 */

#ifndef INC_DRIVERS_AUDIO_AUDIO_OUT_H_
#define INC_DRIVERS_AUDIO_AUDIO_OUT_H_

void audio_out_init();
void audio_out_start();
void audio_out_stop();
void audio_out_submit_buffer();
void audio_out_poll_events();

#endif /* INC_DRIVERS_AUDIO_AUDIO_OUT_H_ */
