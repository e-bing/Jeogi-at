#ifndef DISPLAY_HPP
#define DISPLAY_HPP

#include <string>

void init_mqtt_display();
void send_display_command(const std::string& action);

#endif // DISPLAY_HPP
