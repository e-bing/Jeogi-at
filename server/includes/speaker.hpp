#ifndef SPEAKER_HPP
#define SPEAKER_HPP

#include <string>

void init_mqtt_speaker();
void send_speaker_command(const std::string& action);

#endif // SPEAKER_HPP
