#include "../stream.h"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#define NUM_OF_URLS     4
#define BUFFER_SIZE     2000

// NOTE: these urls will probably have to be updated daily
const char *urls[NUM_OF_URLS] = {
        "https://r2---sn-ni5f-t8gs.googlevideo.com/videoplayback?lmt=1524503096692151&ei=OjHnW7qBCoTYkgasvpeoDQ&c=WEB&expire=1541899674&ipbits=0&mm=31%2C26&mn=sn-ni5f-t8gs%2Csn-vgqsrned&itag=251&id=o-APss5pz_cjbBsdQdFV7UL0UzpVN4wDRbVJ8vjfsrXfie&initcwndbps=1478750&pl=22&fvip=2&dur=18.961&mime=audio%2Fwebm&signature=4E091D554AF9F9E09195396275EDBD91E4F8913F.C69602F1728DC30CD6816BA58F94146E421A415F&ms=au%2Conr&source=youtube&mv=m&keepalive=yes&ip=50.64.64.90&requiressl=yes&gir=yes&mt=1541878003&sparams=clen%2Cdur%2Cei%2Cgir%2Cid%2Cinitcwndbps%2Cip%2Cipbits%2Citag%2Ckeepalive%2Clmt%2Cmime%2Cmm%2Cmn%2Cms%2Cmv%2Cpl%2Crequiressl%2Csource%2Cexpire&clen=230633&key=yt6&ratebypass=yes",                            // Me at the zoo (audio, 0:19)
        "https://r1---sn-ni5f-t8gz.googlevideo.com/videoplayback?requiressl=yes&sparams=clen%2Cdur%2Cei%2Cgir%2Cid%2Cinitcwndbps%2Cip%2Cipbits%2Citag%2Ckeepalive%2Clmt%2Cmime%2Cmm%2Cmn%2Cms%2Cmv%2Cpl%2Crequiressl%2Csource%2Cexpire&ei=9TDnW8X5EsyOkwaVtoKgAg&fvip=1&lmt=1540922901036524&mime=audio%2Fwebm&expire=1541899605&c=WEB&keepalive=yes&source=youtube&dur=241.012&itag=171&pl=22&mv=m&initcwndbps=1518750&ipbits=0&clen=3964712&id=o-AIVd1xb4Qo9rmaZl1J_k00HHyJ5HnpIQimCkoiixYkH6&mm=31%2C26&mn=sn-ni5f-t8gz%2Csn-vgqskn7e&txp=5411222&ms=au%2Conr&mt=1541877880&gir=yes&ip=50.64.64.90&key=yt6&signature=9776161208197222FB5CD47597CBE9CCCE47AE5C.0672D615E8E17F1206BECA1FA40E2DE5846B8024&ratebypass=yes",              // Sam Gellaitry - Want U 2 (audio, 4:02)
        "https://r4---sn-ni5f-t8gs.googlevideo.com/videoplayback?dur=492.421&lmt=1499080074022612&ip=50.64.64.90&key=yt6&id=o-AKKCMC-dWT-9vasmMtSwgx_oKqqRaVxcLtn2OhaKFUBw&itag=251&fvip=4&sparams=clen%2Cdur%2Cei%2Cgir%2Cid%2Cinitcwndbps%2Cip%2Cipbits%2Citag%2Ckeepalive%2Clmt%2Cmime%2Cmm%2Cmn%2Cms%2Cmv%2Cpl%2Crequiressl%2Csource%2Cexpire&mime=audio%2Fwebm&pl=22&source=youtube&gir=yes&ipbits=0&initcwndbps=1503750&c=WEB&keepalive=yes&clen=8934720&mn=sn-ni5f-t8gs%2Csn-vgqsrnek&mm=31%2C26&requiressl=yes&expire=1541899843&ms=au%2Conr&ei=4zHnW_-zLIbFkgbR0J7ABg&mv=m&mt=1541878123&signature=9D226BE2914D8BD846C95E7AD24A5233E23A8A25.7F56B27F2F459CBCBF141CAD93E7B2FFA4F5DBFB&ratebypass=yes",                          // State Azure - Slipstream (audio, 8:13)
        "https://r1---sn-ni5f-t8gs.googlevideo.com/videoplayback?expire=1541900058&sparams=clen%2Cdur%2Cei%2Cgir%2Cid%2Cinitcwndbps%2Cip%2Cipbits%2Citag%2Ckeepalive%2Clmt%2Cmime%2Cmm%2Cmn%2Cms%2Cmv%2Cpl%2Crequiressl%2Csource%2Cexpire&key=yt6&mime=audio%2Fwebm&ip=50.64.64.90&dur=661.561&keepalive=yes&lmt=1499081688151426&ms=au%2Conr&mv=m&mt=1541878368&source=youtube&c=WEB&initcwndbps=1530000&id=o-AJEM1U498Vac7pka4n5ZH6-3tBYOpBuruGKdc1SGvnwr&mn=sn-ni5f-t8gs%2Csn-vgqsenek&gir=yes&clen=11688747&ipbits=0&ei=ujLnW4fXIJSOkwaa8omgBw&fvip=5&pl=22&itag=251&requiressl=yes&mm=31%2C26&signature=C476BA34438871BF99189DEDE7E519C0DC2F845A.9592C5582B38EA5ADFCFD6493DAC97D69B12EBF1&ratebypass=yes"                          // State Azure - Moth (audio, 11:02)
};
const char *test_filename = "stream_test%d.txt";

int main(int argc, char *argv[])
{
        time_t start, end;
        stream_t *st;
        char *buffer;
        unsigned int bytes;
        unsigned int total_bytes;
        enum stream_status status;
        FILE *f;
        char fn[32];

        buffer = malloc(sizeof(char) * BUFFER_SIZE);
        if (!buffer) {
                printf("Unable to allocate space for test buffer, exiting\n");
                fflush(stdout);
                return 1;
        }

        for (int i = 0; i < NUM_OF_URLS; ++i) {
                // open file for stream contents
                sprintf(fn, test_filename, i);
                f = fopen(fn, "w+");
                if (!f) {
                        printf("[%d] Unable to open/create file for test, skipping test\n", i);
                        fflush(stdout);
                        continue;
                }

                // start test
                start = time(NULL);
                printf("[%d] Testing url=%s\n", i, urls[i]);
                fflush(stdout);

                st = stream_init(urls[i]);
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
        }

        free(buffer);

        return 0;
}
