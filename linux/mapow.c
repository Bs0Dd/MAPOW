// #MAPOW - Mobile Action USB cable charging control.
//  Linux version
//  Based on chargerma.c
//  Improved by Bs0Dd [bs0dd.net], 2026

#include  <stdio.h>
#include  <termio.h>
#include  <unistd.h>
#include  <string.h>
#include  <stdlib.h>
#include  <stdint.h>
#include  <fcntl.h>

int main(int argc, char* argv[]) {
 int fd;
 int status, result;
 struct termios options;
 
 unsigned int prtnum;
 char port[15] = "/dev/ttyUSB\0\0\0";
 char *endptr;
 char flusher;
 char mode = -1;
 
 uint8_t cmd[6] = {0x55, 0x55, 0x55, 0x55, 0x04, 0x02}; // Charger control sequence
 // Sequence description:
 //  * 0b01010101 x4 - sync clock?
 //  * 0b00000100 - command start?
 //  * Command byte:
 //    ^ 0 - disable charger;
 //    ^ 1 - enable charger;
 //    ^ 2 - get status;
 //    ^ 3 - sync clock??? (answers with 0b1010101 x10);
 //    ^ 4 - same as command 2.
 //    No answer for any other byte.
 //
 //  Baudrate: 9600; DTR should be OFF!
 
 uint8_t arf[4] = {0x55, 0xA5, 0x01, 0xFF}; // Charger control answer reference
 uint8_t ans[4] = {0} ; // Charger control answer reference
 // Answer description:
 //  * 0b01010101 - sync clock?
 //  * 0b10100101 - symmetric sync clock???
 //  * 0b00000001 - status start?
 //  * Status byte:
 //    ^ 00 - charger disabled;
 //    ^ FF - charger enabled.
 //
 //  For command 3 it answers with 0b1010101 x10 instead.
 
 puts("MAPOW - Mobile Action USB cable charging control.");

 if ((argc > 1) && (strcmp(argv[1], "-h") == 0)) {
  puts("Linux edition. By Bs0Dd [bs0dd.net], 2026.");
  puts("Usage: mapow [-h] [port_number] [e/d]");
  puts("       mapow     - interactive port selection, works with [e/d] too");
  puts("       mapow 0   - switch charging mode on /dev/ttyUSB0");
  puts("       mapow 0 e - force charge enabling on /dev/ttyUSB0");
  puts("       mapow 0 d - force charge disabling on /dev/ttyUSB0");
  puts("       mapow -h  - show this short help");
  return 0;
 }
 
 if (argc > 1 && (strcmp(argv[1], "e") != 0) 
              && (strcmp(argv[1], "d") != 0)) {
  strtol(argv[1], &endptr, 10);
  if (*endptr != '\0' || endptr == argv[1]) {
   fputs("[port_number] invalid port number!\n", stderr);
   return -1;
  }
  else {
   strncpy(port+11, argv[1], 3);
  }
 }
 else {
  do {
   printf("Port number (/dev/ttyUSBx): ");
   status = scanf("%3u", &prtnum);
   if (status != 1) {
    while ((flusher = getchar()) != '\n' && flusher != EOF);
   }
  } while (status != 1);
  sprintf(port+11, "%u", prtnum);
 }
 
 // Force enable charging
 if ((argc > 1) && (strcmp(argv[1], "e") == 0) ||
     (argc > 2) && (strcmp(argv[2], "e") == 0)) { 
  mode = 0;
  cmd[5] = 1;
 }
 // Force disable charging
 else if ((argc > 1) && (strcmp(argv[1], "d") == 0) ||
     (argc > 2) && (strcmp(argv[2], "d") == 0)) { 
  mode = 1;
  cmd[5] = 0;
  arf[3] = 0;
 }
 
 printf("Device: %s\n", port);

 fd = open(port, O_RDWR | O_NOCTTY);
 if (fd == -1) {
  perror("[open_port] unable to open port");
  return -1;
 }

 // Init the port 
 tcgetattr(fd, &options);
 cfsetispeed(&options, B9600);
 cfsetospeed(&options, B9600);
 options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
 options.c_iflag &= ~(IXON | IXOFF | IXANY);
 options.c_cflag |= CREAD | CLOCAL;
 options.c_cflag &= ~CRTSCTS;
 options.c_cflag &= ~PARENB;
 options.c_cflag &= ~CSTOPB;
 options.c_cflag &= ~CSIZE;
 options.c_cflag |= CS8;
 options.c_oflag &= ~OPOST;
 options.c_cc[VMIN] = 0;
 options.c_cc[VTIME] = 10;
 tcsetattr(fd, TCSANOW, &options);
  
 ioctl(fd, TIOCMGET, &status);
 status |= TIOCM_RTS;
 status &= ~TIOCM_DTR;
 ioctl(fd, TIOCMSET, &status);
 
 if (tcflush(fd, TCIOFLUSH) != 0) {
  perror("[flush_port] unable to flush port");
 }
 
 // Send the control sequence (get status)
 if (mode == -1) {
  result = write(fd, cmd, 6);
  if (result < 0) {
   perror("[seq_write] write failed");
   return -1;
  }
  else if (result != 6) {
   fputs("[seq_write] write failed: invalid written data length\n", stderr);
   return -1;
  }
  
  usleep(15000);

  // Read the answer
  result = read(fd, ans, 4);
  if (result < 0) {
   perror("[seq_read] read failed");
   return -1;
  }
  else if (result == 0) {
   fputs("[seq_read] read failed: no answer from charger control!\nMaybe wrong port number?\n", stderr);
   return -1;
  }
  else if (result != 4) {
   fputs("[seq_read] read failed: invalid answer length\n", stderr);
   return -1;
  }

  // Check the answer
  result = 1;
  for (int i = 0; i < 3; i++) {
   if (ans[i] != arf[i]) {
    result = 0;
    break;
   }
  }
  
  if (result && (ans[3] != 0) && (ans[3] != 0xFF)) {
   result = 0;
  }
  
  if (!result) {
   fputs("[seq_read] read failed: invalid answer received\n", stderr);
   return -1;
  }
  
  mode = !ans[3] ? 0 : 1;
  printf("Current state: charger %s.\n", (!mode ? "disabled" : "enabled"));
  
  // Enable charging
  if (!mode) { 
   cmd[5] = 1;
  }
  // Disable charging
  else {
   cmd[5] = 0;
   arf[3] = 0;
  }
  
  usleep(15000);
 }
 
 // Send the control sequence (get status)
 result = write(fd, cmd, 6);
 if (result < 0) {
  perror("[seq_write] write failed");
  return -1;
 }
 else if (result != 6) {
  fputs("[seq_write] write failed: invalid written data length\n", stderr);
  return -1;
 }
 
 usleep(15000);

 // Read the answer
 result = read(fd, ans, 4);
 if (result < 0) {
  perror("[seq_read] read failed");
  return -1;
 }
 else if (result == 0) {
  fputs("[seq_read] read failed: no answer from charger control!\nMaybe wrong port number?\n", stderr);
  return -1;
 }
 else if (result != 4) {
  fputs("[seq_read] read failed: invalid answer length\n", stderr);
  return -1;
 }

 // Check the answer
 result = 1;
 for (int i = 0; i < 4; i++) {
  if (ans[i] != arf[i]) {
   result = 0;
   break;
  }
 }
  
 if (!result) {
  fputs("[seq_read] read failed: invalid answer received\n", stderr);
  return -1;
 }
 
 printf("USB charging %s!\n", (mode ? "disabled" : "enabled"));

 close(fd);
 return 0;
}
