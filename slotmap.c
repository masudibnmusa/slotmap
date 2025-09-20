// slotmap.c
// Persistent classroom booking system with booking/cancel history
// Files: users.txt, rooms.txt, bookings.txt (all in text format)

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <conio.h> // getch()
#ifdef _WIN32
#include <windows.h> // for colored output
#endif

// ----------------------------
// 1. Data Structures
// ----------------------------
typedef struct {
    int id;
    char department[20];
    char type[10];        // "lab" or "general" (store lowercase)
    bool schedule[7][24]; // false = available, true = booked
} Classroom;

typedef struct {
    char username[50];
    char password[50];
    bool is_admin;
} User;

typedef struct {
    int room_id;
    int day;              // 0-6
    int hour;             // 0-23
    char username[50];    // who performed the action
    char action;          // 'B' = BOOK, 'C' = CANCEL
} BookingRecord;

// ----------------------------
// 2. Globals & File Paths
// ----------------------------
#define MAX_ROOMS 100
#define MAX_USERS 100

Classroom rooms[MAX_ROOMS];
User users[MAX_USERS];
int room_count = 0;
int user_count = 0;
int current_user_index = -1;

const char *USERS_FILE    = "users.txt";
const char *ROOMS_FILE    = "rooms.txt";
const char *BOOKINGS_FILE = "bookings.txt";

const char *days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

// Function Prototypes
bool parse_ampm_input(const char* input, int* hour24);
void hour_to_ampm(int hour24, char* output);
bool validate_hour(int hour);
void pause_and_clear();
void to_lower_case(char *str);
int  str_casecmp(const char *a, const char *b);
int validate_day(const char *dayStr);
bool validate_room_type(const char *type);
bool validate_room_id(int id);

void user_menu();
void admin_menu();
void register_user();
bool login();
void get_search_input(char *dept, int *day, int *hour, char *type);
void search_classrooms();
int  find_room_by_id(int room_id);
void book_slot();
void cancel_booking();
void add_classroom();
void view_all_bookings();
void my_bookings();

void initialize_sample_data();
void ensure_data_loaded_or_initialized();
bool file_exists(const char *path);

// Text file operations
bool save_users();
bool load_users();
bool save_rooms();
bool load_rooms();
bool append_booking_record_with_action(int room_id, int day, int hour, const char *username, char action);
bool get_last_slot_action(int room_id, int day, int hour, char *out_username, char *out_action, bool *found);

void set_text_color(int color);
void get_password(char *password, size_t maxlen);

// Helper Functions

void pause_and_clear() {
    printf("\n\n\t\t\t\t\tPress any key to continue...");
    getch();
#ifdef _WIN32
    system("CLS");
#else
    system("clear");
#endif
}

void hour_to_ampm(int hour24, char* output) {
    if (hour24 == 0) {
        sprintf(output, "12AM");
    } else if (hour24 < 12) {
        sprintf(output, "%dAM", hour24);
    } else if (hour24 == 12) {
        sprintf(output, "12PM");
    } else {
        sprintf(output, "%dPM", hour24 - 12);
    }
}

bool parse_ampm_input(const char* input, int* hour24) {
    int hour;
    char period[3] = {0};
    int parsed = 0;

    // Try formats: "9AM", "9am"
    if (sscanf(input, "%d%2s%n", &hour, period, &parsed) == 2 && input[parsed] == '\0') {
        // okay
    }
    // Try formats: "9 AM", "9 am"
    else if (sscanf(input, "%d %2s%n", &hour, period, &parsed) == 2 && input[parsed] == '\0') {
        // okay
    }
    else {
        return false; // invalid format
    }

    // Normalize suffix to uppercase
    for (int i = 0; period[i]; i++) {
        period[i] = (char)toupper((unsigned char)period[i]);
    }

    // Must be AM or PM
    if (strcmp(period, "AM") != 0 && strcmp(period, "PM") != 0) {
        return false;
    }

    // Validate hour range
    if (hour < 1 || hour > 12) {
        return false;
    }

    // Convert to 24-hour format
    if (strcmp(period, "AM") == 0) {
        *hour24 = (hour == 12) ? 0 : hour;   // 12AM → 0
    } else { // PM
        *hour24 = (hour == 12) ? 12 : hour + 12;  // 12PM → 12
    }

    return true;
}

void to_lower_case(char *str) {
    for (int i = 0; str[i]; i++) {
        str[i] = (char)tolower((unsigned char)str[i]);
    }
}

int str_casecmp(const char *a, const char *b) {
    char ca[128], cb[128];
    strncpy(ca, a, sizeof(ca)-1);
    strncpy(cb, b, sizeof(cb)-1);
    ca[sizeof(ca)-1] = '\0';
    cb[sizeof(cb)-1] = '\0';
    to_lower_case(ca);
    to_lower_case(cb);
    return strcmp(ca, cb);
}

int validate_day(const char *dayStr) {
    for (int i = 0; i < 7; i++) {
        #ifdef _WIN32
        if (_stricmp(dayStr, days[i]) == 0)
        #else
        if (strcasecmp(dayStr, days[i]) == 0)
        #endif
        {
            return i;
        }
    }
    return -1;
}

