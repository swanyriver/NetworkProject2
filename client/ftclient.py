# /*********************************************************************
#  ** Program Filename: ftclient
#  ** Author: Brandon Swanson
#  ** Date: 07/31/2016
#  ** Description: A file transfer client for connecting to file transfer server contained in same assignment
#  ** Input: Receives on the command line <SERVER_HOST>, <SERVER_PORT>, <COMMAND>, [<FILENAME>], <DATA_PORT>
#  ** Output: Will print status messages from server recieved on control port connection,
#             files requested with -g option will be added in directory that this script was launched
#  *********************************************************************/

import sys
import socket

REC_BUFFER = 512
MSG_LIMIT = 500
MIN_PORT = 0
PRIVILEGED = 1024
MAX_PORT = 65536
EXPECTED_ARGS = 2


def client_main(TCP_IP, TCP_PORT):
    #print "HOST:%s PORT:%d"%(TCP_IP, TCP_PORT)

    # create socket and connect to server specified in command line
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect((TCP_IP, TCP_PORT))
    #for debug
    print "peerName:", s.getpeername()
    #s.settimeout(.2)

    print s.recv(REC_BUFFER)

    s.close()
    print "Connection with server closed"


if __name__ == "__main__":

    if len(sys.argv) < EXPECTED_ARGS:
        print "must supply host and port number"
        sys.exit(1)

    port = None
    try:
        port = int(sys.argv[2])
        if not PRIVILEGED < port < MAX_PORT: raise ValueError("Out of range")
    except ValueError:
        print "Invalid port supplied"
        sys.exit(1)

    client_main(sys.argv[1], port)
