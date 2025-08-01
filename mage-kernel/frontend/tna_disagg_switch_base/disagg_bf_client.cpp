#include "disagg_bf_client.hpp"
#include <mutex>

class MindBfClient
{
public:
    MindBfClient(const std::string &configPath)
    {
        // std::ifstream config_file(configPath);
        // nlohmann::json config;
        // if (!config_file.good())
        // {
        //     std::cerr << "Error opening config file: " << configPath << std::endl;
        //     exit(-1);
        // }
        // config_file >> config;
        //
        // std::string server = config["server"];
        // int port = config["port"];
        //
        // std::cout << "Server IP/port: " << server << ":" << port << std::endl;
        //
        // socket = std::make_shared<apache::thrift::transport::TSocket>(server, port);
        // transport = std::make_shared<apache::thrift::transport::TBufferedTransport>(socket);
        // protocol = std::make_shared<apache::thrift::protocol::TBinaryProtocol>(transport);
        // client = std::make_shared<CacheService::CacheServiceClient>(protocol);
    }

    void open()
    {
        // try
        // {
        //     transport->open();
        // }
        // catch (const std::exception &e)
        // {
        //     // Handle the exception here, such as logging or displaying an error message
        //     std::cerr << "Error connect to Thrift server: " << e.what() << std::endl;
        //     exit(-1);
        // }
    }

    void close()
    {
        // transport->close();
    }

    // Define all the service methods here
    void bfrt_add_cacheline(uint64_t vaddr, uint16_t vaddr_prefix, uint32_t c_idx)
    {
        // client->bfrt_add_cacheline(vaddr, vaddr_prefix, c_idx);
    }

    uint32_t bfrt_get_cacheline(uint64_t vaddr, uint16_t vaddr_prefix)
    {
        // return client->bfrt_get_cacheline(vaddr, vaddr_prefix);
        return 0;
    }

    void bfrt_del_cacheline(uint64_t vaddr, uint16_t vaddr_prefix)
    {
        // client->bfrt_del_cacheline(vaddr, vaddr_prefix);
    }

    void bfrt_set_cacheline_state(uint32_t cache_idx, uint16_t state)
    {
        // client->bfrt_set_cacheline_state(cache_idx, state);
    }

    void bfrt_reset_cacheline_state_on_update(uint32_t cache_idx)
    {
        // client->bfrt_reset_cacheline_state_on_update(cache_idx);
    }

    void bfrt_add_cacheline_reg(CacheService::CachelineRegData regData)
    {
        // client->bfrt_add_cacheline_reg(regData);
    }

    void bfrt_mod_cacheline_reg(CacheService::CachelineRegData regData)
    {
        // client->bfrt_mod_cacheline_reg(regData);
    }

    void bfrt_del_cacheline_reg(uint32_t cache_idx)
    {
        // client->bfrt_del_cacheline_reg(cache_idx);
    }

    CacheService::CachelineRegData bfrt_get_cacheline_reg_state(uint32_t cache_idx)
    {
        CacheService::CachelineRegData data;
        // client->bfrt_get_cacheline_reg_state(data, cache_idx);
        return data;
    }

    CacheService::CachelineRegData bfrt_get_cacheline_reg_state_sharer(uint32_t cache_idx)
    {
        CacheService::CachelineRegData data;
        // client->bfrt_get_cacheline_reg_state_sharer(data, cache_idx);
        return data;
    }

    uint16_t bfrt_get_cacheline_reg_lock(uint32_t cache_idx)
    {
        // uint16_t lock;
        // client->bfrt_get_cacheline_reg_lock(cache_idx);
        // return lock;
        return 0;
    }

    CacheService::CachelineRegData bfrt_get_cacheline_reg(uint32_t cache_idx)
    {
        CacheService::CachelineRegData data;
        // client->bfrt_get_cacheline_reg(data, cache_idx);
        return data;
    }

    uint32_t bfrt_get_cacheline_reg_inv(uint32_t cache_idx)
    {
        // return client->bfrt_get_cacheline_reg_inv(cache_idx);
        return 0;
    }

