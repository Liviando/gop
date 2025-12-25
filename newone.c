#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

// ANSI color codes
#define RESET   "\033[0m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN    "\033[36m"
#define BOLD    "\033[1m"

#define ROWS 5
#define MAX_DIGITS 4
#define MAX_TURNS 20
#define SAVE_FILE "savegame.txt"

#ifdef _WIN32
    #include <windows.h>
    
    #ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
        #define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
    #endif
    static void enable_ansi_colors(void) {
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut == INVALID_HANDLE_VALUE) return;
        DWORD dwMode = 0;
        if (!GetConsoleMode(hOut, &dwMode)) return;
        dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        SetConsoleMode(hOut, dwMode);
    }
#else
    static void enable_ansi_colors(void) { /* noop on Linux/macOS */ }
#endif


/* Game state */
struct GameState {
    int num_digits;
    int total_turns;
    int turns_used;            /* how many turns have been played (for resume & save) */
    int secret[MAX_DIGITS];
    int guesses[MAX_TURNS][MAX_DIGITS];
};

struct GuessRecord {
    int turn_index;           // turn ke berapa (0-based)
    int guess[MAX_DIGITS];    // isi tebakan
    int correct_digit;        // total angka benar
    int correct_position;     // angka di posisi benar
};


/* ASCII art patterns */
const char *patterns[10][ROWS] = {
    { " ***** ", " *   * ", " *   * ", " *   * ", " ***** " },
    { "  **   ", " ***   ", "  **   ", "  **   ", " ***** " },
    { " ***** ", "     * ", " ***** ", " *     ", " ***** " },
    { " ***** ", "     * ", " ***** ", "     * ", " ***** " },
    { " *   * ", " *   * ", " ***** ", "     * ", "     * " },
    { " ***** ", " *     ", " ***** ", "     * ", " ***** " },
    { " ***** ", " *     ", " ***** ", " *   * ", " ***** " },
    { " ***** ", "     * ", "    *  ", "   *   ", "  *    " },
    { " ***** ", " *   * ", " ***** ", " *   * ", " ***** " },
    { " ***** ", " *   * ", " ***** ", "     * ", " ***** " }
};

/* ASCII art letters for title */
const char *letter_G[ROWS] = { " ***** ", " *     ", " *  ** ", " *   * ", " ***** " };
const char *letter_O[ROWS] = { " ***** ", " *   * ", " *   * ", " *   * ", " ***** " };
const char *letter_P[ROWS] = { " ***** ", " *   * ", " ***** ", " *     ", " *     " };
const char *dot[ROWS]      = { "   ", "   ", " * ", "   ", "   " };


void enter(){
	for(int i = 0; i < 50; i++){
		printf("\n");
	}
}

void display_large(const int digits[], int num_digits) {
    for (int row = 0; row < ROWS; ++row) {
        for (int d = 0; d < num_digits; ++d) {
            printf("%s ", patterns[digits[d]][row]);
        }
        printf("\n");
    }
    printf("\n");
}

void display_title(void) {
    printf(BOLD YELLOW);
    for (int row = 0; row < ROWS; ++row) {
        printf("%s  ", letter_G[row]);
        printf("%s  ", dot[row]);
        printf("%s  ", letter_O[row]);
        printf("%s  ", dot[row]);
        printf("%s  ", letter_P[row]);
        printf("\n");
    }
    printf(RESET "\n");
    printf(BOLD "        GUESS THE NUMBER GAME\n\n" RESET);
}

/* Save game: saves digits count, secret, total_turns, turns_used */
int save_game(const struct GameState *gs) {
    FILE *fp = fopen(SAVE_FILE, "w");
    if (!fp) {
        printf("Error: Cannot save game.\n");
        return 0;
    }

    // Header: num_digits, secret, total_turns, turns_used
    fprintf(fp, "%d\n", gs->num_digits);
    for (int i = 0; i < gs->num_digits; ++i) {
        fprintf(fp, "%d", gs->secret[i]);
    }
    fprintf(fp, "\n%d\n%d\n", gs->total_turns, gs->turns_used);

    // Simpan semua tebakan yang sudah dilakukan
    for (int t = 0; t < gs->turns_used; t++) {
        for (int d = 0; d < gs->num_digits; d++) {
            fprintf(fp, "%d", gs->guesses[t][d]);
        }
        fprintf(fp, "\n");
    }

    fclose(fp);
    printf("Game progress saved.\n");
    return 1;
}

