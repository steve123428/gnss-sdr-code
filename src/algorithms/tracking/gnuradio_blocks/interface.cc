#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <sys/time.h>
#include "interface.h"

#define CMD_LOG_FILE "./cmd_log.txt"
#define SPOOFER_LOG_FILE "./spoofer_log.csv"

WINDOW *dashboard_win;
WINDOW *command_win;
WINDOW *debug_win;

//Create 3 windows to display the dashboard, command input, and input information
void init_window() 
{
    initscr();
    cbreak();
    noecho();
    timeout(0);

    // add color pairs to distinguish between previous commands and new command input
    if (has_colors()) 
    {
        start_color();
        init_pair(1, COLOR_GREEN, COLOR_BLACK); // Color pair for previous commands
        init_pair(2, COLOR_WHITE, COLOR_BLACK); // Default color for new command input
    }

    int width = COLS;
    int start_y = 0;
    int start_x = 0;

    //Height is 30, 10, 10 in that order
    //Adjust as needed
    dashboard_win = newwin(30, width, start_y, start_x);
    command_win = newwin(10, width, start_y + 30, start_x);
    debug_win = newwin(10, width, start_y + 40, start_x);

    scrollok(command_win, TRUE);
    scrollok(debug_win, TRUE);
    nodelay(command_win, TRUE); 
    wclear(dashboard_win);
}
/*
void log_data_to_csv(float left_values[]) 
{
    FILE *log_file = fopen(SPOOFER_LOG_FILE, "a");
    if (log_file == NULL) {
        perror("Failed to open log file");
        return;
    }

    struct timeval tv;
    gettimeofday(&tv, NULL);
    time_t rawtime = tv.tv_sec;
    struct tm *timeinfo = localtime(&rawtime);
    int milliseconds = tv.tv_usec / 1000;

    fprintf(log_file, "%04d-%02d-%02d %02d:%02d:%02d.%03d,",
            timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday,
            timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec, milliseconds);
    fprintf(log_file, "%04d-%02d-%02d %02d:%02d:%02d,%d:%.3f,%f,%f,%f,%f,%f,%f,%f\n",
            (int)left_values[0], (int)left_values[1], (int)left_values[2], 
            (int)left_values[3], (int)left_values[4], (int)left_values[5],
            (int)left_values[6], left_values[7], left_values[8], left_values[9], 
            left_values[10], left_values[11], left_values[12], left_values[13], left_values[14]);

    fclose(log_file);
}
*/

void update_dashboard(uint32_t prn)
{
    mvwprintw(dashboard_win, prn*2, 1, "PRN: %d", prn);
}

//First window
/*
void update_dashboard(const char *left_names[], float left_values[], DataArray chan[], int data_count, const char *right_names[])
{
    wclear(dashboard_win);

    //Area division line
    //In order, top horizontal line, bottom horizontal line, left vertical line, middle vertical line, right vertical line
    int max_y, max_x;
    getmaxyx(dashboard_win, max_y, max_x);
    mvwhline(dashboard_win, 0, 1, ACS_HLINE, max_x-2);
    mvwhline(dashboard_win, 28, 1, ACS_HLINE, max_x-2);
    mvwvline(dashboard_win, 1, 0, ACS_VLINE, 27); 
    mvwvline(dashboard_win, 1, 43, ACS_VLINE, 27); 
    mvwvline(dashboard_win, 1, max_x-1, ACS_VLINE, 27);
    

    //Output the current time in the left area
    update_time();

    //Output values in the left area
    //datetime_sim
    mvwprintw(dashboard_win, 2, 1, "%-10s: %04d-%02d-%02d %02d:%02d:%02d", left_names[0], (int)left_values[0],(int)left_values[1],(int)left_values[2],(int)left_values[3],(int)left_values[4],(int)left_values[5]);
    //gpstime_sim
    mvwprintw(dashboard_win, 3, 1, "%-10s: %d:%.3f", left_names[1], (int)left_values[6], left_values[7]);
    
    //Latitude, Longitude, Height
    mvwprintw(dashboard_win, 5, 1, "%-10s: %f", left_names[2], left_values[8]);
    mvwprintw(dashboard_win, 6, 1, "%-10s: %f", left_names[3], left_values[9]);
    mvwprintw(dashboard_win, 7, 1, "%-10s: %f", left_names[4], left_values[10]);

    //X coordinate, Y coordinate, Z coordinate 
    mvwprintw(dashboard_win, 9, 1, "%-10s: %f", left_names[5], left_values[11]);
    mvwprintw(dashboard_win, 10, 1, "%-10s: %f", left_names[6], left_values[12]);
    mvwprintw(dashboard_win, 11, 1, "%-10s: %f", left_names[7], left_values[13]);

    //gain, distance
    mvwprintw(dashboard_win, 13, 1, "%-10s: %f", left_names[8], left_values[14]);
    mvwprintw(dashboard_win, 14, 1, "%-10s: %f", left_names[9], left_values[15]);
    mvwprintw(dashboard_win, 15, 1, "%-10s: %.12f", left_names[10], left_values[16]);

    //You can add other values ​​here.
    //EX) mvwprintw(dashboard_win, 15, 1, "%-10s: %f", left_names[i], left_values[i]);

    //Output the item on the first line in the right area
    for (int i = 0; i < SAT_INFO_CNT; ++i) {
        mvwprintw(dashboard_win, 1, 65 + (i * 20), "%s", right_names[i]);
    }
    
    //This is only for Debugging
    //Output data in the right area 
    //if the first value is not 0, output the corresponding index and values
    int row = 2;
    for (int i = 0; i < data_count; ++i) {
        if (chan[i].prn != 0) {
            mvwprintw(dashboard_win, row, 45, "%-5d:", chan[i].prn); 
            mvwprintw(dashboard_win, row, 65, "%.3f", chan[i].azel_0);
            mvwprintw(dashboard_win, row, 85, "%.3f", chan[i].azel_1);
            mvwprintw(dashboard_win, row, 105, "%.3f", chan[i].rho_d);
            mvwprintw(dashboard_win, row, 125, "%.3f", chan[i].rho_iono);
            mvwprintw(dashboard_win, row, 145, "%.3f", chan[i].SNR);
            row++;
        }
    }
    wrefresh(dashboard_win);
}*/

