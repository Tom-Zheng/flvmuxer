#include "flv.h"

int main(int argc, char **argv)
{
    // Open h264 file with max memory of 3GB
    int fd = open("testHeavy.h264", O_RDONLY);    
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
