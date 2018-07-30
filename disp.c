#include <stdio.h>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#include "disp.h"

#define PRINTF_MODULE   "[display ] "

#define GPIO_PREFIX     "/sys/class/gpio/"
#define GPIO_EXPORT_FN  "export"
#define GPIO_DEV_PREFIX "gpio"
#define GPIO_L_DIGIT    "61"
#define GPIO_R_DIGIT    "44"
#define GPIO_DIR        "out"
#define GPIO_DIR_FN     "direction"
#define GPIO_ENABLE     "1"
#define GPIO_DISABLE    "0"
#define GPIO_VAL_FN     "value"

#define GPIO_L_DIGIT_PREFIX     GPIO_PREFIX GPIO_DEV_PREFIX GPIO_L_DIGIT "/"
#define GPIO_R_DIGIT_PREFIX     GPIO_PREFIX GPIO_DEV_PREFIX GPIO_R_DIGIT "/"

#define DISP_I2C_BUS    "/dev/i2c-1"
#define DISP_ADDR       0x20
#define DISP_ON_TIME_NS 5e6

#define DISP_DIR_REGA   0x00
#define DISP_DIR_REGB   0x01
#define DISP_DIR_OUT    0x00

// display register values
#define DISP_BOT_REG    0x14
#define BOT_N           0x10
#define BOT_S           0x20
#define BOT_E           0x80
#define BOT_W           0x01
#define DISP_TOP_REG    0x15
#define TOP_N           0x04
#define TOP_S           0x08
#define TOP_E           0x02
#define TOP_W           0x80

#define DISP_0_BOT      BOT_S | BOT_E | BOT_W
#define DISP_0_TOP      TOP_N | TOP_E | TOP_W
#define DISP_1_BOT      BOT_E
#define DISP_1_TOP      TOP_E
#define DISP_2_BOT      BOT_N | BOT_W | BOT_S
#define DISP_2_TOP      TOP_N | TOP_E | TOP_S
#define DISP_3_BOT      BOT_N | BOT_E | BOT_S
#define DISP_3_TOP      TOP_N | TOP_E | TOP_S
#define DISP_4_BOT      BOT_N | BOT_E
#define DISP_4_TOP      TOP_S | TOP_E | TOP_W
#define DISP_5_BOT      BOT_N | BOT_E | BOT_S
#define DISP_5_TOP      TOP_N | TOP_W | TOP_S
#define DISP_6_BOT      BOT_N | BOT_E | BOT_W | BOT_S
#define DISP_6_TOP      TOP_N | TOP_W | TOP_S
#define DISP_7_BOT      BOT_E
#define DISP_7_TOP      TOP_N | TOP_E
#define DISP_8_BOT      BOT_N | BOT_E | BOT_W | BOT_S
#define DISP_8_TOP      TOP_N | TOP_E | TOP_W | TOP_S
#define DISP_9_BOT      BOT_N | BOT_E | BOT_S
#define DISP_9_TOP      TOP_N | TOP_E | TOP_W | TOP_S

#define BUF_SIZE        2

static int loop = 0;
static pthread_t th;

static struct timespec ts;

static int fd = 0;

static int number = 0;

/* Helper functions */
static int stringToFile(const char *fn, const char *str)
{
        FILE *f;
        int err;

        (void)fflush(stdout);

        f = fopen(fn, "w");
        if (!f)
                return EIO;
        err = fputs(str, f) == EOF;
        (void)fclose(f);

        return err;
}

static int getFD(void)
{
        fd = open(DISP_I2C_BUS, O_RDWR);
        if (fd < 0)
                goto out;
        if (ioctl(fd, I2C_SLAVE, DISP_ADDR) < 0)
                goto out2;

        return 0;
out2:
        (void)close(fd);
out:
        printf(PRINTF_MODULE "Error: unable to get file descriptor for I2C bus\n");
        (void)fflush(stdout);
        return EIO;
}

