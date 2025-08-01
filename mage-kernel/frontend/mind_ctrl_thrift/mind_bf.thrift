namespace cpp CacheService

typedef i64 uint64_t
typedef i32 uint32_t
typedef i16 uint16_t
typedef byte uint8_t
typedef string char_ptr

struct CachelineRegData {
    1: required uint32_t cache_idx,
    2: required uint16_t state,
    3: required uint16_t sharer,
    4: required uint16_t dir_size,
    5: required uint16_t dir_lock,
    6: required uint32_t inv_cnt,
    7: required uint16_t update,
}

struct CachestateData {
    1: required uint8_t cur_state,
    2: required uint8_t perm,
    3: required uint8_t write_req,
    4: required uint8_t next_state,
    5: required uint8_t reset_sharer,
    6: required uint8_t send_inval,
}

struct AddrTransData {
    1: required uint64_t vaddr,
    2: required uint16_t vaddr_prefix,
    3: required char_ptr dst_ip_addr,
    4: required uint64_t va_to_dma,
}

struct AddrExceptTransData {
    1: required uint64_t vaddr,
    2: required uint16_t vaddr_prefix,
    3: required char_ptr dst_ip_addr,
    4: required uint64_t va_to_dma,
    5: required uint8_t permission,
    6: required i32 account,
}

struct RoceReqData {
    1: required char_ptr src_ip_addr,
    2: required char_ptr dst_ip_addr,
    3: required uint32_t qp,
    4: required uint32_t new_qp,
    5: required uint32_t rkey,
    6: required uint16_t reg_idx,
}

struct RoceAckData {
    1: required uint32_t qp,
    2: required uint32_t new_qp,
    3: required char_ptr new_ip_addr,
    4: required uint16_t reg_idx,
}

struct RoceDummyAckData {
    1: required uint32_t qp,
    2: required char_ptr ip_addr,
    3: required uint32_t new_qp,
    4: required uint64_t vaddr,
}

struct RoceAckDestData {
    1: required uint32_t dummy_qp,
    2: required char_ptr ip_addr,
    3: required uint32_t dest_qp_id,
    4: required uint32_t dummy_qp_id,
}

struct SetQpIdxData {
    1: required uint32_t qp_id,
    2: required char_ptr src_ip_addr,
    3: required uint16_t global_qp_idx,
}

struct SenderQpData {
    1: required uint32_t cpu_qp_id,
    2: required char_ptr src_ip_addr,
    3: required uint16_t mem_qp_id,
}

struct EgressInvRouteData {
    1: required i32 nid,
    2: required i32 inv_idx,
    3: required uint32_t qp,
    4: required uint32_t rkey,
    5: required uint64_t vaddr,
    6: required uint16_t reg_idx,
}

struct AckTransData {
    1: required char_ptr dst_ip_addr,
    2: required uint32_t qp,
    3: required uint32_t new_qp,
    4: required uint32_t rkey,
    5: required uint64_t vaddr,
    6: required uint16_t reg_idx,
}

service CacheService {
    // Cacheline / directory
    void bfrt_add_cacheline(1: uint64_t vaddr, 2: uint16_t vaddr_prefix, 3: uint32_t c_idx),
    uint32_t bfrt_get_cacheline(1: uint64_t vaddr, 2: uint16_t vaddr_prefix),
    void bfrt_del_cacheline(1: uint64_t vaddr, 2: uint16_t vaddr_prefix),
    void bfrt_set_cacheline_state(1: uint32_t cache_idx, 2: uint16_t state),
    void bfrt_reset_cacheline_state_on_update(1: uint32_t cache_idx),
    void bfrt_add_cacheline_reg(1: CachelineRegData regData),
    void bfrt_mod_cacheline_reg(1: CachelineRegData regData),
    void bfrt_del_cacheline_reg(1: uint32_t cache_idx),
    CachelineRegData bfrt_get_cacheline_reg_state(1: uint32_t cache_idx),
    CachelineRegData bfrt_get_cacheline_reg_state_sharer(1: uint32_t cache_idx),
    uint16_t bfrt_get_cacheline_reg_lock(1: uint32_t cache_idx),
    CachelineRegData bfrt_get_cacheline_reg(1: uint32_t cache_idx),
    uint32_t bfrt_get_cacheline_reg_inv(1: uint32_t cache_idx),
    // Cache state transition
    void bfrt_add_cachestate(1: CachestateData stateData),
    CachestateData bfrt_get_cachestate(1: uint8_t cur_state, 2: uint8_t perm, 3: uint8_t write_req),
    void bfrt_add_cachesharer(1: char_ptr ip_addr, 2: uint16_t sharer),
    void bfrt_del_cachesharer(1: char_ptr ip_addr),
    uint16_t bfrt_get_cachesharer(1: char_ptr ip_addr),
    void bfrt_add_eg_cachesharer(1: char_ptr ip_addr, 2: uint16_t sharer),
    // Cache lock
    void bfrt_set_cacheline_lock(1: uint32_t cache_idx, 2: uint16_t dir_lock),
    void bfrt_set_cacheline_inv(1: uint32_t cache_idx, 2: uint32_t inv_cnt),
    // Address translation
    void bfrt_add_addr_trans(1: AddrTransData transData),
    void bfrt_add_addr_except_trans(1: AddrExceptTransData exceptTransData),
    // Access control
    void bfrt_modify_addr_except_trans(1: AddrExceptTransData exceptTransData),
    void bfrt_del_addrExceptTrans_rule(1: uint64_t vaddr, 2: uint16_t vaddr_prefix, 3: i32 account),
    // RoCE request
    void bfrt_add_roce_req(1: RoceReqData reqData),
    // RoCE ack
    void bfrt_add_roce_ack(1: RoceAckData ackData),
    // RoCE dummy ack for NACK
    void bfrt_add_roce_dummy_ack(1: RoceDummyAckData dummyAckData),
    // RoCE dummy ack forwarding for NACK
    void bfrt_add_roce_ack_dest(1: RoceAckDestData ackDestData),
    // RoCE req to ack conversion
    void bfrt_add_set_qp_idx(1: SetQpIdxData qpIdxData),
    void bfrt_add_sender_qp(1: SenderQpData senderQpData),
    // Inval to Roce
    void bfrt_add_egressInvRoute_rule(1: EgressInvRouteData egressInvData),
    // Ack to Roce translation
    void bfrt_add_ack_trans(1: AckTransData ackTransData),
    // Print function
    void print_bfrt_addr_trans_rule_counters(),
}