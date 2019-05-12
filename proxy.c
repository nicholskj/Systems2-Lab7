#include <stdio.h>
#include "csapp.h"  
#include "cache.h"

//prototypes for functions defined and used in this file
static void handleRequest(int connfd);
static void getRequest(int connfd, char * method, char * uri, 
                       char * version, char * host);
static int isValid(char * method, char * version);
static void parseURI(char * uri, char * host, char * path, char * port);
static void getPort(char * port, char * str);
static void getPath(char * port, char * str);
static void getHost(char * port, char * str);
static void makeRequest(int connfd, char * uri, char * request, 
                        char * host, char * port);
static void buildRequest(char * request, char * host,
                         char * path, char * version);

//
// Create a listening socket. Accept connection requests on
// the listening socket.  Read one request from the connected
// socket, respond to it, and close the socket.
//
// Helpful code: main in tiny.c
//
int main(int argc, char **argv)
{
    int listenfd, connfd;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }

    listenfd = Open_listenfd(argv[1]);
    cacheInit();
    while(1) {
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *) &clientaddr, &clientlen);
        Getnameinfo((SA *) &clientaddr, clientlen, hostname, MAXLINE,
            port, MAXLINE, 0);
        printf("Accepted connection from (%s, %s)\n", hostname, port);
        handleRequest(connfd);
        Close(connfd);
    }
    return 1;
}

// handleRequest
// Reads the request from the connected socket by calling getRequest.
// If the method or version isn't valid, the function returns
// without doing anything.
//
// Otherwise, it checks to see if the item is cached. 
// If it is, the item is written to the connected socket.  
// If it is not, it parses the uri to get the host, the path 
// to the file, and the port. Then, it builds the
// request. Finally, it sends the request, gets the response, 
// sends the response to the connected socket, and potentially 
// caches the response.
//
// Helper functions: getRequest, isValid, findCacheItem, 
//                   Rio_writen (for writing object from cache),
//                   parseURI, buildRequest, makeRequest
void handleRequest(int connfd)
{
    char method[MAXLINE];  //first word on request line, such as GET
    char uri[MAXLINE];     //uri on the request line
    char version[MAXLINE]; //version on the request line, such as HTTP/1.1
    char port[MAXLINE];    //port obtained from the request line or 80 
    char host[MAXLINE];    //host from the HOST: header or from the uri if no header
    char path[MAXLINE];    //path to table obtained from the request line
    char request[MAXBUF];  //request to send to origin server

    getRequest(connfd, method, uri, version, host);
    printf("Method: %s\n", method);
    printf("Uri: %s\n", uri);
    printf("Version: %s\n", version);
    printf("Host: %s\n", host);
    if (!isValid(method, version)) {
        return;
    }
    cacheItem * item = findCacheItem(uri);
    if (item != NULL) {
        Rio_writen(connfd, item->content, item->size);
        return;
    }
    parseURI(uri, host, path, port);
    buildRequest(request, host, path, version);
    printf(request);
    makeRequest(connfd, uri, request, host, port);
    printCacheList();
    return;
}

// buildRequest
// Build the request to send to the origin server.
// The request needs to have the: 
//    1) request line (GET path version)
//    2) Host: header line
//    3) User-Agent: header line
//    4) Connection: close
//    5) Proxy-Connection: close
//
// Helpful code: serve_static in tiny.c 
void buildRequest(char * request, char * host, char * path, char * version)
{
   char *userAgentHdr = "Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3";
   sprintf(request, "GET %s %s\r\n", path, version);
   sprintf(request, "%sHost: %s\r\n", request, host);
   sprintf(request, "%sUser-Agent: %s\r\n", request, userAgentHdr);
   sprintf(request, "%sConnection: close\r\n", request);
   sprintf(request, "%sProxy-Connection: close\r\n\r\n", request);

   return;
}

