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
#include <dirent.h>


const int REC_BUFFER_SIZE = 512;
const int PRIVILEGED = 1024;
const int MAX_PORT = 65536;
const int GET_ADDR_NO_ERROR = 0;
const int CONNECT_ERROR = -1;
const int EXPECTED_ARGS = 1;
const int POOL_SIZE = 5;
const int ACTION_LIST = 100;
const int ACTION_GET = 200;
const int ERROR_DURING_READ  = -2;
const int FILE_NOT_FOUND = -1;
int FILE_TRANSFER_SUCCESS = 1;
#define MIN(a,b) (((a)<(b))?(a):(b))


/**
 * prints received message, and if available perror message
 * exits program and indicates failure
 * Used to halt program from unrecoverable errors that occur within a function
 *
 */
void error_exit(char* message){
    fprintf(stderr,"%s\n",message);
    if(errno){
        const char* error = strerror(errno);
        fprintf(stderr, "%s\n",error);
    }
    exit(1);
}

/**
 * Returns a socket that is connected to remoteHostName on int(remotePortSt) and ready for rec/send
 * pre-conditions: remoteHostName is reachable IP-address or name
 *                 remotePortSt is numeric string representing valid port number
 *
 */
int getConnectedSocket(const char* remoteHostName, const char* remotePortSt){
    // set up hints struct and other socket initialization referenced from Beej's guide and linux manual examples
    // http://beej.us/guide/bgnet/output/html/multipage/clientserver.html
    // http://man7.org/linux/man-pages/man3/getaddrinfo.3.html

    // set up structs for requesting ip address from hostname
    struct addrinfo hints, *servinfo;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    // Use hints and getaddrinfo to construct addrinfo struct linked-list
    int getadderResult = getaddrinfo(remoteHostName, remotePortSt, &hints, &servinfo);
    if (getadderResult != GET_ADDR_NO_ERROR){
        fprintf(stderr, "getaddrinfo call failed with error: %s\n", gai_strerror(getadderResult));
        return CONNECT_ERROR;
    }

    // Attempt to connect using addrinfo structs returned by getaddrinfo
    int s = 0;
    struct addrinfo *rp;
    for (rp = servinfo; rp != NULL; rp = rp->ai_next) {
        // create file descriptor
        s = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (s == -1){
            continue;
        }

        // connect socket to server
        if (connect(s, rp->ai_addr, rp->ai_addrlen) != CONNECT_ERROR){
            break;
        }
        close(s);
    }
    // free up memory used by addr-struct-linked-list
    freeaddrinfo(servinfo);


    // ensure that fd is a valid socket, and not a closed socket ln:88
    struct stat statbuf;
    fstat(s, &statbuf);
    if (!S_ISSOCK(statbuf.st_mode)){
        s = CONNECT_ERROR;
    }

    return s;
}

/**
 * Returns a socket that is bound and listening on portNum,  ready to accept() incoming connections
 */
