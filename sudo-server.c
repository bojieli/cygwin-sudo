#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <errno.h>
#include <pty.h>
#include <poll.h>

#define IF_ERROR(expr,msg) if ((expr) == -1) { fprintf(stderr, "[%d] " msg "\n", errno); return 1; }
#define debug printf
#define BUF_SIZE 1024

// receive until error or filled up the buffer
int recvn(int fd, void* buf, size_t size) {
    int recvtotal = 0;
    int recvlen;
    while (recvtotal < size) {
        recvlen = recv(fd, buf, size - recvtotal, 0);
        if (recvlen == -1)
            return -1;
        if (recvlen == 0)
            return size;
        recvtotal += recvlen;
        buf += recvlen;
    }
    return recvtotal;
}

int init_recv(int newfd, int* pid, int* amaster) {
    int i;

    struct termios termp;
    IF_ERROR(recvn(newfd, &termp, sizeof(termp)), "termp")
    struct winsize winp;
    IF_ERROR(recvn(newfd, &winp, sizeof(winp)), "winp")
    
    int command_size, argv_size, envp_size;
    IF_ERROR(recvn(newfd, &command_size, sizeof(int)), "command_size")
    char *command = malloc(command_size+1);
    IF_ERROR(recvn(newfd, command, command_size), "command")
    command[command_size] = '\0';
    debug("command: %s\nargv: ", command);
    
    IF_ERROR(recvn(newfd, &argv_size, sizeof(int)), "argv_size")
    char **argv = malloc(argv_size+1);
    for (i=0; i<argv_size; i++) {
        int len;
        IF_ERROR(recvn(newfd, &len, sizeof(int)), "argv length")
        argv[i] = malloc(len+1);
        IF_ERROR(recvn(newfd, argv[i], len), "argv")
        *(argv[i] + len) = '\0';
        debug("%s ", argv[i]);
    }
    argv[argv_size] = NULL;
    debug("\n");

    IF_ERROR(recvn(newfd, &envp_size, sizeof(int)), "envp_size")
    debug("envp size: %d...", envp_size);
    char **envp = malloc(envp_size+1);
    for (i=0; i<envp_size; i++) {
        int len;
        IF_ERROR(recvn(newfd, &len, sizeof(int)), "envp length")
        envp[i] = malloc(len+1);
        IF_ERROR(recvn(newfd, envp[i], len), "envp")
        *(envp[i] + len) = '\0';
    }
    envp[envp_size] = NULL;
    debug("\n");

    int cwd_size;
    IF_ERROR(recvn(newfd, &cwd_size, sizeof(int)), "cwd_size")
    char *cwd = malloc(cwd_size+1);
    IF_ERROR(recvn(newfd, cwd, cwd_size), "cwd");
    cwd[cwd_size] = '\0';
    chdir(cwd);
    debug("cwd: %s\n", cwd);
    fflush(stdout);

    IF_ERROR((*pid = forkpty(amaster, NULL, &termp, &winp)), "forkpty")
    if (*pid == 0) {
        execvpe(command, argv, envp);
        return 0;
    }
    debug("forked pty, pid = %d, amaster = %d\n", *pid, *amaster);
    return 0;
}

int msg_loop(int newfd, int pid, int amaster) {

    char buf[BUF_SIZE];
    struct pollfd fds[2] = {{.fd = amaster, .events = POLLIN}, {.fd = newfd, .events = POLLIN}};
    while (1) {
        IF_ERROR(poll(fds, 2, -1), "poll")
        if (fds[0].revents & POLLERR || fds[1].revents & POLLERR)
            return -1;

        int readlen;
        if (fds[0].revents & POLLIN) {
            IF_ERROR((readlen = read(amaster, buf, BUF_SIZE)), "read from pty")
            if (readlen == 0) {
                debug("pty exit, pid = %d, amaster = %d\n", pid, amaster);
                return 0;
            }
            IF_ERROR(write(newfd, buf, readlen), "write to socket")
        }
        if (fds[1].revents & POLLIN) {
//            IF_ERROR((readlen = recv(newfd, buf, BUF_SIZE, 0)), "read from socket")
//            IF_ERROR(write(amaster, buf, readlen), "write to pty")
            int datalen;
            IF_ERROR((readlen = recvn(newfd, &datalen, sizeof(int))), "read datalen")
            if (readlen < sizeof(int)) {
                debug("user exit, socket = %d\n", newfd);
                return 0;
            }
            if (datalen == -1) { // special signal to change window size
                struct winsize wins;
                IF_ERROR(recvn(newfd, &wins, sizeof(wins)), "read window size")
                debug("winsize changed, socket = %d, amaster = %d\n", newfd, amaster);
                ioctl(amaster, TIOCSWINSZ, &wins);
                kill(pid, SIGWINCH);
            }
            else if (datalen > BUF_SIZE) {
                debug("[ERROR] socket data length %d exceeds buffer size %d\n", datalen, BUF_SIZE);
                return 0;
            }
            else { // normal data
                IF_ERROR(recvn(newfd, buf, datalen), "read from socket")
                IF_ERROR(write(amaster, buf, datalen), "write to pty")
            }
        }
    }
    return 0;
}

int child(int newfd) {
    int flag;
    int pid, amaster;
    if (0 != (flag = init_recv(newfd, &pid, &amaster)))
        goto exit;
    if (0 != (flag = msg_loop(newfd, pid, amaster)))
        goto exit;
exit:
    close(newfd);
    close(amaster);
    kill(pid, SIGTERM);
    return flag;
}

int accept_new(int sockfd) {
    struct sockaddr_in client_addr;
    int sin_size = sizeof(client_addr);
    int newfd;
    IF_ERROR((newfd = accept(sockfd, (struct sockaddr *)&client_addr, &sin_size)), "accept")
    
    if (!fork()) {
        debug("Spawned new child: pid = %d, sock = %d\n", getpid(), newfd);
        exit(child(newfd));
    }
    close(newfd);
    return 0;
}

int main() {
    int sockfd;
    struct sockaddr_in myaddr;
    IF_ERROR((sockfd = socket(AF_INET, SOCK_STREAM, 0)), "socket")
    myaddr.sin_family = AF_INET;
    myaddr.sin_port = htons(12345);
    myaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    bzero(&(myaddr.sin_zero), 8);
    IF_ERROR(bind(sockfd, (struct sockaddr *)&myaddr, sizeof(myaddr)), "bind")
    IF_ERROR(listen(sockfd, 10), "listen")
    while (1) {
        accept_new(sockfd);
    }
}
