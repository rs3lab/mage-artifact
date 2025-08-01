#include <disagg/pid_ns_disagg.h>
#include <disagg/network_disagg.h>
#include <disagg/print_disagg.h>
#include <disagg/config.h>

/**
 * @brief Sends mm to kernel to generate a tgid
 *
 * @return int
 */
static pid_t __send_gen_tgid_mm(pid_t pid_ns_id)
{
  int ret;
  pid_t tgid;
  struct tgid_reply_struct *reply;
  struct tgid_msg_struct payload;

  payload.option = 0;
  payload.sender_id = get_local_node_id();
  payload.pid_ns_id = pid_ns_id;
  reply = kmalloc(sizeof(struct tgid_reply_struct), GFP_KERNEL);
  if (!reply)
    return -ENOMEM;
  pr_syscall("__send_gen_tgid_mm send_msg_to_control\n");
  ret = send_msg_to_control(DISAGG_NEW_TGID, &payload, sizeof(payload),
                           reply, sizeof(*reply));

  pr_syscall("__send_gen_tgid_mm, received: [%d] TGID: [%d]\n",
             ret, reply->tgid);

  tgid = reply->tgid;
  kfree(reply);
  return tgid;
}

/**
 * @brief Sends mm to kernel to generate a pid_ns
 *
 * @return int
 */
static pid_t __send_gen_pid_ns_mm(void)
{
  int ret;
  pid_t pid_ns_id;
  struct pid_ns_reply_struct *reply;
  struct pid_ns_msg_struct payload;

  payload.option = 0;
  payload.parent_pid_ns_id = current->pid_ns_id;
  
  reply = kmalloc(sizeof(struct pid_ns_reply_struct), GFP_KERNEL);
  if (!reply)
    return -ENOMEM;

  pr_syscall("__send_gen_pid_ns_mm send_msg_to_control\n");
  ret = send_msg_to_control(DISAGG_NEW_PID_NS, &payload, sizeof(payload),
                           reply, sizeof(*reply));

  pr_syscall("GEN_PID_NS: ret: [%d] NS: [%d]\n",
             ret, reply->pid_ns_id);

  pid_ns_id = reply->pid_ns_id;
  kfree(reply);
  return pid_ns_id;
}

static pid_t __send_get_tgid_in_ns_mm(pid_t tgid, int pid_ns_id)
{
  int ret;
  int tgid_in_ns;
  struct get_tgid_ns_reply_struct *reply;
  struct get_tgid_ns_msg_struct payload;

  payload.option = 0;
  payload.pid_ns_id = pid_ns_id;
  payload.tgid = tgid;

  reply = kmalloc(sizeof(struct get_tgid_ns_reply_struct), GFP_KERNEL);
  if (!reply)
    return -ENOMEM;

  pr_syscall("__send_gen_tgid_in_ns_mm send_msg_to_control, tgid: %d, pid: %d, pid_ns_id: %d\n", tgid, current->pid, pid_ns_id);
  ret = send_msg_to_control(DISAGG_GET_TGID_IN_NS, &payload, sizeof(payload),
                           reply, sizeof(*reply));

  pr_syscall("__send_gen_tgid_in_ns_mm: ret: [%d] tgid_in_ns: [%u]\n",
             ret, reply->tgid_in_ns);

  tgid_in_ns = reply->tgid_in_ns;
  kfree(reply);
  return tgid_in_ns;
}

pid_t disagg_get_tgid_in_ns(pid_t tgid, int pid_ns_id)
{
  pr_syscall("gen_tgid_in_ns received!\n");
  return __send_get_tgid_in_ns_mm(tgid, pid_ns_id);
}

/*
 * Asks the switch to generate us a tgid
 * Should only be called on processes that need a global (cluster-wide tgid)
 */
pid_t disagg_gen_tgid(pid_t pid_ns_id)
{
  pr_syscall("disagg_gen_tgid received!\n");
  return __send_gen_tgid_mm(pid_ns_id);
}

/*
 * Asks the switch to generate us a new ns
 * Should only be called on processes that need a global (cluster-wide pid_ns)
 */
pid_t disagg_gen_pid_ns(void)
{
  pr_syscall("disagg_gen_pid_ns received!\n");
  return __send_gen_pid_ns_mm();
}
