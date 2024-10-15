#include <iostream>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <string.h>

int main(int argc, char* argv[]) {
    if (argc == 1) {
        std::cout << "wzip: file1 [file2 ...]" << std::endl;
        return EXIT_FAILURE;
    }

    char currChar = -1;
    int currCount = 0;
    int ret;
    char buffer[4096];
    char writeBuf[4095];
    int writeBufIdx = 0;

    for (int i = 1; i < argc; i ++) {
        int fileDescriptor = open(argv[i], O_RDONLY);
        if (fileDescriptor == -1) {
            std::cout << "wzip: cannot open file" << std::endl;
            return EXIT_FAILURE;
        }
        
        while ((ret = read(fileDescriptor, buffer, sizeof(buffer))) > 0) {
            if (ret == -1) {
                std::cout << "error reading from argv[i]" << std::endl;
                return EXIT_FAILURE;
            }
            for (int j = 0; j < ret; j ++) {
                if (buffer[j] == currChar) {
                    currCount ++;
                } else {
                    if (currChar != -1) {
                        uint32_t count = currCount;
                        memcpy(writeBuf + writeBufIdx, &count, 4);
                        writeBuf[writeBufIdx + 4] = currChar;
                        writeBufIdx += 5;
                        if (writeBufIdx == sizeof(writeBuf)) {
                            int writeRes = write(STDOUT_FILENO, writeBuf, sizeof(writeBuf));
                            writeBufIdx = 0;
                            if (writeRes == -1) {
                                std::cout << "error writing to stdout" << std::endl;
                                return EXIT_FAILURE;
                            }
                        }
                    } 
                    currChar = buffer[j];
                    currCount = 1;
                }
            }
        }
        int closeRes = close(fileDescriptor);
        if (closeRes == -1) {
            std::cout << "error closing " << argv[i] << std::endl;
        }
    }
    uint32_t count = currCount;
    memcpy(writeBuf + writeBufIdx, &count, 4);
    writeBuf[writeBufIdx + 4] = currChar;
    writeBufIdx += 5;
    int writeRes = write(STDOUT_FILENO, writeBuf, writeBufIdx);
    if (writeRes == -1) {
        std::cout << "error writing to stdout" << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}