int getListeningSocket(int portNum) {
    // Creating a listening socket taken from my own previous classwork
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

/**
 * Reads from control connection the hostname and data port number to be used for the Data-Connection
 * pre-conditions:  conSock is a connected socket (control connection)
 *                  readBuffer is an allocated memory space of at least REC_BUFFER_SIZE
 * post-conditions: *hostname contains the clients hostname or ip address
 *                  *portString contains the string representation of the data connection port number
 *
 * on failure *hostName and/or *portString are left unmodified
 */
void getClientHostNameAndDataPort(int conSock, char** hostName, char** portString, char* readBuffer){
    ssize_t bytesRead = recv(conSock, readBuffer, REC_BUFFER_SIZE, 0);

    if (bytesRead == 0){
        return;
    }

    //add null terminator to received string
    *hostName = readBuffer;
    *(readBuffer + bytesRead) = 0;

    //Divide string into hostname and portnumber by placing null terminator in space and setting portString to next char
    char* cursor;
    for(cursor = readBuffer; cursor - readBuffer < bytesRead - 1; ++cursor){
        if (*cursor == ' '){
            *cursor = 0;
            *portString = cursor+1;
            break;
        }
    }
}

/**
 * read requested action from control socket connection and retrieve filename if appropriate
 * returns 0 on failure or appropriate ACTION_* constant on successful parse of requested action.
 * pre-conditions:  conSock is a connected socket (control connection)
 *                  readBuffer is an allocated memory space of at least REC_BUFFER_SIZE
 *
 */
int getRequestedAction(int controlSocket, char* readBuffer, char** filename){
    ssize_t bytesRead = recv(controlSocket, readBuffer, REC_BUFFER_SIZE, 0);

    if (bytesRead == 0){
        return 0;
    }

    readBuffer[bytesRead] = 0;

    if (readBuffer[0] == '-' && readBuffer[1] == 'l'){
        printf("%s\n","Client requested directory list");
        return ACTION_LIST;
    }

    if (readBuffer[0] == '-' && readBuffer[1] == 'g' && readBuffer[2] == ' '){
        *filename = &readBuffer[3];
        printf("%s {%s}\n","Client requested to get file ", *filename);
        return ACTION_GET;
    }

    fprintf(stderr, "Request from client badly formed   {%s} \n", readBuffer);
    return 0;
}

/**
 * Close connected sockets and free memory used for read-buffer
 * Called at any point during processing client request, will close or free whatever resources have already been used
 * Called when request has failed, or at completion of clients request.
 */
void connectionCleanUp(int dataSocket, int controlSocket, char* readBuffer){
    if (dataSocket){
        shutdown(dataSocket, 2);
        close(dataSocket);
    }

    if (controlSocket){
        shutdown(controlSocket, 2);
        close(controlSocket);
    }

    if (readBuffer){
        free(readBuffer);
    }

    printf("%s\n","Connection with client closed");
}


/**
 * Reads directory contents and sends a space separated list of all regular files to connected client over data socket
 * pre-conditions:  dataSocket is connected to client
 *
 * returns 1 on success 0 on failure
 */
int sendDirList(int dataSocket) {

    char* dirString = malloc(REC_BUFFER_SIZE);
    char* writeCursor = dirString;
    *dirString = 0;
    size_t remainingBytes = REC_BUFFER_SIZE;

    DIR *d;
    struct dirent *dir;
    d = opendir(".");

    if (!d) {
        return 0;
    }

    while (remainingBytes && (dir = readdir(d)) != NULL)
    {
        if (dir->d_type != DT_REG){
            continue;
        }
        size_t nameLength = strlen(dir->d_name);
        strncpy(writeCursor, dir->d_name, MIN(nameLength, remainingBytes));
        *(writeCursor + nameLength) = ' ';
        writeCursor += nameLength + 1;
        remainingBytes -= nameLength + 1;
    }

    *writeCursor = 0;

    closedir(d);

    char* msg = strlen(dirString) ? dirString : "FTP-SERVER: Server Directory is Empty";

    //Send directy listing to client
    send(dataSocket, msg, strlen(msg), 0);

    free(dirString);
    return 1;
}

/**
 * Opens a file (binary or ascii) and transmits the contents to connected client on dataSocket
 * pre-conditions:  dataSocket is connected to client
 *
 * Return Values:
 * FILE_NOT_FOUND - file was not able to opened or file does not exist in server directory
 * ERROR_DURING_READ - fread resulted in ferror() or returned 0 before reaching EOF
 * FILE_TRANSFER_SUCCESS: all of the bytes in the file were sent to the client
 */
int sendFile(int dataSocket, const char* fileName) {

    FILE *filePointer = fopen(fileName, "rb");

    if (!filePointer){
        fprintf(stderr, "File not found  {%s}\n", fileName);
        return FILE_NOT_FOUND;
    }

    printf("%s {%s}\n", "sending file to connected client", fileName);


    char* readBuffer = malloc(REC_BUFFER_SIZE);
    size_t bytesReadFromFile = 0;
    do {
        bytesReadFromFile = fread(readBuffer, sizeof(char), REC_BUFFER_SIZE, filePointer);
        send(dataSocket, readBuffer, bytesReadFromFile, 0);
    } while (bytesReadFromFile);

    int status = (ferror(filePointer) || !feof(filePointer))? ERROR_DURING_READ : FILE_TRANSFER_SUCCESS;
    fclose(filePointer);
    return status;
}


/*
 * Main server process
 *
 * returns 1 on socket set up failure
 * otherwise runs and accepts concurrent incoming connections until killed by supervisor
 */
int main(int argc, char const *argv[]) {

    // CHECK NUMBER OF ARGS AND VALID PORT NUMBER
    if (argc < EXPECTED_ARGS + 1){
        error_exit("Server port number must be specified, example:\n./ftserver 9999");
    }
    const int serverPortNum = atoi(argv[1]);
    const char* serverPortSt = argv[1];

    // assert port range, also error when non-numeric argument supplied as atoi returns 0 and 0 is privileged
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


    // Accept and processes client connections one at a time until terminated
    while (1) {
        int connectedSocket=0;
        int dataSocket=0;

        struct sockaddr clientAddr;
        socklen_t addrSize = sizeof clientAddr;
        connectedSocket = accept(serverSocket, &clientAddr, &addrSize);

        // connected to new client

        char* msg = "FTP-SERVER: You are connected to FTP Server";
        send(connectedSocket, msg, strlen(msg), 0);

        char* clientName = 0;
        char* dataPortString = 0;
        char *readBuffer = malloc(REC_BUFFER_SIZE);
        getClientHostNameAndDataPort(connectedSocket, &clientName, &dataPortString, readBuffer);

        if (!clientName || !dataPortString){
            fprintf(stderr, "%s\n", "error retrieving data port number from client");
            connectionCleanUp(dataSocket, connectedSocket, readBuffer);
            continue;
        }

        dataSocket = getConnectedSocket(clientName, dataPortString);

        if (dataSocket == CONNECT_ERROR || dataSocket == 0){
            fprintf(stderr, "failed to create second connection for data\n");
            connectionCleanUp(dataSocket, connectedSocket, readBuffer);
            continue;
        }

        ///// INVARIANT
        ///// control and data connection established client
        printf("data connection established with: %s %s\n", clientName, dataPortString);

        char* filename = NULL;
        int requestedAction = getRequestedAction(connectedSocket, readBuffer, &filename);

        if (!requestedAction || (requestedAction == ACTION_GET && !filename)){
            char* errorMessage = "FTP-SERVER: Request must take form of:  -g <filename>  ||  -l";
            send(connectedSocket, errorMessage, strlen(errorMessage), 0);
            connectionCleanUp(dataSocket, connectedSocket, readBuffer);
            continue;
        }

        ///// INVARIANT
        ///// client has requested an available action
        if (requestedAction == ACTION_LIST) {
            int success = sendDirList(dataSocket);
            if (!success) {
                msg = "FTP-SERVER: There was a server error when attempting to send directory contents";
                send(connectedSocket, msg, strlen(msg), 0);
            }
        } else if (requestedAction == ACTION_GET) {
            int status = sendFile(dataSocket, filename);
            if (status == FILE_TRANSFER_SUCCESS){
                msg = "FTP-SERVER: file transfer completed successfully";
            } else if (status == FILE_NOT_FOUND) {
                msg = "FTP-SERVER: requested file not found on server";
            } else if (status == ERROR_DURING_READ){
                msg = "FTP-SERVER: error occurred while reading requested file";
            } else {
                msg = "FTP-SERVER: There was a server error when attempting to send the file";
            }
            send(connectedSocket, msg, strlen(msg), 0);
        }

        connectionCleanUp(dataSocket, connectedSocket, readBuffer);
    }
}
