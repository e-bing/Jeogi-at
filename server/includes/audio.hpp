#ifndef AUDIO_HPP
#define AUDIO_HPP

#include <string>

void init_mqtt_audio();
void send_audio_command(const std::string& action);

#endif // AUDIO_HPP
