/* Program to simulate scroll version od Unix "more" command. Provided text file as an argument, this program displays the content of 
 * the file. If Space key is pressed, it displays the next screenfull, and if Enter is pressed, the scroll function gets activated.
 * User is able to control the speed of scrolling. Pressinf f will speed it up, while s will slow down the speed. Current speed in 
 * miliseconds is shown in line/milisecond in the lower left corner.
 * Author: Aleksandra Milinovic
 */

//imports
#include        <stdio.h>
#include        <stdlib.h>
#include        <termios.h>
#include        <fcntl.h>
#include        <signal.h>
#include        <string.h>
#include        <sys/time.h>
#include        <sys/signal.h>
#include        <sys/ioctl.h>
#include        <unistd.h>

//declarations
char *read_file(FILE *);
void do_more(char *,long);
int see_more(int);
int get_screen_length();
int get_screen_width();
void handle_enter(int);
char *print_line(char *, char *, int);

double c = 700000;            //start time 700000 microseconds (.7 second)
int reply2;
int bool = 0;                // boolean to indicate reached end of the file
//functions
int main (int ac,char *av[]){

        //disable canonical mode and echo bit
        struct termios info;
        tcgetattr(0, &info);
        info.c_lflag &= ~ICANON;
        info.c_lflag &= ~ECHO;
        info.c_cc[VMIN] = 1;
        info.c_cc[VTIME] = 0;
        tcsetattr(0, TCSANOW, &info);

        //install handler
        struct sigaction sa;
        sa.sa_handler = &handle_enter;
        sigaction(SIGALRM, &sa, NULL);


        //deal with args
        FILE *fp;
        if (ac ==1 ){
                read_file(stdin);
        }else {
                while (--ac) {
                        if ( (fp = fopen (*++av, "r")) != NULL) {
                        read_file(fp);
                        fclose(fp);
                        exit(1);
                        }else {
                        printf("Usage: arguments in incorrect format\n");
                        exit(1);
                        }
                }
        }

        //restore settings for canonical mode
        tcgetattr(0, &info);
        info.c_lflag |= ICANON;
        info.c_lflag |= ECHO;
        tcsetattr(0, TCSANOW, &info);

        return 0;
}
/*
 * Takes a pointer of type FILE, checks it's size and allocates enough memory for the buffer. Then copies
 * contents of the file to the buffer and calls do_more() for further action. Returns pointer to allocated
 * memory.
 */
char *read_file (FILE *fp) {


        char *source = NULL;
        FILE *fp_tty;
        fp_tty = fopen("/dev/tty", "r");
        if (fp_tty == NULL) {
        exit(1);
        }


        if (fp != 0) {

                //go to the end
                if (fseek (fp,0L, SEEK_END) == 0) {
                        //get position of file pointer
                        long buff_size = ftell (fp);
                        if (buff_size == -1) {
                        fputs("Error\n",stdout);        //Error
                        }
                        source = malloc (buff_size + sizeof(char));

                        if (fseek (fp, 0L, SEEK_SET) != 0) {
                        fputs("Error\n",stdout);//error
                        }
                        //store contents of a file into a buffer
                        size_t new_len = fread (source, sizeof(char), buff_size, fp);

                        if (ferror(fp) != 0) {
                        fputs("Error reading file", stderr);
                        }else {
                        source[new_len++] = '\0';
                        do_more(source,buff_size);
                        }
                }
        fclose (fp);
        }
        return source;
}
/*
 * Takes pointer to the buffer, listens for user input and behaves acording to it. Calls helper function
 * see_more(int) and acts with respect to it's return value. If 1 exits the process, if 2, starts scroll
 * mode and if pagelen prints the pagelen number of lines. Uses string tokenizer to get lines out of the
 * buffer.
 */