    void bfrt_add_cachestate(CacheService::CachestateData stateData)
    {
        // client->bfrt_add_cachestate(stateData);
    }

    CacheService::CachestateData bfrt_get_cachestate(uint8_t cur_state, uint8_t perm, uint8_t write_req)
    {
        CacheService::CachestateData stateData;
        // client->bfrt_get_cachestate(stateData, cur_state, perm, write_req);
        return stateData;
    }

    void bfrt_add_cachesharer(CacheService::char_ptr ip_addr, uint16_t sharer)
    {
        // client->bfrt_add_cachesharer(ip_addr, sharer);
    }

    void bfrt_del_cachesharer(CacheService::char_ptr ip_addr)
    {
        // client->bfrt_del_cachesharer(ip_addr);
    }

    uint16_t bfrt_get_cachesharer(CacheService::char_ptr ip_addr)
    {
        // return client->bfrt_get_cachesharer(ip_addr);
        return 0;
    }

    void bfrt_add_eg_cachesharer(CacheService::char_ptr ip_addr, uint16_t sharer)
    {
        // client->bfrt_add_eg_cachesharer(ip_addr, sharer);
    }

    void bfrt_set_cacheline_lock(uint32_t cache_idx, uint16_t dir_lock)
    {
        // client->bfrt_set_cacheline_lock(cache_idx, dir_lock);
    }

    void bfrt_set_cacheline_inv(uint32_t cache_idx, uint32_t inv_cnt)
    {
        // client->bfrt_set_cacheline_inv(cache_idx, inv_cnt);
    }

    void bfrt_add_addr_trans(CacheService::AddrTransData transData)
    {
        // client->bfrt_add_addr_trans(transData);
    }

    void bfrt_add_addr_except_trans(CacheService::AddrExceptTransData exceptTransData)
    {
        // client->bfrt_add_addr_except_trans(exceptTransData);
    }

    void bfrt_modify_addr_except_trans(CacheService::AddrExceptTransData exceptTransData)
    {
        // client->bfrt_modify_addr_except_trans(exceptTransData);
    }

    void bfrt_del_addrExceptTrans_rule(uint64_t vaddr, uint16_t vaddr_prefix, int32_t account)
    {
        // client->bfrt_del_addrExceptTrans_rule(vaddr, vaddr_prefix, account);
    }

    void bfrt_add_roce_req(CacheService::RoceReqData reqData)
    {
        // client->bfrt_add_roce_req(reqData);
    }

    void bfrt_add_roce_ack(CacheService::RoceAckData ackData)
    {
        // client->bfrt_add_roce_ack(ackData);
    }

    void bfrt_add_roce_dummy_ack(CacheService::RoceDummyAckData dummyAckData)
    {
        // client->bfrt_add_roce_dummy_ack(dummyAckData);
    }

    void bfrt_add_roce_ack_dest(CacheService::RoceAckDestData ackDestData)
    {
        // client->bfrt_add_roce_ack_dest(ackDestData);
    }

    void bfrt_add_set_qp_idx(CacheService::SetQpIdxData qpIdxData)
    {
        // client->bfrt_add_set_qp_idx(qpIdxData);
    }

    void bfrt_add_sender_qp(CacheService::SenderQpData senderQpData)
    {
        // client->bfrt_add_sender_qp(senderQpData);
    }

    void bfrt_add_egressInvRoute_rule(CacheService::EgressInvRouteData egressInvData)
    {
        // client->bfrt_add_egressInvRoute_rule(egressInvData);
    }

    void bfrt_add_ack_trans(CacheService::AckTransData ackTransData)
    {
        // client->bfrt_add_ack_trans(ackTransData);
    }

    void print_bfrt_addr_trans_rule_counters()
    {
        // client->print_bfrt_addr_trans_rule_counters();
    }

private:
    // std::shared_ptr<TSocket> socket;
    // std::shared_ptr<TTransport> transport;
    // std::shared_ptr<TProtocol> protocol;
    // std::shared_ptr<CacheService::CacheServiceClient> client;
};

