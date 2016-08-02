/*
 * otp_crypt_d.c
 *
 *  Created on: May 28, 2015
 *      Author: Brandon Swanson
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#include <errno.h>
#include <sys/prctl.h>

#include "packet.h"
#include "handshake.h"

void error_exit(char* message);
void communicate(int sock);
int __crypt(char* text, char* key);

int main(int argc, char **argv) {

    ////////////////////////////////////////////////////////////////////////
    ///////////CHECK NUMBER OF ARGS AND VALID PORT NUMBER //////////////////
    ////////////////////////////////////////////////////////////////////////
    if( argc < 2) error_exit("Must provide port number");

    if(*(argv[1])=='0')
        error_exit("Port Zero is not a valid port");

    int port = strtol(argv[1],NULL,10);
    if(!port)
        error_exit("problem converting provided port number to numeric value");

    //fork and socket variables
    const int NONSENSE = -5;
    const int POOL_SIZE = 5;
    int spawnpid;
    int socketFD, commSocket;
    struct sockaddr_in serv_addr, cli_addr;
    socklen_t clength;

    //open and verify socket
    socketFD = socket(AF_INET, SOCK_STREAM, 0);
    if(socketFD<0) error_exit("Socket Error: Opening");

    //set ports to be reused
    int yes=1;
    setsockopt(socketFD, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

    //set up sever addr struct
    port = strtol(argv[1],NULL,10);
    bzero((void *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);

    //register socket as passive, with cast to generic socket address
    if (bind(socketFD, (struct sockaddr *) &serv_addr,
                  sizeof(serv_addr)) < 0)
        error_exit("Socket Error: Opening");

    listen(socketFD,POOL_SIZE);

    int i = 1;
    for(; ;){
        spawnpid = NONSENSE;

        //begin separate processes
        spawnpid = fork();

        //Unlikely to be reached
        if(spawnpid == NONSENSE){
            fprintf(stderr,"%s","FORK ERROR: fork returned nonsense value");
            exit(1);
        }
        if(spawnpid == -1){
            fprintf(stderr,"%s","FORK ERROR: forking process failed");
            exit(1);
        }

        //child process exit and begin communicating
        if ( !spawnpid ) break;

        //spin up 5 processes then begin waiting to fork more if one is killed
        if(i<POOL_SIZE){
            i++;
        }else{
            //if one of their is a child to be reaped (unexpected but possible)
            //the loop will run again and a new process will be spawned
            int status;
            wait(&status);
        }
    }

    //child processes

    //ensure child dies with parent
    //grading script kills all by name but if user killed by pid provided 
    //when launched in background all 6 processes will terminate
    prctl(PR_SET_PDEATHSIG, SIGHUP);

    //become a listening daemon
    while(1){
        //all daemons in pool are blocked on this call, only one will get to accept at a time
        commSocket = accept(socketFD, (struct sockaddr *) &cli_addr,
                &clength);

        if(commSocket < 0){
            fprintf(stderr,"[%d] %s\n",getpid(),"error on accept");
            if(errno){
               const char* error = strerror(errno);
               fprintf(stderr, "(%d)%s\n",errno,error);
            }
        } else {
            //ready to beging socket communication
            communicate(commSocket);
        }

        //connection to client ended
        close(commSocket);
    }

}

void communicate(int sock){
    char buffer[PACKETSIZE];
    int numRead;

    do{
        //read text and key buffer[0-HALFPACKET-1] = text
        //                     buffer[HALFPACKET - PACKETSIZE-1] = key
        numRead = read(sock,buffer,PACKETSIZE);
        if(numRead < 0) {
            fprintf(stderr,"[%d] %s\n",getpid(),"error on socket read");
            if(errno){
               const char* error = strerror(errno);
               fprintf(stderr, "%s\n",error);
            }
        }
        else if(numRead){
            //process text/key send back crypted text
            int numOut = __crypt(buffer,buffer+HALFPACKET);
            write(sock,buffer,numOut);
        }

    }while(numRead);
}

//formula for OTP ENCRYPTION
char modAdd(char text, char key){
    return 'A' + ((text - 'A') + (key - 'A')) % 27;
}

//formula for OTP DECRYPTION
char modSub(char text, char key){
    return 'A' + ((((text - 'A') + 27) - (key - 'A'))%27);
}

//returns either halfpacket or parital if /n encoutered
int __crypt(char* text, char* key){

    //function pointer for encryption/decryption
    char (*modCrypt)(char,char);

    //point to appropriate function using compile flag
#ifdef ENC
    modCrypt = modAdd;
#else
    modCrypt = modSub;
#endif

    int numRead = 0;
    char spaceEncode = 'A' + 26;
    while(numRead < HALFPACKET && *text != '\n'){

        //encode spaces for the ecryption formula
        if(*text == ' ') *text = spaceEncode;
        if(*key == ' ') *key = spaceEncode;

        //outputs msg to stderr when character that is not A-Z || ' '
        //leaves character untouched in returned message
        if(*text<'A' || *text > spaceEncode){
            fprintf(stderr,"[%d] %s\n",getpid(),"BAD CHARACTER IN TEXT");
        } else if(*key<'A' || *key > spaceEncode){
            fprintf(stderr,"[%d] %s\n",getpid(),"BAD CHARACTER IN KEY");
        } else {
            //character is modified acording to OTP protocol
            *text = modCrypt(*text,*key);
        }

        //decode spaces
        if(*text == spaceEncode) *text = ' ';

        //advance character pointers and read count
        ++text;
        ++key;
        ++numRead;
    }

    // \n was not encoded but will be returned 
    // becuase client expects parity in characters read and recieved
    if(*text == '\n') ++numRead;
    return numRead;
}

//print message, and if available perror messege, exit in failure
void error_exit(char* message){
   fprintf(stderr,"%s\n",message);
   if(errno){
       const char* error = strerror(errno);
       fprintf(stderr, "%s\n",error);
   }
   exit(1);
}