void update_time() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    time_t rawtime = tv.tv_sec;
    struct tm *timeinfo = localtime(&rawtime);
    int milliseconds = tv.tv_usec / 1000;

    wattron(dashboard_win, A_BOLD);

    //Set the coordinates representing the current time to (1,1)
    //Output the current time
    mvwprintw(dashboard_win, 1, 1, "Current Time: %04d-%02d-%02d %02d:%02d:%02d.%03d",
              timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday,
              timeinfo->tm_hour, timeinfo->tm_min,
              timeinfo->tm_sec, milliseconds);
    wattroff(dashboard_win, A_BOLD);
    wrefresh(dashboard_win);
}
/*
void print_time_file(FILE *filename)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    time_t rawtime = tv.tv_sec;
    struct tm *timeinfo = localtime(&rawtime);
    int milliseconds = tv.tv_usec / 1000;

    fprintf(filename, "Current Time: %04d-%02d-%02d %02d:%02d:%02d.%03d",
              timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday,
              timeinfo->tm_hour, timeinfo->tm_min,
              timeinfo->tm_sec, milliseconds);
}
*/
/*
// Log "Program Start" and initial time to the log file
void log_program_start() {
    FILE *log_file = fopen(CMD_LOG_FILE, "a");
    if (log_file == NULL) {
        log_file = fopen(CMD_LOG_FILE, "w");
    }
    print_time_file(log_file);

    const char *start_msg = "Program Start!";
    fprintf(log_file, "         ");
    fprintf(log_file, "%s\n", start_msg);
    fclose(log_file);
}
*/
/*
//Second window
//Receive and process commands entered by the user
char* get_command() {
    static char cmd[256];
    int ch;
    static int index = 0;

    static int is_first_command = 1;  // To check if this is the first command
    if (is_first_command) {
        log_program_start();  // Log the program start time and "Program Start" command
        is_first_command = 0;  // Set the flag to false after the first log
    }

    wtimeout(command_win, 0); 

    //Repeatedly process the entered keys
    while ((ch = wgetch(command_win)) != ERR) {
        
        //If the Enter key is pressed
        if (ch == '\n') 
        {
            cmd[index] = '\0';
            index = 0;

            // Move the cursor up one line and clear it
            int current_y = getcury(command_win);
            wmove(command_win, current_y, 0); // Move to the previous line
            wclrtoeol(command_win); // Clear the line

            // Change color for the previous commands
            wattron(command_win, COLOR_PAIR(2)); // Apply color to previous command
            wprintw(command_win, "%s\n", cmd);
            wattroff(command_win, COLOR_PAIR(2)); // Turn off color for future input

            wrefresh(command_win);
            
            //Log commands to a log file
            FILE *log_file = fopen(CMD_LOG_FILE, "a");
            if(log_file == NULL)
            {
                FILE *log_file = fopen(CMD_LOG_FILE, "w");
            }
            print_time_file(log_file);
            fprintf(log_file, "         ");
            fprintf(log_file, "%s\n", cmd);
            fclose(log_file);
            return cmd;
        } 
        
        //If backspace is entered
        else if (ch == KEY_BACKSPACE || ch == 127) {
            if (index > 0) {
                index--;
                mvwdelch(command_win, getcury(command_win), getcurx(command_win) - 1);
            }
        } 
        
        //If the entered command does not exceed the array size
        else if (index < sizeof(cmd) - 1) 
        {
            cmd[index++] = ch;
            // Apply color for typing text
            wattron(command_win, COLOR_PAIR(1)); // Use white color for typing
            waddch(command_win, ch); // Print the character to the screen
            wattroff(command_win, COLOR_PAIR(1)); // Turn off color for future input
        }
    }
    
    return NULL;
}
*/
//Third window
//Output debug message (string)
void debug_message(const char* message) {
    wclear(debug_win);
    mvwprintw(debug_win, 1, 1, "Debug Info: %s", message);
    wrefresh(debug_win);
}

//Output debug message (int)
void debug_message_int(int message) {
    wclear(debug_win);
    mvwprintw(debug_win, 1, 1, "Debug Info: %d", message);
    wrefresh(debug_win);
}

//Output debug message (float)
void debug_message_float(float message) {
    wclear(debug_win);
    mvwprintw(debug_win, 1, 1, "Debug Info: %lf", message);
    wrefresh(debug_win);
}

//Close all windows and exit ncurses mode
void close_windows() {
    delwin(dashboard_win);
    delwin(command_win);
    delwin(debug_win);
    endwin();
}