/* Load game */
int load_game(struct GameState *gs) {
    FILE *fp = fopen(SAVE_FILE, "r");
    if (!fp) {
        printf("No save file found.\n");
        return 0;
    }

    // Baca header
    if (fscanf(fp, "%d", &gs->num_digits) != 1 || gs->num_digits != 4) {
        fclose(fp);
        printf("Invalid or incompatible save (digits must be 4).\n");
        return 0;
    }

    for (int i = 0; i < gs->num_digits; ++i) {
        if (fscanf(fp, "%1d", &gs->secret[i]) != 1) {
            fclose(fp);
            printf("Error reading secret from save.\n");
            return 0;
        }
    }

    if (fscanf(fp, "%d%d", &gs->total_turns, &gs->turns_used) != 2) {
        fclose(fp);
        printf("Error reading turns from save.\n");
        return 0;
    }

    // Reset guesses
    for (int t = 0; t < MAX_TURNS; ++t)
        for (int d = 0; d < MAX_DIGITS; ++d)
            gs->guesses[t][d] = 0;

    // Baca tebakan yang sudah dilakukan
    char line[10];
    for (int t = 0; t < gs->turns_used; t++) {
        if (fscanf(fp, "%4s", line) != 1) {
            fclose(fp);
            printf("Error reading guess %d from save.\n", t+1);
            return 0;
        }
        for (int d = 0; d < 4; d++) {
            gs->guesses[t][d] = line[d] - '0';
        }
    }

    fclose(fp);
    printf("Game loaded. Resuming from turn %d of %d.\n", gs->turns_used + 1, gs->total_turns);
    return 1;
}


/* New game setup — force 4 digits to preserve Code 2 logic */
void new_game(struct GameState *gs) {
    int requested_turns;
    gs->num_digits = 4; /* Code 2 uses fixed loops with 4 */

    printf("Number of turns (10-20): ");
    if (scanf("%d", &requested_turns) != 1 || requested_turns < 10 || requested_turns > 20) {
        gs->total_turns = 15;
    } else {
        gs->total_turns = requested_turns;
    }

    gs->turns_used = 0;
    srand((unsigned)time(NULL));
    for (int i = 0; i < gs->num_digits; ++i) gs->secret[i] = rand() % 10;

    /* clear guesses */
    for (int t = 0; t < MAX_TURNS; ++t)
        for (int d = 0; d < MAX_DIGITS; ++d)
            gs->guesses[t][d] = 0;

    printf("New game started! Secret number generated.\n");
}

/* Code 2 logic: preserved */
int check(int current_turn, int digitCount, int guess[][digitCount], int answer[]) {
    int i, j;
    int usedJ[4];              // track which secret digits have been matched
    int usedI[digitCount];     // track which guess digits have been matched
    int turnAnswerMatrix[digitCount][digitCount];

    // Build comparison matrix: each guess digit vs each secret digit
    for (i = 0; i < digitCount; i++) {
        for (j = 0; j < digitCount; j++) {
            if (guess[current_turn][i] == answer[j]) {
                turnAnswerMatrix[i][j] = 1;   // digit exists in secret
            } else {
                turnAnswerMatrix[i][j] = 0;
            }
            //printf("%d ", turnAnswerMatrix[i][j]);
        }
        // printf("\n");
    }

    // Count correct-location matches (diagonal)
    int corrDigitPlacement = 0;
    for (i = 0; i < digitCount; i++) {
        if (guess[current_turn][i] == answer[i]) {
            corrDigitPlacement++;
        }
    }
    
    printf("\n" BOLD "?? TURN %d RESULT" RESET "\n", current_turn + 1);
    printf(BOLD "-------------------------\n" RESET);
    if (corrDigitPlacement > 0) {
        printf(GREEN "? Correct Location: %d\n" RESET, corrDigitPlacement);
    } else {
        printf(RED "? Correct Location: %d\n" RESET, corrDigitPlacement);
    }

    // Initialize usage trackers
    for (i = 0; i < digitCount; i++) {
        usedI[i] = 0;
        usedJ[i] = 0;
    }

    // Count correct digits (regardless of position, no double-counting)
    int corrDigit = 0;
    for (i = 0; i < digitCount; i++) {
        for (j = 0; j < digitCount; j++) {
            if (usedI[i] == 1 || usedJ[j] == 1) {
                continue; // already matched
            }
            if (turnAnswerMatrix[i][j] == 1) {
                usedI[i] = 1;
                usedJ[j] = 1;
                corrDigit++;
            }
        }
    }
    if (corrDigit > 0) {
        printf(YELLOW "?? Correct Number: %d\n" RESET, corrDigit);
    } else {
        printf(RED "? Correct Number: %d\n" RESET, corrDigit);
    }

	printf("Your Guess: \n");
	for (int k = 0; k < digitCount; ++k) printf("%d", guess[current_turn][k]);
	printf("\n");
    return 0;
}