bool validate_hour(int hour) {
    bool valid = (hour >= 0 && hour <= 23);
    if (!valid) {
        fprintf(stderr, "System Alert: Invalid hour detected (%d)\n", hour);
    }
    return valid;
}

bool validate_room_type(const char *type) {
    char lower_type[10];
    strncpy(lower_type, type, sizeof(lower_type)-1);
    lower_type[sizeof(lower_type)-1] = '\0';
    to_lower_case(lower_type);
    return strcmp(lower_type, "lab") == 0 || strcmp(lower_type, "general") == 0;
}

bool validate_room_id(int id) {
    if (id < 101 || id > 999) return false;
    int floor = id / 100;
    int room = id % 100;
    return (floor >= 1 && floor <= 9) && (room >= 1 && room <= 99);
}

void set_text_color(int color) {
#ifdef _WIN32
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hConsole, (WORD)color);
#else
    (void)color;
#endif
}

void get_password(char *password, size_t maxlen) {
    size_t i = 0;
    int ch;
    while (1) {
        ch = getch();
        if (ch == 13 || ch == '\n') {
            password[i] = '\0';
            printf("\n");
            break;
        } else if (ch == 8 || ch == 127) {
            if (i > 0) {
                i--;
                printf("\b \b");
            }
        } else if (ch == 0 || ch == 224) {
            (void)getch();
        } else if (i + 1 < maxlen && ch >= 32 && ch <= 126) {
            password[i++] = (char)ch;
            printf("*");
        }
    }
}

// Text File Operations

bool file_exists(const char *path) {
    FILE *fp = fopen(path, "r");
    if (fp) {
        fclose(fp);
        return true;
    }
    return false;
}

bool save_users() {
    FILE *fp = fopen(USERS_FILE, "w");
    if (!fp) return false;

    fprintf(fp, "%d\n", user_count);

    for (int i = 0; i < user_count; i++) {
        fprintf(fp, "%s %s %d\n",
               users[i].username,
               users[i].password,
               users[i].is_admin ? 1 : 0);
    }

    fclose(fp);
    return true;
}

bool load_users() {
    FILE *fp = fopen(USERS_FILE, "r");
    if (!fp) return false;

    if (fscanf(fp, "%d", &user_count) != 1) {
        fclose(fp);
        return false;
    }

    for (int i = 0; i < user_count; i++) {
        int is_admin;
        if (fscanf(fp, "%49s %49s %d",
                  users[i].username,
                  users[i].password,
                  &is_admin) != 3) {
            fclose(fp);
            return false;
        }
        users[i].is_admin = (is_admin == 1);
    }

    fclose(fp);
    return true;
}

bool save_rooms() {
    FILE *fp = fopen(ROOMS_FILE, "w");
    if (!fp) return false;

    fprintf(fp, "%d\n", room_count);

    for (int i = 0; i < room_count; i++) {
        fprintf(fp, "%d %s %s\n",
               rooms[i].id,
               rooms[i].department,
               rooms[i].type);

        for (int d = 0; d < 7; d++) {
            for (int h = 0; h < 24; h++) {
                fprintf(fp, "%d ", rooms[i].schedule[d][h] ? 1 : 0);
            }
            fprintf(fp, "\n");
        }
    }

    fclose(fp);
    return true;
}

bool load_rooms() {
    FILE *fp = fopen(ROOMS_FILE, "r");
    if (!fp) return false;

    if (fscanf(fp, "%d", &room_count) != 1) {
        fclose(fp);
        return false;
    }

    for (int i = 0; i < room_count; i++) {
        if (fscanf(fp, "%d %19s %9s",
                  &rooms[i].id,
                  rooms[i].department,
                  rooms[i].type) != 3) {
            fclose(fp);
            return false;
        }

        for (int d = 0; d < 7; d++) {
            for (int h = 0; h < 24; h++) {
                int booked;
                if (fscanf(fp, "%d", &booked) != 1) {
                    fclose(fp);
                    return false;
                }
                rooms[i].schedule[d][h] = (booked == 1);
            }
        }
    }

    fclose(fp);
    return true;
}

bool append_booking_record_with_action(int room_id, int day, int hour, const char *username, char action) {
    FILE *fp = fopen(BOOKINGS_FILE, "a");
    if (!fp) return false;

    fprintf(fp, "%d %d %d %c %s\n",
           room_id, day, hour, action, username);

    fclose(fp);
    return true;
}

bool get_last_slot_action(int room_id, int day, int hour, char *out_username, char *out_action, bool *found) {
    *found = false;
    FILE *fp = fopen(BOOKINGS_FILE, "r");
    if (!fp) return true;

    char line[256];
    BookingRecord last_match;

    while (fgets(line, sizeof(line), fp)) {
        BookingRecord rec;
        if (sscanf(line, "%d %d %d %c %49s",
                  &rec.room_id,
                  &rec.day,
                  &rec.hour,
                  &rec.action,
                  rec.username) == 5) {
            if (rec.room_id == room_id && rec.day == day && rec.hour == hour) {
                last_match = rec;
                *found = true;
            }
        }
    }
    fclose(fp);

    if (*found) {
        if (out_username) strcpy(out_username, last_match.username);
        if (out_action) *out_action = last_match.action;
    }
    return true;
}

// Core Functions

