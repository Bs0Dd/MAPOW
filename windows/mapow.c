// #MAPOW - Mobile Action USB cable charging control.
//  Windows version
//  Based on chargerma.c
//  Improved by Bs0Dd [bs0dd.net], 2026

#include  <stdio.h>
#include  <stdint.h>
#include  <windows.h>

int main(int argc, char* argv[]) {
 HANDLE fd;
 BOOL status;
 DWORD result;
 char errmessage[256];
 char oemmessage[256];
 COMMTIMEOUTS timeouts = {0};
 DCB options = {0};
 options.DCBlength = sizeof(options);
 
 unsigned int prtnum;
 char port[11] = "\\\\.\\COM\0\0\0";
 char *endptr;
 int flusher;
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
  puts("Windows edition. By Bs0Dd [bs0dd.net], 2026.");
  puts("Usage: mapow [-h] [port_number] [e/d]");
  puts("       mapow     - interactive port selection, works with [e/d] too");
  puts("       mapow 1   - switch charging mode on COM1");
  puts("       mapow 1 e - force charge enabling on COM1");
  puts("       mapow 1 d - force charge disabling on COM1");
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
   strncpy(port+7, argv[1], 3);
  }
 }
 else {
  do {
   printf("Port number (COMx): ");
   status = scanf("%3u", &prtnum);
   if (status != 1) {
    while ((flusher = getchar()) != '\n' && flusher != EOF);
   }
  } while (status != 1);
  sprintf(port+7, "%u", prtnum);
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
 
 printf("Device: %s\n", port+4);

 fd = CreateFileA(port, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
 if (fd == INVALID_HANDLE_VALUE) {
  result = GetLastError();
  FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		         NULL, result, 0, errmessage, sizeof(errmessage), NULL);
  CharToOemBuffA(errmessage, oemmessage, sizeof(errmessage));
  fprintf(stderr, "[open_port] unable to open port: %s", oemmessage);
  return -1;
 }

 // Init the port
 GetCommState(fd, &options);
 options.BaudRate = 9600;
 options.ByteSize = 8;
 options.Parity = NOPARITY;
 options.StopBits = ONESTOPBIT;
 SetCommState(fd, &options);

 timeouts.ReadIntervalTimeout = 20;
 timeouts.ReadTotalTimeoutMultiplier = 1;
 timeouts.ReadTotalTimeoutConstant = 1000;
 timeouts.WriteTotalTimeoutMultiplier = 1;
 timeouts.WriteTotalTimeoutConstant = 1000;
 SetCommTimeouts(fd, &timeouts); 

 EscapeCommFunction(fd, SETRTS);
 EscapeCommFunction(fd, CLRDTR);

 if (PurgeComm(fd, PURGE_RXCLEAR | PURGE_TXCLEAR) == FALSE) {
     perror("[flush_port] unable to flush port");
 }
 
 // Send the control sequence (get status)
 if (mode == -1) {
  status = WriteFile(fd, cmd, 6, &result, NULL);
  if (status == FALSE) {
   result = GetLastError();
   FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                  NULL, result, 0, errmessage, sizeof(errmessage), NULL);
   CharToOemBuffA(errmessage, oemmessage, sizeof(errmessage));
   fprintf(stderr, "[seq_write] write failed: %s", oemmessage);
   return -1;
  }
  else if (result != 6) {
   fputs("[seq_write] write failed: invalid written data length\n", stderr);
   return -1;
  }
  
  Sleep(15);

  // Read the answer
  status = ReadFile(fd, ans, 4, &result, NULL);
  if (status == FALSE) {
   result = GetLastError();
   FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                  NULL, result, 0, errmessage, sizeof(errmessage), NULL);
   CharToOemBuffA(errmessage, oemmessage, sizeof(errmessage));
   fprintf(stderr, "[seq_read] read failed: %s", oemmessage);
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
  
  Sleep(15);
 }
 
 // Send the control sequence (get status)
 status = WriteFile(fd, cmd, 6, &result, NULL);
 if (status == FALSE) {
  result = GetLastError();
  FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                 NULL, result, 0, errmessage, sizeof(errmessage), NULL);
  CharToOemBuffA(errmessage, oemmessage, sizeof(errmessage));
  fprintf(stderr, "[seq_write] write failed: %s", oemmessage);
  return -1;
 }
 else if (result != 6) {
  fputs("[seq_write] write failed: invalid written data length\n", stderr);
  return -1;
 }
 
 Sleep(15);

 // Read the answer
 status = ReadFile(fd, ans, 4, &result, NULL);
 if (status == FALSE) {
  result = GetLastError();
  FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                 NULL, result, 0, errmessage, sizeof(errmessage), NULL);
  CharToOemBuffA(errmessage, oemmessage, sizeof(errmessage));
  fprintf(stderr, "[seq_read] read failed: %s", oemmessage);
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

 CloseHandle(fd);
 return 0;
}
