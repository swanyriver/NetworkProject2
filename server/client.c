#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <sys/stat.h>



const int REC_BUFFER_SIZE = 512;
const int MSG_LIMIT = 500;
const int HANDLE_LIMIT = 10;
const int PRIVILEGED = 1024;
const int MAX_PORT = 65536;
const int GET_ADDR_NO_ERROR = 0;
const int CONNECT_ERROR = -1;
const int EXPECTED_ARGS = 2;
const char* QUIT = "\\quit";
#define MIN(a,b) (((a)<(b))?(a):(b))


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

int getChatInput(char* buffer, const int max_read, const char* handle){
    printf("%s",handle);
    fgets(buffer, max_read, stdin);
    return strncmp(buffer, QUIT, MIN(strlen(buffer), strlen(QUIT)));
}

int sendBytes(int sock, char* outgoingBuffer){

    // dprintf(3, "sending to server:{%s}\n",outgoingBuffer);

    size_t len = strlen(outgoingBuffer) + 1;
    /* +1 for terminating null byte */

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