void initialize_sample_data() {
    memset(users, 0, sizeof(users));
    memset(rooms, 0, sizeof(rooms));

    strcpy(users[0].username, "admin");
    strcpy(users[0].password, "admin123");
    users[0].is_admin = true;

    strcpy(users[1].username, "faculty");
    strcpy(users[1].password, "faculty123");
    users[1].is_admin = false;
    user_count = 2;

    int idx = 0;
    for (int i = 101; i <= 123 && idx < MAX_ROOMS; i++) {
        rooms[idx].id = i;
        strcpy(rooms[idx].department, "CSE");
        strcpy(rooms[idx].type, "lab");
        memset(rooms[idx].schedule, 0, sizeof(rooms[idx].schedule));
        idx++;
    }

    for (int i = 201; i <= 223 && idx < MAX_ROOMS; i++) {
        rooms[idx].id = i;
        strcpy(rooms[idx].department, "EEE");
        strcpy(rooms[idx].type, "general");
        memset(rooms[idx].schedule, 0, sizeof(rooms[idx].schedule));
        idx++;
    }

    for (int i = 301; i <= 323 && idx < MAX_ROOMS; i++) {
        rooms[idx].id = i;
        strcpy(rooms[idx].department, "CSE");
        strcpy(rooms[idx].type, "lab");
        memset(rooms[idx].schedule, 0, sizeof(rooms[idx].schedule));
        idx++;
    }
    room_count = idx;

    FILE *fp = fopen(BOOKINGS_FILE, "a");
    if (fp) fclose(fp);
}

void ensure_data_loaded_or_initialized() {
    bool users_ok = file_exists(USERS_FILE) && load_users();
    bool rooms_ok = file_exists(ROOMS_FILE) && load_rooms();

    if (!users_ok || !rooms_ok) {
        printf("\nInitializing sample data...\n");
        initialize_sample_data();
        if (!save_users()) {
            printf("Warning: Failed to save initial users!\n");
        }
        if (!save_rooms()) {
            printf("Warning: Failed to save initial rooms!\n");
        }
    }

    if (!file_exists(BOOKINGS_FILE)) {
        FILE *fp = fopen(BOOKINGS_FILE, "a");
        if (fp) fclose(fp);
    }
}

void register_user() {
    if (user_count >= MAX_USERS) {
        printf("\t\t\t\t\tMaximum user capacity reached.\n");
        pause_and_clear();
        return;
    }

    char uname[50], pass[50];
    printf("\t\t\t\t\tEnter username: ");
    scanf(" %49s", uname);
    while (getchar() != '\n');

    for (int i = 0; i < user_count; i++) {
        if (strcmp(users[i].username, uname) == 0) {
            printf("\t\t\t\t\tUsername already exists.\n");
            pause_and_clear();
            return;
        }
    }

    printf("\t\t\t\t\tEnter password: ");
    get_password(pass, sizeof(pass));

    strncpy(users[user_count].username, uname, sizeof(users[user_count].username)-1);
    users[user_count].username[sizeof(users[user_count].username)-1] = '\0';
    strncpy(users[user_count].password, pass, sizeof(users[user_count].password)-1);
    users[user_count].password[sizeof(users[user_count].password)-1] = '\0';
    users[user_count].is_admin = false;
    user_count++;

    if (!save_users()) {
        printf("\t\t\t\t\tWarning: failed to save users to file!\n");
    } else {
        printf("\t\t\t\t\tRegistration successful and saved.\n");
    }
    pause_and_clear();
}

bool login() {
    char username[50], password[50];
    printf("\t\t\t\t\tUsername: ");
    scanf(" %49s", username);
    while (getchar() != '\n');

    printf("\t\t\t\t\tPassword: ");
    get_password(password, sizeof(password));

    for (int i = 0; i < user_count; i++) {
        if (strcmp(users[i].username, username) == 0 &&
            strcmp(users[i].password, password) == 0) {
            current_user_index = i;
            printf("\t\t\t\t\tLogin successful. Welcome %s!\n", username);
            pause_and_clear();
            return true;
        }
    }

    printf("\t\t\t\t\tLogin failed. Invalid username or password.\n");
    pause_and_clear();
    return false;
}

void get_search_input(char *dept, int *day, int *hour, char *type) {
    char dayInput[10];
    char timeInput[20];

    while (1) {
        printf("\t\t\t\t\tDepartment (CSE/EEE/...): ");
        if (scanf(" %19s", dept) != 1) {
            while (getchar() != '\n');
            continue;
        }

        printf("\t\t\t\t\tDay (Sun, Mon, Tue, Wed, Thu, Fri, Sat): ");
        if (scanf(" %9s", dayInput) != 1) {
            while (getchar() != '\n');
            continue;
        }

        *day = validate_day(dayInput);
        if (*day == -1) {
            printf("\t\t\t\t\tInvalid day. Please try again.\n");
            continue;
        }

        while (getchar() != '\n'); // clear input buffer

        printf("\t\t\t\t\tTime (e.g., 9AM, 2PM, 12AM): ");
        if (!fgets(timeInput, sizeof(timeInput), stdin)) {
            continue;
        }

        timeInput[strcspn(timeInput, "\n")] = '\0'; // remove newline

        if (!parse_ampm_input(timeInput, hour)) {
            printf("\t\t\t\t\tInvalid time format. Please enter like '9AM' or '2PM'.\n");
            continue;
        }

        printf("\t\t\t\t\tRoom Type (Lab/General): ");
        if (scanf(" %9s", type) != 1) {
            while (getchar() != '\n');
            continue;
        }

        to_lower_case(type);
        if (!validate_room_type(type)) {
            printf("\t\t\t\t\tInvalid room type. Please enter 'Lab' or 'General'.\n");
            continue;
        }

        break; // all inputs valid
    }
}


