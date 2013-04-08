#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <poll.h>

#define IF_ERROR(expr,msg) if ((expr) == -1) { fprintf(stderr, "[%d] " msg "\n", errno); return 1; }
#define debug printf
#define BUF_SIZE 1024

int sockfd; // It should be global because signal handler needs it
struct termios save_termios;

int init(int argc, char** argv, char** envp) 
{
    int i;

    struct sockaddr_in server_addr;
    IF_ERROR((sockfd = socket(AF_INET, SOCK_STREAM, 0)), "socket")
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(12345);
    inet_aton("127.0.0.1", &server_addr.sin_addr);
    bzero(&(server_addr.sin_zero), 8);
    IF_ERROR(connect(sockfd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr)), "connect")

    struct termios termp;
    tcgetattr(0, &termp);
    IF_ERROR(send(sockfd, &termp, sizeof(termp), 0), "termp")
    struct winsize winp;
    ioctl(0, TIOCGWINSZ, &winp);
    IF_ERROR(send(sockfd, &winp, sizeof(winp), 0), "winp")

    if (argc < 2) {
        argc = 3;
        argv[1] = "/bin/bash";
        argv[2] = "-i";
    }

    int command_len = strlen(argv[1]);
    IF_ERROR(send(sockfd, &command_len, sizeof(int), 0), "command_len")
    IF_ERROR(send(sockfd, argv[1], command_len, 0), "command")

    int argc_num = argc-1;
    IF_ERROR(send(sockfd, &argc_num, sizeof(int), 0), "argv_size")
    for (i=1; i<argc; i++) {
        int len = strlen(argv[i]);
        IF_ERROR(send(sockfd, &len, sizeof(int), 0), "argv len")
        IF_ERROR(send(sockfd, argv[i], len, 0), "argv")
    }

    int envp_num = -1;
    while (envp[++envp_num] != NULL);
    IF_ERROR(send(sockfd, &envp_num, sizeof(int), 0), "envp_size")
    for (i=0; i<envp_num; i++) {
        int len = strlen(envp[i]);
        IF_ERROR(send(sockfd, &len, sizeof(int), 0), "envp len")
        IF_ERROR(send(sockfd, envp[i], len, 0), "envp")
    }

    char buf[BUF_SIZE];
    getcwd(buf, BUF_SIZE);
    int readlen = strlen(buf);
    IF_ERROR(send(sockfd, &readlen, sizeof(int), 0), "cwd len");
    IF_ERROR(send(sockfd, buf, readlen, 0), "cwd");

    return 0;
}

int msg_loop() 
{
    char buf[BUF_SIZE];
    int readlen;
    struct pollfd fds[2] = {{.fd = 0, .events = POLLIN}, {.fd = sockfd, .events = POLLIN}};
    while (1) {
        IF_ERROR(poll(fds, 2, -1), "poll")
        if (fds[0].revents & POLLERR || fds[1].revents & POLLERR)
            return 0;
        if (fds[0].revents & POLLIN) {
            IF_ERROR((readlen = read(0, buf, BUF_SIZE)), "read from user")
            if (readlen == 0)
                return 0;
            IF_ERROR(send(sockfd, &readlen, sizeof(int), 0), "send data size to socket")
            IF_ERROR(send(sockfd, buf, readlen, 0), "write to socket")
        }
        if (fds[1].revents & POLLIN) {
            IF_ERROR((readlen = recv(sockfd, buf, BUF_SIZE, 0)), "read from socket")
            if (readlen == 0)
                return 0;
            IF_ERROR(write(1, buf, readlen), "write to user")
        }
    }
    close(sockfd);
    return 0;
}

int tty_raw(int fd)
{
    tcgetattr(fd, &save_termios);
    struct termios buf = save_termios;

    buf.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
                    /* echo off, canonical mode off, extended input
                       processing off, signal chars off */
    buf.c_iflag &= ~(BRKINT | ICRNL | ISTRIP | IXON);
                    /* no SIGINT on BREAK, CR-toNL off, input parity
                       check off, don't strip the 8th bit on input,
                       ouput flow control off */
    buf.c_cflag &= ~(CSIZE | PARENB);
                    /* clear size bits, parity checking off */
    buf.c_cflag |= CS8;
                    /* set 8 bits/char */
    buf.c_oflag &= ~(OPOST);
                    /* output processing off */
    buf.c_cc[VMIN] = 1;  /* 1 byte at a time */
    buf.c_cc[VTIME] = 0; /* no timer on input */

    return tcsetattr(fd, TCSAFLUSH, &buf);
}

int tty_reset(int fd)
{
    return tcsetattr(fd, TCSAFLUSH, &save_termios);
}

void winch_handler(int signo)
{
    if (signo == SIGWINCH) {
        struct winsize winp;
        ioctl(0, TIOCGWINSZ, &winp);
        int flag = -1;
        if (-1 == send(sockfd, &flag, sizeof(int), 0))
            goto exit;
        if (-1 == send(sockfd, &winp, sizeof(winp), 0))
            goto exit;
    }
    return;
exit:
    tty_reset(0);
    exit(1);
}

int main(int argc, char **argv, char **envp)
{
    if (init(argc, argv, envp))
        return 1;
    tty_raw(0);
    signal(SIGWINCH, winch_handler);
    int retval = msg_loop();
    tty_reset(0);
    return retval;
}
