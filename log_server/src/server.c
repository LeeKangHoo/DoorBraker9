#include <arpa/inet.h>
#include <libgen.h>
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

#define SERVER_PORT 9490


struct client_ctx {
    int sock;
    struct sockaddr_in addr;
};

static pthread_mutex_t log_file_lock = PTHREAD_MUTEX_INITIALIZER;
static char log_directory[PATH_MAX];

static int init_log_directory(void)
{
    char executable[PATH_MAX];
    ssize_t length = readlink("/proc/self/exe", executable, sizeof(executable));

    if (length < 0)
        return -1;
    if ((size_t)length == sizeof(executable)) {
        errno = ENAMETOOLONG;
        return -1;
    }
    executable[length] = '\0';

    char *build_directory = dirname(executable);
    int written = snprintf(log_directory, sizeof(log_directory), "%s/logs",
                           dirname(build_directory));
    if (written < 0 || (size_t)written >= sizeof(log_directory)) {
        errno = ENAMETOOLONG;
        return -1;
    }

    if (mkdir(log_directory, 0755) == -1 && errno != EEXIST)
        return -1;

    return 0;
}

void save_log(const char *client_ip, unsigned int flag, int tgid, unsigned long long time_ns){
    static const char *const event_names[] = {
        [1] = "cBPF",
        [2] = "dup2",
        [3] = "execve",
    };
    time_t seconds = (time_t)(time_ns / 1000000000ULL);
    unsigned long long millis = (time_ns % 1000000000ULL) / 1000000ULL;
    struct tm date;
    char filename[32];
    char path[PATH_MAX];
    char time_text[32];
    char unknown_event[32];
    const char *event_name;

    if (localtime_r(&seconds, &date) == NULL){
        perror("localtime_r err");
        return;
    }

    if (strftime(time_text, sizeof(time_text), "%Y-%m-%d %H:%M:%S", &date) == 0) {
        fprintf(stderr, "log timestamp format err\n");
        return;
    }

    if (strftime(filename, sizeof(filename), "%y-%m-%d.log", &date) == 0){
        fprintf(stderr, "log file create err");
        return;
    }

    int written = snprintf(path, sizeof(path), "%s/%s", log_directory, filename);
    if (written < 0 || (size_t)written >= sizeof(path)) {
        fprintf(stderr, "log file path too long\n");
        return;
    }

    if (flag < sizeof(event_names) / sizeof(event_names[0]) && event_names[flag]) {
        event_name = event_names[flag];
    } else {
        snprintf(unknown_event, sizeof(unknown_event), "unknown(%u)", flag);
        event_name = unknown_event;
    }

    pthread_mutex_lock(&log_file_lock);

    FILE *file = fopen(path, "a");
    if (file == NULL) {
        perror(path);

    } else {
        if (fprintf(file, "[%s.%03llu] ip=%s event=%s tgid=%d\n",
                    time_text, millis, client_ip, event_name, tgid) < 0)
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

    if (init_log_directory() == -1) {
        perror("log directory");
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
        .sin_port = htons(SERVER_PORT),
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