void search_classrooms() {
    char dept[20], type[10];
    int day, hour;

    get_search_input(dept, &day, &hour, type);

    char time_display[10];
    hour_to_ampm(hour, time_display);

    printf("\n\t\t\t\t\tAvailable rooms in %s department (%s):\n", dept, type);
    printf("\t\t\t\t\tDay: %s, Time: %s\n", days[day], time_display);
    printf("\t\t\t\t\t--------------------------------\n");

    bool found = false;
    for (int i = 0; i < room_count; i++) {
        if (str_casecmp(rooms[i].department, dept) == 0 &&
            str_casecmp(rooms[i].type, type) == 0) {
            int floor = rooms[i].id / 100;
            set_text_color(6);
            printf("\t\t\t\t\tRoom ID: %d (Floor %d) -> ", rooms[i].id, floor);

            if (rooms[i].schedule[day][hour]) {
                set_text_color(12);
                printf("BOOKED\n");
            } else {
                set_text_color(10);
                printf("AVAILABLE\n");
            }
            set_text_color(7);
            found = true;
        }
    }

    if (!found) {
        set_text_color(4);
        printf("\t\t\t\t\tNo rooms found matching criteria.\n");
    }
    pause_and_clear();
}

int find_room_by_id(int room_id) {
    for (int i = 0; i < room_count; i++) {
        if (rooms[i].id == room_id)
            return i;
    }
    return -1;
}

int day_name_to_index(const char *day_name) {
    for (int i = 0; i < 7; i++) {
        if (strcasecmp(day_name, days[i]) == 0)
            return i;
    }
    return -1;
}

void book_slot() {
    if (current_user_index == -1) {
        printf("\t\t\t\t\tYou must be logged in to book a slot.\n");
        pause_and_clear();
        return;
    }

    int room_id, hour;
    char day_str[10];
    char hour_input[20];
    bool valid_input = false;

    // Room ID input with validation
    while (!valid_input) {
        printf("\t\t\t\t\tEnter Room ID (e.g., 101, 202, 303): ");
        if (scanf("%d", &room_id) != 1) {
            while (getchar() != '\n');
            printf("\t\t\t\t\tInvalid input. Please enter a number.\n");
            continue;
        }

        if (!validate_room_id(room_id)) {
            printf("\t\t\t\t\tInvalid Room ID. Must be 3 digits (e.g., 101).\n");
            continue;
        }

        if (find_room_by_id(room_id) == -1) {
            printf("\t\t\t\t\tRoom ID not found. Please try again.\n");
            continue;
        }

        valid_input = true;
    }

    int room_index = find_room_by_id(room_id);
    valid_input = false;

    // Day input with validation
    while (!valid_input) {
        printf("\t\t\t\t\tEnter Day (Sun, Mon, Tue, Wed, Thu, Fri, Sat): ");
        if (scanf("%9s", day_str) != 1) {
            while (getchar() != '\n');
            printf("\t\t\t\t\tInvalid input.\n");
            continue;
        }

        int day = day_name_to_index(day_str);
        if (day == -1) {
            printf("\t\t\t\t\tInvalid day. Please enter one of: ");
            for (int i = 0; i < 7; i++) {
                printf("%s%s", days[i], (i < 6) ? ", " : "\n");
            }
            continue;
        }

        valid_input = true;
    }

    int day = day_name_to_index(day_str);
    valid_input = false;

    // Clear input buffer before time input
    while (getchar() != '\n');

    // Time input with AM/PM validation
    while (!valid_input) {
        printf("\t\t\t\t\tEnter Time (e.g., 9 AM, 2 PM, 12 AM): ");
        fgets(hour_input, sizeof(hour_input), stdin);
        hour_input[strcspn(hour_input, "\n")] = '\0'; // Remove newline

        if (parse_ampm_input(hour_input, &hour) && validate_hour(hour)) {
            char ampm_display[10];
            hour_to_ampm(hour, ampm_display);
            printf("\t\t\t\t\tConfirm booking for Room %d on %s at %s? (Y/N): ",
                  room_id, day_str, ampm_display);

            char confirm = getchar();
            while (getchar() != '\n'); // Clear buffer

            if (toupper(confirm) == 'Y') {
                valid_input = true;
            } else {
                printf("\t\t\t\t\tBooking cancelled.\n");
                pause_and_clear();
                return;
            }
        } else {
            printf("\t\t\t\t\tInvalid time format. Please enter like '9 AM' or '2 PM'.\n");
        }
    }

    // Check availability
    if (rooms[room_index].schedule[day][hour]) {
        char booker[50] = {0};
        char action = 0;
        bool found = false;

        get_last_slot_action(room_id, day, hour, booker, &action, &found);

        if (found && action == 'B') {
            printf("\t\t\t\t\tSlot already booked by %s.\n", booker);
        } else {
            printf("\t\t\t\t\tSlot is already booked.\n");
        }
        pause_and_clear();
        return;
    }

    // Attempt booking
    rooms[room_index].schedule[day][hour] = true;

    // Save room schedule
    if (!save_rooms()) {
        printf("\t\t\t\t\tError: Failed to save room schedule!\n");
        rooms[room_index].schedule[day][hour] = false; // Rollback
        pause_and_clear();
        return;
    }

    // Record booking
    const char *uname = users[current_user_index].username;
    if (!append_booking_record_with_action(room_id, day, hour, uname, 'B')) {
        printf("\t\t\t\t\tWarning: Booking record not saved, but slot is booked!\n");
    }

    int floor = rooms[room_index].id / 100;
    char ampm_display[10];
    hour_to_ampm(hour, ampm_display);

    set_text_color(10); // Green
    printf("\t\t\t\t\tBooking successful!\n");
    printf("\t\t\t\t\tRoom: %d (Floor %d)\n", room_id, floor);
    printf("\t\t\t\t\tDay: %s\n", day_str);
    printf("\t\t\t\t\tTime: %s\n", ampm_display);
    set_text_color(7); // Reset

    pause_and_clear();
}

