#include <arpa/inet.h>
#include <limits.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h> 
#include <sys/stat.h>
#include <time.h>


struct client_ctx {
    int sock;
    struct sockaddr_in addr;
};

static pthread_mutex_t log_file_lock = PTHREAD_MUTEX_INITIALIZER;

void save_log(const char *client_ip, unsigned int flag, int tgid, unsigned long long time_ns){
    time_t seconds = (time_t)(time_ns / 1000000000ULL);
    struct tm date;
    char path[64];

    if (localtime_r(&seconds, &date) == NULL){
        perror("localtime_r err");
        return;
    }

    if (strftime(path, sizeof(path), "logs/%y-%m-%d.log", &date) == 0){
        fprintf(stderr, "log file create err");
        return;
    }

    pthread_mutex_lock(&log_file_lock);

    FILE *file = fopen(path, "a");
    if (file == NULL) {
        perror(path);

    } else {
        if (fprintf(file, "%s,%u,%d,%llu\n", client_ip, flag, tgid, time_ns) < 0)
            perror("fprintf");

        if (fclose(file) == EOF)
            perror("fclose");
    }

    pthread_mutex_unlock(&log_file_lock);
}


static void parse_and_save(const char *client_ip, const char *line)
{
    unsigned long long values[3];
    const char delimiters[] = {',', ',', '\0'};

    for (size_t i = 0; i < 3; i++) {
        char *end;

        if (*line < '0' || *line > '9')
            return;

        errno = 0;
        values[i] = strtoull(line, &end, 10);
        if (errno == ERANGE || *end != delimiters[i])
            return;

        if (i < 2)
            line = end + 1;
    }

    if (values[0] == 0 || values[0] > UINT_MAX ||
        values[1] == 0 || values[1] > INT_MAX)
        return;

    save_log(client_ip, (unsigned int)values[0], (int)values[1], values[2]);
}

void *recv_log(void *ctx){
    struct client_ctx *client = ctx;
    char client_ip[INET_ADDRSTRLEN];
    char client_buf[1024];
    char line[64];
    size_t used = 0;

    if (inet_ntop(AF_INET, &client->addr.sin_addr,
                  client_ip, sizeof(client_ip)) == NULL) {
        perror("inet_ntop");
        close(client->sock);
        free(client);
        return NULL;
    }

    while (1){
        ssize_t n = recv(client->sock, client_buf, sizeof(client_buf), 0);

        if (n == 0)
            break;

        if (n < 0){
            if (errno == EINTR)
                continue;
            perror("recv err");
            break;
        }

        for (ssize_t i = 0; i < n; i++) {
            char ch = client_buf[i];

            if (ch == '\n') {
                if (used < sizeof(line)) {
                    line[used] = '\0';
                    parse_and_save(client_ip, line);
                }
                used = 0;
            } else if (used < sizeof(line)) {
                /* A full buffer marks a line to discard at the next newline. */
                if (ch == '\0')
                    used = sizeof(line);
                else
                    line[used++] = ch;
            }
        }
    }
    close(client->sock);
    free(client);
    return NULL;

}


int main(){
    // 해야할 것
    // 소켓 생성 -> bind -> listen -> 오면 accept -> recv해서 파싱 -> 파싱한거 파일에 저장 
    // 로그 파일은 그날 날짜로. 해당 날짜 파일이 있으면 거기에 추가로 쓰고, 없으면 생성 후 쓰기
    // 여러 서버 요청 받고 로깅할 수 있게 thread로 다받아서 연결해두기 -> pthread에 mutex써서 로그 적는거 관리도해야해

    if (mkdir("logs", 0755) == -1 && errno != EEXIST) {
        perror("mkdir logs");
        return 1;
    }


    int sock = socket(AF_INET, SOCK_STREAM, 0);
    
    if (sock < 0){
        perror("sock err");
        return 1;
    }

    int ret = 0;

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(9490),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };

    ret = bind(sock, (struct sockaddr *)&addr, sizeof(addr));
    if (ret < 0){
        perror("bind err");
        return 1;
    }

    ret = listen(sock, 16);
    if (ret < 0){
        perror("listen err");
        return 1;
    }


    while (1) {
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    pthread_t recv_t;

    int client_sock = accept(sock, (struct sockaddr *)&client_addr, &addr_len);
    if (client_sock < 0) {
        if (errno == EINTR)
            continue;
        perror("accept");
        break;
    }

    struct client_ctx *client = malloc(sizeof(*client));
    if (client == NULL) {
        perror("malloc");
        close(client_sock);
        continue;
    }

    client->sock = client_sock;
    client->addr = client_addr;

    int t_ret = pthread_create(&recv_t, NULL, recv_log, client);
    if (t_ret != 0) {
        fprintf(stderr, "pthread_create: %s\n", strerror(t_ret));
        close(client_sock);
        free(client);
        continue;
    }

    pthread_detach(recv_t);
}

close(sock);
}