// Cpp version
//static std::shared_ptr<MindBfClient> _bf_client = NULL;
void _InitMindBfClient(const std::string &configPath)
{
    // if (_bf_client == NULL)
    //     _bf_client = std::make_shared<MindBfClient>(configPath);
    // _bf_client->open();
}

void _TerminateMindBfClient()
{
    // if (_bf_client != NULL)
    //     _bf_client->close();
    // _bf_client.reset();
}

static std::mutex mtx;
void InitMindBfClient(const std::string &configPath)
{
    // _InitMindBfClient(configPath);
}

static void _check_bf_thrift_client(void)
{
    // if (_bf_client == NULL)
    // {
    //     fprintf(stderr, "MindBfClient is not initialized\n");
    //     exit(1);
    // }
}

// Define all the service methods here
extern "C" void bfrt_add_cacheline(uint64_t vaddr, uint16_t vaddr_prefix, uint32_t c_idx)
{
    // _check_bf_thrift_client();
    // std::lock_guard<std::mutex> thrift_lock(mtx); // acquire lock
    // _bf_client->bfrt_add_cacheline(vaddr, vaddr_prefix, c_idx);
}

// y: sterile. I've removed all calls to this method.
extern "C" int bfrt_get_cacheline(uint64_t vaddr, uint16_t vaddr_prefix, uint32_t *c_idx)
{
    // _check_bf_thrift_client();
    // std::lock_guard<std::mutex> thrift_lock(mtx); // acquire lock
    // uint32_t _c_idx = _bf_client->bfrt_get_cacheline(vaddr, vaddr_prefix);
    // *c_idx = _c_idx;
    return 0;
}

extern "C" void bfrt_del_cacheline(uint64_t vaddr, uint16_t vaddr_prefix)
{
    // _check_bf_thrift_client();
    // std::lock_guard<std::mutex> thrift_lock(mtx); // acquire lock
    // _bf_client->bfrt_del_cacheline(vaddr, vaddr_prefix);
}

extern "C" void bfrt_set_cacheline_state(uint32_t cache_idx, uint16_t state)
{
    // _check_bf_thrift_client();
    // std::lock_guard<std::mutex> thrift_lock(mtx); // acquire lock
    // _bf_client->bfrt_set_cacheline_state(cache_idx, state);
}

extern "C" void bfrt_reset_cacheline_state_on_update(uint32_t cache_idx)
{
    // _check_bf_thrift_client();
    // std::lock_guard<std::mutex> thrift_lock(mtx); // acquire lock
    // _bf_client->bfrt_reset_cacheline_state_on_update(cache_idx);
}

extern "C" void bfrt_add_cacheline_reg(uint32_t cache_idx, uint16_t state, uint16_t sharer,
                                       uint16_t dir_size, uint16_t dir_lock, uint32_t inv_cnt)
{
    // _check_bf_thrift_client();
    // std::lock_guard<std::mutex> thrift_lock(mtx); // acquire lock
    // CacheService::CachelineRegData _regData;
    // _regData.cache_idx = cache_idx;
    // _regData.state = state;
    // _regData.sharer = sharer;
    // _regData.dir_size = dir_size;
    // _regData.dir_lock = dir_lock;
    // _regData.inv_cnt = inv_cnt;
    // _bf_client->bfrt_add_cacheline_reg(_regData);
}

// y: sterile. I've removed all incoming calls to this fn.
extern "C" void bfrt_mod_cacheline_reg(uint32_t cache_idx, uint16_t state, uint16_t sharer,
                                       uint16_t dir_size, uint16_t dir_lock, uint32_t inv_cnt)
{
    // _check_bf_thrift_client();
    // std::lock_guard<std::mutex> thrift_lock(mtx); // acquire lock
    // CacheService::CachelineRegData _regData;
    // _regData.cache_idx = cache_idx;
    // _regData.state = state;
    // _regData.sharer = sharer;
    // _regData.dir_size = dir_size;
    // _regData.dir_lock = dir_lock;
    // _regData.inv_cnt = inv_cnt;
    // _bf_client->bfrt_mod_cacheline_reg(_regData);
}

