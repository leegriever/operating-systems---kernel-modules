#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include "message_slot.h"

int main(int argc, char* argv[]){
    if (argc != 3){ /* two command line arguments (plus the program name) */
        fprintf(stderr, "%s\n", "Invalid number of arguments!");
        exit(1);
    }

    int fd = open(argv[1], O_RDWR | O_NONBLOCK, 0777);
    if (fd == -1){ /* open failed */
        perror("File opening failed");
        exit(1);
    }

    char* ptr;
    int id = strtol(argv[2], &ptr, 10);
    if (id < 0){ /* invalid channel id */
        fprintf(stderr, "%s\n", "Invalid channel id!");
        exit(1);
    }

    if (ioctl(fd, MSG_SLOT_CHANNEL, id) == -1){ /* ioctl failed */
        perror("Setting channel id failed");
        exit(1);
    }

    char* buffer = malloc(128 * sizeof(char)); /* allocating a buffer of size 128 */
    memset(buffer, 0, 128);

    int length = read(fd, buffer, 128);
    if (length == -1){ /* read failed */
        perror("Reading message failed");
        exit(1);
    }

    if (close(fd) == -1){
        perror("File closing failed");
        exit(1);
    }

    if (write(STDOUT_FILENO, buffer, length) == -1){ /* writing to stdout failed */
        perror("Failed to print message");
        exit(1);
    }

    free(buffer);

    exit(0);
}
