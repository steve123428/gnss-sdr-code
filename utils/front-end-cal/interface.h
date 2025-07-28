#ifndef INTERFACE_H
#define INTERFACE_H

//Number of items in the left and right area
#define GEN_INFO_CNT 11
#define SAT_INFO_CNT 5

typedef struct {
    int prn;
    double azel_0;
    double azel_1;
    double rho_d;
    double rho_iono;
    double SNR;
} DataArray;

void init_window();
void update_dashboard(const char *left_names[], float left_values[], DataArray data[], int data_count, const char *right_names[]);
void update_time();
char* get_command();
void close_windows();
void debug_message(const char* message);
void debug_message_double(double message);

#endif //INTERFACE_H