int turnMaker(char turn_type, int total_turn){
    int count_of_turn = 1;
    int f_turn = 1;

    int g_turn;
    if(total_turn % 2 != 0){
        count_of_turn = (total_turn - 1) / 2;
        g_turn = f_turn + count_of_turn;
    } else {
        count_of_turn = total_turn / 2;
        g_turn = f_turn + count_of_turn - 1;
    };

    int l_turn = g_turn + count_of_turn;
    int q_turn = l_turn + 1;

    switch (turn_type){
        case 'f': return f_turn;
        case 'g': return g_turn;
        case 'l': return l_turn;
        case 'q': return q_turn;
        case 'a': return q_turn + 1;
    }
    return 0;
}

/* In-game prompt */
int in_game_prompt(void) {
    int choice = 0;
    printf("\n" BOLD CYAN "?? In-Game Menu" RESET "\n");
    printf("1. " GREEN "Guess" RESET "\n");
    printf("2. " YELLOW "Save Progress" RESET "\n");
    printf("3. " BLUE "Sort & View Guess History" RESET "\n");
    printf("4. " MAGENTA "Search Guess History" RESET "\n");
    printf("5. " RED "Quit" RESET "\n");
    printf("Choose (1-5): ");
    if (scanf("%d", &choice) != 1) {
        while (getchar() != '\n');
        return 0;
    }
    return choice;
}

/* Evaluasi tebakan tanpa efek samping (tidak mencetak apa-apa) */
void evaluate_guess(int guess[], int secret[], int num_digits,
                    int *out_correct_digit, int *out_correct_position) {
    int i, j;
    int used_secret[MAX_DIGITS] = {0};
    int used_guess[MAX_DIGITS] = {0};
    int correct_pos = 0;
    int correct_digit = 0;

    // 1. Hitung correct position (digit dan posisi cocok)
    for (i = 0; i < num_digits; i++) {
        if (guess[i] == secret[i]) {
            correct_pos++;
            used_guess[i] = 1;
            used_secret[i] = 1;
        }
    }

    // 2. Hitung digit benar di posisi salah (tanpa double-count)
    for (i = 0; i < num_digits; i++) {
        if (used_guess[i]) continue;
        for (j = 0; j < num_digits; j++) {
            if (used_secret[j]) continue;
            if (guess[i] == secret[j]) {
                correct_digit++;
                used_secret[j] = 1;
                break;
            }
        }
    }

    *out_correct_position = correct_pos;
    *out_correct_digit = correct_pos + correct_digit; // total angka yang benar
}

/* Urutkan dari paling mendekati jawaban (prioritas: posisi benar, lalu total digit) */
int compare_guess_records(const void *a, const void *b) {
    struct GuessRecord *x = (struct GuessRecord *)a;
    struct GuessRecord *y = (struct GuessRecord *)b;

    if (x->correct_position != y->correct_position)
        return y->correct_position - x->correct_position; // descending
    return y->correct_digit - x->correct_digit;           // descending
}