void do_more(char *buff , long buff_size) {

        char *buffer_end = buff + buff_size;
        int linelen = 512;
        int pagelen;
        int pagew;
        if (get_screen_length() != -1){
        pagelen = get_screen_length()-1;
        }

        int reply = pagelen;
        //display the first page
        for (int i = 0; i < pagelen; i++) {
        buff =  print_line(buff, buffer_end,get_screen_width());
        }

        while(reply = see_more(pagelen)) {

                if (buff == buffer_end) {
                bool = 1; //indicates that the end of file is reached
                }         //so that appropriate prompt can be printed

                // pressed q
                if (reply == 1) {
                fputs("\n",stdout);
                break;
                }
                //pressed space
                if (reply == pagelen) {
                        //print pagelen lines
                        for (int i = 0; i < pagelen; i ++){
                        buff = print_line(buff, buffer_end, get_screen_width());
                        }
                }

                //pressed enter
                if (reply == 2 && bool != 1) {
                        //get width
                        if (get_screen_width() != -1) {
                        pagew = get_screen_width();
                        }else {
                        fputs ("Error: page width unavailable\n",stderr);
                        }
                        //instal interval timer
                        struct itimerval timer;
                        timer.it_value.tv_sec = 2;//initial time
                        timer.it_value.tv_usec = 0;
                        timer.it_interval.tv_sec = 0;//repeat interval
                        timer.it_interval.tv_usec = c;
                        setitimer(ITIMER_REAL, &timer, NULL);

                        while (buff != buffer_end) {
                        printf("\033[7m Speed: %8.0f miliseconds\033[0m",c);
                        //listen for potential input and pause to receive a signal
                        reply2 = getchar();
                        pause(); //pause until signal is received
                        printf("\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\033[0K");
                        fflush(stdout);

                        buff = print_line(buff, buffer_end, pagew);

                        timer.it_interval.tv_sec = 0;
                        timer.it_interval.tv_usec = c;
                        setitimer(ITIMER_REAL, &timer, &timer);
                        reply2 = '\0';
                        //if enter is pressed for the second time
                        //reset value of c and get out of the loop
                        if (c == 0) {
                        c = 700000;
                        break;
                        }
                        }//while
                }

        }//while
}
/*
 * Returns number of rows on the screen, or -1 using winsize struct.
 */
int get_screen_length() {
        struct winsize wbuf;
                if (ioctl(0, TIOCGWINSZ, &wbuf) != -1) {
                return wbuf.ws_row;
                }
        return -1;
}
/*
 *Returns number of columns on the screen, or -1 using winsize struct.
 */
int get_screen_width() {
        struct winsize wbuf;
                if (ioctl(0, TIOCGWINSZ, &wbuf) != -1) {
                return wbuf.ws_col;
                }
        return -1;
}
/*
 * Takes integer representing the number of lines in a page program had when it got started. Listens for
 * user input, and returns 1 if 'q',length of the page if space, and 2 if Enter was pressed. Prints
 *  prompt on the terminal and deletes it before exiting.
 */
int see_more (int pagelen) {

        int ch;
        //check if we are at the end of file
        if (bool == 1) {
        printf("\033[7m THE END. \033[m");
        }
        else { //or print regular prompt
        printf("\033[7m --More-- \033[m");
        }

                while (ch = getchar()) {
                        //case quit
                        if (ch == 'q') {
                        printf("\b\b\b\b\b\b\b\b\b\b\b\b\033[K\b");
                        return 1;
                        }//case space
                        if (ch == 32) {
                        //delete old prompt
                        printf("\b\b\b\b\b\b\b\b\b\b\033[0K\b");
                        return pagelen;
                        }//case enter
                        if (ch == '\n') {
                        printf("\b\b\b\b\b\b\b\b\b\b\033[0K");
                        return 2;
                        }
                }//while
        return 0;
}

/*
 * After enter is pressed it adjusts the speed according to the received char.
 * If reply2 == 'f' scrolling speeds up for .2%
 * If reply2 == 's' scrolling slows down for .2%
 * If enter pressed, speed gets set to 0, deactivating the alarm
 */
void handle_enter(int sig) {

        if (reply2 == 'f'){
        if((c - (c * 0.2)) > 0);//speed can't be less than or equal to 0
        c = c - (c * 0.2);
        }
        if (reply2 == 's'){
        c = c + (c * 0.2);
        }
        if (reply2 == '\n') {
        c = 0;
        }


}

/*
 * Takes the position in the buffer and width of the page. Prints the line of
 * pagew characters. Displays char by char, replacing tabs for 8 spaces.
 * Returns position of the buffer.
 */
char *print_line(char *buff, char *buffer_end, int pagew) {

        int chars = 0;
        int ch;

        while (chars < pagew && buff != buffer_end) {
                ch = *buff;
                if (ch == '\t') {
                        if ( (chars+8) >= pagew) {
                        break;
                        } else {
                        for (int i = 0; i < 8; i++ ) {
                        putchar(32);
                        chars ++;
                        }
                        buff ++;
                        }
                }else {

                putchar(ch);
                chars++;
                if ( ch == '\n') {
                buff++;
                break;
                }
                buff++;
                }
        }// while
        if (buff == buffer_end) {
        bool = 1;
        }
        return buff;
}

