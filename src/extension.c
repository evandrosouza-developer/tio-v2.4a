/*
 * Keys Map - Map reader from MSX Keyboard Emulator
 * It is based on TIO, a simple TTY terminal I/O application
 *
 * Copyright (c) 2022  Evandro Souza
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <errno.h>
#include "config.h"
#include "socket.h"
#include "print.h"
#include "error.h"
#include "tty.h"
#include "time.h"
#include "extension.h"
#include "options.h"


extern struct termios tio, tio_old, stdout_new, stdout_old, stdin_new, stdin_old;
extern void           (*print)(char c);
extern int            fd;
extern unsigned long  rx_total, tx_total;
extern bool           map_o_cr_nl, map_o_nl_crnl;

#ifdef ENABLE_PARALLEL_KEYBOARD


//Constants for screen postioning
#define SIZE_X        3                                        //Number of lines each X occupies
#define SIZE_Y        6                                        //Number of lines each Y occupies
#define OFFSET_X      2                                        //Line of X0
#define OFFSET_Y      5                                        //Colunm of Y0

/****************************************************************************************************/
/****** Starting with this point, there are my code to show parallel keyboard layout from tio. ******/
/****************************************************************************************************/

//8 MSX lines, 16 colunms, 2 print lines each MSX line (upper and lower keytop serigraphy), 6 bytes each ASCIIZ string
char key_matrix[8][16][2][6];