// y: sterile. I've removed all incoming calls to this fn.
extern "C" void bfrt_del_cacheline_reg(uint32_t cache_idx)
{
    // // _check_bf_thrift_client();
    // std::lock_guard<std::mutex> thrift_lock(mtx); // acquire lock
    //                                               // _bf_client->bfrt_del_cacheline_reg(cache_idx);
}

// y: sterile. I've removed all incoming calls to this fn..
extern "C" void bfrt_get_cacheline_reg_state(uint32_t cache_idx, uint16_t *state, uint16_t *update)
{
    // _check_bf_thrift_client();
    // std::lock_guard<std::mutex> thrift_lock(mtx); // acquire lock
    // CacheService::CachelineRegData _regData;
    // _regData = _bf_client->bfrt_get_cacheline_reg_state(cache_idx);
    // *state = _regData.state;
    // *update = _regData.update;
}

// y: sterile. I've removed all incoming calls to this fn.
extern "C" void bfrt_get_cacheline_reg_state_sharer(uint32_t cache_idx, uint16_t *state,
                                                    uint16_t *update, uint16_t *sharer)
{
    // _check_bf_thrift_client();
    // std::lock_guard<std::mutex> thrift_lock(mtx); // acquire lock
    // CacheService::CachelineRegData _regData;
    // _regData = _bf_client->bfrt_get_cacheline_reg_state_sharer(cache_idx);
    // *state = _regData.state;
    // *update = _regData.update;
    // *sharer = _regData.sharer;
}

// y: sterile. I've removed all incoming calls to this fn.
extern "C" uint16_t bfrt_get_cacheline_reg_lock(uint32_t cache_idx)
{
    // _check_bf_thrift_client();
    // std::lock_guard<std::mutex> thrift_lock(mtx); // acquire lock
    // uint16_t lock = _bf_client->bfrt_get_cacheline_reg_lock(cache_idx);
    // return lock;
    return 0;
}

// y: sterile. I've removed all incoming calls to this fn.
extern "C" void bfrt_get_cacheline_reg(uint32_t cache_idx, uint16_t *state, uint16_t *st_update, uint16_t *sharer,
                                       uint16_t *dir_size, uint16_t *dir_lock, uint32_t *inv_cnt)
{
    // _check_bf_thrift_client();
    // std::lock_guard<std::mutex> thrift_lock(mtx); // acquire lock
    // CacheService::CachelineRegData _regData;
    // _regData = _bf_client->bfrt_get_cacheline_reg(cache_idx);
    // *state = _regData.state;
    // *st_update = _regData.update;
    // *sharer = _regData.sharer;
    // *dir_size = _regData.dir_size;
    // *dir_lock = _regData.dir_lock;
    // *inv_cnt = _regData.inv_cnt;
}

// y: sterile. I've removed all incoming calls to this fn.
extern "C" uint32_t bfrt_get_cacheline_reg_inv(uint32_t cache_idx)
{
    // _check_bf_thrift_client();
    // std::lock_guard<std::mutex> thrift_lock(mtx); // acquire lock
    // return _bf_client->bfrt_get_cacheline_reg_inv(cache_idx);
    return 0;
}

// y: sterile. I've removed all incoming calls to this fn.
extern "C" void bfrt_add_cachestate(uint8_t cur_state, uint8_t perm, uint8_t write_req,
                                    uint8_t next_state, uint8_t reset_sharer, uint8_t send_inval)
{
    // _check_bf_thrift_client();
    // std::lock_guard<std::mutex> thrift_lock(mtx); // acquire lock
    // CacheService::CachestateData _stateData;
    // _stateData.cur_state = cur_state;
    // _stateData.perm = perm;
    // _stateData.write_req = write_req;
    // _stateData.next_state = next_state;
    // _stateData.reset_sharer = reset_sharer;
    // _stateData.send_inval = send_inval;
    // _bf_client->bfrt_add_cachestate(_stateData);
}

