#include "flv.h"

int main(int argc, char **argv)
{
    char* filename;
    if(argc == 2)    {
        filename = argv[1];
    }
    else {
        printf("usage: unitTest <h264 filename>\n");
    }
    // Open h264 file with max memory of 3GB
    int fd = open(filename, O_RDONLY);
    if(fd < 0)    {
        printf("File does not exist. Abort.");
        return;
    }
    uint8_t * buf = malloc(300 *1024 * 1024);
    uint32_t total;
    total = read(fd, buf, (1024*1024 *300));
    close(fd);
    uint8_t *buf_offset = buf;

    printf("File size: %d\n", total);
    printf("FLV muxing started.\n");

    write_flv(buf_offset, total, "test.flv");

    printf("FLV muxing finished.\n");
    
}