/*Returns true if OK or false if EOF has been reached*/
static bool copy_two_lines_of_X(uint8_t*x, long*filecount, long*fileSize, FILE *fp)
{
  uint8_t   y, j, page, mark[4];
  volatile  char  ch_file_in;

  //first line of X (Characters to be addressed with SHIFT)
  page = 0;                                                   //positioning to lower serigraphy
  //Search for ("X=")
  mark[3] = 0; mark[2] = 0; mark[1] = 0; mark[0] = 0;         //clean each position of mark
  while(strcmp((char*)mark, "X="))
  {
    ch_file_in = fgetc(fp);
    (*filecount)++;
    if(*filecount > *fileSize)
    {
      fflush(stdout);
      return false;
    }
    mark[0] = mark[1];
    mark[1] = ch_file_in;
  }
  //Positioned at "mark"

  /*first 3 chars of the line "X=0 "*/
  //cursor positioning to upper serigraphy (page = 0)
  printf("\x1B[%d;%dH", SIZE_X * (*x) + OFFSET_X + page, 0);  //3*(*x)+2+page, 0);
  printf("%s", mark);  //"X="
  ch_file_in = fgetc(fp);
  (*filecount)++;
  if(*filecount > *fileSize)
  {
    fflush(stdout);
    return false;
  }
  printf("%c", ch_file_in);

  // Copy from Y=0 to Y=12
  mark[3] = 0; mark[2] = 0; mark[1] = 0; mark[0] = 0;
  y = 0;
  //cursor positioning to upper serigraphy (page = 0)
  while((ch_file_in != '\n') && (ch_file_in != '\r') && (y < 12))
  {
    //Search for "; $"
    while(strcmp((char*)mark, "; $"))
    {
      ch_file_in = fgetc(fp);
      (*filecount)++;
      if(*filecount > *fileSize)
      {
        key_matrix[*x][y][page][5] = 0;
        fflush(stdout);
        return false;
      }
      mark[0] = mark[1];
      mark[1] = mark[2];
      mark[2] = ch_file_in;
    }
    //Positioned at "; $"
    for(j = 0; j < 5; j++)
    {
      ch_file_in = fgetc(fp);
      if((ch_file_in != '\n') && (ch_file_in != '\r'))
      {
        //printf("%c", ch_file_in);
        key_matrix[*x][y][page][j] = (uint8_t)ch_file_in;
      }
      (*filecount)++;
      if(*filecount > *fileSize)
      {
        key_matrix[*x][y][page][j + 1] = 0;
        fflush(stdout);
        return false;
      }
    }
    key_matrix[*x][y][page][5] = 0;

    //Print at position what was read
    //cursor positioning to upper serigraphy (page = 0)
    printf("\x1B[%d;%dH", SIZE_X * (*x) + OFFSET_X + page, SIZE_Y * y + OFFSET_Y);  //3*(*x)+2+page3*(*x)+2+page, 6*y+5);
    if(y != 11)
      printf("%s", &key_matrix[*x][y][page][0]);
    else
      printf("%s\r", &key_matrix[*x][y][page][0]);
    y++;  //next  colunm
    mark[3] = 0; mark[2] = 0; mark[1] = 0; mark[0] = 0;
  }

  //second line of X (Characters to be addressed w/o SHIFT)
  page = 1;                                                   //positioning to lower serigraphy
  //Search for ("X=")
  while(strcmp((char*)mark, "X="))
  {
    ch_file_in = fgetc(fp);
    (*filecount)++;
    if(*filecount > *fileSize)
    {
      fflush(stdout);
      return false;
    }
    mark[0] = mark[1];
    mark[1] = ch_file_in;
  }
  //Positioned at "mark"

  //first 3 chars of the line "X="
  //cursor positioning to lower serigraphy (page = 1)
  printf("\x1B[%d;%dH", SIZE_X * (*x) + OFFSET_X + page, 0);  //3*(*x)+2+page
  printf("%s", mark);  //"X="
  ch_file_in = fgetc(fp);
  (*filecount)++;
  if(*filecount > *fileSize)
  {
    fflush(stdout);
    return false;
  }
  printf("%c", ch_file_in);
  // Copy from Y=0 to Y=12
  y = 0;
  while((ch_file_in != '\n') && (ch_file_in != '\r') && (y < 12))
  {
    //Search for "; $"
    while(strcmp((char*)mark, "; $"))
    {
      ch_file_in = fgetc(fp);
      (*filecount)++;
      if(*filecount > *fileSize)
      {
        key_matrix[*x][y][page][5] = 0;
        fflush(stdout);
        return false;
      }
      mark[0] = mark[1];
      mark[1] = mark[2];
      mark[2] = ch_file_in;
    }
    //Positioned at "; $"
    for(j = 0; j < 5; j++)
    {
      ch_file_in = fgetc(fp);
      if((ch_file_in != '\n') && (ch_file_in != '\r'))
      {
        key_matrix[*x][y][page][j] = (uint8_t)ch_file_in;
      }
      (*filecount)++;
      if(*filecount > *fileSize)
      {
        key_matrix[*x][y][page][j + 1] = 0;
        fflush(stdout);
        return false;
      }
    }
    key_matrix[*x][y][page][5] = 0;

    //Print at position what was read
    //cursor positioning to lower serigraphy (page = 1)
    printf("\x1B[%d;%dH", SIZE_X * (*x) + OFFSET_X + page, SIZE_Y * y + OFFSET_Y);  //3*(*x)+2+page3*(*x)+2+page, 6*y+5);
    if(y != 11)
      printf("%s", &key_matrix[*x][y][page][0]);
    else
      printf("%s\r", &key_matrix[*x][y][page][0]);
    y++;
    mark[3] = 0; mark[2] = 0; mark[1] = 0; mark[0] = 0;
  } //while((ch_file_in != '\n') && (ch_file_in != '\r') && (y < 12))
  //now read until LF
  while(ch_file_in != '\n')
  {
    ch_file_in = fgetc(fp);
    (*filecount)++;
    if(*filecount > *fileSize)
    {
      fflush(stdout);
      return false;
    }
  }
  return true;
}


