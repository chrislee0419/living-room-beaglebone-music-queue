#include "../stream.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#define NUM_OF_URLS     4
#define BUFFER_SIZE     2000
#define URL_MAX_SIZE    1024

const char *test_filename = "stream_test%d.txt";
const struct timespec wait_time = { .tv_nsec = 1e3 };

int main(int argc, char *argv[])
{
        int i;
        time_t start, end;
        stream_t *st;
        char *buffer;
        unsigned int bytes;
        unsigned int total_bytes;
        enum stream_status status;
        FILE *f;
        char fn[32];
        char url[URL_MAX_SIZE];
        FILE *f_urls;

        if (argc != 2) {
                printf("Provide the name of a file that contains urls to download, separated by new-lines\n");
                fflush(stdout);
                return 1;
        }

        buffer = malloc(sizeof(char) * BUFFER_SIZE);
        if (!buffer) {
                printf("Unable to allocate space for test buffer, exiting\n");
                fflush(stdout);
                return 1;
        }

        f_urls = fopen(argv[1], "r");
        if (!f_urls) {
                printf("Unable to open file from provided argument (%s)\n", argv[1]);
                fflush(stdout);
                return 1;
        }

        i = 0;
        memset(url, 0, URL_MAX_SIZE);
        while (fgets(url, URL_MAX_SIZE, f_urls)) {
                if (!strlen(url))
                        continue;

                sprintf(fn, test_filename, i);
                f = fopen(fn, "w+");
                if (!f) {
                        printf("[%d] Unable to open/create file for test, skipping test\n", i);
                        fflush(stdout);
                        continue;
                }

                // start test
                start = time(NULL);
                printf("[%d] Testing url=%s\n", i, url);
                fflush(stdout);

                st = stream_init(url);
                if (!st) {
                        printf("[%d] Unable to create stream object, skipping test\n", i);
                        fflush(stdout);
                        continue;
                }

                bytes = BUFFER_SIZE;
                total_bytes = 0;
                printf("[%d] Beginning stream\n", i);
                fflush(stdout);

                status = stream_pull_data(st, buffer, &bytes);
                while (status == STREAM_STATUS_OK || status == STREAM_STATUS_OK_NODATA) {
                        if (status == STREAM_STATUS_OK_NODATA)
                                nanosleep(&wait_time, NULL);

                        fwrite(buffer, 1, bytes, f);
                        write(STDOUT_FILENO, buffer, bytes);

                        total_bytes += bytes;
                        bytes = BUFFER_SIZE;
                        status = stream_pull_data(st, buffer, &bytes);
                }

                stream_free(st);

                // end test
                end = time(NULL);

                fclose(f);

                printf("[%d] Stream finished, time taken for test: %.2f\n", i, (double)(end-start));
                printf("[%d] Number of bytes downloaded: %d\n\n", i, total_bytes);
                fflush(stdout);

                ++i;
                memset(url, 0, URL_MAX_SIZE);
        }

        free(buffer);

        return 0;
}
