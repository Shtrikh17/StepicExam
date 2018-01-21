#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include <event2/listener.h>
#include <pthread.h>

struct parameters{
    char* ip;
    int port;
    char* dir;
};

struct handlerInfo{
    evutil_socket_t fd;
};

struct handlerCallbackArg{
    struct event* ev;
};

#define DEBUG

void printLog(char* p){
#ifdef DEBUG
    FILE* f = fopen("server.log", "a");
    fprintf(f, "%s\n", p);
    fclose(f);
#endif
}

void handler_cb(evutil_socket_t fd, short ev_flag, void* arg){
    printLog("Incoming data");

    struct handlerCallbackArg* hArg = (struct handlerCallbackArg*)arg;

    char Buffer[1024];
    int RecvSize = recv(fd, Buffer, 1024, MSG_NOSIGNAL);
    if(RecvSize==0 && errno!=EAGAIN){
        shutdown(fd, SHUT_RDWR);
        close(fd);
        //event_free(hArg->ev);
        free(hArg);
    }
    else if(RecvSize!=0){
        send(fd, Buffer, RecvSize, MSG_NOSIGNAL);
    }
}

void* connection_handler(void* arg){

    struct handlerInfo* hInfo = (struct handlerInfo*)arg;

    struct event_base* base = event_base_new();

    struct event* ev;
    struct handlerCallbackArg* hArg = (struct handlerCallbackArg*) malloc (sizeof(struct handlerCallbackArg));
    hArg->ev = ev;
    ev = event_new(base, hInfo->fd, (EV_READ | EV_PERSIST), handler_cb, hArg);
    event_add(ev, NULL);
    event_base_dispatch(base);
}



static void accept_conn_cb(struct evconnlistener* listener,
                           evutil_socket_t fd,
                           struct sockaddr* address,
                           int socklen,
                           void* ctx){

    printLog("Accpting incoming connection");
    int res;

    // Получим соответствующий base для event_base
    struct event_base* base = evconnlistener_get_base(listener);

    char ip[255];
    struct sockaddr_in* sa = (struct sockaddr_in*)address;
    res = inet_ntop(AF_INET, &sa->sin_addr, ip, 255);

    if(!res){
        printLog("Conversion error\n");
        exit(1);
    }

    char buf[1024];
    snprintf(buf, 1024, "Incoming:\nip: %s\nport: %d\n\n", ip, sa->sin_port);
    printLog(buf);

    pthread_t thread;
    struct handlerInfo* hInfo = (struct handlerInfo*) malloc (sizeof(struct handlerInfo));
    hInfo->fd = fd;

    res = pthread_create(&thread, NULL, connection_handler, hInfo);
    if(res!=0){
        printLog("Thread creation error\n");
        exit(1);
    }

    //shutdown(fd, SHUT_RDWR);
    //close(fd);
}

static void accept_error_cb(struct evconnlistener* listener,
                            evutil_socket_t fd,
                            struct sockaddr* address,
                            int socklen,
                            void* ctx){
    printLog("Incoming connection error\n");
    // Получим соответствующий base для event_base
    struct event_base* base = evconnlistener_get_base(listener);

    // Получим код ошибки
    int err = EVUTIL_SOCKET_ERROR();

    // Отправляем сообщение об ошибке в лог
    char buf[1024];
    snprintf(buf, 1024, "Error = %d \"%s\"\n", err, evutil_socket_error_to_string(err));
    printLog(buf);

    // Выходим из цикла обработки сообщений
    event_base_loopexit(base, NULL);
}

int server(struct parameters* params){
    // Создадим сокет

    int MasterSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_port = htons(params->port);
    int t = inet_pton(AF_INET, params->ip, &(sin.sin_addr));
    if(t==0){
        printLog("inet_pton address failure\n");
        exit(1);
    }
    else if(t==-1){
        printLog("inet_pton common failure\n");
        exit(1);
    }

    if(bind(MasterSocket, &sin, sizeof(sin))<0){
        printLog("Bind failure\n");
        exit(1);
    }

    if(listen(MasterSocket, SOMAXCONN)<0){
        printLog("Listen failure\n");
        exit(1);
    }

    // Создадим структуру для цикла обработки событий
    struct event_base* base = event_base_new();

    // Создадим listener
    struct evconnlistener* listener = evconnlistener_new(base,
                                                         accept_conn_cb,
                                                         NULL,
                                                         LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE,
                                                         0,
                                                         MasterSocket);
    evconnlistener_set_error_cb(listener, accept_error_cb);

    printLog("Starting event_base\n");

    // Запустим цикл обработки событий
    event_base_dispatch(base);

    printLog("Server exit\n");
    return 0;
}





int parse_argv(int argc, char** argv, struct parameters* params){

    int opt = 0;
    opterr = 0;
    while((opt=getopt(argc, argv, "h:p:d:"))!=-1){
        switch(opt){
            case 'h':
                params->ip = (char*) malloc (strlen(optarg)+1);
                strcpy(params->ip, optarg);
                break;
            case 'p':
                params->port = atoi(optarg);
                break;
            case 'd':
                params->dir = (char*) malloc (strlen(optarg)+1);
                strcpy(params->dir, optarg);
                break;
            default:
                break;
        }
    }

    if(params->ip==NULL || params->port==0 || params->dir==NULL)
        return -1;

    return 0;
}

void daemonize(void){
    pid_t pid = fork();
    if(pid<0){
        perror("daemonize.fork error");
        exit(1);
    }
    if(pid>0){
        char log[100];
        snprintf(log, 100, "New pid: %d\n", pid);
        printLog(log);
        exit(0);
    }
    if(setsid()==-1){
        perror("daemonize.setsid error");
        exit(1);
    }
    close(STDOUT_FILENO);
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
}

int main(int argc, char** argv){

    printLog("==================================");

    struct parameters params = {NULL, 0, NULL};
    if(parse_argv(argc, argv, &params)!=0){
        perror("Wrong command line arguments.\n");
        exit(1);
    }
    printLog("Successful parsing");
    char log[1024];
    snprintf(log, 1024, "ip: %s\nport: %d\ndir: %s\n", params.ip, params.port, params.dir);
    printLog(log);

    //daemonize();

    //sleep(20);

    server(&params);

    return 0;
}