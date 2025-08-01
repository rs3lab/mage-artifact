#ifndef __DISAGG_BF_CLIENT_HPP__
#define __DISAGG_BF_CLIENT_HPP__

#include <thrift/transport/TSocket.h>
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/transport/TTransportUtils.h>
// #include <boost/shared_ptr.hpp>
// #include <boost/make_shared.hpp>
#include <iostream>
#include <fstream>

#include "gen-cpp/CacheService.h"
#include "json.hpp"

using namespace apache::thrift;
using namespace apache::thrift::protocol;
using namespace apache::thrift::transport;

typedef struct CachelineRegData
{
    uint32_t cache_idx;
    uint16_t state;
    uint16_t sharer;
    uint16_t dir_size;
    uint16_t dir_lock;
    uint32_t inv_cnt;
    uint16_t update;
} CachelineRegData;

typedef struct CachestateData
{
    uint8_t cur_state;
    uint8_t write_req;
    uint8_t next_state;
    uint8_t reset_sharer;
} CachestateData;

typedef struct AddrTransData
{
    uint64_t vaddr;
    uint16_t vaddr_prefix;
    const char *dst_ip_addr;
    uint64_t va_to_dma;
} AddrTransData;

typedef struct AddrExceptTransData
{
    uint64_t vaddr;
    uint16_t vaddr_prefix;
    const char *dst_ip_addr;
    uint64_t va_to_dma;
    uint8_t permission;
    int32_t account;
} AddrExceptTransData;

typedef struct RoceReqData {
  const char *src_ip_addr;
  const char *dst_ip_addr;
  uint32_t qp;
  uint32_t new_qp;
  uint32_t rkey;
  uint16_t reg_idx;
} RoceReqData;

typedef struct RoceAckData {
  uint32_t qp;
  uint32_t new_qp;
  const char *new_ip_addr;
  uint16_t reg_idx;
} RoceAckData;

typedef struct RoceDummyAckData {
  uint32_t qp;
  const char *ip_addr;
  uint32_t new_qp;
  uint64_t vaddr;
} RoceDummyAckData;

typedef struct RoceAckDestData {
  uint32_t dummy_qp;
  const char *ip_addr;
  uint32_t dest_qp_id;
  uint32_t dummy_qp_id;
} RoceAckDestData;

typedef struct SetQpIdxData {
  uint32_t qp_id;
  const char *src_ip_addr;
  uint16_t global_qp_idx;
} SetQpIdxData;

typedef struct SenderQpData {
  uint32_t cpu_qp_id;
  const char *src_ip_addr;
  uint16_t mem_qp_id;
} SenderQpData;

typedef struct EgressInvRouteData {
  int32_t nid;
  int32_t inv_idx;
  uint32_t qp;
  uint32_t rkey;
  uint64_t vaddr;
  uint16_t reg_idx;
} EgressInvRouteData;

typedef struct AckTransData {
  const char *dst_ip_addr;
  uint32_t qp;
  uint32_t new_qp;
  uint32_t rkey;
  uint64_t vaddr;
  uint16_t reg_idx;
} AckTransData;

// public functions
#ifdef __cplusplus
extern "C" {
#endif

void InitMindBfClient(const std::string& configPath);
/*
// Define all the service methods here
void bfrt_add_cacheline(uint64_t vaddr, uint16_t vaddr_prefix, uint32_t c_idx);
void bfrt_get_cacheline(uint64_t vaddr, uint16_t vaddr_prefix, uint32_t c_idx);
void bfrt_del_cacheline(uint64_t vaddr, uint16_t vaddr_prefix);
void bfrt_set_cacheline_state(uint32_t cache_idx, uint16_t state);
void bfrt_reset_cacheline_state_on_update(uint32_t cache_idx);
void bfrt_add_cacheline_reg(CachelineRegData regData);
void bfrt_mod_cacheline_reg(CachelineRegData regData);
void bfrt_del_cacheline_reg(uint32_t cache_idx);
CachelineRegData bfrt_get_cacheline_reg_state(uint32_t cache_idx);
CachelineRegData bfrt_get_cacheline_reg_state_sharer(uint32_t cache_idx);
uint16_t bfrt_get_cacheline_reg_lock(uint32_t cache_idx);
CachelineRegData bfrt_get_cacheline_reg(uint32_t cache_idx);
uint32_t bfrt_get_cacheline_reg_inv(uint32_t cache_idx);
void bfrt_add_cachestate(CachestateData stateData);
CachestateData bfrt_get_cachestate(uint8_t cur_state, uint8_t write_req);
void bfrt_add_cachesharer(const char* ip_addr, uint16_t sharer);
void bfrt_del_cachesharer(const char* ip_addr);
uint16_t bfrt_get_cachesharer(const char* ip_addr);
void bfrt_add_eg_cachesharer(const char* ip_addr, uint16_t sharer);
void bfrt_set_cacheline_lock(uint32_t cache_idx, uint16_t dir_lock);
void bfrt_set_cacheline_inv(uint32_t cache_idx, uint32_t inv_cnt);
void bfrt_add_addr_trans(AddrTransData transData);
void bfrt_add_addr_except_trans(AddrExceptTransData exceptTransData);
void bfrt_modify_addr_except_trans(AddrExceptTransData exceptTransData);
void bfrt_del_addrExceptTrans_rule(uint64_t vaddr, uint16_t vaddr_prefix, int32_t account);
void bfrt_add_roce_req(RoceReqData reqData);
void bfrt_add_roce_ack(RoceAckData ackData);
void bfrt_add_roce_dummy_ack(RoceDummyAckData dummyAckData);
void bfrt_add_roce_ack_dest(RoceAckDestData ackDestData);
void bfrt_add_set_qp_idx(SetQpIdxData qpIdxData);
void bfrt_add_sender_qp(SenderQpData senderQpData);
void bfrt_add_egressInvRoute_rule(EgressInvRouteData egressInvData);
void bfrt_add_ack_trans(AckTransData ackTransData);
void print_bfrt_addr_trans_rule_counters();
*/

#ifdef __cplusplus
}
#endif
#endif
