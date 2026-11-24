/*
 * Project : Gym Management System
 * Language: C (Standard C99)
 * Course  : ICT - Iqra University, M9 Campus
 * Authors : Muhammad Bilal (IU09-0326-0457), Manahil Khan (IU09-0326-0469)
 * Storage : Text file persistence (members.txt, attendance.txt)
 *
 * Description: Console-based application that helps a gym administrator
 *              register members, manage membership plans, record fee
 *              payments and keep a daily attendance log.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#define MAX_MEMBERS     200
#define MEMBER_FILE     "members.txt"
#define ATTENDANCE_FILE "attendance.txt"
#define TEMP_FILE       "attendance.tmp"

/* Membership plan fees (PKR) */
#define FEE_MONTHLY     3000.0f
#define FEE_QUARTERLY   8000.0f
#define FEE_YEARLY      30000.0f

typedef struct {
    int   id;
    char  name[50];
    char  phone[15];
    char  plan[12];        /* Monthly, Quarterly, Yearly */
    float fee;             /* Fee of the chosen plan (PKR) */
    char  payStatus[10];   /* Paid or Unpaid */
    char  joinDate[12];    /* DD-MM-YYYY */
} Member;

Member members[MAX_MEMBERS];
int memberCount = 0;

/* ---------- Function prototypes ---------- */
void loadMembers(void);
void saveMembers(void);
void getToday(char *buffer);
int  readLine(const char *prompt, char *buffer, int size);
int  readInt(const char *prompt, int *value);
void toLowerCase(char *str);
int  findMemberIndexById(int id);
int  isValidPhone(const char *phone);
int  choosePlan(char *plan, float *fee);

void registerMember(void);
void viewAllMembers(void);
void searchMember(void);
void updateMember(void);
void deleteMember(void);

void membershipMenu(void);
void paymentMenu(void);
void attendanceMenu(void);

int  alreadyMarkedToday(int id, const char *date);
void markAttendance(void);
void viewAttendanceHistory(void);
void removeAttendanceOf(int id);

void displayMenu(void);

/* ======================================================= */
int main(void) {
    int choice = 0;
    loadMembers();

    do {
        displayMenu();
        if (!readInt("Enter your choice (1-9): ", &choice)) {
            if (feof(stdin)) {                 /* input closed (Ctrl+D / Ctrl+Z) */
                saveMembers();
                printf("\n\nInput closed. Records saved. Exiting...\n");
                break;
            }
            printf("\n[Error] Invalid input! Please enter a number.\n");
            choice = 0;
            continue;
        }

        switch (choice) {
            case 1: registerMember();  break;
            case 2: viewAllMembers();  break;
            case 3: searchMember();    break;
            case 4: updateMember();    break;
            case 5: deleteMember();    break;
            case 6: membershipMenu();  break;
            case 7: paymentMenu();     break;
            case 8: attendanceMenu();  break;
            case 9:
                saveMembers();
                printf("\nAll records saved to '%s'. Exiting Gym Management System...\n",
                       MEMBER_FILE);
                break;
            default:
                printf("\n[Warning] Please select a valid option between 1 and 9.\n");
        }
    } while (choice != 9);

    return 0;
}

/* ======================================================= */
/*                   Helper functions                      */
/* ======================================================= */

/* Writes today's date into buffer as DD-MM-YYYY (buffer needs 12+ chars) */
void getToday(char *buffer) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(buffer, 12, "%d-%m-%Y", t);
}

