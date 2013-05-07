
MazZe Game

=== How it works: ===

The user must enter a "key" which will be used to derive the secret password that he must find out in order to win.
A "maze" directory will be created at the path specified by "mazedir"/maze.
It will look like: 

    0      1
   0 1    0 1
 .............
 0   1
0 1 0 1 ..

The maze have a depth of 8, which means that each path will have 8 characters of "0" and "1"s, the size of a char.

For every second character from the key provided by the user, the binary representation of that character will be generated in the form of a path, eg.:
1/0/0/1/1/1/0/1
There will be 7 paths. (the key must be 13 chars long)
At each path in the mazedir, a "piece" will be copied and started in a randomly generated order. The "piece" is in fact a client that will connect back to the maze server and it will send a random character (magic char). The server will put the pieces together it the order that it knows. Each piece will also dump a file in its directory which will contain the random position given by the server.

In order to win, the user must first associate the position of each piece with its "magic char" (by looking at the packets), and then associate each char in the key to its position (by searching it in the maze)


=== TODO's: ===
 * ls and cat commands in order to read the dumped pieces in the mazedir
 * tcpdump to a 744 file (readable by the user)
 * The answer will also be put in the home directory of the username used in the "login" command. (must be a user that does not exist)

 


