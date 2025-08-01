#ifndef __DISAGG_PKT_RECORD_HPP__
#define __DISAGG_PKT_RECORD_HPP__

#include <vector>
#include <fstream>
#include <mutex>
#include <memory>
#include <atomic>
#include <condition_variable>
#include <unordered_map>
#include <queue>
#include "json.hpp"

using json = nlohmann::json;

class PacketRecorder {
public:
    // struct Packet {
    //     int id;
    //     char ip_address[16]; // Assuming IPv4
    //     std::vector<char> data;
    // };
    struct Packet {
        int id;
        int index; // New member variable for holding an incremental index
        char ip_address[16];
        std::vector<char> data;
        NLOHMANN_DEFINE_TYPE_INTRUSIVE(Packet, id, index, ip_address, data)
    };

    PacketRecorder(const std::string& filename, bool isRecording);
    ~PacketRecorder();
    void record_packet(int id, const char* ip_address, const void *buf, unsigned int len);
    bool next_packet(int id, char* ip_address, void *buf, unsigned int* len);
    void packet_served(void);
    bool is_recording_mode() const;

private:
    bool isRecording;
    std::atomic<int> counter;
    std::atomic<int> processed_counter;
    std::string filename;
    std::unique_ptr<std::ofstream> record;
    std::unique_ptr<std::ifstream> replay;
    std::queue<json> packetQueue;
    std::mutex mutex;
};

// Global instance that can be used from C code
extern PacketRecorder *packetRecorder;
#endif