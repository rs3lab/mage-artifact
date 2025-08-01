#ifndef __TNA_DISAGG_SWITCH_CONFIG_HPP__
#define __TNA_DISAGG_SWITCH_CONFIG_HPP__

#include <iostream>

void load_config(const std::string& configPath, int argc, char **argv);
void terminate_config(void);

extern "C"
unsigned int get_compute_start_ip_last_digits(void);
extern "C"
void set_compute_start_ip_last_digits(const std::string& ip_last_digit);

extern "C"
unsigned int get_memory_start_ip_last_digits(void);
extern "C"
void set_memory_start_ip_last_digits(const std::string& ip_last_digit);


extern "C"
int is_debug_mode(void);

extern "C"
int is_recording_mode(void);

extern "C"
int get_cluster_id(void);

#endif  //__TNA_DISAGG_SWITCH_CONFIG_HPP__