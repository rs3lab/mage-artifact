#include <disagg/pid_disagg.h>
#include <disagg/network_disagg.h>
#include <disagg/config.h>

/**
 * @brief Sends mm to kernel to generate a pid
 *
 * @return int
 */
static pid_t __send_gen_pid_mm(void)
{
  struct pid_reply_struct *reply;
  struct pid_msg_struct payload;

  payload.option = 0;

  int ret;
  pid_t tgid;

  reply = kmalloc(sizeof(struct pid_reply_struct), GFP_KERNEL);
  if (!reply)
    return -ENOMEM;

  ret = send_msg_to_control(DISAGG_GEN_TGID, &payload, sizeof(payload),
                           reply, sizeof(*reply));

  pr_syscall("GEN_PID: Error: [%d] TGID: [%d]\n",
             ret, reply->tgid);

  tgid = reply->tgid;
  kfree(reply);
  return tgid;
}

/*
 * Asks the switch to generate us a pid
 * Should only be called on processes that need a global (cluster-wide tgid)
 */
pid_t disagg_gen_pid(void)
{
  return __send_gen_pid_mm();
}
