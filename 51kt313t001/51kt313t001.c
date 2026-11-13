#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/time.h>
#include <termios.h>
#include <unistd.h>

#define AUTOPROBE_BPS_300    (1 << 0)
#define AUTOPROBE_BPS_1200   (1 << 1)
#define AUTOPROBE_BPS_2400   (1 << 2)
#define AUTOPROBE_BPS_4800   (1 << 3)
#define AUTOPROBE_BPS_9600   (1 << 4)
#define AUTOPROBE_BPS_19200  (1 << 5)
#define AUTOPROBE_BPS_38400  (1 << 6)
#define AUTOPROBE_BPS_57600  (1 << 7)
#define AUTOPROBE_BPS_115200 (1 << 8)
#define AUTOPROBE_BPS_ALL    (0x1ff)

#define DIV_BY_100(N) (N*41)/(1 << 12) // or "N / 100"

static uint16_t bps[] = {
  DIV_BY_100(57600), /* default setting */
  DIV_BY_100(115200), /* faster would be a logical choice */
  DIV_BY_100(38400), DIV_BY_100(19200), DIV_BY_100(9600), /* or slower */
  DIV_BY_100(4800), DIV_BY_100(2400), DIV_BY_100(1200),
  DIV_BY_100(300), 0
};

static void empty_rx_data(int fd)
{
  char buf[1];
  int status;

  while (1) {
    status = read(fd, &buf[0], 1);
    if (status == -1) {
      if (errno == EAGAIN) {
        break;
      }
    }
  }
}

static int modem_io_read(int fd, uint8_t *buf, int nbytes)
{
  return read(fd, buf, nbytes);
}

static int modem_io_write(int fd, uint8_t *buf, int nbytes)
{
  return write(fd, buf, nbytes);
}

static int modem_io(int fd, uint8_t *buf, int nbytes,
                    int nsecs, int (*io_fn)(int, uint8_t *, int))
{
  struct timeval t = {};  
  struct timeval t2 = {};  
  int status;
  int m;

  if (gettimeofday(&t, 0) != 0) {
    fprintf(stderr, "unable to use gettimeofday\n");
    return -1;
  }
  
  for (m = 0; m < nbytes; m++) {
    do {
      if (gettimeofday(&t2, 0) != 0) {
        fprintf(stderr, "unable to use gettimeofday\n");
        return -1;
      }

      if (t2.tv_sec > (t.tv_sec + nsecs)) {
        break;
      }

      status = io_fn(fd, &buf[m], 1);
      if (status == -1) {
        if (errno == EAGAIN) {
          continue;
        }
      }
      if (status == 1) {
        //fprintf(stderr, "io fn success 0x%02x\n", buf[m]); fflush(stderr);
        break;
      }
    } while(1);
  }

  if (m < nbytes) {
    fprintf(stderr, "failed io access %d/%d bytes\n", m, nbytes);
    return -1;
  }

  return 0;
}

static int send_str(int fd, char *str)
{
  return modem_io(fd, (uint8_t *)str, strlen(str), 1, modem_io_write);
}

static int recv_str(int fd, char *expected_str)
{
  uint8_t buf[80] = {};
  int n = strlen(expected_str);

  if (n > (sizeof(buf) - 1)) {
    return -1;
  }

  modem_io(fd, (uint8_t *)&buf[0], sizeof(buf), 1, modem_io_read);
  if (strcmp((char *)&buf[0], expected_str) != 0) {
#if 0    
    fprintf(stderr, "receive data mismatch '%s' vs '%s'\n",
	    (char *)&buf[0], expected_str);
#endif    
    return -1;
  }

  return 0;
}

static int release_modem(int fd)
{
  if (send_str(fd, "ATZ\n") != 0) {
    return -1;
  }

  recv_str(fd, "ATZ");
  empty_rx_data(fd);
  return 0;
}

static int modem_io_probe_echo(int fd, uint8_t *buf, int nbytes)
{
  empty_rx_data(fd);

  if (send_str(fd, "+") != 0) {
    return 0;
  }

  if (recv_str(fd, "+") != 0) {
    return 0;
  }

  /* success */
  buf[0] = 'Y';
  return 1;
}

static int modem_io_probe(int fd, uint8_t *buf, int nbytes)
{
  empty_rx_data(fd);

  if (send_str(fd, "+++") != 0) {
    return 0;
  }

  if (recv_str(fd, "OK\r\n") != 0) {
    return 0;
  }

  /* success */
  buf[0] = 'Y';
  return 1;
}

