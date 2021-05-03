# -static-libgcc -static-libstdc++
# stdlib is gnu++17 instead of c++17 to allow pipe functions for reading stdout and stderr in child processes

FileTransfer:
	cls
	g++ -DUNICODE -std=gnu++17 -o build/FileTransfer src/FileTransfer.cpp -static-libgcc -static-libstdc++