// y: sterile. I've removed all incoming calls to this fn.
extern "C" void bfrt_get_cachestate(uint8_t cur_state, uint8_t perm, uint8_t write_req,
                                    uint8_t *next_state, uint8_t *reset_sharer, uint8_t *send_inval)
{
    // _check_bf_thrift_client();
    // std::lock_guard<std::mutex> thrift_lock(mtx); // acquire lock
    // CacheService::CachestateData _stateData;
    // _stateData = _bf_client->bfrt_get_cachestate(cur_state, perm, write_req);
    // *next_state = _stateData.next_state;
    // *reset_sharer = _stateData.reset_sharer;
    // *send_inval = _stateData.send_inval;
}

extern "C" void bfrt_add_cachesharer(const char *ip_addr, uint16_t sharer)
{
    // _check_bf_thrift_client();
    // std::lock_guard<std::mutex> thrift_lock(mtx); // acquire lock
    // _bf_client->bfrt_add_cachesharer(ip_addr, sharer);
}

// y: sterile. I've removed all incoming calls to this fn.
extern "C" void bfrt_del_cachesharer(const char *ip_addr)
{
    // _check_bf_thrift_client();
    // std::lock_guard<std::mutex> thrift_lock(mtx); // acquire lock
    // _bf_client->bfrt_del_cachesharer(ip_addr);
}

// y: sterile. I've removed all incoming calls to this fn.
extern "C" void bfrt_get_cachesharer(const char *ip_addr, uint16_t *sharer)
{
    // _check_bf_thrift_client();
    // std::lock_guard<std::mutex> thrift_lock(mtx); // acquire lock
    // *sharer = _bf_client->bfrt_get_cachesharer(ip_addr);
}

extern "C" void bfrt_add_eg_cachesharer(const char *ip_addr, uint16_t sharer)
{
    // _check_bf_thrift_client();
    // std::lock_guard<std::mutex> thrift_lock(mtx); // acquire lock
    // _bf_client->bfrt_add_eg_cachesharer(ip_addr, sharer);
}

extern "C" void bfrt_set_cacheline_lock(uint32_t cache_idx, uint16_t dir_lock)
{
    // _check_bf_thrift_client();
    // std::lock_guard<std::mutex> thrift_lock(mtx); // acquire lock
    // _bf_client->bfrt_set_cacheline_lock(cache_idx, dir_lock);
}

extern "C" void bfrt_set_cacheline_inv(uint32_t cache_idx, uint32_t inv_cnt)
{
    // _check_bf_thrift_client();
    // std::lock_guard<std::mutex> thrift_lock(mtx); // acquire lock
    // _bf_client->bfrt_set_cacheline_inv(cache_idx, inv_cnt);
}

extern "C" void bfrt_add_addr_trans(uint64_t vaddr, uint16_t vaddr_prefix,
                                    char *dst_ip_addr, uint64_t va_to_dma)
{
    printf("DUMMY_BFRT: Adding address translation: vaddr=0x%lx, prefix=0x%hx, ip=%s, va_to_dma=0x%lx\n",
            vaddr, vaddr_prefix, dst_ip_addr, va_to_dma);
    // _check_bf_thrift_client();
    // std::lock_guard<std::mutex> thrift_lock(mtx); // acquire lock
    // CacheService::AddrTransData _transData;
    // _transData.vaddr = vaddr;
    // _transData.vaddr_prefix = vaddr_prefix;
    // _transData.dst_ip_addr = CacheService::char_ptr(dst_ip_addr);
    // _transData.va_to_dma = va_to_dma;
    // _bf_client->bfrt_add_addr_trans(_transData);
}

extern "C" void bfrt_add_addr_except_trans(uint64_t vaddr, uint16_t vaddr_prefix,
                                           char *dst_ip_addr, uint64_t va_to_dma, uint8_t permission,
                                           int account)
{
    printf("DUMMY_BFRT: Adding address translation (\"except trans\"): vaddr=0x%lx, prefix=0x%hx, "
            "ip=%s, va_to_dma=0x%lx\n", vaddr, vaddr_prefix, dst_ip_addr, va_to_dma);
    // _check_bf_thrift_client();
    // std::lock_guard<std::mutex> thrift_lock(mtx); // acquire lock
    // CacheService::AddrExceptTransData _exceptTransData;
    // _exceptTransData.vaddr = vaddr;
    // _exceptTransData.vaddr_prefix = vaddr_prefix;
    // _exceptTransData.dst_ip_addr = CacheService::char_ptr(dst_ip_addr);
    // _exceptTransData.va_to_dma = va_to_dma;
    // _exceptTransData.permission = permission;
    // _exceptTransData.account = account;
    // _bf_client->bfrt_add_addr_except_trans(_exceptTransData);
}