void cancel_booking() {
    if (current_user_index == -1) {
        printf("\t\t\t\t\tYou must be logged in to cancel a booking.\n");
        pause_and_clear();
        return;
    }

    int room_id, hour;
    char day_str[10];
    char hour_input[20];
    bool valid_input = false;
    bool is_admin = users[current_user_index].is_admin;

    // Room ID input with validation
    while (!valid_input) {
        printf("\t\t\t\t\tEnter Room ID (e.g., 101, 202, 303): ");
        if (scanf("%d", &room_id) != 1) {
            while (getchar() != '\n');
            printf("\t\t\t\t\tInvalid input. Please enter a number.\n");
            continue;
        }

        if (!validate_room_id(room_id)) {
            printf("\t\t\t\t\tInvalid Room ID. Must be 3 digits (e.g., 101).\n");
            continue;
        }

        if (find_room_by_id(room_id) == -1) {
            printf("\t\t\t\t\tRoom ID not found. Please try again.\n");
            continue;
        }

        valid_input = true;
    }

    int room_index = find_room_by_id(room_id);
    valid_input = false;

    // Day input with validation
    while (!valid_input) {
        printf("\t\t\t\t\tEnter Day (Sun, Mon, Tue, Wed, Thu, Fri, Sat): ");
        if (scanf("%9s", day_str) != 1) {
            while (getchar() != '\n');
            printf("\t\t\t\t\tInvalid input.\n");
            continue;
        }

        int day = day_name_to_index(day_str);
        if (day == -1) {
            printf("\t\t\t\t\tInvalid day. Please enter one of: ");
            for (int i = 0; i < 7; i++) {
                printf("%s%s", days[i], (i < 6) ? ", " : "\n");
            }
            continue;
        }

        valid_input = true;
    }

    int day = day_name_to_index(day_str);

    // Clear input buffer before time input
    while (getchar() != '\n');

    // Time input with AM/PM validation
    valid_input = false;
    while (!valid_input) {
        printf("\t\t\t\t\tEnter Time (e.g., 9 AM, 2 PM, 12 AM): ");
        fgets(hour_input, sizeof(hour_input), stdin);
        hour_input[strcspn(hour_input, "\n")] = '\0';

        if (parse_ampm_input(hour_input, &hour) && validate_hour(hour)) {
            char ampm_display[10];
            hour_to_ampm(hour, ampm_display);
            printf("\t\t\t\t\tConfirm cancellation for Room %d on %s at %s? (Y/N): ",
                  room_id, day_str, ampm_display);

            char confirm = getchar();
            while (getchar() != '\n');

            if (toupper(confirm) == 'Y') {
                valid_input = true;
            } else {
                printf("\t\t\t\t\tCancellation aborted by user.\n");
                pause_and_clear();
                return;
            }
        } else {
            printf("\t\t\t\t\tInvalid time format. Please enter like '9 AM' or '2 PM'.\n");
        }
    }

    // Check booking status
    if (!rooms[room_index].schedule[day][hour]) {
        printf("\t\t\t\t\tSlot is not currently booked.\n");
        pause_and_clear();
        return;
    }

    // Check permissions for regular users
    if (!is_admin) {
        char last_user[50] = {0};
        char last_action = 0;
        bool found = false;

        if (!get_last_slot_action(room_id, day, hour, last_user, &last_action, &found) ||
            !found || last_action != 'B' || strcmp(last_user, users[current_user_index].username) != 0) {
            printf("\t\t\t\t\tYou can only cancel your own bookings.\n");
            pause_and_clear();
            return;
        }
    }

    // Perform cancellation
    rooms[room_index].schedule[day][hour] = false;

    if (!save_rooms()) {
        printf("\t\t\t\t\tError: Failed to save changes!\n");
        rooms[room_index].schedule[day][hour] = true; // Rollback
        pause_and_clear();
        return;
    }

    // Log cancellation with current username (admin or regular user)
    if (!append_booking_record_with_action(room_id, day, hour,
                                         users[current_user_index].username, 'C')) {
        printf("\t\t\t\t\tWarning: Cancellation not logged!\n");
    }

    // Success message with AM/PM display
    char ampm_display[10];
    hour_to_ampm(hour, ampm_display);
    set_text_color(10); // Green
    printf("\t\t\t\t\tCancellation successful!\n");
    printf("\t\t\t\t\tRoom: %d\n", room_id);
    printf("\t\t\t\t\tDay: %s\n", day_str);
    printf("\t\t\t\t\tTime: %s\n", ampm_display);

    if (is_admin) {
        printf("\t\t\t\t\t(Admin cancellation performed)\n");
    }

    set_text_color(7); // Reset
    pause_and_clear();
}

