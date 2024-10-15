#include <iostream>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <string>

int main(int argc, char* argv[]) {

    if (argc < 2) {
        std::cout << "wgrep: searchterm [file ...]" << std::endl;
        return EXIT_FAILURE;
    }

    bool isStdin = false;

    if (argc == 2) {
        isStdin = true;
    }

    int ret;
    char buffer[4096];
    char currLine[8192];
    int currLinePtr = 0;
    bool containsKey = false;
    std::string keyword = argv[1];
    int keywordPtr = 0;

    for (int i = 1; i < argc; i ++) {
        int fileDescriptor;

        if (i == 1) {
            if (isStdin) {
                fileDescriptor = STDIN_FILENO;
            } else {
                continue;
            }
        } else {
            fileDescriptor = open(argv[i], O_RDONLY);
        }

        if (fileDescriptor == -1) {
            std::cout << "wgrep: cannot open file" << std::endl;
            return EXIT_FAILURE;
        }

        while ((ret = read(fileDescriptor, buffer, sizeof(buffer))) > 0) {
            if (ret == -1) {
                std::cout << "error reading from " << argv[i] << std::endl;
                return EXIT_FAILURE;
            }
            for (int j = 0; j < ret; j ++) {
                currLine[currLinePtr] = buffer[j];
                currLinePtr += 1;
                if (buffer[j] == '\n') {
                    if (containsKey) {
                        int writeRes = write(STDOUT_FILENO, currLine, currLinePtr);
                        if (writeRes == -1) {
                            std::cout << "error writing to stdout" << std::endl;
                            return EXIT_FAILURE;
                        }
                    }
                    currLinePtr = 0;
                    containsKey = false;
                    keywordPtr = 0;
                    continue;
                }
                if (!containsKey && buffer[j] == keyword[keywordPtr]) {
                    keywordPtr += 1;
                    if (keywordPtr == int(keyword.size())) {
                        containsKey = true;
                    }
                } else {
                    keywordPtr = 0;
                }
            }
        }

        int closeRes = close(fileDescriptor);
        if (closeRes == -1) {
            std::cout << "error closing " << argv[i] << std::endl;
            return EXIT_FAILURE;
        }
    }
    
    return EXIT_SUCCESS;
}