#include <linux/socket.h>
#include <linux/kernel.h>
#include <linux/kfifo.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/timekeeping.h>
#include <linux/workqueue.h>
#include <linux/net.h>
#include <linux/in.h>
#include <linux/inet.h>
#include <net/sock.h>
#include <linux/jiffies.h>
#include <net/net_namespace.h>

#include "log.h"

#define MAX_LOG 64
// log받을 서버 ip, port 수정하려면 아래 매크로 값 수정
#define LOG_SERVER_IP "10.1.3.10"
#define LOG_SERVER_PORT 9090

struct log_event{
    enum log_flag flag;
    pid_t tgid;
    u64 time_ns;
};

static void log_worker(struct work_struct *work);


static DEFINE_KFIFO(log_event, struct log_event, MAX_LOG);
static DEFINE_RAW_SPINLOCK(log_lock);
static struct socket *log_sock;
static DECLARE_WORK(log_work, log_worker);


static int log_connect(struct socket **out){
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(LOG_SERVER_PORT),
    };
    struct socket *sock;
    int ret;

    if (!in4_pton(LOG_SERVER_IP, -1, (u8 *)&addr.sin_addr.s_addr, -1, NULL))
        return -EINVAL;

    ret = sock_create_kern(&init_net, AF_INET, SOCK_STREAM, IPPROTO_TCP, &sock);
    if (ret < 0)
        return ret;

    // connect하고 ret 계속 덮어 써서 검사하기, out에 sock 넣어서 전역변수 log_sock이랑 연결(거기다 작업할거), *out으로 써야 log_sock이랑 연결됨
    sock->sk->sk_sndtimeo = HZ;

    ret = kernel_connect(sock, (void *)&addr, sizeof(addr), 0);
    if (ret < 0){
        sock_release(sock);
        return ret;
    }
    *out = sock;
    return 0;


}

// 로그 서버로 보내는 로직
static int log_send(struct socket *sock, const char *buf, size_t len){
    while (len){
        struct msghdr msg = {.msg_flags = MSG_NOSIGNAL};
        struct kvec vec = {
            .iov_base = (void *)buf,
            .iov_len = len
        };
        // int kernel_sendmsg(struct socket * sock, struct msghdr * msg, struct kvec * vec, size_t num, size_t size)
        int sent = kernel_sendmsg(sock, &msg, &vec, 1, len);

        if (sent < 0)
            return sent;
        else if (sent == 0)
            return -EPIPE;

        buf += sent;
        len -= sent;

    }
    return 0;

}


// 워커
static void log_worker(struct work_struct *work){
    struct log_event event;
    char line[64];
    bool got_event;
    size_t len;
    unsigned long irq_flags;

    while (true){
        raw_spin_lock_irqsave(&log_lock, irq_flags);
        got_event = kfifo_get(&log_event, &event);
        raw_spin_unlock_irqrestore(&log_lock, irq_flags);

        if (!got_event)
            return;

        if (!log_sock && log_connect(&log_sock) < 0)
            return;

        len = scnprintf(line, sizeof(line), "%u,%d,%llu\n",
                        (unsigned int)event.flag, event.tgid,
                        (unsigned long long)event.time_ns);

        if (log_send(log_sock, line, len) < 0){
        sock_release(log_sock);
        log_sock = NULL;
        return;
    }

    }
}

void event_send(enum log_flag flag, pid_t tgid){
    struct log_event event = {
        .flag = flag,
        .tgid = tgid,
        .time_ns = ktime_get_real_ns(),
    };

    unsigned long irq_flags;
    bool queued;

    raw_spin_lock_irqsave(&log_lock, irq_flags);
    queued = kfifo_put(&log_event, event);
    raw_spin_unlock_irqrestore(&log_lock,irq_flags);

    if (queued)
        schedule_work(&log_work);


}





// 워커 정리하고 socket 릴리즈 해줌
void log_exit(void){
    cancel_work_sync(&log_work);
    
    if (log_sock){
        sock_release(log_sock);
        log_sock = NULL;
    }

}
