#include <fcntl.h>
#include <linux/spi/spidev.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define SPI_DEV "/dev/spidev0.0"
#define FRAME_BYTES 1920
#define SPI_SPEED_HZ 2000000
#define BITS_PER_WORD 8

int main(void)
{
    int spiFd = -1;
    int pcmFd = -1;
    uint8_t txBuf[FRAME_BYTES];
    uint8_t rxDummy[FRAME_BYTES];
    uint8_t mode = SPI_MODE_0;
    uint8_t bits = BITS_PER_WORD;
    uint32_t speed = SPI_SPEED_HZ;
    int frameCnt = 0;

    spiFd = open(SPI_DEV, O_RDWR);
    if (spiFd < 0)
    {
        perror("open spi");
        return 1;
    }

    if (ioctl(spiFd, SPI_IOC_WR_MODE, &mode) < 0)
    {
        perror("SPI_IOC_WR_MODE");
        close(spiFd);
        return 1;
    }

    if (ioctl(spiFd, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0)
    {
        perror("SPI_IOC_WR_BITS_PER_WORD");
        close(spiFd);
        return 1;
    }

    if (ioctl(spiFd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0)
    {
        perror("SPI_IOC_WR_MAX_SPEED_HZ");
        close(spiFd);
        return 1;
    }

    pcmFd = open("test.pcm", O_RDONLY);
    if (pcmFd < 0)
    {
        perror("open pcm file");
        close(spiFd);
        return 1;
    }

    printf("SPI PCM send start\n");

    while (1)
    {
        ssize_t rd = read(pcmFd, txBuf, FRAME_BYTES);
        if (rd < 0)
        {
            perror("read pcm");
            break;
        }

        if (rd == 0)
        {
            printf("EOF\n");
            break;
        }

        if (rd < FRAME_BYTES)
        {
            memset(txBuf + rd, 0, FRAME_BYTES - rd);
        }

        struct spi_ioc_transfer tr;
        memset(&tr, 0, sizeof(tr));
        memset(rxDummy, 0, sizeof(rxDummy));

        tr.tx_buf = (unsigned long)txBuf;
        tr.rx_buf = (unsigned long)rxDummy;
        tr.len = FRAME_BYTES;
        tr.speed_hz = speed;
        tr.bits_per_word = bits;
        tr.delay_usecs = 0;
        tr.cs_change = 0;

        int ret = ioctl(spiFd, SPI_IOC_MESSAGE(1), &tr);
        if (ret < 1)
        {
            perror("SPI_IOC_MESSAGE");
            break;
        }

        frameCnt++;
        if ((frameCnt % 10) == 0)
        {
            printf("sent frame=%d ret=%d\n", frameCnt, ret);
        }

        usleep(12000);
    }

    close(pcmFd);
    close(spiFd);
    printf("SPI PCM send end\n");
    return 0;
}