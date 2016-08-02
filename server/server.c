/*********************************************************************
 ** Program Filename: server.c
 ** Author: Brandon Swanson
 ** Date: 07/31/2016
 ** Description: transmits directory contents or whole file to connected client
 ** Input: recieves as command line arg <SERVER PORT> on wich it will listen for connections
 ** Output: directory contents, status messages, files over network
 *********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <sys/stat.h>
#include <errno.h>


const int REC_BUFFER_SIZE = 512;
const int HANDLE_LIMIT = 10;
const int PRIVILEGED = 1024;
const int MAX_PORT = 65536;
const int GET_ADDR_NO_ERROR = 0;
const int BIND_SUCCESS = 0;
const int CONNECT_ERROR = -1;
const int EXPECTED_ARGS = 1;
const int POOL_SIZE = 5;
#define MIN(a,b) (((a)<(b))?(a):(b))


//print message, and if available perror messege, exit in failure
void error_exit(char* message){
    fprintf(stderr,"%s\n",message);
    if(errno){
        const char* error = strerror(errno);
        fprintf(stderr, "%s\n",error);
    }
    exit(1);
}

/*int getConnectedDataSocket(struct sockaddr clientAddr, char* portString, char* errorMsg){

    //todo parse portString and set port num here

    struct sockaddr_in *client_in = (struct sockaddr_in *) &clientAddr;
    client_in->sin_port = htons(4444);


    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == -1){
        errorMsg = "could not create socket file-descriptor";
        return CONNECT_ERROR;
    }

    // connect socket to client
    if (connect(s, (const struct sockaddr *) client_in, sizeof client_in->sin_addr) != CONNECT_ERROR){
        errorMsg = "could not connect to client on second socket for data transfer";
        close(s);
        return CONNECT_ERROR;
    }

    return s;
}*/

int getConnectedSocket(const char* remoteHostName, const char* remotePortSt){
    // set up hints struct and other socket initialization referenced from Beej's guide and linux manual examples
    // http://beej.us/guide/bgnet/output/html/multipage/clientserver.html
    // http://man7.org/linux/man-pages/man3/getaddrinfo.3.html

    // set up structs for requesting ip address from hostname
    struct addrinfo hints, *servinfo;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int getadderResult = getaddrinfo(remoteHostName, remotePortSt, &hints, &servinfo);
    if (getadderResult != GET_ADDR_NO_ERROR){
        fprintf(stderr, "getaddrinfo call failed with error: %s\n", gai_strerror(getadderResult));
        return 0;
    }

    // create socket file descriptor
    //int s = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);

    int s = 0;
    for (struct addrinfo *rp = servinfo; rp != NULL; rp = rp->ai_next) {
        // create file descriptor
        // dprintf(3, "attempting to connect to server on using addrinfo struct at:%p\n", rp);
        s = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (s == -1){
            // dprintf(3, "%s\n","failed to get socket file descriptor");
            continue;
        }

        // connect socket to server
        if (connect(s, rp->ai_addr, rp->ai_addrlen) != CONNECT_ERROR){
            // dprintf(3, "%s\n", "socket connected to server");
            break;
        }
        close(s);
    }
    // free up memory used by addr-struct-linked-list
    freeaddrinfo(servinfo);


    // ensure that fd is a valid socket, and not a closed socket ln:57
    struct stat statbuf;
    fstat(s, &statbuf);
    if (!S_ISSOCK(statbuf.st_mode)){
        s = 0;
    }

    return s;
}