void sort_and_display_guesses(struct GameState *gs) {
    if (gs->turns_used <= 0) {
        printf("No guesses to sort.\n");
        return;
    }

    struct GuessRecord records[MAX_TURNS];
    int count = gs->turns_used;

    // Isi records
    for (int t = 0; t < count; t++) {
        records[t].turn_index = t;
        for (int d = 0; d < gs->num_digits; d++) {
            records[t].guess[d] = gs->guesses[t][d];
        }
        evaluate_guess(gs->guesses[t], gs->secret, gs->num_digits,
                       &records[t].correct_digit, &records[t].correct_position);
    }

    // Sort
    qsort(records, count, sizeof(struct GuessRecord), compare_guess_records);

    // Tampilkan
    printf("\n=== Sorted Guess History (Best to Worst) ===\n");
    for (int i = 0; i < count; i++) {
        printf("Turn %2d: ", records[i].turn_index + 1);
        for (int d = 0; d < gs->num_digits; d++) {
            printf("%d", records[i].guess[d]);
        }
        printf(" ? %d digits, %d positions\n",
               records[i].correct_digit, records[i].correct_position);
    }
    printf("\n");
}

void search_guesses(struct GameState *gs) {
    if (gs->turns_used <= 0) {
        printf("No guesses to search.\n");
        return;
    }

    int target_digit, target_pos;
    printf("Enter target correct digits (total): ");
    if (scanf("%d", &target_digit) != 1) {
        while (getchar() != '\n');
        printf("Invalid input.\n");
        return;
    }
    printf("Enter target correct positions: ");
    if (scanf("%d", &target_pos) != 1) {
        while (getchar() != '\n');
        printf("Invalid input.\n");
        return;
    }

    printf("\n=== Search Results (digit=%d, pos=%d) ===\n", target_digit, target_pos);
    int found = 0;
    for (int t = 0; t < gs->turns_used; t++) {
        int cd, cp;
        evaluate_guess(gs->guesses[t], gs->secret, gs->num_digits, &cd, &cp);
        if (cd == target_digit && cp == target_pos) {
            printf("Turn %2d: %d%d%d%d\n",
                   t+1,
                   gs->guesses[t][0], gs->guesses[t][1],
                   gs->guesses[t][2], gs->guesses[t][3]);
            found = 1;
        }
    }
    if (!found) {
        printf("No matching guesses found.\n");
    }
    printf("\n");
}