static int writeReg(int fd, unsigned char reg_addr, int value)
{
        unsigned char buf[BUF_SIZE];

        buf[0] = reg_addr;

        if (reg_addr == DISP_BOT_REG) {
                if (value < 0 || value > 9)
                        return EINVAL;

                switch (value) {
                case 0:
                        buf[1] = DISP_0_BOT;
                        break;
                case 1:
                        buf[1] = DISP_1_BOT;
                        break;
                case 2:
                        buf[1] = DISP_2_BOT;
                        break;
                case 3:
                        buf[1] = DISP_3_BOT;
                        break;
                case 4:
                        buf[1] = DISP_4_BOT;
                        break;
                case 5:
                        buf[1] = DISP_5_BOT;
                        break;
                case 6:
                        buf[1] = DISP_6_BOT;
                        break;
                case 7:
                        buf[1] = DISP_7_BOT;
                        break;
                case 8:
                        buf[1] = DISP_8_BOT;
                        break;
                case 9:
                        buf[1] = DISP_9_BOT;
                        break;
                }
        } else if (reg_addr == DISP_TOP_REG) {
                if (value < 0 || value > 9)
                        return EINVAL;

                switch (value) {
                case 0:
                        buf[1] = DISP_0_TOP;
                        break;
                case 1:
                        buf[1] = DISP_1_TOP;
                        break;
                case 2:
                        buf[1] = DISP_2_TOP;
                        break;
                case 3:
                        buf[1] = DISP_3_TOP;
                        break;
                case 4:
                        buf[1] = DISP_4_TOP;
                        break;
                case 5:
                        buf[1] = DISP_5_TOP;
                        break;
                case 6:
                        buf[1] = DISP_6_TOP;
                        break;
                case 7:
                        buf[1] = DISP_7_TOP;
                        break;
                case 8:
                        buf[1] = DISP_8_TOP;
                        break;
                case 9:
                        buf[1] = DISP_9_TOP;
                        break;
                }
        } else {
                buf[1] = value;
        }

        return write(fd, buf, BUF_SIZE) <= 0 ? EIO : 0;
}

static void *mainLoop(void *arg)
{
        int curr_num;

        // set direction on i2c bus
        (void)writeReg(fd, DISP_DIR_REGA, DISP_DIR_OUT);
        (void)writeReg(fd, DISP_DIR_REGB, DISP_DIR_OUT);

        while (loop) {
                curr_num = number;
                // left digit
                (void)writeReg(fd, DISP_BOT_REG, curr_num / 10);
                (void)writeReg(fd, DISP_TOP_REG, curr_num / 10);
                if (stringToFile(GPIO_L_DIGIT_PREFIX GPIO_VAL_FN, GPIO_ENABLE))
                        continue;
                (void)nanosleep(&ts, NULL);
                if (stringToFile(GPIO_L_DIGIT_PREFIX GPIO_VAL_FN, GPIO_DISABLE))
                        continue;

                // right digit
                (void)writeReg(fd, DISP_BOT_REG, curr_num % 10);
                (void)writeReg(fd, DISP_TOP_REG, curr_num % 10);
                if (stringToFile(GPIO_R_DIGIT_PREFIX GPIO_VAL_FN, GPIO_ENABLE))
                        continue;
                (void)nanosleep(&ts, NULL);
                if (stringToFile(GPIO_R_DIGIT_PREFIX GPIO_VAL_FN, GPIO_DISABLE))
                        continue;
        }

        (void)close(fd);
        return NULL;
}

/* Public functions */
int disp_init(void)
{
        int err;

        ts.tv_sec  = 0;
        ts.tv_nsec = DISP_ON_TIME_NS;

        // enable selecting a digit
        err = stringToFile(GPIO_PREFIX GPIO_EXPORT_FN, GPIO_L_DIGIT);
        if (err)
                goto out;

        err = stringToFile(GPIO_PREFIX GPIO_EXPORT_FN, GPIO_R_DIGIT);
        if (err)
                goto out;

        // setting direction
        err = stringToFile(GPIO_L_DIGIT_PREFIX GPIO_DIR_FN, GPIO_DIR);
        if (err)
                goto out;

        err = stringToFile(GPIO_R_DIGIT_PREFIX GPIO_DIR_FN, GPIO_DIR);
        if (err)
                goto out;

        err = getFD();
        if (err)
                goto out;

        loop = 1;
        return pthread_create(&th, NULL, mainLoop, NULL);
out:
        printf(PRINTF_MODULE "Error: unable to initialize display module\n");
        (void)fflush(stdout);
        return err;
}

void disp_cleanup(void)
{
        loop = 0;
        (void)pthread_join(th, NULL);

        (void)stringToFile(GPIO_L_DIGIT_PREFIX GPIO_VAL_FN, GPIO_DISABLE);
        (void)stringToFile(GPIO_R_DIGIT_PREFIX GPIO_VAL_FN, GPIO_DISABLE);
}

void disp_setNumber(int n)
{
        // clamp number to within bounds
        n = n > 99 ? 99 : n;
        n = n < 0  ? 0  : n;

        number = n;
}
