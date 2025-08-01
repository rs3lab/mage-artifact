#include "disagg_switch.hpp"
#include "disagg_switch_config.hpp"
#include "disagg_pkt_record.hpp"
#include <stdlib.h>
#include <string.h>
// other includes...

/*
#include <fstream>
#include <iostream>

static std::ofstream packet_record;
static int _file_opened = 0;

void init_packet_recording(std::string packet_record_file)
{
    if (_file_opened)
    {
        std::cerr << __func__ << " | Error: file already opened" << std::endl;
        return;
    }
    packet_record = std::ofstream(packet_record_file, std::ios::app);
    _file_opened = 1;
}

void close_packet_recording(void)
{
    if (!_file_opened)
    {
        std::cerr << __func__ << " | Error: file not opened" << std::endl;
        return;
    }
    packet_record.close();
    _file_opened = 0;
}

extern "C"
void record_packet(const void *buf, unsigned int len)
{
    if (!_file_opened)
    {
        std::cerr << __func__ << " | Error: file not opened" << std::endl;
        return;
    }
    packet_record.write((char *)buf, len);
}
*/

PacketRecorder::PacketRecorder(const std::string& filename, bool isRecording) 
    : isRecording(isRecording), counter(0), processed_counter(0),
      record(isRecording ? new std::ofstream(filename, std::ios::binary | std::ios::trunc) : nullptr),  // overide the file if it exists
      replay(!isRecording ? new std::ifstream(filename, std::ios::binary) : nullptr),
      filename(filename) {
    if (isRecording && !*record) {
        throw std::runtime_error("Could not open file for recording");
    }
    if (!isRecording && !*replay) {
        throw std::runtime_error("Could not open file for replaying");
    }
}

PacketRecorder::~PacketRecorder() {
    if (record) record->close();
    if (replay) replay->close();
}

// void PacketRecorder::record_packet(int id, const char* ip_address, const void *buf, unsigned int len) {
//     if (!isRecording) {
//         throw std::runtime_error("Not in recording mode");
//     }
//     if (record->is_open()) {
//         record->close();
//     }
//     record->open(filename, std::ios::binary | std::ios::app);
//     std::lock_guard<std::mutex> lock(mutex);
//     Packet packet = {id, {0}, std::vector<char>(static_cast<const char*>(buf), static_cast<const char*>(buf) + len)};
//     strcpy(packet.ip_address, ip_address);
//     record->write(reinterpret_cast<char*>(&packet.id), sizeof(packet.id));
//     record->write(packet.ip_address, sizeof(packet.ip_address));
//     uint32_t size = packet.data.size();
//     record->write(reinterpret_cast<char*>(&size), sizeof(size));
//     record->write(packet.data.data(), packet.data.size());
//     if (!*record) {
//         throw std::runtime_error("Write to file failed");
//     }
//     record->close();
//     counter++;
// }

// bool PacketRecorder::next_packet(int id, char* ip_address, void *buf, unsigned int* len) {
//     if (isRecording) {
//         throw std::runtime_error("Not in replay mode");
//     }

//     std::lock_guard<std::mutex> lock(mutex);

//     // If there's a packet in the queue and it is for the current thread, remove it from the queue and return its data.
//     if (!packetQueue.empty()) {
//         std::cout << "next_packet: id=" << id << ", counter=" << counter << ", processed_counter=" << processed_counter << ", packetQueue.front().id=" << packetQueue.front().id << std::endl;

//         if (processed_counter < counter) {    // not yet processed
//             return false;
//         } else if (packetQueue.front().id == id) {
//             Packet packet = packetQueue.front();
//             packetQueue.pop();
//             strcpy(ip_address, packet.ip_address);
//             memcpy(buf, packet.data.data(), packet.data.size());
//             *len = packet.data.size();
//             counter ++;
//             return true;
//         } else {
//             return false;
//         }
//     }

//     // If the queue is empty, read the next packet from the file.
//     Packet packet;
//     if (!replay->read(reinterpret_cast<char*>(&packet.id), sizeof(packet.id))) {
//         return false;
//     }
//     if (!replay->read(packet.ip_address, sizeof(packet.ip_address))) {
//         return false;
//     }
//     uint32_t size;
//     if (!replay->read(reinterpret_cast<char*>(&size), sizeof(size))) {
//         return false;
//     }
//     packet.data.resize(size);
//     if (!replay->read(packet.data.data(), size)) {
//         return false;
//     }

//     // Push the packet to the queue and call `next_packet` recursively.
//     packetQueue.push(packet);
//     lock.~lock_guard(); // explicitly unlock the mutex
//     return next_packet(id, ip_address, buf, len);
// }

