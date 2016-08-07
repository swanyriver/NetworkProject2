# /*********************************************************************
#  ** Program Filename: ftclient
#  ** Author: Brandon Swanson
#  ** Date: 07/31/2016
#  ** Description: A file transfer client for connecting to file transfer server contained in same assignment
#  ** Input: Receives on the command line <SERVER_HOST>, <SERVER_PORT>, <COMMAND>, [<FILENAME>], <DATA_PORT>
#  ** Output: Will print status messages from server recieved on control port connection,
#             files requested with -g option will be added in directory that this script was launched
#  *********************************************************************/
import os
import sys
import socket

REC_BUFFER = 512
MSG_LIMIT = 500
MIN_PORT = 0
PRIVILEGED = 1024
MAX_PORT = 65536
EXPECTED_ARGS = 4

ACTION_GET = "-g"
ACTION_LIST = "-l"


def nextPort(portNum):
    portNum += 1
    return portNum if PRIVILEGED < portNum <= MAX_PORT else PRIVILEGED


def getListeningSocket(portNum):
    serverSocket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    while True:
        try:
            serverSocket.bind((socket.gethostname(), portNum))
            # binding to port was successful exit loop
            break
        except socket.error as e:
            # for convenience of testing on flip server with congested ports, advance consecutively
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
    return serverSocket, portNum


def getDirContents(dataConnection):
    """
    :type sock: socket.socket
    :return:
    """
    print "\nFTP Directory contents:"
    print "-----------------------"
    print "\n".join(dataConnection.recv(REC_BUFFER).split())
    print ""


def resolveNameConflict(fileName, filesInDir):
    index = 0
    parts = fileName.split(".")
    prefix, suffix = (".".join(parts[:-1]), "." + parts[-1]) if len(parts) > 1 else (fileName, "")

    while fileName in filesInDir:
        index += 1
        fileName = "%s-%d%s" % (prefix, index, suffix)

    return fileName


def getFile(dataConnection, fileName):
    """
    :type sock: socket.socket
    :return:
    """

    #check if file already exists,  open filename(#).ext instead
    filesInDir = os.listdir(".")
    if fileName in filesInDir:
        fileName = resolveNameConflict(fileName, filesInDir)
        print "Filename already taken saving file as: %s"%fileName

    # read bytes from socket until disconnect
    # properly saves ascii or binary data
    outputFile = None
    try:
        bytes = dataConnection.recv(REC_BUFFER)
        # wait to create file until at least first group of bytes received
        # avoid creating empty files on File-not-found error
        if bytes:
            outputFile = open(fileName, "w")

        while bytes:
            outputFile.write(bytes)
            bytes = dataConnection.recv(REC_BUFFER)

    except (socket.error, IOError) as e:
        print "Error reading from socket or writing to file"
        print e

    finally:
        if outputFile:
            outputFile.close()


def client_main(serverName, serverPort, dataPort, action, filename):

    # create socket and connect to server specified in command line
    try:
        controlSocket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        controlSocket.connect((serverName, serverPort))
    except socket.error as e:
        print "Unable to create socket and connect to server: ", e
        return

    print controlSocket.recv(REC_BUFFER)

    # Create a listening socket on supplied data port number
    dataSocket, dataPort = getListeningSocket(dataPort)

    # Inform server of clients address and data port number
    controlSocket.send(socket.gethostbyname(socket.gethostname()) + " " + str(dataPort))

    dataConnection, address = dataSocket.accept()

    if action == ACTION_GET:
        controlSocket.send(action + " " + filename)
        getFile(dataConnection, filename)
    elif action == ACTION_LIST:
        controlSocket.send(action)
        getDirContents(dataConnection)

    # receive and display status message from server
    while True:
        msg = controlSocket.recv(REC_BUFFER)
        if not msg:
            break
        print msg

    # disconnect from server
    dataSocket.close()
    dataConnection.close()
    controlSocket.close()
    print "Connection with server closed"


if __name__ == "__main__":

    if len(sys.argv) < EXPECTED_ARGS + 1:
        print "must supply server name/ip port, requested action, data port number"
        print "examples:"
        print "python ftclient.py flip1 9999 -l 8888"
        print "python ftclient.py flip1 9999 -g smallFile.txt 8888"

        sys.exit(1)

    serverPort = None
    dataPort = None
    try:
        serverPort = int(sys.argv[2])
        if not PRIVILEGED < serverPort < MAX_PORT: raise ValueError("Out of range")
        dataPort = int(sys.argv[-1])
        if not PRIVILEGED < dataPort < MAX_PORT: raise ValueError("Out of range")
    except ValueError:
        print "Invalid port numbers supplied"
        sys.exit(1)


    # ensure that actions match -l or -g
    if (sys.argv[3] != ACTION_LIST and sys.argv[3] != ACTION_GET)\
            or sys.argv[3] == ACTION_GET and len(sys.argv) < EXPECTED_ARGS + 2:
        print "Action must be list (-l) or get (-g <filename>)"
        sys.exit(1)



    client_main(
        serverName=sys.argv[1],
        serverPort=serverPort,
        dataPort=dataPort,
        action=sys.argv[3],
        filename=sys.argv[4]
    )