static int try_access_modem(int fd, int bps)
{
  struct termios t = {};
  uint8_t buf[1];
  int m;

  if (tcgetattr(fd, &t) != 0) {
    fprintf(stderr, "unable to use tcgetattr()\n");
    return -1;
  }

  if (bps) {
    if (cfsetspeed(&t, bps) != 0) {
      fprintf(stderr, "unable to use cfsetspeed()\n");
      return -1;
    }
  }

  t.c_cflag &= ~CSTOPB;
  t.c_cflag &= ~PARENB;
  t.c_cflag &= ~PARODD;
  t.c_cflag &= ~HUPCL;
  t.c_cflag &= ~CSIZE;
  t.c_cflag |= CS8 | CREAD;
  t.c_cflag |= CLOCAL;
  /* TODO: IGNBRK, CRTSCTS */

  if (tcsetattr(fd, TCSANOW, &t) != 0) {
    fprintf(stderr, "unable to use tcsetattr()\n");
    return -1;
  }

  for (m = 0; m < 3; m++) {
    /* send +, see if we get the same char echoed back */
    buf[0] = 'N';
    modem_io_probe_echo(fd, &buf[0], 1);
    if (buf[0] != 'Y') {
      goto enter_mode; /* no response, try entering mode */
    }

    /* invalid response, guess that we might need to exit AT Command mode */
    send_str(fd, "\n");
    empty_rx_data(fd);
    release_modem(fd); /* exit AT Command mode */

  enter_mode:
    /* again send +++, see if we get OK back */
    buf[0] = 'N';
    modem_io_probe(fd, &buf[0], 1);
    if (buf[0] == 'Y') {
      return 0; /* success, we have entered AT Command mode */
    }

    /* try again if needed */
  }

  return -1;
}

static int open_probe_device(char *tty_device,
                             uint16_t autoprobe_bitmask,
                             int *out_bps)
{
  int curr_bps;
  int fd;
  int m;

  fd = open(tty_device, O_RDWR | O_NONBLOCK);
  if (fd == -1) {
    fprintf(stderr, "unable to open device %s\n", tty_device);
    return -1;
  }

  for (m = 0; bps[m]; m++) {
    if (autoprobe_bitmask & (1 << m)) {
      curr_bps = bps[m] * 100;
      if (try_access_modem(fd, curr_bps) == 0) {
        break;
      }
      curr_bps = -1;
    }
  }

  if (curr_bps >= 0) {
    *out_bps = curr_bps;
    return fd;
  }

  release_modem(fd);
  close(fd);
  return -1;
}

static void character_io_puts(int fd, char *str)
{
  char buf[1];
  int status;
  int m = 0;
  int tail_mode = 0;
  int tail_idx = 0;

  empty_rx_data(fd);

  do {
    status = read(fd, &buf[0], 1);
    if (status == 1) {
      if (tail_mode) {
        if (buf[0] == '\n') {
          fprintf(stdout, " "); /* space separator between ATCMD and result */
          tail_mode++;
        }
      }
      if ((buf[0] != '\n') && (buf[0] != '\r')) {
        fprintf(stdout, "%c", buf[0]);
      }
    }

    switch (tail_mode) {
    case 0:
      if (str[m] != '\0') {
        status = write(fd, &str[m], 1);
        if (status == 1) {
          m++;
        }
      } else {
        tail_mode = 1;
      }
      break;

    case 3:
      fprintf(stdout, "\n"); /* one line per command */
      fflush(stdout);
      return;

    default:
      if (tail_idx == 0) {
        status = write(fd, "\n", 1);
        if (status == 1) {
          tail_idx++;
        }
      } else if (tail_idx == 1) {
        status = write(fd, "\r", 1);
        if (status == 1) {
          tail_idx++;
        }
      }
    }
  } while(1);
}

int main(int argc, char **argv)
{
  int bps = 0;
  int fd;
  int m;

  if (argc >= 2) {
    fd = open_probe_device(argv[1], AUTOPROBE_BPS_ALL, &bps);
    if (fd == -1) {
      exit(1);
    }
#if 0
    fprintf(stdout, "%s: successful probe of %s at %d bps\n",
            argv[0], argv[1], bps);
#endif
    if (argc > 2) {
      for (m = 2; m < argc; m++) {
	character_io_puts(fd, argv[m]);
      }
    } else {
      character_io_puts(fd, "ATI");
      character_io_puts(fd, "ATS1?");
      character_io_puts(fd, "ATS2?");
      character_io_puts(fd, "ATS3?");
      character_io_puts(fd, "ATS4?");
      character_io_puts(fd, "ATS5?");
      character_io_puts(fd, "ATS6?");
    }
    release_modem(fd);
    close(fd);
    exit(0);
  }
  fprintf(stderr, "%s: usage %s tty-device [ATCMDs]\n", argv[0], argv[0]);
  exit(0);
}