bool load_key_matrix(void)
{
  uint8_t x;
  char    ch_file_in, fname[256];
  FILE    *fp;
  long    fileSize, filecount;

  strcpy(fname, "msx.keyboard");
 
  if ((fp=fopen (fname,"r")) != NULL)
  {
    //If msx.keyboard file exists, then prepares base screen
    printf("\x1B[2J");                                        //Clear screen
    printf("\x1B[H");                                         //Move cursor to home position (0, 0)
    printf("\x1B[m");                                         //Standard style and char format
    fflush(stdout);

    // Get file size
    fseek(fp, 0, SEEK_END);
    fileSize = ftell(fp);
    fseek(fp, 0, SEEK_SET);                                   //Positioning to read from the beginning of file
    
    filecount = 0;
    //Get first char from file
    ch_file_in = fgetc(fp);
    printf("%c", ch_file_in);
    filecount++;
    if(filecount > fileSize)
    {
      fflush(stdout);
      fclose(fp);
      return false;
    }

    // read first line (header)
    while(ch_file_in != '\n')
    {
      ch_file_in = fgetc(fp);
      printf("%c", ch_file_in);
      filecount++;
      if(filecount > fileSize)
      {
        fflush(stdout);
        fclose(fp);
        return false;
      }
    }
    // read 7 of the 8 MSX lines
    for(x = 0; x < 7; x++)
    {
      //Two lines of X
      if(!copy_two_lines_of_X(&x, &filecount, &fileSize, fp))
      {
        fclose(fp);
        return false;
      }
      //now copy the display third line
      ch_file_in = 0;
      while(ch_file_in != '\n')
      {
        ch_file_in = fgetc(fp);
        printf("%c", ch_file_in);
        filecount++;
        if(filecount > fileSize)
        {
          fflush(stdout);
          fclose(fp);
          return false;
        }
      }
      fflush(stdout);
    }   //for(x = 0; x < 7; x++)
    // X=7 (8th line), that does NOT have the third line
    x = 7;
    if(!copy_two_lines_of_X(&x, &filecount, &fileSize, fp))
    {
      fclose(fp);
      return false;
    }
    fflush(stdout);

    filecount++;
    fclose(fp);
    return true;
  } //if ((fp=fopen (fname,"r")) != NULL)
  else
  {
    tio_error_printf_silent("File not found!\r\n");
    return false;
  }
}


/****************************************************************************************************/
/****************************************************************************************************/
/***** Starting with this point, there are my code to show parallel keyboard activity from tio. *****/
/****************************************************************************************************/
/****************************************************************************************************/

static void update_bit_cells(uint8_t y, uint8_t x_received, uint8_t which_x_bit)
{
  /*ANSI Escape codes

  To clear screen, use the value 2 followed by J character.
  printf("\x1B[2J");

  Cursor positioning: 0x1B [ Line; colunm H
  printf("\x1B[%d;%dH",6, 5);
  printf("\x1B[%d;%df",6, 5);

  Style, foregrounf and background colors:
  printf("\x1B[%d;%d;%dm",0, 30, 40);

  Style: 0: None; 1: Bold; 4: Underline; 7: Negative
  Text: 30: white; 31: Red; 32: green; 33: yellow; 34: dark blue; 35: pink; 36: Light blue; 37: grey
  Back: 40: white; 41: Red; 42: green; 43: yellow; 44: dark blue; 45: pink; 46: Light blue; 47: grey

  */
uint8_t   page;

#define NEGATIVE    7
#define LIGHT_BLUE  36

  if(!(x_received & (1 << which_x_bit)))
  {
    //This bit (key) is pressed. Set to marked at computed screen position
    page = 0;                                                 //upper serigraphy
    printf("\x1B[%d;%dH", SIZE_X * which_x_bit + OFFSET_X + page, SIZE_Y * y + OFFSET_Y);
    printf("\x1B[%d;%dm%s\x1B[m", NEGATIVE, LIGHT_BLUE, &key_matrix[which_x_bit][y][0][0]);
    page = 1;                                                 //lower serigraphy
    printf("\x1B[%d;%dH", SIZE_X * which_x_bit + OFFSET_X + page, SIZE_Y * y + OFFSET_Y);
    printf("\x1B[%d;%dm%s\x1B[m", NEGATIVE, LIGHT_BLUE, &key_matrix[which_x_bit][y][1][0]);
  }
  else
  {
    //This bit (key) is released. Set to unmarked (default) at computed screen position
    page = 0;                                                 //upper serigraphy
    printf("\x1B[%d;%dH", SIZE_X * which_x_bit + OFFSET_X + page, SIZE_Y * y + OFFSET_Y);
    printf("\x1B[m%s", &key_matrix[which_x_bit][y][0][0]);
    page = 1;                                                 //lower serigraphy
    printf("\x1B[%d;%dH", SIZE_X * which_x_bit + OFFSET_X + page, SIZE_Y * y + OFFSET_Y);
    printf("\x1B[m%s", &key_matrix[which_x_bit][y][1][0]);
  }
}


