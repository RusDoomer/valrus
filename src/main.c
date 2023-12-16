/* TODO:
 * get rid of global variables
 * prepare for multithreading
 * test where multithreading is worthwhile
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <time.h>

#define ROW 3
#define COL 10
#define LEFT_HAND 4
#define RIGHT_HAND 5

static long unsigned int monogram[128];
static long unsigned int bigram[128][128];
static long unsigned int trigram[128][128][128];

char* corpus;
int corpus_malloc;
char* layout;
int layout_malloc;
char* weight;
int weights_malloc;
char mode;
int generation_quantity;
char output;

enum patterns
{
    SFB, SFS, SFT, BAD_SFB, BAD_SFS, BAD_SFT,
    LSB, LSS, LST,
    HRB, HRS, HRT,
    FRB, FRS, FRT,
    ALT, RED, BAD_RED,
    ONE, ONE_IN, ONE_OUT, SR_ONE, SR_ONE_IN, SR_ONE_OUT,
    AF_ONE, AF_ONE_IN, AF_ONE_OUT, SRAF_ONE, SRAF_ONE_IN, SRAF_ONE_OUT,
    ROL, ROL_IN, ROL_OUT, SR_ROL, SR_ROL_IN, SR_ROL_OUT,
    AF_ROL, AF_ROL_IN, AF_ROL_OUT, SRAF_ROL, SRAF_ROL_IN, SRAF_ROL_OUT,
    HAND_BALANCE, LHU, RHU, TRU, HRU, BRU,
    LPU, LRU, LMU, LIU, LLU, RLU, RIU, RMU, RRU, RPU,
    LPB, LRB, LMB, LIB, LLB, RLB, RIB, RMB, RRB, RPB,
    LPS, LRS, LMS, LIS, LLS, RLS, RIS, RMS, RRS, RPS,
    LPT, LRT, LMT, LIT, LLT, RLT, RIT, RMT, RRT, RPT,
    END
};

char names[END][30] =
{
    "SFB", "SFS", "SFT", "BAD", "BAD", "BAD",
    "LSB", "LSS", "LST",
    "HRB", "HRS", "HRT",
    "FRB", "FRS", "FRT",
    "ALT", "RED", "BAD",
    "ONE HAND                ", "IN", "OUT", "SAME ROW                ", "IN", "OUT",
    "ADJACENT FINGER         ", "IN", "OUT", "SAME ROW ADJACENT FINGER", "IN", "OUT",
    "ROLL                    ", "IN", "OUT", "SAME ROW                ", "IN", "OUT",
    "ADJACENT FINGER         ", "IN", "OUT", "SAME ROW ADJACENT FINGER", "IN", "OUT",
    "HAND BALANCE", "LHU", "RHU", "TRU", "HRU", "BRU",
    "LPU", "LRU", "LMU", "LIU", "LLU", "RLU", "RIU", "RMU", "RRU", "RPU",
    "LPB", "LRB", "LMB", "LIB", "LLB", "RLB", "RIB", "RMB", "RRB", "RPB",
    "LPS", "LRS", "LMS", "LIS", "LLS", "RLS", "RIS", "RMS", "RRS", "RPS",
    "LPT", "LRT", "LMT", "LIT", "LLT", "RLT", "RIT", "RMT", "RRT", "RPT"
};

char totals[END] =
{
    'b', 's', 't', 'b', 's', 't',
    'b', 's', 't',
    'b', 's', 't',
    'b', 's', 't',
    't', 't', 't',
    't', 't', 't', 't', 't', 't',
    't', 't', 't', 't', 't', 't',
    't', 't', 't', 't', 't', 't',
    't', 't', 't', 't', 't', 't',
    'm', 'm', 'm', 'm', 'm', 'm',
    'm', 'm', 'm', 'm', 'm', 'm', 'm', 'm', 'm', 'm',
    'b', 'b', 'b', 'b', 'b', 'b', 'b', 'b', 'b', 'b',
    's', 's', 's', 's', 's', 's', 's', 's', 's', 's',
    't', 't', 't', 't', 't', 't', 't', 't', 't', 't'
};

struct ranking
{
    char *name;
    double score;
    struct ranking *next;
};

struct ranking *head;
struct ranking *ptr;

struct keyboard_layout
{
    char matrix[ROW][COL];
    double score;
    long unsigned int mon_total;
    long unsigned int big_total;
    long unsigned int ski_total;
    long unsigned int tri_total;
    long unsigned int stats[END];
};

struct keyboard_layout *current;
struct keyboard_layout *max;

struct stat_weights
{
    int pins[3][10];
    double multiplier[END];
};

struct stat_weights *weights;

void error_out(char *message)
{
    printf("Error: %s\n", message);
    printf("Freeing, and exiting.\n");
    if (current != NULL) {free(current);}
    if (max != NULL) {free(max);}
    while (head != NULL)
    {
        ptr = head;
        while (ptr != NULL && ptr->next != NULL) {ptr = ptr->next;}
        free(ptr->name);
        free(ptr);
    }
    if (corpus_malloc) {free(corpus);}
    if (layout_malloc) {free(layout);}
    if (weights_malloc) {free(weight);}
    exit(1);
}

char convert_char(char c)
{
    //lower case all letters, and symbols that are within the "alpha" area
    switch(c)
    {
        case '{':
            return '[';
        case '}':
            return ']';
        case '|':
            return '\\';
        case '?':
            return '/';
        case '>':
            return '.';
        case '<':
            return ',';
        case ':':
            return ';';
        case '_':
            return '-';
        case '+':
            return '=';
        default:
            if (c <= 32) {return ' ';}
            if (c <= 64) {return c;}
            if (c <= 90) {return c + 32;}
            if (c <= 126) {return c;}
    }
    return ' ';
}

char key(int row, int col)
{
    return current->matrix[row][col];
}

char hand(int col)
{
    if (col <= LEFT_HAND) {return 'l';}
    else {return 'r';}
}

int finger(int col)
{
    switch(col)
    {
        default:
        case 0:
            return 0;
        case 1:
            return 1;
        case 2:
            return 2;
        case 3:
        case 4:
            return 3;
        case 5:
        case 6:
            return 4;
        case 7:
            return 5;
        case 8:
            return 6;
        case 9:
            return 7;
    }
}

int same_finger(int col0, int col1)
{
    return finger(col0) == finger (col1);
}

int same_row(int row0, int row1)
{
    return row0 == row1;
}

int same_pos(int row0, int col0, int row1, int col1)
{
    return row0 == row1 && col0 == col1;
}

int same_hand(int col0, int col1)
{
    return hand(col0) == hand(col1);
}

int adjacent_fingers(int col0, int col1)
{
    return (same_hand(col0, col1)
        && (finger(col0) - finger(col1) == 1
            || finger(col0) - finger(col1) == -1)
        && (col0 - col1 == 1 || col0 - col1 == -1));
}

int distance(int row0, int row1)
{
    int dist = row0 - row1;
    if (dist >= 0) {return dist;}
    else {return -dist;}
}

int not_stretch(int col0, int col1, int col2)
{
	return col0 != 4 && col0 != 5
		&& col1 != 4 && col1 != 5
		&& col2 != 4 && col2 != 5;
}

int is_lsb(int col0, int col1)
{
    return ((col0 == 2 && col1 == 4) || (col0 == 4 && col1 == 2)
        || (col0 == 5 && col1 == 7) || (col0 == 7 && col1 == 5));
}

int is_russor(int col0, int col1)
{
    return (same_hand(col0, col1) && !same_finger(col0, col1)
        && (col0 == 1 || col0 == 2 || col0 == 7 || col0 == 8 || col1 == 1
        || col1 == 2 || col1 == 7 || col1 == 8));
}

int is_one(int col0, int col1, int col2)
{
    return (finger(col0) < finger(col1) && finger(col1) < finger(col2))
        || (finger(col0) > finger(col1) && finger(col1) > finger(col2));
}

int is_one_in(int col0, int col1, int col2)
{
    return (hand(col0) == 'l' && col0 < col1)
        || (hand(col0) == 'r' && col1 > col2);
}

int is_same_row_one(int row0, int row1, int row2, int col0, int col1, int col2)
{
    return same_row(row0, row1) && same_row(row1, row2)
        && not_stretch(col0, col1, col2);
}

int is_adjacent_finger_one(int col0, int col1, int col2)
{
    return adjacent_fingers(col0, col1) && adjacent_fingers(col1, col2);
}

int is_bad_red(int col0, int col1, int col2)
{
    return ((col0 == 0 || col0 == 1 || col0 == 2 || col0 == 7 || col0 == 8 || col0 == 9)
        && (col1 == 0 || col1 == 1 || col1 == 2 || col1 == 7 || col1 == 8 || col1 == 9)
        && (col2 == 0 || col2 == 1 || col2 == 2 || col2 == 7 || col2 == 8 || col2 == 9));
}

int is_roll(int col0, int col1, int col2)
{
    return same_hand(col0, col1) != same_hand(col1, col2)
        && finger(col0) != finger(col1) && finger(col1) != finger(col2);
}

int is_roll_in(int col0, int col1, int col2)
{
    return (same_hand(col0, col1) && hand(col1) == 'l' && col0 < col1)
        || (same_hand(col0, col1) && hand(col1) == 'r' && col0 > col1)
        || (same_hand(col1, col2) && hand(col1) == 'l' && col1 < col2)
        || (same_hand(col1, col2) && hand(col1) == 'r' && col1 > col2);
}

int is_same_row_roll(int col0, int col1, int col2, int row0, int row1, int row2)
{
    return col0 != 4 && col0 != 5
    	&& col1 != 4 && col1 != 5
    	&& col2 != 4 && col2 != 5
   		&& ((same_hand(col0, col1) && same_row(row0, row1))
        || (same_hand(col1, col2) && same_row(row1, row2)));
}

int is_adjacent_finger_roll(int col0, int col1, int col2)
{
    return (same_hand(col0, col1) && adjacent_fingers(col0, col1))
        || (same_hand(col1, col2) && adjacent_fingers(col1, col2));
}

void read_corpus(char *name)
{
    //define variables
    FILE *data;
    char *ngram, *raw;
    int a, b, c;
    //get full filename for ngram data of layout
    ngram = (char*)malloc(strlen("./corpora/ngram/") + strlen(name) + 1);
    strcpy(ngram, "./corpora/ngram/");
    strcat(ngram, name);
    //get full filename for raw data of layout
    raw = (char*)malloc(strlen("./corpora/raw/") + strlen(name) + 1);
    strcpy(raw, "./corpora/raw/");
    strcat(raw, name);
    //check if ngram data exists, create if doesn't
    data = fopen(ngram, "r");
    if (data == NULL)
    {
        printf("Ngram data does not yet exist, attempting to create file...\n");
        data = fopen(raw, "r");
        if (data == NULL)
        {
            free(raw);
            free(ngram);
            error_out("No such corpus was found.");
            return;
        }
        //read corpus from raw file and create ngram file
        a = fgetc(data);
        b = fgetc(data);
        //fill buffer by reading first two chars
        if (a != EOF && b != EOF)
        {
            a = convert_char(a);
            b = convert_char(b);
            monogram[a]++;
            monogram[b]++;
            bigram[a][b]++;
        }
        else {error_out("Corpus not long enough.");}
        while ((c = fgetc(data)) != EOF)
        {
            c = convert_char(c);
            monogram[c]++;
            bigram[b][c]++;
            trigram[a][b][c]++;
            a = b;
            b = c;
        }
        fclose(data);
        //actually write ngram file
        data = fopen(ngram, "w");
        for (int i = 0; i < 128; i++)
        {
            for (int j = 0; j < 128; j++)
            {
                for (int k = 0; k < 128; k++)
                {
                    //print trigrams
                    if (trigram[i][j][k]) {fprintf(data, "t%c%c%c %lu\n", i, j, k, trigram[i][j][k]);}
                }
                //print bigrams
                if (bigram[i][j] != 0) {fprintf(data, "b%c%c%c %lu\n", i, j, ' ', bigram[i][j]);}
            }
            //print monograms
            if (monogram[i] != 0) {fprintf(data, "m%c%c%c %lu\n", i, ' ', ' ', monogram[i]);}
        }
    }
    //use ngram data if it exists
    else
    {
        //read corpus from ngram file
        while ((c = fgetc(data)) != EOF)
        {
            switch(c)
            {
                case 'm':
                    a = fgetc(data);
                    fscanf(data, "%lu", &monogram[a]);
                    fgetc(data);
                    break;
                case 'b':
                    a = fgetc(data);
                    b = fgetc(data);
                    fscanf(data, "%lu", &bigram[a][b]);
                    fgetc(data);
                    break;
                case 't':
                    a = fgetc(data);
                    b = fgetc(data);
                    c = fgetc(data);
                    fscanf(data, "%lu", &trigram[a][b][c]);
                    fgetc(data);
                    break;
            }
        }
    }
    fclose(data);
    free(ngram);
    free(raw);
    return;
}

void read_layout(char *name)
{
    //find layout file
    FILE * data;
    char *layout = (char*)malloc(strlen("./layouts/") + strlen(name) + 1);
    strcpy(layout, "./layouts/");
    strcat(layout, name);
    data = fopen(layout, "r");
    //check if layout exists
    if (data == NULL)
    {
        free(layout);
        error_out("No such layout was found.");
        return;
    }
    current = calloc(1, sizeof(struct keyboard_layout));
    //read layout
    for (int  i = 0; i < ROW; i++)
    {
        for (int j = 0; j < COL; j++)
        {
            fscanf(data, " %c", &current->matrix[i][j]);
        }
    }
    free(layout);
    fclose(data);
}

//sft, bsft, lst, hrt, frt, red, onehands, rolls, sft by finger
void analyze_trigram(int row0, int col0, int row1, int col1, int row2, int col2,  long unsigned int value)
{
    if (value == 0) {return;}
    current->tri_total += value;
    //sft
    if(same_finger(col0, col1) && same_finger(col1, col2)
        && !same_pos(row0, col0, row1, col1) && !same_pos(row1, col1, row2, col2))
    {
        current->stats[SFT] += value;
        //bad sft
        if (distance(row0, row1) == 2 || distance(row1, row2) == 2)
        {
            current->stats[BAD_SFT] += value;
        }
        //sft by finger
        switch(col0)
        {
            case 0:
                current->stats[LPT] += value;
                break;
            case 1:
                current->stats[LRT] += value;
                break;
            case 2:
                current->stats[LMT] += value;
                break;
            case 4:
                current->stats[LLT] += value;
            case 3:
                current->stats[LIT] += value;
                break;
            case 5:
                current->stats[RLT] += value;
            case 6:
                current->stats[RIT] += value;
                break;
            case 7:
                current->stats[RMT] += value;
                break;
            case 8:
                current->stats[RRT] += value;
                break;
            case 9:
                current->stats[RPT] += value;
                break;
        }
    }
    //lst
    if (is_lsb(col0, col1) && is_lsb(col1, col2))
    {
        current->stats[LST] += value;
    }
    //russors -- needs fixing
    if (is_russor(col0, col1) && is_russor(col1, col2))
    {
        if (distance(col0, col1) == 1 && distance(col1, col2) == 1)
        {
            current->stats[HRT] += value;
        }
        if (distance(col0, col1) == 2 && distance(col1, col2) == 2)
        {
            current->stats[FRT] += value;
        }
    }
    //alt
    if (same_hand(col0, col2) && !same_hand(col0, col1))
    {
        current->stats[ALT] += value;
    }

    //redirects and onehands
    if (same_hand(col0, col1) && same_hand(col1, col2)
        && !same_finger(col0, col1) && !same_finger(col1, col2)
        && !same_finger(col0, col2))
    {
        //one
        if (is_one(col0, col1, col2))
        {
            int one_in = is_one_in(col0, col1, col2);
            int one_sr = is_same_row_one(row0, row1, row2, col0, col1, col2);
            int one_af = is_adjacent_finger_one(col0, col1, col2);
            current->stats[ONE]     += value;
            current->stats[ONE_IN]  += value * one_in;
            current->stats[ONE_OUT] += value * !one_in;

            current->stats[SR_ONE]     += value * one_sr;
            current->stats[SR_ONE_IN]  += value * one_sr * one_in;
            current->stats[SR_ONE_OUT] += value * one_sr * !one_in;

            current->stats[AF_ONE]     += value * one_af;
            current->stats[AF_ONE_IN]  += value * one_af * one_in;
            current->stats[AF_ONE_OUT] += value * one_af * !one_in;

            current->stats[SRAF_ONE]     += value * one_sr * one_af;
            current->stats[SRAF_ONE_IN]  += value * one_sr * one_af * one_in;
            current->stats[SRAF_ONE_OUT] += value * one_sr * one_af * !one_in;
        }
        //redirects
        else
        {
            current->stats[RED] += value;
            if (is_bad_red(col0, col1, col2))
            {
                current->stats[BAD_RED] += value;
            }
        }
    }

    //rolls
    if (is_roll(col0, col1, col2))
    {
        int roll_in = is_roll_in(col0, col1, col2);
        int roll_sr = is_same_row_roll(col0, col1, col2, row0, row1, row2);
        int roll_af = is_adjacent_finger_roll(col0, col1, col2);

        current->stats[ROL]     += value;
        current->stats[ROL_IN]  += value * roll_in;
        current->stats[ROL_OUT] += value * !roll_in;

        current->stats[SR_ROL]     += value * roll_sr;
        current->stats[SR_ROL_IN]  += value * roll_sr * roll_in;
        current->stats[SR_ROL_OUT] += value * roll_sr * !roll_in;

        current->stats[AF_ROL]     += value * roll_af;
        current->stats[AF_ROL_IN]  += value * roll_af * roll_in;
        current->stats[AF_ROL_OUT] += value * roll_af * !roll_in;

        current->stats[SRAF_ROL]     += value * roll_sr * roll_af;
        current->stats[SRAF_ROL_IN]  += value * roll_sr * roll_af * roll_in;
        current->stats[SRAF_ROL_OUT] += value * roll_sr * roll_af * !roll_in;
    }
}

//sfs, bsfs,  lss, hrs, frs, sfs by finger
void analyze_skipgram(int row0, int col0, int row2, int col2, long unsigned int value)
{
    if (value == 0) {return;}
    current->ski_total += value;
    //sfs
    if(same_finger(col0, col2) && !same_pos(row0, col0, row2, col2))
    {
        current->stats[SFS] += value;
        //bad sfs
        if (distance(row0, row2) == 2) {current->stats[BAD_SFS] += value;}
        //sfs by finger
        switch(col0)
        {
            case 0:
                current->stats[LPS] += value;
                break;
            case 1:
                current->stats[LRS] += value;
                break;
            case 2:
                current->stats[LMS] += value;
                break;
            case 4:
            case 3:
                current->stats[LIS] += value;
                break;
            case 5:
            case 6:
                current->stats[RIS] += value;
                break;
            case 7:
                current->stats[RMS] += value;
                break;
            case 8:
                current->stats[RRS] += value;
                break;
            case 9:
                current->stats[RPS] += value;
                break;
        }
        if (col0 == 4 || col2 == 4)
        {
            current->stats[LLS] += value;
        }
        if (col0 == 5 || col2 == 5)
        {
            current->stats[RLS] += value;
        }
    }
    //lss
    if (is_lsb(col0, col2)) {current->stats[LSS] += value;}
    //russors
    if (is_russor(col0, col2))
    {
        if (distance(row0, row2) == 1) {current->stats[HRS] += value;}
        if (distance(row0, row2) == 2) {current->stats[FRS] += value;}
    }
}

//sfb, bsfb, lsb, hrb, frb, alt, sfb by finger
void analyze_bigram(int row0, int col0, int row1, int col1, long unsigned int value)
{
    //if (value == 0) {return;}
    current->big_total += value;
    //sfb
    if(same_finger(col0, col1) && !same_pos(row0, col0, row1, col1))
    {
        current->stats[SFB] += value;
        //bad sfb
        if (distance(row0, row1) == 2) {current->stats[BAD_SFB] += value;}
        //sfb by finger
        switch(col0)
        {
            case 0:
                current->stats[LPB] += value;
                break;
            case 1:
                current->stats[LRB] += value;
                break;
            case 2:
                current->stats[LMB] += value;
                break;
            case 4:
            case 3:
                current->stats[LIB] += value;
                break;
            case 5:
            case 6:
                current->stats[RIB] += value;
                break;
            case 7:
                current->stats[RMB] += value;
                break;
            case 8:
                current->stats[RRB] += value;
                break;
            case 9:
                current->stats[RPB] += value;
                break;
        }
        if (col0 == 4 || col1 == 4)
        {
            current->stats[LLB] += value;
        }
        if (col0 == 5 || col1 == 5)
        {
            current->stats[RLB] += value;
        }
    }
    //lsb
    if (is_lsb(col0, col1)) {current->stats[LSB] += value;}
    //russors
    if (is_russor(col0, col1))
    {
        if (distance(row0, row1) == 1) {current->stats[HRB] += value;}
        if (distance(row0, row1) == 2) {current->stats[FRB] += value;}
    }
}

//hand, row, and finger usage
void analyze_monogram(int row, int col, long unsigned int value)
{
    if (value == 0) {return;}
    current->mon_total += value;
    //hand usage
    if (hand(col) == 'l') {current->stats[LHU] += value;}
    else                  {current->stats[RHU] += value;}
    //row usage
    if (row == 0)      {current->stats[TRU] += value;}
    else if (row == 1) {current->stats[HRU] += value;}
    else               {current->stats[BRU] += value;}
    //finger usage
    switch(col)
    {
        case 0:
            current->stats[LPU] += value;
            break;
        case 1:
            current->stats[LRU] += value;
            break;
        case 2:
            current->stats[LMU] += value;
            break;
        //FOR STRETCH FINGERS IT COUNTS FOR BOTH STRETCH AND INDEX USAGE
        case 4:
            current->stats[LLU] += value;
        case 3:
            current->stats[LIU] += value;
            break;
        //SAME HERE
        case 5:
            current->stats[RLU] += value;
        case 6:
            current->stats[RIU] += value;
            break;
        case 7:
            current->stats[RMU] += value;
            break;
        case 8:
            current->stats[RRU] += value;
            break;
        case 9:
            current->stats[RPU] += value;
            break;
    }
}

void analyze_layout()
{
    //first key
    for (int r1 = 0; r1 < ROW; r1++)
    {
        for (int c1 = 0; c1 < COL; c1++)
        {
            analyze_monogram(r1, c1, monogram[key(r1, c1)]);

            //second key
            for (int r2 = 0; r2 < ROW; r2++)
            {
                for (int c2 = 0; c2 < COL; c2++)
                {
                    analyze_bigram(r1, c1, r2, c2, bigram[key(r1, c1)][key(r2, c2)]);
                    //skipgrams with all possible trigrams, not just on this layout
                    for (int i = 0; i < 128; i++)
                    {
                        analyze_skipgram(r1, c1, r2, c2, trigram[key(r1, c1)][i][key(r2, c2)]);
                    }
                    //third key
                    for (int r3 = 0; r3 < ROW; r3++)
                    {
                        for (int c3 = 0; c3 < COL; c3++)
                        {
                            analyze_trigram(r1, c1, r2, c2, r3, c3, trigram[key(r1, c1)][key(r2, c2)][key(r3, c3)]);
                        }
                    }
                }
            }
        }
    }
    if (current->stats[RHU] > current->stats[LHU])
    {
        current->stats[HAND_BALANCE] = current->stats[RHU] - current->stats[LHU];
    }
    else
    {
        current->stats[HAND_BALANCE] = current->stats[LHU] - current->stats[RHU];
    }
}

void read_weights(char *name)
{
    char temp;
    FILE * data;
    char *weight = (char*)malloc(strlen("./weights/") + strlen(name) + 1);
    strcpy(weight, "./weights/");
    strcat(weight, name);
    data = fopen(weight, "r");
    if (data == NULL)
    {
        free(weight);
        error_out("No such weights were found.");
        return;
    }
    weights = calloc(1, sizeof(struct stat_weights));
    for (int i = 0; i < ROW; i++)
    {
        for (int j = 0; j < COL; j++)
        {
            fscanf(data, " %c", &temp);
            if (temp == '.') {weights->pins[i][j] = 0;}
            else {weights->pins[i][j] = 1;}
        }
    }
    for(enum patterns i = SFB; i < END; i++)
    {
        fscanf(data, " %*[^0-9.-]%lf", &weights->multiplier[i]);
    }
    free(weight);
    fclose(data);
}

long unsigned int total(enum patterns stat)
{
    switch(totals[stat])
    {
        case('m'):
            return current->mon_total;
            break;
        case('b'):
            return current->big_total;
            break;
        case('s'):
            return current->ski_total;
            break;
        default:
            return current->tri_total;
            break;
    }
}

void get_score()
{
    for (enum patterns i = SFB; i < END; i++)
    {
        current->score += ((double)current->stats[i]/total(i)*100) * weights->multiplier[i];
    }
}

int print_new_line(enum patterns stat)
{
    return stat == SFT || stat == BAD_SFT || stat == LST || stat == HRT
        || stat == FRT || stat == BAD_RED || stat == ONE_OUT
        || stat == SR_ONE_OUT || stat == AF_ONE_OUT || stat == SRAF_ONE_OUT
        || stat == ROL_OUT || stat == SR_ROL_OUT || stat == AF_ROL_OUT
        || stat == SRAF_ROL_OUT || stat == RHU || stat == BRU;
}

void print_layout()
{
    printf("Score : %f\n", current->score);
    puts("");
    for (int i = 0; i < ROW; i++)
    {
        for (int j = 0; j < COL; j++)
        {
            if (j == RIGHT_HAND) {printf(" ");}
            printf("%c ", current->matrix[i][j]);
        }
        puts("");
    }
    puts("");
    for (enum patterns i = SFB; i < LPU; i++)
    {
        printf("%s: %06.3f%%", names[i], (double)current->stats[i]/total(i) * 100);
        if (print_new_line(i)) {puts("");}
        else {printf(" | ");}
    }
    printf("Finger Usage: \n[");
    for (enum patterns i = LPU; i < RPU; i++)
    {
        printf("%05.2f, ", (double)current->stats[i]/total(i) * 100);
    }
    printf("%05.2f]\nFinger Bigrams: \n[", (double)current->stats[RPU]/total(RPU) * 100);
    for (enum patterns i = LPB; i < RPB; i++)
    {
        printf("%05.2f, ", (double)current->stats[i]/total(i) * 100);
    }
    printf("%05.2f]\nFinger Skipgrams: \n[", (double)current->stats[RPB]/total(RPB) * 100);
    for (enum patterns i = LPS; i < RPS; i++)
    {
        printf("%05.2f, ", (double)current->stats[i]/total(i) * 100);
    }
    printf("%05.2f]\nFinger Trigrams: \n[", (double)current->stats[RPS]/total(RPS) * 100);
    for (enum patterns i = LPT; i < RPT; i++)
    {
        printf("%05.2f, ", (double)current->stats[i]/total(i) * 100);
    }
    printf("%05.2f]\n", (double)current->stats[RPT]/total(RPT) * 100);
}

void short_print()
{
    for (int i = 0; i < ROW; i++)
    {
        for (int j = 0; j < COL; j++)
        {
            if (j == RIGHT_HAND) {printf(" ");}
            printf("%c ", current->matrix[i][j]);
        }
        puts("");
    }
    puts("");
    for (enum patterns i = SFB; i < LPU; i++)
    {
        if (i == SR_ONE) {i = ROL;}
        printf("%s: %06.3f%%", names[i], (double)current->stats[i]/total(i) * 100);
        if (print_new_line(i)) {puts("");}
        else {printf(" | ");}
    }
    printf("Finger Usage: \n[");
    for (enum patterns i = LPU; i < RPU; i++)
    {
        printf("%05.2f, ", (double)current->stats[i]/total(i) * 100);
    }
    printf("%05.2f]\n", (double)current->stats[RPU]/total(RPU) * 100);
}

void print_rankings()
{
    ptr = head;
    while (ptr != NULL)
    {
        printf("%s -> %lf\n", ptr->name, ptr->score);
        ptr = ptr->next;
    }
}

void rank_layouts()
{
    DIR *dir;
    struct dirent *entry;
    dir = opendir("./layouts");
    if (dir == NULL)
    {
        error_out("No layouts found.");
        return;
    }

    while ((entry = readdir(dir)) != NULL)
    {
        if (entry->d_type == DT_REG)
        {
            read_layout(entry->d_name);
            analyze_layout();
            get_score();
            struct ranking *node = malloc(sizeof(struct ranking));
            node->name = (char*)malloc(strlen(entry->d_name) + 1);
            strcpy(node->name, entry->d_name);
            node->score = current->score;
            node->next = NULL;
            if (head == NULL) {head = node;}
            else if (head->score < node->score)
            {
                node->next = head;
                head = node;
            }
            else if (head->next == NULL) {head->next = node;}
            else
            {
                ptr = head;
                while (ptr->next != NULL && ptr->next->score > node->score)
                    {ptr = ptr->next;}
                node->next = ptr->next;
                ptr->next = node;
            }
            free(current);
            current = NULL;
        }
    }

    print_rankings();

    ptr = head;
    while (head != NULL && ptr != NULL)
    {
        ptr = head;
        while (ptr->next != NULL) {ptr = ptr->next;}
        free(ptr->name);
        free(ptr);
        ptr = NULL;
    }
    closedir(dir);
}

void swap (int r1, int c1, int r2, int c2)
{
    char temp = current->matrix[r1][c1];
    current->matrix[r1][c1] = current->matrix[r2][c2];
    current->matrix[r2][c2] = temp;
}

void shuffle()
{
    srand(time(NULL));
    for (int i = ROW - 1; i > 0; i--)
    {
        for (int j = COL - 1; j > 0; j--)
        {
            int r1 = i;
            int c1 = j;
            int r2 = rand() % (i + 1);
            int c2 = rand() % (j + 1);
            if (r1 != r2 || c1 != c2)
            {
                swap(r1, c1, r2, c2);
            }
        }
    }
}

struct keyboard_layout* copy_layout(struct keyboard_layout *src)
{
    struct keyboard_layout *dest;
    dest = calloc(1, sizeof(struct keyboard_layout));
    for (int i = 0; i < ROW; i++)
    {
        for (int j = 0; j < COL; j++)
        {
            dest->matrix[i][j] = src->matrix[i][j];
        }
    }
    for (enum patterns i = SFB; i < END; i++)
    {
        dest->stats[i] = src->stats[i];
    }
    dest->score = src->score;
    dest->mon_total = src->mon_total;
    dest->big_total = src->big_total;
    dest->ski_total = src->ski_total;
    dest->tri_total = src->tri_total;
    return dest;
}

struct keyboard_layout* blank_layout(struct keyboard_layout *src)
{
    struct keyboard_layout *dest;
    dest = calloc(1, sizeof(struct keyboard_layout));
    for (int i = 0; i < ROW; i++)
    {
        for (int j = 0; j < COL; j++)
        {
            dest->matrix[i][j] = src->matrix[i][j];
        }
    }
    return dest;
}

void generate()
{
    printf("Generating layouts...\n");
    srand(time(NULL));
    max = copy_layout(current);
    int r1, c1, r2, c2;
    for (int i = generation_quantity; i > 0; i--)
    {
        printf("%d / %d\r", generation_quantity - i, generation_quantity);
        free(current);
        current = blank_layout(max);
        for (int j = (i / (generation_quantity/30)) + 1; j > 0; j--)
        {
            r1 = rand() % 3;
            c1 = rand() % 10;
            r2 = rand() % 3;
            c2 = rand() % 10;
            while(weights->pins[r1][c1])
            {
                r1 = rand() % 3;
                c1 = rand() % 10;
            }
            while(same_pos(r1, c1, r2, c2) || weights->pins[r2][c2])
            {
                r2 = rand() % 3;
                c2 = rand() % 10;
            }
            swap(r1, c1, r2, c2);
        }
        analyze_layout();
        get_score();
        if (current->score > max->score)
        {
            free(max);
            max = copy_layout(current);
        }
    }
    free(current);
    current = copy_layout(max);
}

int main(int argc, char **argv)
{
    //set defaults
    corpus = "shai";
    corpus_malloc = 0;
    layout = "hiyou";
    layout_malloc = 0;
    weight = "default";
    weights_malloc = 0;
    mode = 'a';
    generation_quantity = 100;
    output = 'l';
    //read arguments
    for (int i = 1; i < argc; i++)
    {
        if (argv[i][0] == '-')
        {
            switch (argv[i][1])
            {
                case 'c':
                    if (i + 1 < argc)
                    {
                        corpus = (char*)malloc(strlen(argv[i+1])+1);
                        strcpy(corpus, argv[i+1]);
                        corpus_malloc = 1;
                    }
                    break;
            	case 'l':
            		if (i + 1 < argc)
            		{
                        layout = (char*)malloc(strlen(argv[i+1])+1);
                        strcpy(layout, argv[i+1]);
                        layout_malloc = 1;
            		}
            		break;
            	case 'w':
            		if (i + 1 < argc)
            		{
                        weight = (char*)malloc(strlen(argv[i+1])+1);
                        strcpy(weight, argv[i+1]);
                        weights_malloc = 1;
            		}
            		break;
            	case 'g':
                    if (mode != 'a') {error_out("Too many mode arguments.");}
            		mode = 'g';
            		if (i + 1 < argc && argv[i+1][0] != '-')
            		{
            			generation_quantity = atoi(argv[i+1]);
            		}
                    if (generation_quantity < 50) {error_out("Invalid generation quantity.");}
                    break;
                case 'q':
                    output = 'q';
                    break;
                case 'r':
                    if (mode != 'a') {error_out("Too many mode arguments.");}
                    mode = 'r';
                    break;
                case 'e':
                    error_out("Test Error");
                    break;
            }
        }
    }

    //actually start doing stuff
    switch (mode)
    {
        case 'a':
            read_corpus(corpus);
            read_layout(layout);
            analyze_layout();
            read_weights(weight);
            get_score();
            if (output == 'l') {print_layout();}
            else {short_print();}
            break;
        case 'r':
            read_corpus(corpus);
            read_weights(weight);
            rank_layouts();
            break;
        case 'g':
            read_corpus(corpus);
            read_layout(layout);
            analyze_layout();
            read_weights(weight);
            get_score();
            generate();
            print_layout();
            break;
    }

    if(current != NULL) {free(current);}
    if(max != NULL) {free(max);}
    if(corpus_malloc){free(corpus);}
    if(layout_malloc){free(layout);}
    if(weights_malloc){free(weight);}
}