void PacketRecorder::record_packet(int id, const char* ip_address, const void *buf, unsigned int len) {
    if (!isRecording) {
        throw std::runtime_error("Not in recording mode");
    }
    std::lock_guard<std::mutex> lock(mutex);
    Packet packet = {id, counter++, {0}, std::vector<char>(static_cast<const char*>(buf), static_cast<const char*>(buf) + len)};
    strcpy(packet.ip_address, ip_address);
    json j_packet = packet;
    *record << j_packet.dump() << "\n";
    if (!*record) {
        throw std::runtime_error("Write to file failed");
    }
    record->flush(); // Make sure the packet is immediately written to the file
}

bool PacketRecorder::next_packet(int id, char* ip_address, void *buf, unsigned int* len) {
    if (isRecording) {
        throw std::runtime_error("Not in replay mode");
    }

    std::lock_guard<std::mutex> lock(mutex);
    std::cout << "next_packet: id=" << id << ", counter=" << counter << ", processed_counter=" << processed_counter << ", packetQueue.size()=" << packetQueue.size() << std::endl;

    // If there's a packet in the queue and it is for the current thread, remove it from the queue and return its data.
    if (!packetQueue.empty()) {
        if (packetQueue.front().contains("id")) {
            std::cout << "next_packet: id=" << id << ", counter=" << counter << ", processed_counter=" << processed_counter << ", packetQueue.front().id=" << packetQueue.front()["id"] << std::endl;
        } else {
            std::cerr << "Invalid packet format: missing 'id' key" << std::endl;
            return false;
        }
        if (processed_counter < counter) {    // not yet processed
            return false;
        } else if (packetQueue.front()["id"] == id) {
            nlohmann::json packetJson = packetQueue.front();
            packetQueue.pop();
            Packet packet;
            packet.id = packetJson.at("id").get<int>();
            packet.index = packetJson.at("index").get<int>();
            strcpy(packet.ip_address, packetJson.at("ip_address").get<std::string>().c_str());
            packet.data = packetJson.at("data").get<std::vector<char>>();
            // Packet packet = packetJson.get<Packet>();
            // strcpy(ip_address, packet.ip_address);
            memcpy(buf, packet.data.data(), packet.data.size());
            *len = packet.data.size();
            counter++;
            return true;
        } else {
            return false;
        }
    }

    // If the queue is empty, read the next packet from the file.
    std::string line;
    if (!std::getline(*replay, line)) {
        return false;
    }
    nlohmann::json packetJson = nlohmann::json::parse(line);

    // Push the packet to the queue and call `next_packet` recursively.
    std::cout << "read_packet: id=" << packetJson["id"] << ", counter=" << counter << ", processed_counter=" << processed_counter << ", packetQueue.size()=" << packetQueue.size() << std::endl;
    packetQueue.push(packetJson);
    std::cout << "Return false: id=" << id << ", counter=" << counter << ", processed_counter=" << processed_counter << ", packetQueue.size()=" << packetQueue.size() << std::endl; 
    // lock.~lock_guard(); // explicitly unlock the mutex
    return false;
}

void PacketRecorder::packet_served(void) {
    if (isRecording) {
        throw std::runtime_error("Not in recording mode");
    }

    std::lock_guard<std::mutex> lock(mutex);
    processed_counter++;
}

bool PacketRecorder::is_recording_mode() const {
    return isRecording;
}

extern "C" {
    void record_packet(int id, char* ip_address, const void *buf, unsigned int len) {
        if (!packetRecorder) {
            std::cerr << __func__ << " | Error: packet recorder not initialized" << std::endl;
            return;
        }
        packetRecorder->record_packet(id, ip_address, buf, len);
    }

    int next_packet(int id, char* ip_address, void *buf, unsigned int *size)
    {
        if (!packetRecorder)
        {
            std::cerr << __func__ << " | Error: packet recorder not initialized" << std::endl;
            return 0;
        }
        return packetRecorder->next_packet(id, ip_address, buf, size) ? 1 : 0;
    }

    void packet_served(void)
    {
        if (!packetRecorder)
        {
            std::cerr << __func__ << " | Error: packet recorder not initialized" << std::endl;
            return;
        }
        packetRecorder->packet_served();
    }

    int is_pkt_recording_mode(void)
    {
        if (!packetRecorder)
        {
            std::cerr << __func__ << " | Error: packet recorder not initialized" << std::endl;
            return 0;
        }
        return packetRecorder->is_recording_mode() ? 1 : 0;
    }
}