//precondition serverPort is a string representin a valid non-reserved port number
//postcondition, returns file descriptor int of bound and listening socket
int getListeningSocket(int portNum) {
    // Creating a listening socket taken from my previous classwork
    // https://github.com/swanyriver/OS-program-4/blob/master/otp_crypt_d.c
    int socketFD;
    struct sockaddr_in serv_addr;

    //open and verify socket
    socketFD = socket(AF_INET, SOCK_STREAM, 0);
    if(socketFD<0) error_exit("Socket Error: Opening");

    //set ports to be reused
    int yes=1;
    setsockopt(socketFD, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

    //set up sever addr struct
    bzero((void *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons((uint16_t) portNum);

    //register socket as passive, with cast to generic socket address
    if (bind(socketFD, (struct sockaddr *) &serv_addr,
             sizeof(serv_addr)) < 0)
        error_exit("Socket Error: Binding");

    listen(socketFD,POOL_SIZE);

    return socketFD;
}

void getClientHostNameAndDataPort(int conSock, char** hostName, char** portString, char* readBuffer){
    ssize_t bytesRead = recv(conSock, readBuffer, REC_BUFFER_SIZE, 0);

    if (bytesRead == 0){
        return;
    }

    *hostName = readBuffer;
    *(readBuffer + bytesRead) = 0;

    for(char* cursor = readBuffer; cursor - readBuffer < bytesRead - 1; ++cursor){
        if (*cursor == ' '){
            *cursor = 0;
            *portString = cursor+1;
            break;
        }
    }


}


int main(int argc, char const *argv[]) {
    ////////////////////////////////////////////////////////////////////////
    ///////////CHECK NUMBER OF ARGS AND VALID PORT NUMBER //////////////////
    ////////////////////////////////////////////////////////////////////////

    if (argc < EXPECTED_ARGS + 1){
        error_exit("Server port number must be specified, example:\n./ftserver 9999");
    }
    const int serverPortNum = atoi(argv[1]);
    const char* serverPortSt = argv[1];

    // assert port range, also errror when non-numeric argument supplied as atoi returns 0 and 0 is privileged
    if (serverPortNum < PRIVILEGED || serverPortNum > MAX_PORT){
        fprintf(stderr, "invalid port number supplied (nummeric between %d-%d required)\n", PRIVILEGED, MAX_PORT);
        return 1;
    }

    ///// INVARIANT
    ///// server launched with valid port number

    int serverSocket = getListeningSocket(serverPortNum);
    if (!serverSocket){
        fprintf(stderr, "Server Terminating, unable to listen on specified port\n");
        return 1;
    }

    ///// INVARIANT
    ///// serverSocket is listening for incoming connections on requested port

    const size_t NAME_BUFF_LENGTH = 256;
    char* serverName = malloc(NAME_BUFF_LENGTH);
    gethostname(serverName, NAME_BUFF_LENGTH);
    printf("Awaiting incoming client connections,  connect via client program:\n");
    printf("python ftclient.py %s %s <COMMAND> [<FILENAME>] <DATA_PORT>\n", serverName, serverPortSt);


    //struct sockaddr_storage clientAddrSt;
    struct sockaddr clientAddr;
    socklen_t addrSize = sizeof clientAddr;
    int connectedSocket = accept(serverSocket, &clientAddr, &addrSize);

//    struct sockaddr clientIP;
//    addrSize = sizeof clientIP;
//    getpeername(connectedSocket, &clientIP, &addrSize);
//    printf("server: got connection from %s\n", clientIP.sa_data);

    //struct sockaddr clientAddr = clientAddrSt.ss_family == AF_INET ? (((struct sockaddr_in)clientAddrSt).sin_addr)

    //todo get rid of this
    char* msg = "hello there client";
    send(connectedSocket, msg, strlen(msg), 0);

    //todo get command and dataport from socket
    char* clientName = 0;
    char* dataPortString = 0;
    char* readBuffer = malloc(REC_BUFFER_SIZE);
    getClientHostNameAndDataPort(connectedSocket, &clientName, &dataPortString, readBuffer);

    if (!clientName || !dataPortString){
        fprintf(stderr, "%s\n", "error retrieving data port number from client");
        return 1;
    }

    printf("data connection: %s %s\n", clientName, dataPortString);
    int dataSocket = getConnectedSocket(clientName, dataPortString);

    if (dataSocket == CONNECT_ERROR){
        fprintf(stderr, "failed to create second connection for data\n");
    } else {
        printf("connect to client, fd=%d\n", dataSocket);
        struct sockaddr_in peer;
        getpeername(dataSocket, (struct sockaddr *) &peer, (socklen_t *) sizeof peer);
        //printf("%s\n", peer.sin_addr.s_addr);
    }

    msg = "DATA connection says hello";
    send(dataSocket, msg, strlen(msg), 0);


    shutdown(connectedSocket, 2);
    close(connectedSocket);
    shutdown(dataSocket, 2);
    close(dataSocket);
    close(serverSocket);
    free(readBuffer);

}



/*
int getConnectedSocket(const char* remoteHostName, const char* remotePortSt){
    // set up hints struct and other socket initialization referenced from Beej's guide and linux manual examples
    // http://beej.us/guide/bgnet/output/html/multipage/clientserver.html
    // http://man7.org/linux/man-pages/man3/getaddrinfo.3.html

    // set up structs for requesting ip address from hostname
    struct addrinfo hints, *servinfo;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int getadderResult = getaddrinfo(remoteHostName, remotePortSt, &hints, &servinfo);
    if (getadderResult != GET_ADDR_NO_ERROR){
        fprintf(stderr, "getaddrinfo call failed with error: %s\n", gai_strerror(getadderResult));
        return 0;
    }

    // create socket file descriptor
    //int s = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);

    int s = 0;
    for (struct addrinfo *rp = servinfo; rp != NULL; rp = rp->ai_next) {
        // create file descriptor
        // dprintf(3, "attempting to connect to server on using addrinfo struct at:%p\n", rp);
        s = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (s == -1){
            // dprintf(3, "%s\n","failed to get socket file descriptor");
            continue;
        }

        // connect socket to server
        if (connect(s, rp->ai_addr, rp->ai_addrlen) != CONNECT_ERROR){
            // dprintf(3, "%s\n", "socket connected to server");
            break;
        }
        close(s);
    }
    // free up memory used by addr-struct-linked-list
    freeaddrinfo(servinfo);


    // ensure that fd is a valid socket, and not a closed socket ln:57
    struct stat statbuf;
    fstat(s, &statbuf);
    if (!S_ISSOCK(statbuf.st_mode)){
        s = 0;
    }

    return s;
}

int sendBytes(int sock, char* outgoingBuffer){

    // dprintf(3, "sending to server:{%s}\n",outgoingBuffer);

    size_t len = strlen(outgoingBuffer) + 1;
    */
/* +1 for terminating null byte *//*


    if (write(sock, outgoingBuffer, len) != len) {
        // dprintf(3, "partial/failed write\n");
        return 0;
    }

    return 1;
}

void chat(int sock, const char* handle){
    // dprintf(3, "%s\n", "beginning chat program");


    //allocate char* buffer,  readBuffer is not seperate space, it is just a pointer within outgoing buffer
    char* outgoingBuffer = malloc(REC_BUFFER_SIZE + 1);
    char* readBuffer = outgoingBuffer + strlen(handle);
    strncpy(outgoingBuffer, handle, HANDLE_LIMIT);

    char* recieveBuffer = malloc(REC_BUFFER_SIZE + 1);


    while (getChatInput(readBuffer, MSG_LIMIT, handle)) {

        sendBytes(sock, outgoingBuffer);

        // NOTE: the server will have to send a message between each 500ch peice, and client will see msg//500
        // messages from server before getting to input a message again
        // continue to send messages until stdin is consumed
        while(feof(stdin)){
            fgets(readBuffer, MSG_LIMIT, stdin);
            sendBytes(sock, outgoingBuffer);
        }

        //recieve message from server and display it
        size_t nread = (size_t) read(sock, recieveBuffer, REC_BUFFER_SIZE);
        if (nread == -1) {
            perror("read");
            //exit(EXIT_FAILURE);
        }

        printf("%s\n",recieveBuffer);

        // dprintf(3,"Received %ld bytes\n", (long) nread);

    }

    // dprintf(3,"%s\n"," chat loop exited");
    free(outgoingBuffer);
    free((void *) handle);
    free(recieveBuffer);
}

char* getHandle(){
    char* handle = malloc(sizeof(char*) * (HANDLE_LIMIT + 2));
    int bufferSize = HANDLE_LIMIT * 3;
    char* inputBuffer = malloc(sizeof(char*) * HANDLE_LIMIT * 3);
    *handle = 0;

    while (!strlen(handle)){
        //read into inputBuffer
        printf("%s","What would you like your chat handle to be>");
        fgets(inputBuffer, bufferSize, stdin);
        //todo copy nonWhitespace chars to handle

        char* readCursor = inputBuffer;
        char* writeCursor = handle;
        while(*readCursor && readCursor - inputBuffer < HANDLE_LIMIT){
            if (*readCursor != ' ' && *readCursor != '\n'){
                *writeCursor++ = *readCursor;
            }
            readCursor++;
        }
        *writeCursor = 0;
    }

    // place ">" prompt chevron at end of handle
    char* write = handle;
    while (*write){
        ++write;
    }
    *write++='>';
    *write = 0;

    free(inputBuffer);
    return handle;
}


int main(int argc, char const *argv[])
{
    if (argc < EXPECTED_ARGS + 1){
        fprintf(stderr, "%s\n%s\n",
               "Hostname and port must be specified in command args,  example",
               "./chatclient flip2.engr.oregonstate.edu 9999"
        );
        return 1;
    }
    const char* remoteHostName = argv[1];
    const int remotePortNum = atoi(argv[2]);
    const char* remotePortSt = argv[2];

    // assert port range, also errror when non-numeric argument supplied as atoi returns 0 and 0 is privileged
    if (remotePortNum < PRIVILEGED || remotePortNum > MAX_PORT){
        fprintf(stderr, "invalid port number supplied (nummeric between %d-%d required)\n", PRIVILEGED, MAX_PORT);
        return 1;
    }

    printf("Client chat program launched preparing to connect to:\nHost:%s, port:%d\n",remoteHostName, remotePortNum);
    ///// INVARIANT
    ///// client launched with host string and valid port number

    int sock = getConnectedSocket(remoteHostName, remotePortSt);
    if (!sock){
        fprintf(stderr, "%s\n", "(CRITICAL ERROR) failed to connect to server");
        return 1;
    }

    ///// INVARIANT
    ///// client script has connected to server using socket sock

    char* handle = getHandle();
    chat(sock, handle);

    // client has exited with \quit command or server has disconnected, close socket
    close(sock);
    free(handle);

    printf("\n%s\n\n", "Disconnected from server");

    return 0;
}
*/

