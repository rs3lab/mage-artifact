#include "controller.h"

// Cacheline / directory
void bfrt_add_cacheline(uint64_t vaddr, uint16_t vaddr_prefix, uint32_t c_idx){}
void bfrt_del_cacheline(uint64_t vaddr, uint16_t vaddr_prefix){}
void bfrt_set_cacheline_state(uint32_t cache_idx, uint16_t state){}
void bfrt_reset_cacheline_state_on_update(uint32_t cache_idx){}
void bfrt_add_cacheline_reg(uint32_t cache_idx, uint16_t state, uint16_t sharer,
                                   uint16_t dir_size, uint16_t dir_lock, uint32_t inv_cnt){}
void bfrt_mod_cacheline_reg(uint32_t cache_idx, uint16_t state, uint16_t sharer,
                                   uint16_t dir_size, uint16_t dir_lock, uint32_t inv_cnt){}
void bfrt_del_cacheline_reg(uint32_t cache_idx){}

// Cache state transition
void bfrt_add_cachestate(uint8_t cur_state, uint8_t perm, uint8_t write_req,
                         uint8_t next_state, uint8_t reset_sharer, uint8_t send_inval){}
void bfrt_add_cachesharer(const char *ip_addr, uint16_t sharer){}
void bfrt_del_cachesharer(const char *ip_addr){}
void bfrt_add_eg_cachesharer(const char *ip_addr, uint16_t sharer){}

// Cache lock
void bfrt_set_cacheline_lock(uint32_t cache_idx, uint16_t dir_lock){}
void bfrt_set_cacheline_inv(uint32_t cache_idx, uint32_t inv_cnt){}

// Address translation
void bfrt_add_addr_trans( //uint32_t rkey,
    uint64_t vaddr, uint16_t vaddr_prefix,
    char *dst_ip_addr, uint64_t va_to_dma){}
void bfrt_add_addr_except_trans(uint64_t vaddr, uint16_t vaddr_prefix,
                                       char *dst_ip_addr, uint64_t va_to_dma,
                                       uint8_t permission, int account){}
// Access control
void bfrt_modify_addr_except_trans(uint64_t vaddr, uint16_t vaddr_prefix,
                                          char *dst_ip_addr, uint64_t va_to_dma, uint8_t permission){}
void bfrt_del_addrExceptTrans_rule(uint64_t vaddr, uint16_t vaddr_prefix, int account){}

// RoCE request
void bfrt_add_roce_req(char *src_ip_addr, char *dst_ip_addr, uint32_t qp,
                              uint32_t new_qp, uint32_t rkey, uint16_t reg_idx){}
// RoCE ack
void bfrt_add_roce_ack(uint32_t qp, // char *src_ip_addr,
                              uint32_t new_qp, char *new_ip_addr, uint16_t reg_idx){}
// RoCE dummy ack for NACK
void bfrt_add_roce_dummy_ack(uint32_t qp, char *ip_addr,
                                    uint32_t new_qp, uint64_t vaddr){}
// RoCE dummy ack forwarding for NACK
void bfrt_add_roce_ack_dest(uint32_t dummy_qp, char *ip_addr,
                                   uint32_t dest_qp_id, uint32_t dummy_qp_id){}
// RoCE req to ack conversion
void bfrt_add_set_qp_idx(uint32_t qp_id, char *src_ip_addr,
                                uint16_t global_qp_idx){}
void bfrt_add_sender_qp(uint32_t cpu_qp_id, char *src_ip_addr,
                               uint16_t mem_qp_id){}
// Inval to Roce
void bfrt_add_egressInvRoute_rule(int nid, int inv_idx, uint32_t qp, uint32_t rkey,
                                         uint64_t vaddr, uint16_t reg_idx){}
// Ack to Roce translation
void bfrt_add_ack_trans(char *dst_ip_addr, uint32_t qp, uint32_t new_qp,
                               uint32_t rkey, uint64_t vaddr, uint16_t reg_idx){}
void print_bfrt_addr_trans_rule_counters(void){}