void check_input_kb_event(char input_char, char*mount_string)
{
  char *p;
  uint8_t x_received, y, ch;
  ssize_t i;	                                                // count_bytes_read;

  if(input_char != '\n')
  {
    //put input_char at last position of the string
    i = 0;
    while(mount_string[i])
    {
      i++;
    } //compute string length 
    mount_string[i+1] = 0;
    mount_string[i] = input_char;
  }
  else
  {
    //Parse line.
    // First compute Y
    p = strstr(mount_string, "Y");
    if( (p != NULL) && (*(p+2) == ' ') )
    {
      ch = (uint8_t)*(p+1);                                   //get the char after Y
      if(((ch > ('0' - 1) && ch < ('9' + 1)) || (ch > ('A' - 1) && ch < ('F' + 1))))
      {
        y = (ch > ('A' - 1) && ch < ('F' + 1)) ? (ch-('A' - 10)) : (ch - '0');
        //Now compute X
        p = strstr(mount_string, " X");
        if(p != NULL)
        {
          ch = (uint8_t)*(p+2);   //get the first char after X
          x_received = 0xF0;
          if(((ch > ('0' - 1) && ch < ('9' + 1)) || (ch > ('A' - 1) && ch < ('F' + 1))))
          {
            x_received = (ch > ('A'-1) && ch < ('F'+1)) ? (ch-('A'-10)) : (ch-'0');
            x_received <<= 4;
          } //if((ch > ('0' - 1) && ch < ('9' + 1)) || (ch > ('A' - 1) && ch < ('F' + 1)))
          ch = (uint8_t)*(p + 3);   //get the second char after X
          if(((ch > ('0' - 1) && ch < ('9' + 1)) || (ch > ('A' - 1) && ch < ('F' + 1))))
            x_received += (ch > ('A'-1) && ch < ('F'+1)) ? (ch-('A'-10)) : (ch-'0');
          update_bit_cells(y, x_received, 7);                 //bit 7, 128
          update_bit_cells(y, x_received, 6);                 //bit 6, 64
          update_bit_cells(y, x_received, 5);                 //bit 5, 32
          update_bit_cells(y, x_received, 4);                 //bit 4, 16
          update_bit_cells(y, x_received, 3);                 //bit 3, 8
          update_bit_cells(y, x_received, 2);                 //bit 2, 4
          update_bit_cells(y, x_received, 1);                 //bit 1, 2
          update_bit_cells(y, x_received, 0);                 //bit 0, 1
          printf("\x1B[%d;%dH", 24, 0); //Moves the cursor to last line
        } //if(p != NULL)
      } //if((ch > ('0' - 1) && ch < ('9' + 1)) || (ch > ('A' - 1) && ch < ('F' + 1)))
    } //if(p != NULL)
    mount_string[0] = 0;
  } //else if(input_char != (char)10)
}

#endif  //#ifdef ENABLE_PARALLEL_KEYBOARD



#ifdef ENABLE_SEND_FILE
/****************************************************************************************************/
/****************************************************************************************************/
/************ Starting with this point, there are my code to manage send file from tio. *************/
/****************************************************************************************************/
/****************************************************************************************************/

#define X_ON  17
#define X_OFF 19