void add_classroom() {
    if (current_user_index == -1 || !users[current_user_index].is_admin) {
        printf("\t\t\t\t\tOnly admins can add classrooms.\n");
        pause_and_clear();
        return;
    }

    if (room_count >= MAX_ROOMS) {
        printf("\t\t\t\t\tMaximum room capacity reached.\n");
        pause_and_clear();
        return;
    }

    int id;
    char dept[20], type[10];

    printf("\t\t\t\t\tEnter room ID (3 digits, e.g., 101): ");
    if (scanf("%d", &id) != 1) {
        while (getchar() != '\n');
        printf("\t\t\t\t\tInvalid input.\n");
        pause_and_clear();
        return;
    }

    if (!validate_room_id(id)) {
        printf("\t\t\t\t\tInvalid room ID. Must be 3 digits (e.g., 101).\n");
        pause_and_clear();
        return;
    }

    if (find_room_by_id(id) != -1) {
        printf("\t\t\t\t\tA room with this ID already exists.\n");
        pause_and_clear();
        return;
    }

    printf("\t\t\t\t\tEnter department: ");
    scanf(" %19s", dept);

    while (1) {
        printf("\t\t\t\t\tEnter room type (Lab/General): ");
        scanf(" %9s", type);
        to_lower_case(type);
        if (validate_room_type(type))
            break;
        printf("\t\t\t\t\tInvalid type. Please enter 'Lab' or 'General'.\n");
    }

    rooms[room_count].id = id;
    strncpy(rooms[room_count].department, dept, sizeof(rooms[room_count].department)-1);
    rooms[room_count].department[sizeof(rooms[room_count].department)-1] = '\0';
    strncpy(rooms[room_count].type, type, sizeof(rooms[room_count].type)-1);
    rooms[room_count].type[sizeof(rooms[room_count].type)-1] = '\0';

    for (int d = 0; d < 7; d++)
        for (int h = 0; h < 24; h++)
            rooms[room_count].schedule[d][h] = false;

    room_count++;

    if (!save_rooms()) {
        printf("\t\t\t\t\tWarning: Failed to save rooms to file!\n");
    } else {
        printf("\t\t\t\t\tClassroom added and saved successfully.\n");
    }
    pause_and_clear();
}

const char* day_index_to_name(int day) {
    if (day >= 0 && day < 7) {
        return days[day];
    }
    return "Invalid";
}

void view_all_bookings() {
    set_text_color(14); // Yellow
    printf("\n\t\t\t\t\tAll Classroom Bookings (Current Status)\n");
    printf("\t\t\t\t\t--------------------------------------\n");
    set_text_color(7); // Reset

    bool any_bookings = false;

    for (int i = 0; i < room_count; i++) {
        bool room_has_bookings = false;

        // Print room header
        set_text_color(11); // Cyan
        printf("\n\t\t\t\t\tRoom %d | %s | %s\n",
              rooms[i].id, rooms[i].department, rooms[i].type);
        set_text_color(7); // Reset

        // Check each day and hour
        for (int d = 0; d < 7; d++) {
            for (int h = 0; h < 24; h++) {
                if (rooms[i].schedule[d][h]) {
                    char last_user[50] = {0};
                    char last_action = 0;
                    bool found = false;
                    get_last_slot_action(rooms[i].id, d, h, last_user, &last_action, &found);

                    char time_display[10];
                    hour_to_ampm(h, time_display);

                    if (found && last_action == 'B') {
                        set_text_color(10); // Green
                        printf("\t\t\t\t\t  %s at %s - Booked by %s\n",
                              days[d], time_display, last_user);
                        set_text_color(7); // Reset
                        room_has_bookings = true;
                        any_bookings = true;
                    }
                }
            }
        }

        if (!room_has_bookings) {
            set_text_color(8); // Gray
            printf("\t\t\t\t\t  (No current bookings)\n");
            set_text_color(7); // Reset
        }
    }

    if (!any_bookings) {
        set_text_color(12); // Red
        printf("\n\t\t\t\t\tNo bookings found in any rooms.\n");
        set_text_color(7); // Reset
    }

    // Display booking history
    set_text_color(14); // Yellow
    printf("\n\n\t\t\t\t\tBooking History Log\n");
    printf("\t\t\t\t\t-------------------\n");
    set_text_color(7); // Reset

    FILE *fp = fopen(BOOKINGS_FILE, "r");
    if (fp) {
        char line[256];
        int record_count = 0;

        while (fgets(line, sizeof(line), fp)) {
            BookingRecord rec;
            if (sscanf(line, "%d %d %d %c %49s",
                      &rec.room_id,
                      &rec.day,
                      &rec.hour,
                      &rec.action,
                      rec.username) == 5) {
                char time_display[10];
                hour_to_ampm(rec.hour, time_display);

                if (rec.action == 'B') {
                    set_text_color(10); // Green
                    printf("\t\t\t\t\t[BOOKED] ");
                } else {
                    set_text_color(12); // Red
                    printf("\t\t\t\t\t[CANCELLED] ");
                }

                printf("Room %d | %s at %s | by %s\n",
                      rec.room_id, days[rec.day], time_display, rec.username);
                set_text_color(7); // Reset
                record_count++;
            }
        }
        fclose(fp);

        if (record_count == 0) {
            set_text_color(8); // Gray
            printf("\t\t\t\t\tNo booking history records found.\n");
            set_text_color(7); // Reset
        }
    } else {
        set_text_color(12); // Red
        printf("\t\t\t\t\tCould not open booking history file.\n");
        set_text_color(7); // Reset
    }

    pause_and_clear();
}

