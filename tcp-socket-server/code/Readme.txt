What to submit: - Your Readme file must be well organized. It should contain a User
Documentation, explaining how to compile and run your program. Though a Testing
Documentation is not required, you should give several examples on how to run your program.
Readme should also contain a brief System Documentation, stating the major parts that you have
completed, and where these parts are in your source, and how they are implemented. Clearly label
each of these documentation sections in your write-up. Your source code should have brief Program
Documentation in it. This is also called comments. Your source code should have a decent amount
of comments with good style. Your comments should not repeat what the line of code says, rather
should add to it to ease code understanding.

User Documentation

How to compile:

make

How to run:

first, open server by:
./server

second, open client Parent by:
./client localhost

third: open another client Parent by:
./client 127.0.0.1



Testing Documentation

One:	How to test: 
gethostbyname & gethostbyaddr functions

first open server by:  
./server
then open client parent 1 by :
./client localhost  

then open client parent 2 by :
./client 127.0.0.1



Then you will see the following info in client 1:
{
	address type: hostname
	the server host is: localhost
	ip address: 127.0.0.1
}

Then you will see the following info in client 2:
{
	address type: decimal address
	the server host is: localhost
	ip address: 127.0.0.1
}





Two:	How to test: 
The exec family of functions (Section 4.7 of Chapter 4)

In client 1 and 2, you can see the main menu:
{
		if you want to echo, please enter <echo> 
		if you want to report time, please enter <time> 
		if you want to quit, please enter <quit> 
}

Please enter  echo in client 1.
Then a xterm window of echo child will pop out.
Please enter  time in client 2.
Then a xterm window of time child will pop out.




Three:	How to test:
Using pipes for interprocess communication (IPC) in Unix

In echo child window, you can type something to echo.

For example: you type: hello world.   Then hello world is sent to the server (you can check the server), 
and then the message goes back to echo child. Finally, the child send this message to parent through pipe.

 
If you want to terminate, enter ctrl+D or ctrl+C.







Four:	How to test:
Error scenarios induced by process terminations & host crashes.

When you terminate the echo child(ctrl+d or ctrl+c) or time child(ctrl+c), 
you will see the termination message in client parent and the server.

When the server host crashes or terminates (ctrl+c), 
the echo child and time child window will terminate immediately 
and the client parent will read message from children that the server host crashes.




Notice: Please open the server window, parent client window, 
time child window and the echo child window simultaneusly 
to better test and observe this program.








System Documentation:
I have finished all the requiremtns.


The exec family of functions:
where:	line 95-101, line 152-154 in tcpechotimecli.c
Implementation:	I pass the string format of the writing descriptor of pipe to the exec function 
so the echo and time child can use it to send info to parent client


Command line arguments processsing:gethostbyaddr,gethostbyname
line 34-55 in tcpechotimecli.c


The client parent: fork,exec
where:	line 86-195 in tcpechotimecli.c
Implementation:	 the parent process first calls fork to make a copy of
itself, and then child process copies calls exec to replace itself with the echo or time service. 




The client child
where: line 98,152 in tcpechotimecli.c, and code in echo_cli.c, time_cli.c
Implementation:	 pass the descriptor of pipe , ip address into the execlp function.
execlp("xterm", "xterm", "-e", "./echo_cli",decimalAddressC, pfdStr, (char *) 0))
Then the echo or time child can use this desciptor to send info to parent client.


Service request termination and program robustness
To terminate the echo client, the user can type CTRL D and CTRL C. To terminate the time client, the only option is for the user to
type in ^C (CTRL C). To terminate the server, type CTRL C.

where: line 253-260 in tcpechotimesrv.c  (if echo and time client terminates, the read socket in time server will return zero) 
	   line 94 - 101 in echo_cli.c   (if echo client enter ctrl+D, echo client will send FIN to time server and  tell parent that echo client has terminated.)
	   line 75-82 in in echo_cli.c (if server crash, the read socket in echo client return 0. Then we send info to parent)
	   line 65-68 in time_cli.c  (if server crash, the read socket in time client return 0. Then we send info to parent)
	   line 128-130,188-190 in tcpechotimecli.c (when the chilren terminates, the read function for pipe in parent will return 0, )
	   






IPC using a pipe: 
Where:	line 83-99,113 in tcpechotimecli.c , line 77-103 in echo_cli.c		line 59-73 in time_cli.c
Implementation： here we pass the sending descriptor to the child  
so that  the parent can use the reading descriptor to read the message.


More robustness:
Where:  line 7-14,66 in tcpechotimecli.c  (catch SIGCHLD)



Server:

Multi-threading and server services 
Listening on multiple services:
Where:	line 104 -263 in tcpechotimesrv.c
Implementation: I use select to monitor the echo and time accept descriptors.
The server accepts the connection and creates a thread which provides the service. 
The thread detaches itself. Meanwhile, the main thread goes back to the select to await further clients.



Thread safety:
I use the readline from directory ~cse533/Stevens
/unpv13e_solaris2.10/threads

Robustness:
Where:  line 7-14,66 in tcpechotimecli.c  (catch SIGCHLD)

Time server implementation:
where: line 207-264 in tcpechotimesrv.c
here i use the "Readable_timeo" function from Figure 14.3. Every 5 seconds, it will send the time info to
time child process.And at the same time, it is mornitoring the socket. 
If time child terminates, the read socket in time server will return 0.  


Proper status messages at the server:
where: line 194, 257 in tcpechotimesrv.c
Please run the program to check the status messages.


SO_REUSEADDR socket option:
Where: line 54-60 in tcpechotimesrv.c


Nonblocking accept :
set non-blocking
line 64-78   in tcpechotimesrv.c

set children socket back to blocking:
line 120-126, line 147-153 in tcpechotimesrv.c
