Brandon Swanson 
CS 372  Project-2
08/07/2016

------------------------------------
Building and running the FTP-Server
------------------------------------
Build with supplied makefile:

$ make ftserver
gcc server.c -o ftserver -Wall -std=gnu99


Run with valid port number as only argument
./ftserver <SERVER_PORT>


------------------------------------
Running FTP client
------------------------------------
Run using the python interpreter and supplying host connection info and command as args

-l to retrieve directory contents of FTP server
-g <FILENAME> to retrieve file from FTP server

python ftclient.py <SERVER_HOST> <SERVER_PORT> (-l || -g <FILENAME>) <DATA_PORT>


-----------------------------------
Extra Credit
-----------------------------------
I implemented an FTP server and client that can transfer binary files or ascii files indiscriminately
I tested it by transferring not only .txt files but also .mp4 movies or .png images
Comparing the transfered files with diff proved they transfered successful and I was also able to
view the images and watch the movies I transfered with the FTP server and client

In order to accomplish this I used fread() to read binary data as a byte array from files and 
avoided any methods that relied on traditional null terminated c-strings.

server.c Line:293


-----------------------------------
Inter server connections
-----------------------------------
I have successfully tested this server and client connection with the server 
running on flip1 and the client running on flip2 and vice-versa


-----------------------------------
Data Socket Connection
-----------------------------------
because the client specifies the data port number I chose to have the client be the one 
that listens on that port because it would be unusual for the client to specify the port a 
server should use.
The client transmits its address and port on the control connection and then awaits a connection
the server connects to this open port on the client and then receives a command on the control connection.


-------------------------------------------
Grading Criteria Verification code sections
--------------------------------------------
ftserve starts on host A, validates the parameter, and waits on <SERVER_PORT> for client request
server.c ln 311 - 322 validate params
server.c getListeningSocket()


ftclient starts on host B, and validates parameters
ftclient.py ln 185 - 219 


ftserve and ftclient establish a control connection
server.c ln 350
ftclient.py ln 142


ftclient sends a command
ftclient.py ln 158 | ln 161


ftserve accepts and interprets command
server.c getRequestedAction()


ftserve and ftclient send/receive control messages on the control connection
server.c ln 355 385 396 409
ftclient.py ln 147 165-169


ftserve and ftclient establish a data connection
server.c getClientHostNameAndDataPort(), getConnectedSocket(), ln 360-378
ftclient.py ln 150, 155


When appropriate, ftserve sends its directory to ftclient on the data connection, 
and ftclient displays the directory on-screen
server.c ln 392, sendDirList()
ftclient.py getDirContents()


When appropriate, ftserve validates filename , and either sends the contents 
of filename on the data connection or sends an appropriate error message to ftclient
on the control connection 
server.c sendFile()
ftclient.py getFile()


ftclient saves the file, and handles "duplicate file name" error if necessary
ftclient.py getFile(), resolveNameConflict(), ln 101


ftclient displays "transfer complete" message on-screen
sent by server on success, displayed by client


ftserve closes the data connection after directory or file transfer
server.c ln 421, connectionCleanUp()


Control connection closed by ftclient
ftclient.py ln 172


ftserve repeats until terminated by a supervisor
server.c ln 344
while(1){ accept(), process, close()}



-----------------------------------
Example Execution
-----------------------------------

----------------------
---server on flip 2---
----------------------

$ ./ftserver 9999
Awaiting incoming client connections,  connect via client program:
python ftclient.py flip2.engr.oregonstate.edu 9999 <COMMAND> [<FILENAME>] <DATA_PORT>

data connection established with: 128.193.54.168 7777
Client requested directory list
Connection with client closed
data connection established with: 128.193.54.168 7777
Client requested to get file  {notThere.txt}
File not found  {notThere.txt}
Connection with client closed
data connection established with: 128.193.54.168 7777
Client requested to get file  {test.txt}
sending file to connected client {test.txt}
Connection with client closed


----------------------
--client on flip 1----
----------------------
$ python ftclient.py flip2.engr.oregonstate.edu 9999 -l 7777
FTP-SERVER: You are connected to FTP Server

FTP Directory contents:
-----------------------
mobyDick.txt
test.txt
osufavicon.ico

Connection with server closed

$ python ftclient.py flip2.engr.oregonstate.edu 9999 -g notThere.txt 7777
FTP-SERVER: You are connected to FTP Server
FTP-SERVER: requested file not found on server
Connection with server closed

$ python ftclient.py flip2.engr.oregonstate.edu 9999 -g test.txt 7777
FTP-SERVER: You are connected to FTP Server
FTP-SERVER: file transfer completed successfully
Connection with server closed