void my_bookings() {
    if (current_user_index == -1) {
        printf("\t\t\t\t\tYou must be logged in to view your bookings.\n");
        pause_and_clear();
        return;
    }

    const char *username = users[current_user_index].username;
    bool found_any = false;

    // Display current active bookings
    set_text_color(14); // Yellow
    printf("\n\t\t\t\t\tYour Current Active Bookings (%s)\n", username);
    printf("\t\t\t\t\t-------------------------------\n");
    set_text_color(7); // Reset

    for (int i = 0; i < room_count; i++) {
        for (int d = 0; d < 7; d++) {
            for (int h = 0; h < 24; h++) {
                if (rooms[i].schedule[d][h]) {
                    char last_user[50] = {0};
                    char last_action = 0;
                    bool found = false;
                    get_last_slot_action(rooms[i].id, d, h, last_user, &last_action, &found);

                    if (found && last_action == 'B' && strcmp(last_user, username) == 0) {
                        char time_display[10];
                        hour_to_ampm(h, time_display);

                        set_text_color(11); // Cyan
                        printf("\t\t\t\t\tRoom %d | %s | %s\n",
                              rooms[i].id, days[d], time_display);
                        set_text_color(7); // Reset
                        found_any = true;
                    }
                }
            }
        }
    }

    if (!found_any) {
        set_text_color(8); // Gray
        printf("\t\t\t\t\tNo active bookings found.\n");
        set_text_color(7); // Reset
    }

    // Display complete booking history
    set_text_color(14); // Yellow
    printf("\n\n\t\t\t\t\tYour Complete Booking History\n");
    printf("\t\t\t\t\t---------------------------\n");
    set_text_color(7); // Reset

    FILE *fp = fopen(BOOKINGS_FILE, "r");
    if (fp) {
        found_any = false;
        char line[256];

        // First pass: Show all user's bookings
        while (fgets(line, sizeof(line), fp)) {
            BookingRecord rec;
            if (sscanf(line, "%d %d %d %c %49s",
                      &rec.room_id, &rec.day, &rec.hour, &rec.action, rec.username) == 5) {
                if (strcmp(rec.username, username) == 0) {
                    found_any = true;
                    char time_display[10];
                    hour_to_ampm(rec.hour, time_display);

                    if (rec.action == 'B') {
                        set_text_color(10); // Green
                        printf("\t\t\t\t\t[BOOKED] ");
                    } else {
                        set_text_color(12); // Red
                        printf("\t\t\t\t\t[CANCELLED] ");
                    }

                    printf("Room %d | %s at %s", rec.room_id, days[rec.day], time_display);

                    if (rec.action == 'C') {
                        printf(" (by you)");
                    }
                    printf("\n");
                    set_text_color(7); // Reset
                }
            }
        }

        // Second pass: Show admin cancellations of user's bookings
        rewind(fp);
        while (fgets(line, sizeof(line), fp)) {
            BookingRecord rec;
            if (sscanf(line, "%d %d %d %c %49s",
                      &rec.room_id, &rec.day, &rec.hour, &rec.action, rec.username) == 5) {
                if (rec.action == 'C' && strcmp(rec.username, username) != 0) {
                    // Check if this cancellation affects one of the user's bookings
                    char original_booker[50] = {0};
                    char original_action = 0;
                    bool booking_found = false;
                    get_last_slot_action(rec.room_id, rec.day, rec.hour,
                                       original_booker, &original_action, &booking_found);

                    if (booking_found && original_action == 'B' &&
                        strcmp(original_booker, username) == 0) {
                        found_any = true;
                        char time_display[10];
                        hour_to_ampm(rec.hour, time_display);

                        set_text_color(12); // Red
                        printf("\t\t\t\t\t[CANCELLED] Room %d | %s at %s (by admin)\n",
                              rec.room_id, days[rec.day], time_display);
                        set_text_color(7); // Reset
                    }
                }
            }
        }

        fclose(fp);

        if (!found_any) {
            set_text_color(8); // Gray
            printf("\t\t\t\t\tNo booking history found.\n");
            set_text_color(7); // Reset
        }
    } else {
        set_text_color(12); // Red
        printf("\t\t\t\t\tCould not open booking history file.\n");
        set_text_color(7); // Reset
    }

    pause_and_clear();
}