/* Discards whatever is left of the current input line */
static void flushLine(void) {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

/* Reads a full line safely. Returns 1 on success, 0 if the text was longer
   than the field allows (the buffer is then left empty). The newline is
   removed and '|' is replaced so it can never break the file format. */
int readLine(const char *prompt, char *buffer, int size) {
    char temp[256];
    printf("%s", prompt);
    if (fgets(temp, sizeof(temp), stdin) == NULL) {
        buffer[0] = '\0';
        return 1;
    }
    if (strchr(temp, '\n') == NULL) flushLine();   /* very long line */
    temp[strcspn(temp, "\n")] = '\0';

    if ((int)strlen(temp) >= size) {
        buffer[0] = '\0';
        printf("[Error] Input too long (maximum %d characters).\n", size - 1);
        return 0;
    }
    for (int i = 0; temp[i]; i++) {
        if (temp[i] == '|') temp[i] = ' ';
    }
    strcpy(buffer, temp);
    return 1;
}

/* Reads an integer from a line. Returns 1 on success, 0 on bad input. */
int readInt(const char *prompt, int *value) {
    char line[64];
    printf("%s", prompt);
    if (fgets(line, sizeof(line), stdin) == NULL) return 0;
    if (strchr(line, '\n') == NULL) flushLine();
    return sscanf(line, "%d", value) == 1;
}

void toLowerCase(char *str) {
    for (int i = 0; str[i]; i++) {
        str[i] = (char)tolower((unsigned char)str[i]);
    }
}

int findMemberIndexById(int id) {
    for (int i = 0; i < memberCount; i++) {
        if (members[i].id == id) return i;
    }
    return -1;
}

/* Phone must be 7-14 characters made of digits, '+' or '-' */
int isValidPhone(const char *phone) {
    int len = (int)strlen(phone);
    if (len < 7 || len > 14) return 0;
    for (int i = 0; i < len; i++) {
        if (!isdigit((unsigned char)phone[i]) && phone[i] != '+' && phone[i] != '-')
            return 0;
    }
    return 1;
}

/* Lets the admin pick a plan; fills plan name and fee. Returns 1 if chosen. */
int choosePlan(char *plan, float *fee) {
    int opt;
    printf("\n  1. Monthly   - PKR %.2f\n", FEE_MONTHLY);
    printf("  2. Quarterly - PKR %.2f\n", FEE_QUARTERLY);
    printf("  3. Yearly    - PKR %.2f\n", FEE_YEARLY);
    if (!readInt("Select plan (1-3): ", &opt)) {
        printf("[Error] Invalid selection.\n");
        return 0;
    }
    if (opt == 1)      { strcpy(plan, "Monthly");   *fee = FEE_MONTHLY; }
    else if (opt == 2) { strcpy(plan, "Quarterly"); *fee = FEE_QUARTERLY; }
    else if (opt == 3) { strcpy(plan, "Yearly");    *fee = FEE_YEARLY; }
    else {
        printf("[Error] Invalid plan option.\n");
        return 0;
    }
    return 1;
}

/* ======================================================= */
/*                    File handling                        */
/* ======================================================= */

void loadMembers(void) {
    FILE *fp = fopen(MEMBER_FILE, "r");
    if (fp == NULL) {
        /* First run: create a few sample members */
        Member samples[3] = {
            {1001, "Ali Raza",    "03001234567", "Monthly",   FEE_MONTHLY,   "Paid",   "01-11-2026"},
            {1002, "Sara Khan",   "03211234567", "Quarterly", FEE_QUARTERLY, "Unpaid", "05-11-2026"},
            {1003, "Hamza Ahmed", "03331234567", "Yearly",    FEE_YEARLY,    "Paid",   "10-11-2026"}
        };
        for (int i = 0; i < 3; i++) {
            members[memberCount++] = samples[i];
        }
        saveMembers();
        return;
    }

    memberCount = 0;
    while (memberCount < MAX_MEMBERS &&
           fscanf(fp, "%d|%49[^|]|%14[^|]|%11[^|]|%f|%9[^|]|%11[^\n]\n",
                  &members[memberCount].id,
                  members[memberCount].name,
                  members[memberCount].phone,
                  members[memberCount].plan,
                  &members[memberCount].fee,
                  members[memberCount].payStatus,
                  members[memberCount].joinDate) == 7) {
        memberCount++;
    }
    fclose(fp);
}

void saveMembers(void) {
    FILE *fp = fopen(MEMBER_FILE, "w");
    if (fp == NULL) {
        printf("\n[Error] Unable to open '%s' for saving!\n", MEMBER_FILE);
        return;
    }
    for (int i = 0; i < memberCount; i++) {
        fprintf(fp, "%d|%s|%s|%s|%.2f|%s|%s\n",
                members[i].id, members[i].name, members[i].phone,
                members[i].plan, members[i].fee,
                members[i].payStatus, members[i].joinDate);
    }
    fclose(fp);
}

/* ======================================================= */
/*                      Main menu                          */
/* ======================================================= */

void displayMenu(void) {
    printf("\n============================================================\n");
    printf("            GYM MANAGEMENT SYSTEM - ADMIN PANEL             \n");
    printf("============================================================\n");
    printf("  1. Register New Member\n");
    printf("  2. View All Members\n");
    printf("  3. Search Member (by ID or Name)\n");
    printf("  4. Update Member Details\n");
    printf("  5. Delete Member\n");
    printf("  6. Membership Plans\n");
    printf("  7. Payment Management\n");
    printf("  8. Attendance\n");
    printf("  9. Save & Exit\n");
    printf("------------------------------------------------------------\n");
}

/* ======================================================= */
/*              Member registration & records              */
/* ======================================================= */

void registerMember(void) {
    if (memberCount >= MAX_MEMBERS) {
        printf("\n[Error] Gym is full (%d members max).\n", MAX_MEMBERS);
        return;
    }

    Member m;
    printf("\n--- Register New Member ---\n");
    if (!readInt("Enter Member ID (e.g., 1004): ", &m.id) || m.id <= 0) {
        printf("[Error] Invalid Member ID.\n");
        return;
    }
    if (findMemberIndexById(m.id) != -1) {
        printf("[Error] Member ID %d already exists!\n", m.id);
        return;
    }

    if (!readLine("Enter Full Name: ", m.name, sizeof(m.name))) return;
    if (strlen(m.name) == 0) {
        printf("[Error] Name cannot be empty.\n");
        return;
    }

    if (!readLine("Enter Phone Number: ", m.phone, sizeof(m.phone))) return;
    if (!isValidPhone(m.phone)) {
        printf("[Error] Phone must be 7-14 characters (digits, + or -).\n");
        return;
    }

    if (!choosePlan(m.plan, &m.fee)) return;

    strcpy(m.payStatus, "Unpaid");
    getToday(m.joinDate);

    members[memberCount++] = m;
    saveMembers();
    printf("[Success] Member '%s' registered with %s plan (PKR %.2f, Unpaid).\n",
           m.name, m.plan, m.fee);
}

void viewAllMembers(void) {
    if (memberCount == 0) {
        printf("\n[Info] No members registered yet.\n");
        return;
    }

    printf("\n%-6s | %-20s | %-14s | %-10s | %-10s | %-8s | %-10s\n",
           "ID", "Name", "Phone", "Plan", "Fee (PKR)", "Payment", "Joined");
    printf("-------------------------------------------------------------------------------------------\n");
    for (int i = 0; i < memberCount; i++) {
        printf("%-6d | %-20s | %-14s | %-10s | %-10.2f | %-8s | %-10s\n",
               members[i].id, members[i].name, members[i].phone,
               members[i].plan, members[i].fee,
               members[i].payStatus, members[i].joinDate);
    }
    printf("Total members: %d\n", memberCount);
}

static void printMember(int i) {
    printf("  ID: %d | %s | %s | %s plan (PKR %.2f) | %s | Joined: %s\n",
           members[i].id, members[i].name, members[i].phone,
           members[i].plan, members[i].fee,
           members[i].payStatus, members[i].joinDate);
}

void searchMember(void) {
    int mode;
    if (!readInt("\nSearch by: 1) Member ID   2) Name : ", &mode)) {
        printf("[Error] Invalid choice.\n");
        return;
    }

    if (mode == 1) {
        int id;
        if (!readInt("Enter Member ID: ", &id)) {
            printf("[Error] Invalid ID.\n");
            return;
        }
        int idx = findMemberIndexById(id);
        if (idx == -1) {
            printf("[Info] No member found with ID %d.\n", id);
        } else {
            printf("\nFound:\n");
            printMember(idx);
        }
    } else if (mode == 2) {
        char query[50], tempName[50];
        if (!readLine("Enter name (or part of it): ", query, sizeof(query))) return;
        if (strlen(query) == 0) {
            printf("[Error] Search text cannot be empty.\n");
            return;
        }
        toLowerCase(query);

        int found = 0;
        for (int i = 0; i < memberCount; i++) {
            strcpy(tempName, members[i].name);
            toLowerCase(tempName);
            if (strstr(tempName, query) != NULL) {
                if (!found) printf("\nMatching Members:\n");
                printMember(i);
                found++;
            }
        }
        if (!found) printf("[Info] No member matched '%s'.\n", query);
    } else {
        printf("[Error] Invalid search option.\n");
    }
}

void updateMember(void) {
    int id, opt;
    if (!readInt("\nEnter Member ID to update: ", &id)) {
        printf("[Error] Invalid ID.\n");
        return;
    }
    int idx = findMemberIndexById(id);
    if (idx == -1) {
        printf("[Error] Member ID %d not found.\n", id);
        return;
    }

    printf("Current record:\n");
    printMember(idx);
    if (!readInt("Update: 1) Name   2) Phone : ", &opt)) {
        printf("[Error] Invalid selection.\n");
        return;
    }

    if (opt == 1) {
        char name[50];
        if (!readLine("Enter new name: ", name, sizeof(name))) return;
        if (strlen(name) == 0) {
            printf("[Error] Name cannot be empty.\n");
            return;
        }
        strcpy(members[idx].name, name);
    } else if (opt == 2) {
        char phone[15];
        if (!readLine("Enter new phone: ", phone, sizeof(phone))) return;
        if (!isValidPhone(phone)) {
            printf("[Error] Phone must be 7-14 characters (digits, + or -).\n");
            return;
        }
        strcpy(members[idx].phone, phone);
    } else {
        printf("[Error] Invalid option.\n");
        return;
    }

    saveMembers();
    printf("[Success] Member record updated and saved.\n");
}

void deleteMember(void) {
    int id;
    if (!readInt("\nEnter Member ID to delete: ", &id)) {
        printf("[Error] Invalid ID.\n");
        return;
    }
    int idx = findMemberIndexById(id);
    if (idx == -1) {
        printf("[Error] Member ID %d not found.\n", id);
        return;
    }

    char answer[8];
    printf("Delete '%s' (ID %d)? ", members[idx].name, id);
    readLine("Type y to confirm: ", answer, sizeof(answer));
    if (tolower((unsigned char)answer[0]) != 'y') {
        printf("[Info] Deletion cancelled.\n");
        return;
    }

    char deletedName[50];
    strcpy(deletedName, members[idx].name);
    for (int i = idx; i < memberCount - 1; i++) {
        members[i] = members[i + 1];
    }
    memberCount--;
    saveMembers();
    removeAttendanceOf(id);
    printf("[Success] Member '%s' (ID %d) removed.\n", deletedName, id);
}

/* ======================================================= */
/*                 Membership management                   */
/* ======================================================= */

void membershipMenu(void) {
    int opt;
    printf("\n--- Membership Plans ---\n");
    printf("  1. View plans and fees\n");
    printf("  2. Change a member's plan\n");
    if (!readInt("Choose (1-2): ", &opt)) {
        printf("[Error] Invalid selection.\n");
        return;
    }

    if (opt == 1) {
        printf("\n%-12s | %-12s\n", "Plan", "Fee (PKR)");
        printf("---------------------------\n");
        printf("%-12s | %-12.2f\n", "Monthly",   FEE_MONTHLY);
        printf("%-12s | %-12.2f\n", "Quarterly", FEE_QUARTERLY);
        printf("%-12s | %-12.2f\n", "Yearly",    FEE_YEARLY);
    } else if (opt == 2) {
        int id;
        if (!readInt("Enter Member ID: ", &id)) {
            printf("[Error] Invalid ID.\n");
            return;
        }
        int idx = findMemberIndexById(id);
        if (idx == -1) {
            printf("[Error] Member ID %d not found.\n", id);
            return;
        }
        printf("%s currently has the %s plan.\n", members[idx].name, members[idx].plan);

        char plan[12];
        float fee;
        if (!choosePlan(plan, &fee)) return;

        strcpy(members[idx].plan, plan);
        members[idx].fee = fee;
        strcpy(members[idx].payStatus, "Unpaid");  /* new plan = new fee due */
        saveMembers();
        printf("[Success] %s moved to %s plan (PKR %.2f). Payment marked Unpaid.\n",
               members[idx].name, plan, fee);
    } else {
        printf("[Error] Invalid option.\n");
    }
}

/* ======================================================= */
/*                  Payment management                     */
/* ======================================================= */

void paymentMenu(void) {
    int opt;
    printf("\n--- Payment Management ---\n");
    printf("  1. Record a payment (mark Paid)\n");
    printf("  2. Start new billing cycle (mark Unpaid)\n");
    printf("  3. List unpaid members\n");
    printf("  4. Payment summary\n");
    if (!readInt("Choose (1-4): ", &opt)) {
        printf("[Error] Invalid selection.\n");
        return;
    }

    if (opt == 1 || opt == 2) {
        int id;
        if (!readInt("Enter Member ID: ", &id)) {
            printf("[Error] Invalid ID.\n");
            return;
        }
        int idx = findMemberIndexById(id);
        if (idx == -1) {
            printf("[Error] Member ID %d not found.\n", id);
            return;
        }

        const char *target = (opt == 1) ? "Paid" : "Unpaid";
        if (strcmp(members[idx].payStatus, target) == 0) {
            printf("[Info] %s is already marked %s.\n", members[idx].name, target);
            return;
        }
        strcpy(members[idx].payStatus, target);
        saveMembers();
        if (opt == 1)
            printf("[Success] Received PKR %.2f from %s. Marked Paid.\n",
                   members[idx].fee, members[idx].name);
        else
            printf("[Success] %s marked Unpaid for the new billing cycle (PKR %.2f due).\n",
                   members[idx].name, members[idx].fee);
    } else if (opt == 3) {
        int count = 0;
        float pending = 0;
        for (int i = 0; i < memberCount; i++) {
            if (strcmp(members[i].payStatus, "Unpaid") == 0) {
                if (!count) {
                    printf("\n%-6s | %-20s | %-10s | %-10s\n", "ID", "Name", "Plan", "Due (PKR)");
                    printf("--------------------------------------------------------\n");
                }
                printf("%-6d | %-20s | %-10s | %-10.2f\n",
                       members[i].id, members[i].name, members[i].plan, members[i].fee);
                pending += members[i].fee;
                count++;
            }
        }
        if (!count) printf("[Info] All members have paid.\n");
        else printf("Unpaid members: %d | Total pending: PKR %.2f\n", count, pending);
    } else if (opt == 4) {
        int paidCount = 0, unpaidCount = 0;
        float collected = 0, pending = 0;
        for (int i = 0; i < memberCount; i++) {
            if (strcmp(members[i].payStatus, "Paid") == 0) {
                paidCount++;
                collected += members[i].fee;
            } else {
                unpaidCount++;
                pending += members[i].fee;
            }
        }
        printf("\n========== PAYMENT SUMMARY ==========\n");
        printf("  Paid members     : %d\n", paidCount);
        printf("  Unpaid members   : %d\n", unpaidCount);
        printf("  Fees collected   : PKR %.2f\n", collected);
        printf("  Fees pending     : PKR %.2f\n", pending);
        printf("=====================================\n");
    } else {
        printf("[Error] Invalid option.\n");
    }
}

/* ======================================================= */
/*                      Attendance                         */
/* ======================================================= */

void attendanceMenu(void) {
    int opt;
    printf("\n--- Attendance ---\n");
    printf("  1. Mark today's attendance\n");
    printf("  2. View a member's attendance history\n");
    if (!readInt("Choose (1-2): ", &opt)) {
        printf("[Error] Invalid selection.\n");
        return;
    }
    if (opt == 1)      markAttendance();
    else if (opt == 2) viewAttendanceHistory();
    else               printf("[Error] Invalid option.\n");
}

/* Returns 1 if the member already has an entry for the given date */
int alreadyMarkedToday(int id, const char *date) {
    FILE *fp = fopen(ATTENDANCE_FILE, "r");
    if (fp == NULL) return 0;

    int fileId;
    char fileDate[12];
    while (fscanf(fp, "%d|%11[^\n]\n", &fileId, fileDate) == 2) {
        if (fileId == id && strcmp(fileDate, date) == 0) {
            fclose(fp);
            return 1;
        }
    }
    fclose(fp);
    return 0;
}

void markAttendance(void) {
    int id;
    if (!readInt("Enter Member ID: ", &id)) {
        printf("[Error] Invalid ID.\n");
        return;
    }
    int idx = findMemberIndexById(id);
    if (idx == -1) {
        printf("[Error] Member ID %d not found.\n", id);
        return;
    }

    char today[12];
    getToday(today);
    if (alreadyMarkedToday(id, today)) {
        printf("[Info] %s is already marked present for %s.\n", members[idx].name, today);
        return;
    }

    FILE *fp = fopen(ATTENDANCE_FILE, "a");
    if (fp == NULL) {
        printf("[Error] Unable to open '%s'.\n", ATTENDANCE_FILE);
        return;
    }
    fprintf(fp, "%d|%s\n", id, today);
    fclose(fp);

    printf("[Success] %s marked present on %s.", members[idx].name, today);
    if (strcmp(members[idx].payStatus, "Unpaid") == 0)
        printf(" (Reminder: fee is unpaid)");
    printf("\n");
}

void viewAttendanceHistory(void) {
    int id;
    if (!readInt("Enter Member ID: ", &id)) {
        printf("[Error] Invalid ID.\n");
        return;
    }
    int idx = findMemberIndexById(id);
    if (idx == -1) {
        printf("[Error] Member ID %d not found.\n", id);
        return;
    }

    FILE *fp = fopen(ATTENDANCE_FILE, "r");
    int count = 0;
    if (fp != NULL) {
        int fileId;
        char fileDate[12];
        while (fscanf(fp, "%d|%11[^\n]\n", &fileId, fileDate) == 2) {
            if (fileId == id) {
                if (!count) printf("\nAttendance of %s (ID %d):\n", members[idx].name, id);
                printf("  %d. %s\n", count + 1, fileDate);
                count++;
            }
        }
        fclose(fp);
    }
    if (!count) printf("[Info] No attendance recorded for %s yet.\n", members[idx].name);
    else        printf("Total days present: %d\n", count);
}

/* Removes all attendance lines of a deleted member */
void removeAttendanceOf(int id) {
    FILE *in = fopen(ATTENDANCE_FILE, "r");
    if (in == NULL) return;
    FILE *out = fopen(TEMP_FILE, "w");
    if (out == NULL) {
        fclose(in);
        return;
    }

    int fileId;
    char fileDate[12];
    while (fscanf(in, "%d|%11[^\n]\n", &fileId, fileDate) == 2) {
        if (fileId != id) fprintf(out, "%d|%s\n", fileId, fileDate);
    }
    fclose(in);
    fclose(out);
    remove(ATTENDANCE_FILE);
    rename(TEMP_FILE, ATTENDANCE_FILE);
}