extern "C" void bfrt_modify_addr_except_trans(uint64_t vaddr, uint16_t vaddr_prefix,
                                              char *dst_ip_addr, uint64_t va_to_dma, uint8_t permission)
{
    printf("DUMMY_BFRT: Modifying address translation (\"except trans\"): "
            "vaddr=0x%lx, prefix=0x%hx, ip=%s, va_to_dma=0x%lx\n",
            vaddr, vaddr_prefix, dst_ip_addr, va_to_dma);
    // _check_bf_thrift_client();
    // std::lock_guard<std::mutex> thrift_lock(mtx); // acquire lock
    // CacheService::AddrExceptTransData _exceptTransData;
    // _exceptTransData.vaddr = vaddr;
    // _exceptTransData.vaddr_prefix = vaddr_prefix;
    // _exceptTransData.dst_ip_addr = CacheService::char_ptr(dst_ip_addr);
    // _exceptTransData.va_to_dma = va_to_dma;
    // _exceptTransData.permission = permission;
    // _bf_client->bfrt_modify_addr_except_trans(_exceptTransData);
}

extern "C" void bfrt_del_addrExceptTrans_rule(uint64_t vaddr, uint16_t vaddr_prefix, int32_t account)
{
    printf("DUMMY_BFRT: Removing address translation (\"except trans\"): "
            "vaddr=0x%lx, prefix=0x%hx, account=%d\n", vaddr, vaddr_prefix, account);
    // _check_bf_thrift_client();
    // std::lock_guard<std::mutex> thrift_lock(mtx); // acquire lock
    // _bf_client->bfrt_del_addrExceptTrans_rule(vaddr, vaddr_prefix, account);
}

extern "C" void bfrt_add_roce_req(char *src_ip_addr, char *dst_ip_addr, uint32_t qp,
                                  uint32_t new_qp, uint32_t rkey, uint16_t reg_idx)
{
    // _check_bf_thrift_client();
    // std::lock_guard<std::mutex> thrift_lock(mtx); // acquire lock
    // CacheService::RoceReqData _reqData;
    // _reqData.src_ip_addr = CacheService::char_ptr(src_ip_addr);
    // _reqData.dst_ip_addr = CacheService::char_ptr(dst_ip_addr);
    // _reqData.qp = qp;
    // _reqData.new_qp = new_qp;
    // _reqData.rkey = rkey;
    // _reqData.reg_idx = reg_idx;
    // _bf_client->bfrt_add_roce_req(_reqData);
}

extern "C" void bfrt_add_roce_ack(uint32_t qp, uint32_t new_qp, char *new_ip_addr, uint16_t reg_idx)
{
    // _check_bf_thrift_client();
    // std::lock_guard<std::mutex> thrift_lock(mtx); // acquire lock
    // CacheService::RoceAckData _ackData;
    // _ackData.qp = qp;
    // _ackData.new_qp = new_qp;
    // _ackData.new_ip_addr = CacheService::char_ptr(new_ip_addr);
    // _ackData.reg_idx = reg_idx;
    // _bf_client->bfrt_add_roce_ack(_ackData);
}

extern "C" void bfrt_add_roce_dummy_ack(uint32_t qp, char *ip_addr,
                                        uint32_t new_qp, uint64_t vaddr)
{
    // _check_bf_thrift_client();
    // std::lock_guard<std::mutex> thrift_lock(mtx); // acquire lock
    // CacheService::RoceDummyAckData _dummyAckData;
    // _dummyAckData.qp = qp;
    // _dummyAckData.ip_addr = CacheService::char_ptr(ip_addr);
    // _dummyAckData.new_qp = new_qp;
    // _dummyAckData.vaddr = vaddr;
    // _bf_client->bfrt_add_roce_dummy_ack(_dummyAckData);
}