static char console_getc(void)
{
  int     status;
  char    input_char;

  status = 0;                                                 //force to enter the loop
  while(status < 1)
  {
    /* Check input from stdin. It is inside a while because now it is a non blocking function */
    status = read(STDIN_FILENO, &input_char, 1);
    if (status < 0)
    {
      tio_error_printf_silent("Could not read from stdin");
      tty_disconnect();
      return TIO_ABORT;
    }
  }
  return input_char;
}


/*
 * console_get_filename(char *s, int len)
 *
 * Wait for a string to be entered on the console, limited
 * support for editing characters (back space only)
 * end when a <CR> character is received. Functio aborted with an <Esc>
 * 
 * It returns the number of chars in buffer
 */
static int console_get_filename(char *s, int len)
{
  char  c;
  bool  valid_fname_char;
  const char inv_filename_ch[25] = {27,32,33,34,35,36,37,38,39,'(',')','*','[',']','{','}',';','^',60,'=',62,'@',0}; //<Esc> !“#=$%&‘()*[]{}|;^<=>@
  char  *t = s;

  printf("\r\nEnter file name to send: ");
  fflush(stdout);
  print = print_normal;
  *t = '\000';
  /* read until a <CR> is received */
  while ((c = console_getc()) != '\r')
  {
    /* First check valid characters in filename */
    valid_fname_char = true;
    for (unsigned int i = 0; i < sizeof(inv_filename_ch); i++)
    {
      if(c == inv_filename_ch[i])
      valid_fname_char = false;
    }
    if (c == 0x7F)                                            //Backspace
    {
      if (t > s)
      {
        /* send ^H' '^H to erase previous character */
        printf("\b \b");
        t--;
      }
    }   //if (c == 0x7F)
    else {
      if (valid_fname_char)
      {
        *t = c;
        print(c);
        if ((t - s) < len)
          t++;
      }
    }   //else if (c == 0x7F)
    if (c == 0x1B)                                            //Esc
    {
      return 0;
    }   //if (c == 0x1B)
    /* update end of string with NUL */
    *t = '\000';
  }   //while (c = console_getc()) != '\r')
  return t - s;
}


