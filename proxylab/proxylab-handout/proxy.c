#include <stdio.h>
#include "csapp.h"
/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define WEB_PREFIX "http://"
#define NTHREADS 4
#define SBUFSIZE 16


typedef struct {
    int *buf;          /* Buffer array */         
    int n;             /* Maximum number of slots */
    int front;         /* buf[(front+1)%n] is first item */
    int rear;          /* buf[rear%n] is last item */
    sem_t mutex;       /* Protects accesses to buf */
    sem_t slots;       /* Counts available slots */
    sem_t items;       /* Counts available items */
} sbuf_t;
/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static sbuf_t sbuf;


void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename,char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
void replaceHTTPVersion(char *);
void sbuf_init(sbuf_t *sp, int n);
void sbuf_deinit(sbuf_t *sp);
void sbuf_insert(sbuf_t *sp, int item);
int sbuf_remove(sbuf_t *sp);

void sbuf_init(sbuf_t *sp, int n)
{
    sp->buf = Calloc(n, sizeof(int));
    sp->n = n;
    sp->front = sp->rear = 0;
    Sem_init(&sp->mutex, 0, 1);
    Sem_init(&sp->slots, 0, n);
    Sem_init(&sp->items, 0, 0);
}

void sbuf_deinit(sbuf_t *sp)
{
    Free(sp->buf);
}

void sbuf_insert(sbuf_t *sp, int item)
{
    P(&sp->slots);
    P(&sp->mutex);
    sp->buf[(++sp->rear)%(sp->n)] = item;
    V(&sp->mutex);
    V(&sp->items);
}

int sbuf_remove(sbuf_t *sp)
{
    int item;
    P(&sp->items);
    P(&sp->mutex);
    item = sp->buf[(++sp->front) % (sp->n)];
    V(&sp->mutex);
    V(&sp->slots);
    return item;
}

void doit(int fd)
{
    int is_static;
    struct stat sbuf;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE], fileName[MAXLINE];
    char clientRequest[MAXLINE];
    char host[MAXLINE], port[MAXLINE];
    rio_t rio, rioTiny;

    //read request line and headers
    Rio_readinitb(&rio, fd);
    Rio_readlineb(&rio, buf, MAXLINE);
    replaceHTTPVersion(buf);
    parseLine(buf, host, port, method, uri, version, fileName);

    if(strcasecmp(method, "GET")){
        clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
        return;
    }
    int rvv = readAndFormatRequestHeader(&rio, clientRequest, host, port, method, uri, version, fileName);
    if(rvv == 0){
        return;
    }

    printf("========= $$BEGIN we have formatted the reqeust into $$=========\n");
    printf("%s", clientRequest);
    printf("========= $$END we have formatted the reqeust into $$=========\n");

    //step3 establish connection with tiny server
    // localhost:1025

    char hostName[100];
    char *colon = strstr(host, ":");
    strncpy(hostName, host, colon - host);
    printf("host is %s\n", hostName);
    printf("port is %s\n", port);
    int clientfd = Open_clientfd(hostName, port);

    Rio_readinitb(&rioTiny, clientfd);
    Rio_writen(rioTiny.rio_fd, clientRequest, strlen(clientRequest));

    //step4 read the response from tiny and send it to the client
    printf("---prepare to get the response---\n");
    char tinyResponse[MAXLINE];
    char *tinyResponseP = tinyResponse;

    int n;
    while((n = Rio_readnb(&rioTiny, tinyResponse, MAXLINE)) != 0){
        Rio_writen(fd, tinyResponse, n);
    }
    
}


void replaceHTTPVersion(char *buf){
    char *pos = NULL;
    if((pos = strstr(buf, "HTTP/1.1")) != NULL){
        buf[pos - buf + strlen("HTTP/1.1") - 1] = '0';
    }
}
void parseLine(char *buf, char *host, char *port, char *method, char *uri, char* version, char *fileName){
    sscanf(buf, "%s %s %s", method, uri, version);
    //method = "GET", uri = "http://localhost:15213/home.html", version = "HTTP1.0"

    char *hostp = strstr(uri, WEB_PREFIX) + strlen(WEB_PREFIX);
    char *slash = strstr(hostp, "/");
    char *colon = strstr(hostp, ":");
    //get host name
    strncpy(host, hostp, slash - hostp);
    //get port number
    strncpy(port, colon + 1, slash - colon - 1);
    //get file name
    strcpy(fileName, slash);
}
int readAndFormatRequestHeader(rio_t* rio, char* clientRequest, char* Host, char* port, char *method, char *uri, char *version, char * fileName){
    int UserAgent = 0, Connection = 0, ProxyConnection = 0, HostInfo = 0;
    char buf[MAXLINE / 2];
    int n;
    
    // 1. add get hostname HTTP/1.0 to header
    sprintf(clientRequest, "GET %s HTTP/1.0\r\n", fileName);

    n = Rio_readlineb(rio, buf, MAXLINE);
    char *findp;
    while(strcmp("\r\n", buf) != 0 && n != 0){
        strcat(clientRequest, buf);
        printf("receive buf %s\n", buf);

        if((findp = strstr(buf, "User-Agent:")) != NULL){
            UserAgent = 1;
        }
        else if((findp = strstr(buf, "Proxy-Connection:")) != NULL){
            ProxyConnection = 1;
        }
        else if((findp = strstr(buf, "Connection:")) != NULL){
            Connection = 1;
        }
        else if((findp = strstr(buf, "Host:")) != NULL){
            HostInfo = 1;
        }

        n = Rio_readlineb(rio, buf, MAXLINE);
    }

    if(n == 0){
        return 0;
    }

    if(HostInfo == 0){
        sprintf(buf, "Host: %s\r\n", Host);
        strcat(clientRequest, buf);
    }

    if(UserAgent == 0){
        strcat(clientRequest, user_agent_hdr);
    }

    if(Connection == 0){
        sprintf(buf, "Connection: close\r\n");
        strcat(clientRequest, buf);
    }

    if(ProxyConnection == 0){
        sprintf(buf, "Proxy-Connection: close\r\n");
        strcat(clientRequest, buf);
    }

    strcat(clientRequest, "\r\n");
    return 1;
}
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
    char buf[MAXLINE], body[MAXBUF];

     // Build the HTTP response body
    sprintf(body, "<html><title>Tiny Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

    //Print the HTTP response
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    Rio_writen(fd, buf, strlen(buf));
    Rio_writen(fd, body, strlen(body));
}

void read_requesthdrs(rio_t *rp)
{
    char buf[MAXLINE];
    Rio_readlineb(rp, buf, MAXLINE);
    while(strcmp(buf, "\r\n")){
        Rio_readlineb(rp, buf, MAXLINE);
        printf("%s", buf);
    }
    return;
}

void *thread(void *vargp){
    Pthread_detach(pthread_self());
    while(1){
        int connfd = sbuf_remove(&sbuf);
        doit(connfd);
        Close(connfd);
    }
}

int main(int argc, char **argv)
{
    int listenfd, connfd;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;

    //check command line
    if(argc != 2){
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }

    listenfd = Open_listenfd(argv[1]);
    pthread_t tid;
    sbuf_init(&sbuf, SBUFSIZE);

    int i;
    for(i = 0; i < NTHREADS; i++){
        Pthread_create(&tid, NULL, thread, NULL);
    }
    
    while(1){
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA*)&clientaddr, &clientlen);
        Getnameinfo((SA*)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
        printf("Accepted connection from(%s, %s)\n", hostname, port);
        sbuf_insert(&sbuf, connfd);
    }
}