// makeRequest
// This function connects to the origin server.
// If the open succeeds, the request is written to the socket.
// After that, the response from the origin server is read in
// a loop and sent to the client via the connected socket.
//
// In addition, space is dynamically allocated to hold the object
// in case it is not too large to be cached. The response from the
// origin server is copied (piece by piece) into the dynamically 
// allocated space using a memcpy.  If the object isn't too big,
// it is added to the cache.  If it is too big, the dynamically
// allocated space is freed.
//
// Helper functions: Calloc, Open_clientfd, Rio_writen, Rio_readinitb, 
//                   Rio_readlineb, addCacheLine, Close, various C functions
//
// Helpful code: Nothing complete fits, but you could look at
//               serve_static and read_requesthdrs in tiny.c
void makeRequest(int connfd, char * uri, char * request, 
                 char * host, char * port)
{
    int srcfd;
    char buf[MAXLINE];
    rio_t rio;
    char * cache = (char*) Calloc(1, MAX_OBJECT_SIZE);
    int objSize = 0;

    srcfd = Open_clientfd(host, port);
    Rio_readinitb(&rio, srcfd);
    Rio_writen(srcfd, request, MAXLINE);
    size_t size = Rio_readlineb(&rio, buf, MAXLINE);
    while (size) {
        Rio_writen(connfd, buf, size);
        objSize += size;
        if (objSize <= MAX_OBJECT_SIZE)
            memcpy(cache + objSize - size, buf, size);
        size = Rio_readlineb(&rio, buf, MAXLINE);
    }
    if (objSize <= MAX_OBJECT_SIZE)	
        addCacheItem(uri, cache, objSize);
    else 
		free(cache);
    Close(srcfd);
    return;
}

// getPort
// Retrieves the port from the uri if present.
// If not present, the port defaults to "80"
// Example:  http://localhost:12345/godzilla.jpg
//          port is 12345
void getPort(char * port, char * uri)
{
   int i = 7;
   int k = 0;
   while(uri[i] != ':' && uri[i] != '/')
       i++;
   i++;
   if (uri[i - 1] == '/') {
       port[0] = '8';
       port[1] = '0';
       port[2] = '\0';
       return;
   }
   while(uri[i] != '/') {
       port[k] = uri[i];
       i++;
       k++;
   }
   port[k] = '\0';
   return;
}

// getHost
// Retrieves the host from the uri.  This function is
// called if the request doesn't have a Host line.
// Example:  http://localhost:12345/godzilla.jpg
//           host is localhost
void getHost(char * host, char * uri)
{
   return;
}

// getPath
// Retrieves the path from the uri.
// Example:  http://localhost:12345/godzilla.jpg
//           path is /godzilla.jpg
void getPath(char * path, char * uri)
{
    int i = 7;
    int k = 0;
    while(uri[i] != '/')
        i++;
    while(uri[i] != '\0') {
        path[k] = uri[i];
        i++;
        k++;
    }
    path[k] = '\0';
    return;
}

// parseURI
// Parses the uri to obtain the host, path, and port from
// the uri. host may have been already obtained from a
// Host: header liner.
//
// Helper functions: getHost, getPort, getPath, various C functions
void parseURI(char * uri, char * host, char * path, char * port)
{
    getPath(path, uri);
    getPort(port, uri);
    //printf("(%s, %s, %s)\n", host, path, port);
    return;
}

// getRequest
// Reads the request from the client via the connected
// socket. Gets the method, uri, and version from the
// request line.  The headers are read and discarded except
// for the Host: header line.  In that case, the host
// buffer is initialized to the host from the Host:
// header line.  For example,
// Host: localhost:12345
// causes host to be set to "localhost"
//
// Helper functions: Rio_readinitb, Rio_readlineb,
//                   various C functions
// Helpful code: read_requesthdrs in tiny.c
void getRequest(int connfd, char * method, char * uri, 
                char * version, char * host)
{
    char buf[MAXLINE];
    rio_t rio;

    Rio_readinitb(&rio, connfd);
    if (!Rio_readlineb(&rio, buf, MAXLINE))
        return;
    printf("%s", buf);
    sscanf(buf, "%s %s %s", method, uri, version);
    Rio_readlineb(&rio, buf, MAXLINE);
    while (strcmp(buf, "\r\n")) {
        Rio_readlineb(&rio, buf, MAXLINE);
        if (!strncmp(buf, "Host:", 4)) {
            int i = 6;
            while(buf[i] != ':' && buf[i] != '\r') {
                host[i - 6] = buf[i];
                i++;
            }
            host[i - 6] = '\0';
        }
    }

    return;
}

// isValid
// This function returns 1 if the method is GET
// and the version is either HTTP/1.1 or HTTP/1.0.
// Otherwise this function returns 0.
int isValid(char * method, char * version)
{
    if (!strcmp(method, "GET")) {
        if (!strcmp(version, "HTTP/1.1") || !strcmp(version, "HTTP/1.0"))
            return 1;
    }
    return 0;
}
   
