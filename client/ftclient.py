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
from multiprocessing import process

REC_BUFFER = 512
MSG_LIMIT = 500
MIN_PORT = 0
PRIVILEGED = 1024
MAX_PORT = 65536
EXPECTED_ARGS = 2


def nextPort(portNum):
    portNum += 1
    return portNum if PRIVILEGED < portNum <= MAX_PORT else PRIVILEGED


def getListeningSocket(portNum):
    serverSocket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    # todo port avail and permission check (can be coppied from above pretty easily)
    while True:
        try:
            serverSocket.bind((socket.gethostname(), portNum))
            # binding to port was successful exit loop
            break
        except socket.error as e:
            # for convenience of testing on flip server with congestion ports, advance consecutively
            if e.errno == 98 or e.errno == 13:
                candidate = nextPort(portNum)
                print "Port # %d is %s"%(portNum, ("unavailable" if e.errno == 98 else "privileged")),
                response = raw_input("would you like to try %d? (y/n)"%candidate)
                if response.lower() == "y" or response.lower == "yes":
                    portNum = candidate
                else:
                    print "(SERVER-TERMINATED) due to port unavailability"
                    exit()

    serverSocket.listen(1)
    return serverSocket


def getDirContents(sock):
    """
    :type sock: socket.socket
    :return:
    """
    dataConnection, address = sock.accept()
    print dataConnection.recv(REC_BUFFER)


def getFile(sock):
    """
    :type sock: socket.socket
    :return:
    """
    dataConnection, address = sock.accept()



def client_main(TCP_IP, TCP_PORT):
    #print "HOST:%s PORT:%d"%(TCP_IP, TCP_PORT)

    # create socket and connect to server specified in command line
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect((TCP_IP, TCP_PORT))
    #for debug
    print "peerName:", s.getpeername()
    #s.settimeout(.2)

    print s.recv(REC_BUFFER)

    #todo retieve this from commandline
    dataSocket = getListeningSocket(4444)
    #todo send port num to server
    s.send(socket.gethostbyname(socket.gethostname()) + " " + "4444")

    dataConnection, address = dataSocket.accept()
    print "connected to ", address
    print dataConnection.recv(REC_BUFFER)

    dataSocket.close()
    s.close()
    print "Connection with server closed"


if __name__ == "__main__":

    # todo should be at least 4
    if len(sys.argv) <= EXPECTED_ARGS:
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