/* Play game — resumes from turns_used, allows save mid-game, quit to menu */
void play_game(struct GameState *gs) {
    int g_turn = turnMaker('g', gs->total_turns);
    int l_turn = turnMaker('l', gs->total_turns);
    int q_turn = turnMaker('q', gs->total_turns);
    int a_turn = turnMaker('a', gs->total_turns);


	enter();
    printf("\n=== GAME MODE ===\n");
    printf("Digits: %d, Total Turns: %d\n", gs->num_digits, gs->total_turns);
    printf("Starting at turn %d.\n", gs->turns_used + 1);

    for (int current_turn = gs->turns_used; current_turn < gs->total_turns; /* increment inside */) {
        
        
        int menu_choice = in_game_prompt();

        if (menu_choice == 2) {
            /* Save current progress */
            gs->turns_used = current_turn;
            save_game(gs);
            continue;
        } else if (menu_choice == 5) {
            /* Quit */
            gs->turns_used = current_turn;
            printf("Returning to main menu. Progress saved in memory.\n");
            break;
        } else if (menu_choice == 3) {
            /* Sort & View */
            sort_and_display_guesses(gs);
            continue;
        } else if (menu_choice == 4) {
            /* Search */
            search_guesses(gs);
            continue;
        } else if (menu_choice != 1) {
            printf("Please choose 1-5.\n");
            continue;
        }

        /* Option 1: Guess */
        char input[10];
		while (1) {
    		printf("Turn %d/%d - Enter a 4-digit guess: ", current_turn + 1, gs->total_turns);
    		if (scanf("%9s", input) != 1) {
        		while (getchar() != '\n');
        		printf("Invalid input.\n");
        		continue;
    		}

    	// Validasi: panjang 4 dan semua digit
    	if (strlen(input) != 4) {
        	printf("Please enter exactly 4 digits.\n");
        	continue;
    	}

    	int valid = 1;
    	for (int i = 0; i < 4; i++) {
        	if (input[i] < '0' || input[i] > '9') {
            	valid = 0;
            	break;
        	}
    	}
    	if (!valid) {
        	printf("All characters must be digits (0-9).\n");
        	continue;
    	}
    	
    	enter();
    // Konversi char ke int
    	for (int i = 0; i < 4; i++) {
        	gs->guesses[current_turn][i] = input[i] - '0';
    	}
    	break;
		}

		/* Run Code 2 checking */
		check(current_turn, gs->num_digits, gs->guesses, gs->secret);
		
        /* Visual phase markers (informational only) */
//        if (current_turn + 1 == g_turn) printf("[Phase: Guessing]\n");
//        if (current_turn + 1 == l_turn) printf("[Phase: Location]\n");
//        if (current_turn + 1 == q_turn) printf("[Phase: Question]\n");
//        if (current_turn + 1 == a_turn) printf("[Phase: FFA]\n");

        /* Win detection (external to Code 2) */
        int correct_pos = 0;
        for (int i = 0; i < gs->num_digits; i++) {
            if (gs->guesses[current_turn][i] == gs->secret[i]) correct_pos++;
        }
        if (correct_pos == gs->num_digits) {
            printf("\n" BOLD GREEN "?? Congratulations! You guessed the number!\n" RESET);
    		printf("Your final guess: " GREEN);
    		for (int i = 0; i < gs->num_digits; i++) printf("%d", gs->guesses[current_turn][i]);
    		printf(RESET "\n\n" BOLD "Secret number (ASCII art):\n" RESET);
    		display_large(gs->secret, gs->num_digits);

            gs->turns_used = current_turn + 1; /* mark game as finished */
            return;
        }

        current_turn++; /* proceed to next turn after a valid guess */
        gs->turns_used = current_turn; /* update progress for save/load */

        printf("Turns left: %d/%d\n", gs->total_turns - current_turn, gs->total_turns);
    }

    /* If out of turns, reveal secret */
    if (gs->turns_used >= gs->total_turns) {
        printf("\n" BOLD RED "?? Out of turns!\n" RESET);
    	printf("The secret number was: " BOLD);
    	for (int i = 0; i < gs->num_digits; i++) printf("%d", gs->secret[i]);
    	printf(RESET "\n\n" BOLD "Secret number (ASCII art):\n" RESET);
    	display_large(gs->secret, gs->num_digits);
    }
}

/* Main menu */
int main(void) {
	enable_ansi_colors(); 
    struct GameState gs;
    int choice;
    int valid;

    /* Initialize state to safe defaults */
    gs.num_digits = 4;
    gs.total_turns = 15;
    gs.turns_used = 0;
    for (int i = 0; i < MAX_DIGITS; ++i) gs.secret[i] = 0;
    for (int t = 0; t < MAX_TURNS; ++t)
        for (int d = 0; d < MAX_DIGITS; ++d)
            gs.guesses[t][d] = 0;

    display_title();

    do {
        printf("\n" BOLD CYAN "?? Main Menu:" RESET "\n");
        printf("1. " GREEN "New Game" RESET "\n");
        printf("2. " YELLOW "Load Game" RESET "\n");
        printf("3. " BLUE "Save Game" RESET "\n");
        printf("4. " CYAN "Play" RESET "\n");
        printf("5. " RED "Exit" RESET "\n");
        printf("Choose (1-5): ");

        valid = scanf("%d", &choice);
        if (valid != 1) {
            while (getchar() != '\n');  /* Clear invalid input */
            choice = 0;
        }

        switch (choice) {
            case 1:
                new_game(&gs);
                break;
            case 2:
                if (!load_game(&gs)) {
                    printf("Failed to load game.\n");
                }
                break;
            case 3:
                if (!save_game(&gs)) {
                    printf("Failed to save game.\n");
                }
                break;
            case 4:
                play_game(&gs);
                break;
            case 5:
                printf("\nThanks for playing! Goodbye :)\n\n");
                break;
            default:
                printf("Please choose 1-5!\n");
        }
    } while (choice != 5);

    return 0;
}