void user_menu() {
    while (1) {
        set_text_color(1);
        printf("\t\t\t\t\t-----------------------------------------");
        set_text_color(12);
        printf("\n\t\t\t\t\t Slot-Map:Classroom Availability Checker\n");
        set_text_color(1);
        printf("\t\t\t\t\t-----------------------------------------\n");
        set_text_color(6);
        printf("\n\t\t\t\t\t-------:User Menu:-------\n");
        printf("\t\t\t\t\t1. Search Rooms\n");
        printf("\t\t\t\t\t2. Book Slot\n");
        printf("\t\t\t\t\t3. Cancel Booking\n");
        printf("\t\t\t\t\t4. My Bookings\n");
        printf("\t\t\t\t\t5. Logout\n");
        printf("\t\t\t\t\t0. Back to Main Menu\n");
        printf("\t\t\t\t\tEnter your choice: ");

        int choice;
        if (scanf("%d", &choice) != 1) {
            while (getchar() != '\n');
            printf("\t\t\t\t\tInvalid input.\n");
            pause_and_clear();
            continue;
        }

        switch (choice) {
            case 1: search_classrooms();
            break;
            case 2: book_slot();
            break;
            case 3: cancel_booking();
            break;
            case 4: my_bookings();
            break;
            case 5:
                printf("\t\t\t\t\tLogging out...\n");
                current_user_index = -1;
                pause_and_clear();
                return;
            case 0: return;
            default:
                printf("\t\t\t\t\tInvalid option. Please try again.\n");
                pause_and_clear();
        }
    }
}

void admin_menu() {
    while (1) {
        set_text_color(1);
        printf("\t\t\t\t\t-----------------------------------------");
        set_text_color(12);
        printf("\n\t\t\t\t\t Slot-Map:Classroom Availability Checker\n");
        set_text_color(1);
        printf("\t\t\t\t\t-----------------------------------------\n");
        set_text_color(8);
        printf("\n\t\t\t\t\t-------:Admin Menu:-------\n");
        printf("\t\t\t\t\t1. Search Rooms\n");
        printf("\t\t\t\t\t2. Book Slot\n");
        printf("\t\t\t\t\t3. Cancel Booking\n");
        printf("\t\t\t\t\t4. Add Classroom\n");
        printf("\t\t\t\t\t5. View All Bookings\n");
        printf("\t\t\t\t\t6. Logout\n");
        printf("\t\t\t\t\t0. Back to Main Menu\n");
        printf("\t\t\t\t\tEnter your choice: ");

        int choice;
        if (scanf("%d", &choice) != 1) {
            while (getchar() != '\n');
            printf("\t\t\t\t\tInvalid input.\n");
            pause_and_clear();
            continue;
        }

        switch (choice) {
            case 1: search_classrooms();
            break;
            case 2: book_slot();
            break;
            case 3: cancel_booking();
            break;
            case 4: add_classroom();
            break;
            case 5: view_all_bookings();
            break;
            case 6:
                printf("\t\t\t\t\tLogging out...\n");
                current_user_index = -1;
                pause_and_clear();
                return;
            case 0: return;
            default:
                printf("\t\t\t\t\tInvalid option. Please try again.\n");
                pause_and_clear();
        }
    }
}

int main() {
    ensure_data_loaded_or_initialized();

    while (1) {
        set_text_color(5);
        printf("\n");
        printf(
"\t\t\t\t\t  ____  _       _     __  __             \n"
"\t\t\t\t\t / ___|| | ___ | |_  |  \\/  | __ _ _ __ \n"
"\t\t\t\t\t \\___ \\| |/ _ \\| __| | |\\/| |/ _` | '_ \\\n"
"\t\t\t\t\t  ___) | | (_) | |_  | |  | | (_| | |_) |\n"
"\t\t\t\t\t |____/|_|\\___/ \\__| |_|  |_|\\__,_| .__/ \n"
"\t\t\t\t\t                                  |_|    \n\n"
        );
        printf("\t\t\t\t    Classroom Availability Checker And Booking System\n");
        printf("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\t\t\t\t\t\t\t\t\t\t\t\tDeveloped by Masud");
        pause_and_clear();
        set_text_color(1);
        printf("\t\t\t\t\t-----------------------------------------");
        set_text_color(12);
        printf("\n\t\t\t\t\t Slot-Map:Classroom Availability Checker\n");
        set_text_color(1);
        printf("\t\t\t\t\t-----------------------------------------\n");
        set_text_color(6);
        printf("\t\t\t\t\t1. Register\n");
        printf("\t\t\t\t\t2. Login\n");
        printf("\t\t\t\t\t3. Exit\n");
        printf("\t\t\t\t\tEnter your choice: ");

        int choice;
        if (scanf("%d", &choice) != 1) {
            while (getchar() != '\n');
            printf("\t\t\t\t\tInvalid input.\n");
            pause_and_clear();
            continue;
        }

        switch (choice) {
            case 1: register_user(); break;
            case 2:
                if (login()) {
                    if (users[current_user_index].is_admin) {
                        admin_menu();
                    } else {
                        user_menu();
                    }
                }
                break;
            case 3:
                printf("\t\t\t\t\tExiting program...\n");
                exit(0);
            default:
                printf("\t\t\t\t\tInvalid choice. Please try again.\n");
                pause_and_clear();
        }
    }
    return 0;
}
