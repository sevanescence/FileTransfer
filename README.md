# FileTransfer

A Windows application to cache and transfer files via OpenSSH SCP written in C++.

# Something to know

This program is not added to your path for ease of access. This is done on purpose
in order for the program to be non-instrusive, as it is primarily an academic
demonstration, and I disapprove of bloat.

# Notice

There is currently no way to deal with different files with the same name. Avoid
file name duplicates. I will probably fix this in a later patch.

Also, directories will not be created for file targets. I may or may not patch
this in a later update.
