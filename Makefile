# -static-libgcc -static-libstdc++

FileTransfer:
	cls
	g++ -DUNICODE -std=c++17 -o build/FileTransfer src/FileTransfer.cpp