#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ─────────────────────────────────────────────
   CONSTANTS & LIMITS
───────────────────────────────────────────── */
#define MAX_SKILLS    20
#define MAX_TASKS     50
#define MAX_TESTS     50
#define MAX_STUDENTS  10
#define MAX_BACKLOG   20
#define UNDO_STACK    30
#define NAME_LEN      64
#define DESC_LEN      128
#define DATE_LEN      12

/* ─────────────────────────────────────────────
   STRUCTURES
───────────────────────────────────────────── */
typedef struct {
    char name[NAME_LEN];
    int  progress;       /* 0–100 */
    char level[16];      /* Beginner / Moderate / Strong */
    int  on_resume;      /* 1 if resume-ready (progress >= 70) */
} Skill;

typedef struct {
    char name[NAME_LEN];
    char topic[NAME_LEN];
    char priority[8];    /* High / Medium / Low */
    char deadline[DATE_LEN];
    int  done;
} Task;

typedef struct {
    char topic[NAME_LEN];
    int  score;
    int  max_score;
    char date[DATE_LEN];
} TestScore;

typedef struct {
    char topic[NAME_LEN];
    char reason[32];     /* Skipped / Incomplete / Pending / Too Hard */
} BacklogItem;

typedef struct {
    char name[NAME_LEN];
    char branch[NAME_LEN];
    int  year;
    int  semester;
    char student_id[20];
    char target_role[NAME_LEN];
    char target_company[NAME_LEN];
    int  streak;
    int  placement_score; /* computed */
} Profile;

/* Leaderboard entry */
typedef struct {
    char name[NAME_LEN];
    int  score;
} LBEntry;

/* Badge */
typedef struct {
    char name[NAME_LEN];
    char desc[DESC_LEN];
    int  earned;
} Badge;

/* ─────────────────────────────────────────────
   UNDO STACK  (stores plain-text descriptions)
───────────────────────────────────────────── */
typedef struct {
    char description[DESC_LEN];
    int  type;      /* 1=task toggle, 2=skill update, 3=add task, 4=add skill */
    int  index;
    int  prev_int;  /* previous integer value (progress / done) */
} UndoEntry;

/* ─────────────────────────────────────────────
   GLOBAL STATE
───────────────────────────────────────────── */
Profile   profile;
Skill     skills[MAX_SKILLS];
int       skill_count = 0;
Task      tasks[MAX_TASKS];
int       task_count  = 0;
TestScore tests[MAX_TESTS];
int       test_count  = 0;
BacklogItem backlog[MAX_BACKLOG];
int       backlog_count = 0;

Badge badges[] = {
    {"C Beginner",    "Started C Programming",         1},
    {"C Champion",    "95%+ progress in C",            0},
    {"DSA Starter",   "Started DSA topics",            1},
    {"Streak 7",      "7-day study streak",            1},
    {"Python Basics", "Complete Python intro",         0},
    {"Bug Crusher",   "Debugged 10 programs",          1},
    {"Problem Solver","Solved 50+ problems",           0},
    {"DLD Master",    "80%+ in Digital Logic",         1},
};
int badge_count = 8;

LBEntry leaderboard[] = {
    {"Priya Sharma",     91},
    {"Aditya Kulkarni",  87},
    {"Siddharth Rao",    82},
    {"You",              0 },   /* index 3 — filled at runtime */
    {"Mohammed Rehan",   74},
    {"Sumaiyy Nadaf",    70},
};
int lb_count = 6;

/* Undo stack */
UndoEntry undo_stack[UNDO_STACK];
int undo_top = 0;

