#include <fstream>
#include <iostream>
#include "json.hpp"
#include "disagg_switch.hpp"
#include "disagg_switch_config.hpp"
#include "disagg_pkt_record.hpp"

static std::string compute_start_ip;
static std::string memory_start_ip;
static int debug_mode = 0;
static int record_mode = 0;
static std::string packet_record_file = "packets.dat";
static int cluster_id = 0;
PacketRecorder *packetRecorder = nullptr;

void load_config(const std::string& configPath, int argc, char **argv)
{
    std::ifstream config_file(configPath);
    nlohmann::json config;
    if (!config_file.good())
    {
        std::cerr << __func__ << " | Error opening config file: " << configPath << std::endl;
        exit(-1);
    }
    config_file >> config;
    compute_start_ip = config["compute_start_ip"];
    memory_start_ip = config["memory_start_ip"];
    debug_mode = config["debug_mode"];
    record_mode = config["recording_mode"];
    packet_record_file = config["packet_record_file"];
    cluster_id = config["cluster_id"];


    std::cout << "Is debug mode: " << debug_mode << std::endl;
    std::cout << "Is record mode: " << record_mode << std::endl;
    std::cout << "Packet recording path: " << packet_record_file << std::endl;
    std::cout << "Cluster ID: " << cluster_id << std::endl;

    if (debug_mode)
    {
        packetRecorder = new PacketRecorder(packet_record_file, record_mode);
    }

    // override config with command line arguments
    if (argc >= 2)
    {
        std::cout << "Last digit of compute blade starts at: " << argv[1] << std::endl;
        set_compute_start_ip_last_digits(argv[1]);
    }
    if (argc >= 3)
    {
        std::cout << "Last digit of memory blade starts at: " << argv[2] << std::endl;
        set_memory_start_ip_last_digits(argv[2]);
    }
}

extern "C"
unsigned int get_compute_start_ip_last_digits(void)
{
    // change the string into unsigned int
    return std::stoul(compute_start_ip);
}

extern "C"
void set_compute_start_ip_last_digits(const std::string& ip_last_digit)
{
    compute_start_ip = ip_last_digit;
}

extern "C"
unsigned int get_memory_start_ip_last_digits(void)
{
    // change the string into unsigned int
    return std::stoul(memory_start_ip);
}

extern "C"
void set_memory_start_ip_last_digits(const std::string& ip_last_digit)
{
    memory_start_ip = ip_last_digit;
}

extern "C"
int is_debug_mode(void)
{
    return debug_mode;
}

extern "C"
int is_recording_mode(void)
{
    return record_mode;
}

extern "C"
int get_cluster_id(void)
{
    return cluster_id;
}

void terminate_config(void)
{
    if (packetRecorder)
    {
        delete packetRecorder;  // close the file and exit
        packetRecorder = nullptr;
    }
}