void file_send(void)
{
  fd_set  rdfs;                                               // Read file descriptor set
  int     maxfd;                                              // Maximum file descriptor used
  char    input_buffer[BUFSIZ];
  struct  timeval tv;
  int     status, state;
  ssize_t rx_tty_received;
  char    ch_stdin, ch_tty_in[64], ch_file_in, fname[256];
  FILE    *fp;
  long    fileSize, filecount;
  bool    x_on_state;                                         //TX go flag
  bool    hw_flow_ctrl = false;
  bool    sw_flow_ctrl = false;


  print = print_normal;

  //First of all, identify type of Flow Control used
  if(!((tio.c_cflag & CRTSCTS) || (tio.c_iflag & (IXON | IXOFF))))
    //Flow Control is NONE
    x_on_state = true;
  else
  {
    //Flow Control is ON
    if(tio.c_cflag & CRTSCTS)
    {
      //Flow Control is Hardware: Replicate CTS state to x_on_state
      hw_flow_ctrl = true;
    }
    else
    {
      //Flow Control is SW: If received is X_OFF, lock TX, than wait for a X_ON to get a new clearance
      if( (((tio.c_iflag & IXON) == IXON) || ((tio.c_iflag & IXOFF) == IXOFF)) )
        sw_flow_ctrl = true;
    }
  }

  //Get from user the file name to transmit
  status = console_get_filename(fname, 255);
  if (status == 0)
  {
    //Aborted
    printf("\r\nSend file aborted by user!\r\n");
    return;
  }
  printf("\r\n");
  fflush(stdout);
 
  if ((fp=fopen (fname,"r")) != NULL)
  {
    /* Get file size */
    fseek(fp, 0, SEEK_END);
    fileSize = ftell(fp);
    fseek(fp, 0, SEEK_SET);                                   //Positioning to read from the beginning of file
    
    if(fileSize)
    {
      filecount = 0;
      //Get first char from file
      ch_file_in = fgetc(fp);
      x_on_state = true;                                      //Starts with "TX can go on"

      while(filecount < fileSize)
      {
        // In file transfer mode, while we are reading file and sending to tty device, we need to
        // read from stdin to react on <Esc> input key to abort, and tty input to send to stdout.
        FD_ZERO(&rdfs);
        FD_SET(fd, &rdfs);
        FD_SET(STDIN_FILENO, &rdfs);

        maxfd = MAX(fd, STDIN_FILENO);

        /* Manage timeout: No timeout */
        tv.tv_sec = 0;
        tv.tv_usec = 0;

        status = select(maxfd + 1, &rdfs, NULL, NULL, &tv);
        if (status > 0)
        {
          if (FD_ISSET(fd, &rdfs))
          {
            // Input from tty device ready
            rx_tty_received = read(fd, input_buffer, BUFSIZ);
            if (rx_tty_received < 0)
            {
              // Error reading - device is likely unplugged
              tio_printf("Could not read from tty device");
              tty_disconnect();
              return;
            }
            if(rx_tty_received > 0)
            {
              // Update receive statistics
              rx_total += rx_tty_received;
              // Print received tty character to stdout
              for (int i = 0; i < rx_tty_received; i++)
                print(input_buffer[i]);
            }
          }   //if (FD_ISSET(fd, &rdfs))
          if (FD_ISSET(STDIN_FILENO, &rdfs))
          {
            // Input from stdin ready
            status = read(STDIN_FILENO, &ch_stdin, 1);
            if(status < 0)
            {
              tio_error_printf("Could not read from stdin");
              tty_disconnect();
              return;
            }
            if( (status > 0) && (TIO_ABORT == ch_stdin) )
            {
              tio_warning_printf("\r\nSend file aborted by user!\r\n");
              break;                                          //quit while(filecount < fileSize)
            }
          }   //else if (FD_ISSET(STDIN_FILENO, &rdfs))
        }   //if (status > 0)

        // Read transmission's clearance
        //Flow Control is ON
        if(hw_flow_ctrl)
        {
          //Flow Control is Hardware: Replicate CTS state to x_on_state
          if(ioctl(fd, TIOCMGET, &state) < 0)
          {
            tio_printf("Could not get line state: %s", strerror(errno));
            break;
          }
          else
            x_on_state = (state & TIOCM_CTS);
        } //if(hw_flow_ctrl)
        else
        {
          //Flow Control is SW: If received is X_OFF, lock TX, than wait for a X_ON to get a new clearance
          if(sw_flow_ctrl)
          {
            for (int i = 0; i < rx_tty_received; i++)
            {
              if(x_on_state && (ch_tty_in[i] == X_OFF))
                x_on_state = false;
              else if(!x_on_state && (ch_tty_in[i] == X_ON))
                x_on_state = true;
            } //for (int i = 0; i < rx_tty_received; i++)
          } //if(sw_flow_ctrl)
        } //else if(hw_flow_ctrl)

        // Prepare data and send it, if cleared
        if(x_on_state)
        {
          // Map newline character
          if( (ch_file_in == '\n') && (map_o_nl_crnl) )
            ch_file_in = '\r';

          // Map output character
          if( (ch_file_in == '\r') && (map_o_cr_nl) )
            ch_file_in = '\n';

          //No need to use epoll here, as it will block if not ready
          forward_to_tty(fd, ch_file_in);

          fsync(fd);                                      //update log file
          ch_file_in = fgetc(fp);                         //Get next char from file to transmit
          filecount++;
          delay(option.output_delay);
        } //if(x_on_state)
      } //while(filecount < fileSize)
    }   //if(fileSize)

    fclose(fp);
    tio_printf("File send concluded!\r\n");
  } //if ((fp=fopen (fname,"r")) != NULL)
  else
    tio_printf("Operation aborted: file not found!\r\n");
} //file_send(void)
#endif  //#ifdef ENABLE_SEND_FILE