/* ─────────────────────────────────────────────
   HELPER UTILITIES
───────────────────────────────────────────── */
void clear_screen(void) {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

void pause_prompt(void) {
    printf("\n  Press Enter to continue...");
    while (getchar() != '\n');
}

void print_line(char c, int n) {
    for (int i = 0; i < n; i++) putchar(c);
    putchar('\n');
}

void print_header(const char *title) {
    printf("\n");
    print_line('=', 58);
    printf("  %-54s\n", title);
    print_line('=', 58);
}

void print_section(const char *title) {
    printf("\n  -- %s --\n", title);
    print_line('-', 42);
}

/* Flush leftover chars from stdin */
void flush_input(void) {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

/* Safe string input */
void read_string(const char *prompt, char *buf, int maxlen) {
    printf("  %s: ", prompt);
    fflush(stdout);
    if (fgets(buf, maxlen, stdin)) {
        buf[strcspn(buf, "\n")] = '\0';
    }
}

int read_int(const char *prompt, int min, int max) {
    int val;
    char line[32];
    while (1) {
        printf("  %s (%d-%d): ", prompt, min, max);
        fflush(stdout);
        if (!fgets(line, sizeof(line), stdin)) continue;
        if (sscanf(line, "%d", &val) == 1 && val >= min && val <= max)
            return val;
        printf("  Invalid input. Try again.\n");
    }
}

/* Compute level string from progress */
void compute_level(Skill *s) {
    if      (s->progress >= 70) { strcpy(s->level, "Strong");   s->on_resume = 1; }
    else if (s->progress >= 40) { strcpy(s->level, "Moderate"); s->on_resume = 0; }
    else                         { strcpy(s->level, "Beginner"); s->on_resume = 0; }
}

/* Current date string */
void today_str(char *buf) {
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    strftime(buf, DATE_LEN, "%Y-%m-%d", tm);
}

/* Compute overall progress */
int overall_progress(void) {
    if (skill_count == 0) return 0;
    int sum = 0;
    for (int i = 0; i < skill_count; i++) sum += skills[i].progress;
    return sum / skill_count;
}

/* Compute placement score */
int compute_placement(void) {
    int dsa_prog = 0, dsa_cnt = 0;
    for (int i = 0; i < skill_count; i++) {
        if (strstr(skills[i].name, "DSA") || strstr(skills[i].name, "dsa")) {
            dsa_prog += skills[i].progress; dsa_cnt++;
        }
    }
    int dsa = dsa_cnt ? dsa_prog / dsa_cnt : overall_progress();
    int tasks_done = 0;
    for (int i = 0; i < task_count; i++) if (tasks[i].done) tasks_done++;
    int task_pct = task_count ? (tasks_done * 100 / task_count) : 0;
    int avg_test = 0;
    for (int i = 0; i < test_count; i++) avg_test += (tests[i].score * 100 / tests[i].max_score);
    if (test_count) avg_test /= test_count;
    return (dsa * 40 + task_pct * 30 + avg_test * 30) / 100;
}

/* Simple bar using ASCII */
void print_bar(int pct, int width) {
    int filled = pct * width / 100;
    putchar('[');
    for (int i = 0; i < width; i++) putchar(i < filled ? '#' : '-');
    printf("] %3d%%", pct);
}

/* ─────────────────────────────────────────────
   UNDO SYSTEM
───────────────────────────────────────────── */
void push_undo(int type, int index, int prev, const char *desc) {
    if (undo_top >= UNDO_STACK) {
        /* shift left */
        memmove(undo_stack, undo_stack + 1, (UNDO_STACK - 1) * sizeof(UndoEntry));
        undo_top = UNDO_STACK - 1;
    }
    undo_stack[undo_top].type     = type;
    undo_stack[undo_top].index    = index;
    undo_stack[undo_top].prev_int = prev;
    strncpy(undo_stack[undo_top].description, desc, DESC_LEN - 1);
    undo_top++;
}

void undo_last(void) {
    if (undo_top == 0) { printf("  Nothing to undo.\n"); return; }
    undo_top--;
    UndoEntry *e = &undo_stack[undo_top];
    printf("  Undoing: %s\n", e->description);
    switch (e->type) {
        case 1: /* task toggle */
            if (e->index < task_count) tasks[e->index].done = e->prev_int;
            break;
        case 2: /* skill progress update */
            if (e->index < skill_count) {
                skills[e->index].progress = e->prev_int;
                compute_level(&skills[e->index]);
            }
            break;
        case 3: /* add task — remove last */
            if (task_count > 0) task_count--;
            break;
        case 4: /* add skill — remove last */
            if (skill_count > 0) skill_count--;
            break;
        default: break;
    }
    printf("  Done!\n");
}

/* ─────────────────────────────────────────────
   FILE HANDLING  (save / load)
───────────────────────────────────────────── */
#define SAVE_FILE "skillforge_data.dat"

void save_data(void) {
    FILE *f = fopen(SAVE_FILE, "wb");
    if (!f) { printf("  Error: could not save data.\n"); return; }
    fwrite(&profile,        sizeof(Profile),     1,             f);
    fwrite(&skill_count,    sizeof(int),         1,             f);
    fwrite(skills,          sizeof(Skill),       skill_count,   f);
    fwrite(&task_count,     sizeof(int),         1,             f);
    fwrite(tasks,           sizeof(Task),        task_count,    f);
    fwrite(&test_count,     sizeof(int),         1,             f);
    fwrite(tests,           sizeof(TestScore),   test_count,    f);
    fwrite(&backlog_count,  sizeof(int),         1,             f);
    fwrite(backlog,         sizeof(BacklogItem), backlog_count, f);
    fwrite(badges,          sizeof(Badge),       badge_count,   f);
    fclose(f);
    printf("  Data saved to '%s'.\n", SAVE_FILE);
}

void load_data(void) {
    FILE *f = fopen(SAVE_FILE, "rb");
    if (!f) return; /* first run — no file yet */
    fread(&profile,        sizeof(Profile),     1,             f);
    fread(&skill_count,    sizeof(int),         1,             f);
    fread(skills,          sizeof(Skill),       skill_count,   f);
    fread(&task_count,     sizeof(int),         1,             f);
    fread(tasks,           sizeof(Task),        task_count,    f);
    fread(&test_count,     sizeof(int),         1,             f);
    fread(tests,           sizeof(TestScore),   test_count,    f);
    fread(&backlog_count,  sizeof(int),         1,             f);
    fread(backlog,         sizeof(BacklogItem), backlog_count, f);
    fread(badges,          sizeof(Badge),       badge_count,   f);
    fclose(f);
}

/* ─────────────────────────────────────────────
   SEED DEFAULT DATA (first run)
───────────────────────────────────────────── */
void seed_defaults(void) {
    /* Profile */
    strcpy(profile.name,           "Ritesh Patil");
    strcpy(profile.branch,         "Computer Science");
    profile.year = 2; profile.semester = 2;
    strcpy(profile.student_id,     "SCFU125126");
    strcpy(profile.target_role,    "Software Engineer");
    strcpy(profile.target_company, "Google / Microsoft");
    profile.streak = 7;

    /* Skills */
    char *snames[] = {"C Programming","DSA Basics","Python","Web Dev (HTML/CSS)",
                      "Digital Logic Design","Mathematics","Applied Chemistry","OOP Concepts"};
    int   sprogs[] = {95, 70, 40, 55, 80, 85, 78, 60};
    skill_count = 8;
    for (int i = 0; i < skill_count; i++) {
        strcpy(skills[i].name, snames[i]);
        skills[i].progress = sprogs[i];
        compute_level(&skills[i]);
    }

    /* Tasks */
    struct { char *n, *t, *p, *d; } td[] = {
        {"Solve 10 Array problems",      "DSA – Arrays",     "High",   "2026-05-22"},
        {"Complete Python loops chapter","Python Basics",    "Medium", "2026-05-23"},
        {"Watch OS lecture",             "OS",               "Low",    "2026-05-25"},
        {"Practice Linked List",         "DSA – LinkedList", "High",   "2026-05-24"},
        {"Build calculator in C",        "C Programming",    "Medium", "2026-05-26"},
    };
    task_count = 5;
    for (int i = 0; i < task_count; i++) {
        strcpy(tasks[i].name,     td[i].n);
        strcpy(tasks[i].topic,    td[i].t);
        strcpy(tasks[i].priority, td[i].p);
        strcpy(tasks[i].deadline, td[i].d);
        tasks[i].done = 0;
    }

    /* Tests */
    struct { char *tp; int sc, mx; char *dt; } ts[] = {
        {"Arrays",        15, 20, "2026-05-15"},
        {"Linked List",    8, 20, "2026-05-16"},
        {"Stacks",        11, 20, "2026-05-17"},
        {"Python Basics", 14, 20, "2026-05-18"},
        {"Digital Logic", 18, 20, "2026-05-19"},
        {"Recursion",      7, 20, "2026-05-12"},
    };
    test_count = 6;
    for (int i = 0; i < test_count; i++) {
        strcpy(tests[i].topic, ts[i].tp);
        tests[i].score = ts[i].sc;
        tests[i].max_score = ts[i].mx;
        strcpy(tests[i].date, ts[i].dt);
    }

    /* Backlog */
    backlog_count = 3;
    strcpy(backlog[0].topic, "Dynamic Programming"); strcpy(backlog[0].reason, "Too Hard");
    strcpy(backlog[1].topic, "Graph Algorithms");    strcpy(backlog[1].reason, "Pending");
    strcpy(backlog[2].topic, "Recursion Advanced");  strcpy(backlog[2].reason, "Incomplete");
}

/* ─────────────────────────────────────────────
   MODULE: PROFILE
───────────────────────────────────────────── */
void view_profile(void) {
    print_header("STUDENT PROFILE");
    printf("  Name          : %s\n",   profile.name);
    printf("  Branch        : %s\n",   profile.branch);
    printf("  Year / Sem    : %d / %d\n", profile.year, profile.semester);
    printf("  Student ID    : %s\n",   profile.student_id);
    printf("  Target Role   : %s\n",   profile.target_role);
    printf("  Target Company: %s\n",   profile.target_company);
    printf("  Streak        : %d days\n", profile.streak);
    int ps = compute_placement();
    printf("  Placement Score: ");
    print_bar(ps, 20);
    printf("\n");
}

void edit_profile(void) {
    print_header("EDIT PROFILE");
    char buf[NAME_LEN];
    read_string("Full Name (Enter to keep)", buf, NAME_LEN);
    if (strlen(buf)) strcpy(profile.name, buf);
    read_string("Target Role  (Enter to keep)", buf, NAME_LEN);
    if (strlen(buf)) strcpy(profile.target_role, buf);
    read_string("Target Company (Enter to keep)", buf, NAME_LEN);
    if (strlen(buf)) strcpy(profile.target_company, buf);
    printf("  Profile updated.\n");
}

void profile_menu(void) {
    int ch;
    do {
        view_profile();
        printf("\n  1. Edit Profile\n  0. Back\n");
        ch = read_int("Choice", 0, 1);
        if (ch == 1) edit_profile();
    } while (ch);
}

/* ─────────────────────────────────────────────
   MODULE: SKILLS
───────────────────────────────────────────── */
void list_skills(void) {
    print_header("SKILLS & TOPICS");
    if (skill_count == 0) { printf("  No skills added yet.\n"); return; }
    printf("  %-3s %-24s %-10s %-22s %s\n","#","Skill","Level","Progress","Resume");
    print_line('-', 58);
    for (int i = 0; i < skill_count; i++) {
        printf("  %-3d %-24s %-10s ", i+1, skills[i].name, skills[i].level);
        print_bar(skills[i].progress, 12);
        printf("  %s\n", skills[i].on_resume ? "[READY]" : "");
    }
}

void add_skill(void) {
    if (skill_count >= MAX_SKILLS) { printf("  Skill list full.\n"); return; }
    Skill s;
    read_string("Skill Name", s.name, NAME_LEN);
    s.progress = read_int("Progress", 0, 100);
    compute_level(&s);
    push_undo(4, skill_count, 0, "Add skill");
    skills[skill_count++] = s;
    printf("  Skill added!\n");
}

void update_skill_progress(void) {
    list_skills();
    int idx = read_int("Skill # to update", 1, skill_count) - 1;
    int prev = skills[idx].progress;
    int newp = read_int("New progress", 0, 100);
    char desc[DESC_LEN];
    snprintf(desc, DESC_LEN, "Update skill '%s' from %d to %d", skills[idx].name, prev, newp);
    push_undo(2, idx, prev, desc);
    skills[idx].progress = newp;
    compute_level(&skills[idx]);
    printf("  Updated!\n");
}

void skills_menu(void) {
    int ch;
    do {
        print_header("SKILLS MENU");
        printf("  1. View All Skills\n");
        printf("  2. Add New Skill\n");
        printf("  3. Update Skill Progress\n");
        printf("  0. Back\n");
        ch = read_int("Choice", 0, 3);
        switch (ch) {
            case 1: list_skills(); pause_prompt(); break;
            case 2: add_skill(); break;
            case 3: if (skill_count) update_skill_progress(); else printf("  No skills yet.\n"); break;
        }
    } while (ch);
}

/* ─────────────────────────────────────────────
   MODULE: TASK MANAGER
───────────────────────────────────────────── */
void list_tasks(int filter) {
    /* filter: 0=all, 1=pending, 2=done */
    print_header("TASK MANAGER");
    printf("  %-3s %-28s %-8s %-10s %s\n","#","Task","Priority","Deadline","Status");
    print_line('-', 58);
    int shown = 0;
    for (int i = 0; i < task_count; i++) {
        if (filter == 1 && tasks[i].done)  continue;
        if (filter == 2 && !tasks[i].done) continue;
        printf("  %-3d %-28s %-8s %-10s %s\n",
               i+1, tasks[i].name, tasks[i].priority, tasks[i].deadline,
               tasks[i].done ? "[DONE]" : "[ ]");
        shown++;
    }
    if (!shown) printf("  (none)\n");
}

void add_task(void) {
    if (task_count >= MAX_TASKS) { printf("  Task list full.\n"); return; }
    Task t;
    read_string("Task Name", t.name, NAME_LEN);
    read_string("Topic/Skill", t.topic, NAME_LEN);
    printf("  Priority — 1.High  2.Medium  3.Low\n");
    int p = read_int("Choice", 1, 3);
    strcpy(t.priority, p==1?"High":p==2?"Medium":"Low");
    read_string("Deadline (YYYY-MM-DD)", t.deadline, DATE_LEN);
    t.done = 0;
    push_undo(3, task_count, 0, "Add task");
    tasks[task_count++] = t;
    printf("  Task added!\n");
}

void toggle_task(void) {
    list_tasks(0);
    int idx = read_int("Task # to toggle", 1, task_count) - 1;
    char desc[DESC_LEN];
    snprintf(desc, DESC_LEN, "Toggle task '%s' (was %s)", tasks[idx].name, tasks[idx].done?"done":"pending");
    push_undo(1, idx, tasks[idx].done, desc);
    tasks[idx].done = !tasks[idx].done;
    printf("  Task marked as %s.\n", tasks[idx].done ? "DONE" : "PENDING");
}

void tasks_menu(void) {
    int ch;
    do {
        print_header("TASK MENU");
        printf("  1. View All Tasks\n");
        printf("  2. View Pending Tasks\n");
        printf("  3. Add New Task\n");
        printf("  4. Toggle Task Done / Pending\n");
        printf("  0. Back\n");
        ch = read_int("Choice", 0, 4);
        switch (ch) {
            case 1: list_tasks(0); pause_prompt(); break;
            case 2: list_tasks(1); pause_prompt(); break;
            case 3: add_task(); break;
            case 4: if (task_count) toggle_task(); else printf("  No tasks yet.\n"); break;
        }
    } while (ch);
}

/* ─────────────────────────────────────────────
   MODULE: TEST SCORES
───────────────────────────────────────────── */
void list_tests(void) {
    print_header("TEST SCORE TRACKER");
    printf("  %-3s %-20s %-6s %-6s %-6s %s\n","#","Topic","Score","Max","Pct","Date");
    print_line('-', 58);
    for (int i = 0; i < test_count; i++) {
        int pct = tests[i].score * 100 / tests[i].max_score;
        char grade = pct >= 75 ? 'A' : pct >= 50 ? 'B' : 'C';
        printf("  %-3d %-20s %-6d %-6d %3d%%  %c   %s\n",
               i+1, tests[i].topic, tests[i].score, tests[i].max_score,
               pct, grade, tests[i].date);
    }
}

void add_test(void) {
    if (test_count >= MAX_TESTS) { printf("  Test list full.\n"); return; }
    TestScore t;
    read_string("Topic", t.topic, NAME_LEN);
    t.score     = read_int("Score", 0, 1000);
    t.max_score = read_int("Max Score", 1, 1000);
    today_str(t.date);
    tests[test_count++] = t;
    printf("  Score saved!\n");
}

void tests_menu(void) {
    int ch;
    do {
        print_header("TEST SCORES");
        printf("  1. View Scores\n");
        printf("  2. Add New Score\n");
        printf("  0. Back\n");
        ch = read_int("Choice", 0, 2);
        switch (ch) {
            case 1: list_tests(); pause_prompt(); break;
            case 2: add_test(); break;
        }
    } while (ch);
}

/* ─────────────────────────────────────────────
   MODULE: WEAK AREA DETECTION
───────────────────────────────────────────── */
void detect_weak_areas(void) {
    print_header("WEAK AREA DETECTION");
    int found = 0;
    for (int i = 0; i < test_count; i++) {
        int pct = tests[i].score * 100 / tests[i].max_score;
        if (pct < 60) {
            printf("  [!] %-20s  Score: %d/%d (%d%%)  -- Needs revision!\n",
                   tests[i].topic, tests[i].score, tests[i].max_score, pct);
            printf("      Tip: Re-study, solve 5 extra problems, re-take mini test.\n\n");
            found++;
        }
    }
    if (!found) printf("  No weak areas detected. Keep it up!\n");
}

/* ─────────────────────────────────────────────
   MODULE: BACKLOG
───────────────────────────────────────────── */
void list_backlog(void) {
    print_header("BACKLOG");
    if (!backlog_count) { printf("  Backlog is empty!\n"); return; }
    printf("  %-3s %-30s %s\n","#","Topic","Reason");
    print_line('-', 48);
    for (int i = 0; i < backlog_count; i++)
        printf("  %-3d %-30s %s\n", i+1, backlog[i].topic, backlog[i].reason);
}

void add_backlog(void) {
    if (backlog_count >= MAX_BACKLOG) { printf("  Backlog full.\n"); return; }
    BacklogItem b;
    read_string("Topic", b.topic, NAME_LEN);
    printf("  Reason — 1.Skipped  2.Incomplete  3.Pending  4.Too Hard\n");
    int r = read_int("Choice", 1, 4);
    strcpy(b.reason, r==1?"Skipped":r==2?"Incomplete":r==3?"Pending":"Too Hard");
    backlog[backlog_count++] = b;
    printf("  Added to backlog.\n");
}

void remove_backlog(void) {
    list_backlog();
    if (!backlog_count) return;
    int idx = read_int("Item # to remove", 1, backlog_count) - 1;
    printf("  Removed: %s\n", backlog[idx].topic);
    for (int i = idx; i < backlog_count - 1; i++) backlog[i] = backlog[i+1];
    backlog_count--;
}

void backlog_menu(void) {
    int ch;
    do {
        print_header("BACKLOG MENU");
        printf("  1. View Backlog\n");
        printf("  2. Add to Backlog\n");
        printf("  3. Remove from Backlog\n");
        printf("  0. Back\n");
        ch = read_int("Choice", 0, 3);
        switch (ch) {
            case 1: list_backlog(); pause_prompt(); break;
            case 2: add_backlog(); break;
            case 3: if (backlog_count) remove_backlog(); break;
        }
    } while (ch);
}

/* ─────────────────────────────────────────────
   MODULE: LEADERBOARD
───────────────────────────────────────────── */
void show_leaderboard(void) {
    print_header("LEADERBOARD");
    leaderboard[3].score = compute_placement();
    /* Simple insertion sort */
    for (int i = 1; i < lb_count; i++) {
        LBEntry key = leaderboard[i];
        int j = i - 1;
        while (j >= 0 && leaderboard[j].score < key.score) {
            leaderboard[j+1] = leaderboard[j]; j--;
        }
        leaderboard[j+1] = key;
    }
    char *medals[] = {"[1st]","[2nd]","[3rd]"};
    for (int i = 0; i < lb_count; i++) {
        int is_you = strcmp(leaderboard[i].name, "You") == 0;
        printf("  %s %-20s  ", i < 3 ? medals[i] : "     ", leaderboard[i].name);
        print_bar(leaderboard[i].score, 15);
        if (is_you) printf("  <- YOU");
        printf("\n");
    }
}

/* ─────────────────────────────────────────────
   MODULE: BADGES
───────────────────────────────────────────── */
void check_auto_badges(void) {
    /* Auto-award based on state */
    for (int i = 0; i < skill_count; i++) {
        if (strcmp(skills[i].name, "C Programming") == 0 && skills[i].progress >= 95)
            badges[1].earned = 1;
        if (strcmp(skills[i].name, "Digital Logic Design") == 0 && skills[i].progress >= 80)
            badges[7].earned = 1;
    }
    if (profile.streak >= 7) badges[3].earned = 1;
    int solved = 0;
    for (int i = 0; i < task_count; i++) if (tasks[i].done) solved++;
    if (solved >= 5) badges[6].earned = 1; /* problem solver proxy */
}

void show_badges(void) {
    check_auto_badges();
    print_header("BADGES & ACHIEVEMENTS");
    int earned = 0;
    for (int i = 0; i < badge_count; i++) {
        printf("  %s %-20s  %s\n",
               badges[i].earned ? "[*]" : "[ ]",
               badges[i].name,
               badges[i].desc);
        if (badges[i].earned) earned++;
    }
    printf("\n  Earned: %d / %d\n", earned, badge_count);
}

/* ─────────────────────────────────────────────
   MODULE: REPORT CARD
───────────────────────────────────────────── */
void generate_report(void) {
    print_header("REPORT CARD");

    print_section("SKILLS SUMMARY");
    for (int i = 0; i < skill_count; i++) {
        printf("  %-24s ", skills[i].name);
        print_bar(skills[i].progress, 14);
        printf("  %s\n", skills[i].on_resume ? "[RESUME OK]" : "");
    }

    print_section("TASK COMPLETION");
    int done = 0, hi = 0;
    for (int i = 0; i < task_count; i++) {
        if (tasks[i].done) done++;
        if (strcmp(tasks[i].priority,"High")==0 && !tasks[i].done) hi++;
    }
    printf("  Completed  : %d / %d tasks\n", done, task_count);
    printf("  High-prio pending: %d\n", hi);

    print_section("TEST PERFORMANCE");
    int avg = 0;
    for (int i = 0; i < test_count; i++)
        avg += tests[i].score * 100 / tests[i].max_score;
    if (test_count) avg /= test_count;
    printf("  Avg Score  : %d%%\n", avg);
    printf("  Weak Topics: ");
    int wf = 0;
    for (int i = 0; i < test_count; i++)
        if (tests[i].score * 100 / tests[i].max_score < 60) {
            printf("%s%s", wf?", ":"", tests[i].topic); wf++;
        }
    if (!wf) printf("None");
    printf("\n");

    print_section("OVERALL");
    printf("  Overall Progress : ");
    print_bar(overall_progress(), 16);
    printf("\n");
    printf("  Placement Score  : ");
    print_bar(compute_placement(), 16);
    printf("\n");
    printf("  Study Streak     : %d days\n", profile.streak);
    printf("  Backlog Items    : %d\n", backlog_count);

    print_section("NEXT STEPS");
    if (backlog_count) printf("  - Clear %d backlog item(s)\n", backlog_count);
    if (hi)            printf("  - Complete %d high-priority task(s)\n", hi);
    if (avg < 60)      printf("  - Re-study weak topics and re-take tests\n");
    printf("  - Keep up the streak!\n");
}

/* ─────────────────────────────────────────────
   DASHBOARD (home screen)
───────────────────────────────────────────── */
void show_dashboard(void) {
    print_header("SKILLFORGE — DASHBOARD");
    printf("  Student  : %s  |  ID: %s  |  %s, Sem %d\n",
           profile.name, profile.student_id, profile.branch, profile.semester);
    printf("  Role Goal: %-30s  Streak: %d days\n", profile.target_role, profile.streak);
    print_line('-', 58);
    printf("  Overall Progress  : "); print_bar(overall_progress(), 18); printf("\n");
    printf("  Placement Score   : "); print_bar(compute_placement(), 18); printf("\n");
    print_line('-', 58);

    /* pending high-priority tasks */
    printf("  Urgent Tasks:\n");
    int shown = 0;
    for (int i = 0; i < task_count && shown < 3; i++) {
        if (!tasks[i].done && strcmp(tasks[i].priority,"High")==0) {
            printf("    [!] %-30s  due %s\n", tasks[i].name, tasks[i].deadline);
            shown++;
        }
    }
    if (!shown) printf("    None! Great job.\n");

    /* Quick weak-area summary */
    printf("  Weak Areas: ");
    int wf = 0;
    for (int i = 0; i < test_count; i++)
        if (tests[i].score * 100 / tests[i].max_score < 60) {
            printf("%s%s", wf?", ":"", tests[i].topic); wf++;
        }
    if (!wf) printf("None detected");
    printf("\n");

    check_auto_badges();
    int earned = 0;
    for (int i = 0; i < badge_count; i++) if (badges[i].earned) earned++;
    printf("  Badges Earned: %d / %d\n", earned, badge_count);
}

/* ─────────────────────────────────────────────
   MAIN MENU
───────────────────────────────────────────── */
void main_menu(void) {
    int ch;
    do {
        clear_screen();
        show_dashboard();
        printf("\n");
        print_line('-', 40);
        printf("  1.  My Profile\n");
        printf("  2.  Skills & Topics\n");
        printf("  3.  Task Manager\n");
        printf("  4.  Test Scores\n");
        printf("  5.  Weak Area Detection\n");
        printf("  6.  Backlog\n");
        printf("  7.  Leaderboard\n");
        printf("  8.  Badges\n");
        printf("  9.  Report Card\n");
        printf("  10. Undo Last Action\n");
        printf("  11. Save & Exit\n");
        print_line('-', 40);
        ch = read_int("Choice", 1, 11);
        clear_screen();
        switch (ch) {
            case 1:  profile_menu();            break;
            case 2:  skills_menu();             break;
            case 3:  tasks_menu();              break;
            case 4:  tests_menu();              break;
            case 5:  detect_weak_areas(); pause_prompt(); break;
            case 6:  backlog_menu();            break;
            case 7:  show_leaderboard(); pause_prompt(); break;
            case 8:  show_badges();      pause_prompt(); break;
            case 9:  generate_report();  pause_prompt(); break;
            case 10: undo_last();        pause_prompt(); break;
            case 11: save_data(); printf("  Goodbye!\n"); return;
        }
    } while (1);
}

/* ─────────────────────────────────────────────
   ENTRY POINT
───────────────────────────────────────────── */
int main(void) {
    load_data();
    /* If no skills were loaded, seed defaults */
    if (skill_count == 0) seed_defaults();
    /* Sync leaderboard "You" entry */
    leaderboard[3].score = compute_placement();
    main_menu();
    return 0;
}
