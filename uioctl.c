/*
 * uioctl.c: Userspace I/O manipulation utility
 *
 * Copyright (C) 2013, Bryan Newbold <bnewbold@leaflabs.com>
 *
 * GPLv3 License
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Changelog:
 *  2013-12-04: bnewbold: Initial version; missing width management, list mode
 *  2013-12-19: bnewbold: GPLv3; fix mmap size
 *
 */

#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/timeb.h>

#define DEFAULT_WIDTH 4
#define PROGRAM_NAME "uioctl"

enum mode_type {
    MODE_READ,
    MODE_WRITE,
    MODE_LIST,
    MODE_MONITOR
};

static void usage(int exit_status) {
    fprintf(exit_status == EXIT_SUCCESS ? stdout : stderr,
        "Usage: %s [options] [-l] [/dev/uioX [-m] [<addr> [<value>]]]\n"
        "\n"
        "Functions:\n"
        "  monitor (-m) the device for interrupts\n"
        "  list (-l) all devices and their mappings\n"
        "  read words from <addr>\n"
        "  write <value> to <addr> (will zero-pad word width)\n"
        "\n"
        "Options:\n"
        "  -r\tselect the device's memory region to map (default: 0)\n"
        "  -w\tword size (in bytes; default: %d)\n"
        "  -n\tnumber of words to read (in words; default: 1)\n"
        "  -x\texit with success after the first interrupt (implies -m mode)\n"
        ,
        PROGRAM_NAME, DEFAULT_WIDTH);
    exit(exit_status);
}

static void list_devices() {
    // TODO:
    // - try to list /sys/devices/uio*
    // - for each in the above, print memory regions
    fprintf(stderr, "listing not yet implemented\n");
    exit(EXIT_FAILURE);
}

static void monitor(char *fpath, int forever) {
    char buf[4];
    int bytes;
    int fd;
    struct timeb tp;
    char startval[] = {0,0,0,1};
   
    printf("Waiting for interrupts on %s\n", fpath);
    fd = open(fpath, O_RDWR|O_SYNC);
    if (fd < 1) {
        perror("uioctl");
        fprintf(stderr, "Couldn't open UIO device file: %s\n", fpath);
        exit(EXIT_FAILURE);
    }
    do {
        bytes = pwrite(fd, &startval, 4, 0);
        if (bytes != 4) {
            perror("uioctl");
            fprintf(stderr, "Problem clearing device file\n");
            exit(EXIT_FAILURE);
        }
        //lseek(fd, 0, SEEK_SET);
        bytes = pread(fd, buf, 4, 0);
        ftime(&tp);
        if (bytes != 4) {
            perror("uioctl");
            fprintf(stderr, "Problem reading from device file\n");
            exit(EXIT_FAILURE);
        }
        printf("[%ld.%03d] interrupt: %d\n", tp.time, tp.millitm,
            (buf[3] * 16777216 + buf[2] * 65536 + buf[1] * 256 + buf[0]));
    } while (forever);
    close(fd);
    exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[]) {
    int c;
    int fd;
    enum mode_type mode = MODE_READ;
    char *fpath;
    void *ptr;
    int map_size, value, addr;
    int region = 0;
    int count = 1;
    int width = DEFAULT_WIDTH;
    int forever = 1;

    while((c = getopt(argc, argv, "hlmxr:n:w:")) != -1) {
        errno = 0;
        switch(c) {
        case 'h':
            usage(EXIT_SUCCESS);
        case 'l':
            list_devices();
        case 'm':
            mode = MODE_MONITOR;
            break;
        case 'x':
            mode = MODE_MONITOR;
            forever = 0;
            break;
        case 'r':
            region = strtoul(optarg, NULL, 0);
            if (errno) {
                perror("uioctl");
                exit(EXIT_FAILURE);
            }
            if (region != 0) {
                fprintf(stderr, "region != 0 not yet implemented\n");
                exit(EXIT_FAILURE);
            }
            break;
        case 'n':
            count = strtoul(optarg, NULL, 0);
            if (errno) {
                perror("uioctl");
                exit(EXIT_FAILURE);
            }
            break;
        case 'w':
            width = strtoul(optarg, NULL, 0);
            if (errno) {
                perror("uioctl");
                exit(EXIT_FAILURE);
            }
            if (width != 4) {
                fprintf(stderr, "width != 4 not yet implemented\n");
                exit(EXIT_FAILURE);
            }
            break;
        default:
            fprintf(stderr, "Unexpected argument; try -h\n");
            exit(EXIT_FAILURE);
        }
    }

    if (mode == MODE_MONITOR) {
        if ((argc - optind) != 1) {
            fprintf(stderr, "Wrong number of arguments; try -h\n");
            exit(EXIT_FAILURE);
        }
        fpath = argv[optind];
        monitor(fpath, forever);
    }
    if (mode == MODE_READ) {
        if ((argc - optind) == 2) {
            fpath = argv[optind++];
            errno = 0;
            addr = strtoul(argv[optind++], NULL, 0);
            if (errno) {
                perror("uioctl");
                exit(EXIT_FAILURE);
            }
            //printf("addr: 0x%08x\n", addr);
        } else
        if ((argc - optind) == 3) {
            mode = MODE_WRITE;
            fpath = argv[optind++];
            errno = 0;
            addr = strtoul(argv[optind++], NULL, 0);
            value = strtoul(argv[optind++], NULL, 0);
            if (errno) {
                perror("uioctl");
                exit(EXIT_FAILURE);
            }
            //printf("addr: 0x%08x\n", addr);
            //printf("value: 0x%08x\n", value);
        } else {
            fprintf(stderr, "Wrong number of arguments; try -h\n");
            exit(EXIT_FAILURE);
        }
    }

    map_size = count * width;

    fd = open(fpath, O_RDWR|O_SYNC);
    if (fd < 1) {
        perror("uioctl");
        fprintf(stderr, "Couldn't open UIO device file: %s\n", fpath);
        return EXIT_FAILURE;
    }
    /* NOTE: with UIO the offset is not a normal offset; it's a region
     * selector.
     */
    ptr = mmap(NULL, map_size + addr, PROT_READ|PROT_WRITE,
               MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED) {
        perror("mmap");
        fprintf(stderr, "Couldn't mmap.\n");
        return EXIT_FAILURE;
    }

    if (mode == MODE_READ) {
        for (; count > 0; count--) {
            value = *((unsigned *) (ptr + addr));
            printf("0x%08x\t%08x\n", addr, value);
            addr += width;
        }
    } else {
        *((unsigned *)(ptr + addr)) = value;
    }

    /* clean up */
    munmap(ptr, map_size);
    close(fd);

    return EXIT_SUCCESS;
}