extern "C" void bfrt_add_roce_ack_dest(uint32_t dummy_qp, char *ip_addr,
                                       uint32_t dest_qp_id, uint32_t dummy_qp_id)
{
    // _check_bf_thrift_client();
    // std::lock_guard<std::mutex> thrift_lock(mtx); // acquire lock
    // CacheService::RoceAckDestData _ackDestData;
    // _ackDestData.dummy_qp = dummy_qp;
    // _ackDestData.ip_addr = CacheService::char_ptr(ip_addr);
    // _ackDestData.dest_qp_id = dest_qp_id;
    // _ackDestData.dummy_qp_id = dummy_qp_id;
    // _bf_client->bfrt_add_roce_ack_dest(_ackDestData);
}

extern "C" void bfrt_add_set_qp_idx(uint32_t qp_id, char *src_ip_addr,
                                    uint16_t global_qp_idx)
{
    // _check_bf_thrift_client();
    // std::lock_guard<std::mutex> thrift_lock(mtx); // acquire lock
    // CacheService::SetQpIdxData _qpIdxData;
    // _qpIdxData.qp_id = qp_id;
    // _qpIdxData.src_ip_addr = CacheService::char_ptr(src_ip_addr);
    // _qpIdxData.global_qp_idx = global_qp_idx;
    // _bf_client->bfrt_add_set_qp_idx(_qpIdxData);
}

extern "C" void bfrt_add_sender_qp(uint32_t cpu_qp_id, char *src_ip_addr,
                                   uint16_t mem_qp_id)
{
    // _check_bf_thrift_client();
    // std::lock_guard<std::mutex> thrift_lock(mtx); // acquire lock
    // CacheService::SenderQpData _senderQpData;
    // _senderQpData.cpu_qp_id = cpu_qp_id;
    // _senderQpData.src_ip_addr = CacheService::char_ptr(src_ip_addr);
    // _senderQpData.mem_qp_id = mem_qp_id;
    // _bf_client->bfrt_add_sender_qp(_senderQpData);
}

extern "C" void bfrt_add_egressInvRoute_rule(int nid, int inv_idx, uint32_t qp, uint32_t rkey,
                                             uint64_t vaddr, uint16_t reg_idx)
{
    // _check_bf_thrift_client();
    // std::lock_guard<std::mutex> thrift_lock(mtx); // acquire lock
    // CacheService::EgressInvRouteData _egressInvData;
    // _egressInvData.nid = nid;
    // _egressInvData.inv_idx = inv_idx;
    // _egressInvData.qp = qp;
    // _egressInvData.rkey = rkey;
    // _egressInvData.vaddr = vaddr;
    // _egressInvData.reg_idx = reg_idx;
    // _bf_client->bfrt_add_egressInvRoute_rule(_egressInvData);
}

extern "C" void bfrt_add_ack_trans(char *dst_ip_addr, uint32_t qp, uint32_t new_qp,
                                   uint32_t rkey, uint64_t vaddr, uint16_t reg_idx)
{
    // _check_bf_thrift_client();
    // std::lock_guard<std::mutex> thrift_lock(mtx); // acquire lock
    // CacheService::AckTransData _ackTransData;
    // _ackTransData.dst_ip_addr = CacheService::char_ptr(dst_ip_addr);
    // _ackTransData.qp = qp;
    // _ackTransData.new_qp = new_qp;
    // _ackTransData.rkey = rkey;
    // _ackTransData.vaddr = vaddr;
    // _ackTransData.reg_idx = reg_idx;
    // _bf_client->bfrt_add_ack_trans(_ackTransData);
}

extern "C" void print_bfrt_addr_trans_rule_counters(void);
void print_bfrt_addr_trans_rule_counters(void)
{
    // _check_bf_thrift_client();
    // std::lock_guard<std::mutex> thrift_lock(mtx); // acquire lock
    // _bf_client->print_bfrt_addr_trans_rule_counters();
}
