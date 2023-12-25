#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>

#define UNUSED(x) (void)(x)

#define ROW 3
#define COL 10
#define LEFT_HAND 4
#define RIGHT_HAND 5

#define LP 0
#define LR 1
#define LM 2
#define LI 3

#define RI 4
#define RM 5
#define RR 6
#define RP 7

static int monogram[128];
static int bigram[128][128];
static int trigram[128][128][128];


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

struct ranking *head = NULL;
struct ranking *ptr = NULL;

struct keyboard_layout
{
    char matrix[ROW][COL];
    double score;
    int mon_total;
    int big_total;
    int ski_total;
    int tri_total;
    int stats[END];
};

struct stat_weights
{
    int pins[3][10];
    double multiplier[END];
};

struct ThreadData
{
    int thread_id;
    int generation_quantity;
    struct keyboard_layout *lt;
    struct stat_weights *wt;
    double score;
};

void error_out(char *message)
{
    printf("Error: %s\n", message);
    printf("Freeing, and exiting.\n");
    while (head != NULL)
    {
        ptr = head;
        head = head->next;
        free(ptr->name);
        free(ptr);
    }
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

char key(int row, int col, struct keyboard_layout *lt)
{
    return lt->matrix[row][col];
}

char hand(int row, int col)
{
    UNUSED(row);
    if (col <= LEFT_HAND) {return 'l';}
    else {return 'r';}
}

int finger(int row, int col)
{
    UNUSED(row);
    switch(col)
    {
        default:
        case 0:
            return LP;
        case 1:
            return LR;
        case 2:
            return LM;
        case 3:
        case 4:
            return LI;
        case 5:
        case 6:
            return RI;
        case 7:
            return RM;
        case 8:
            return RR;
        case 9:
            return RP;
    }
}

int same_finger(int row0, int col0, int row1, int col1)
{
    return finger(row0, col0) == finger (row1, col1);
}

int same_row(int row0, int col0, int row1, int col1)
{
    UNUSED(col0);
    UNUSED(col1);
    return row0 == row1;
}

int same_pos(int row0, int col0, int row1, int col1)
{
    return row0 == row1 && col0 == col1;
}

int same_hand(int row0, int col0, int row1, int col1)
{
    return hand(row0, col0) == hand(row1, col1);
}

int adjacent_finger(int row0, int col0, int row1, int col1)
{
    return same_hand(row0, col0, row1, col1)
        && (finger(row0, col0) - finger(row1, col1) == 1
        || finger(row0, col0) - finger(row1, col1) == -1);
}

int distance(int row0, int col0, int row1, int col1)
{
    UNUSED(col0);
    UNUSED(col1);
    int dist = row0 - row1;
    if (dist >= 0) {return dist;}
    else {return -dist;}
}

int is_stretch(int row0, int col0)
{
    UNUSED(row0);
	return col0 == 4 || col0 == 5;
}

int is_lsb(int row0, int col0, int row1, int col1)
{
    UNUSED(row0);
    UNUSED(row1);
    return ((col0 == 2 && col1 == 4) || (col0 == 4 && col1 == 2)
        || (col0 == 5 && col1 == 7) || (col0 == 7 && col1 == 5));
}

int adjacent_finger_stat(int row0, int col0, int row1, int col1)
{
    return adjacent_finger(row0, col0, row1, col1)
        && !is_lsb(row0, col0, row1, col1);
}

int is_russor(int row0, int col0, int row1, int col1)
{
    return (same_hand(row0, col0, row1, col1)
        && !same_finger(row0, col0, row1, col1)
        && (col0 == 1 || col0 == 2 || col0 == 7 || col0 == 8
            || col1 == 1 || col1 == 2 || col1 == 7 || col1 == 8));
}

int is_one(int row0, int col0, int row1, int col1, int row2, int col2)
{
    return (finger(row0, col0) < finger(row1, col1)
        && finger(row1, col1) < finger(row2, col2))
        ||
        (finger(row0, col0) > finger(row1, col1)
        && finger(row1, col1) > finger(row2, col2));
}

int is_one_in(int row0, int col0, int row1, int col1, int row2, int col2)
{
    UNUSED(row1);
    UNUSED(row2);
    return (hand(row0, col0) == 'l' && col0 < col1)
        || (hand(row0, col0) == 'r' && col1 > col2);
}

int is_same_row_one(int row0, int col0, int row1, int col1, int row2, int col2)
{
    return same_row(row0, col0, row1, col1)
        && same_row(row1, col1, row2, col2)
        && !is_stretch(row0, col0)
        && !is_stretch(row1, col1)
        && !is_stretch(row2, col2);
}

int is_adjacent_finger_one(int row0, int col0, int row1, int col1, int row2, int col2)
{
    return adjacent_finger_stat(row0, col0, row1, col1)
        && adjacent_finger_stat(row1, col1, row2, col2);
}

int is_bad_red(int row0, int col0, int row1, int col1, int row2, int col2)
{
    UNUSED(row0);
    UNUSED(row1);
    UNUSED(row2);
    return ((col0 == 0 || col0 == 1 || col0 == 2 || col0 == 7 || col0 == 8 || col0 == 9)
        && (col1 == 0 || col1 == 1 || col1 == 2 || col1 == 7 || col1 == 8 || col1 == 9)
        && (col2 == 0 || col2 == 1 || col2 == 2 || col2 == 7 || col2 == 8 || col2 == 9));
}

int is_roll(int row0, int col0, int row1, int col1, int row2, int col2)
{
    return same_hand(row0, col0, row1, col1) != same_hand(row1, col1, row2, col2)
        && finger(row0, col0) != finger(row1, col1)
        && finger(row1, col1) != finger(row2, col2);
}

int is_roll_in(int row0, int col0, int row1, int col1, int row2, int col2)
{
    return (same_hand(row0, col0, row1, col1) && hand(row1, col1) == 'l' && col0 < col1)
        || (same_hand(row0, col0, row1, col1) && hand(row1, col1) == 'r' && col0 > col1)
        || (same_hand(row1, col1, row2, col2) && hand(row1, col1) == 'l' && col1 < col2)
        || (same_hand(row1, col1, row2, col2) && hand(row1, col1) == 'r' && col1 > col2);
}

int is_same_row_roll(int row0, int col0, int row1, int col1, int row2, int col2)
{
    return !is_stretch(row0, col0) && !is_stretch(row1, col1) && !is_stretch(row2, col2)
   		&& ((same_hand(row0, col0, row1, col1) && same_row(row0, col0, row1, col1))
            || (same_hand(row1, col1, row2, col2) && same_row(row1, col1, row2, col2)));
}

int is_adjacent_finger_roll(int row0, int col0, int row1, int col1, int row2, int col2)
{
    return (same_hand(row0, col0, row1, col1) && adjacent_finger_stat(row0, col0, row1, col1))
        || (same_hand(row1, col1, row2, col2) && adjacent_finger_stat(row1, col1, row2, col2));
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
                    if (trigram[i][j][k]) {fprintf(data, "t%c%c%c %d\n", i, j, k, trigram[i][j][k]);}
                }
                //print bigrams
                if (bigram[i][j] != 0) {fprintf(data, "b%c%c%c %d\n", i, j, ' ', bigram[i][j]);}
            }
            //print monograms
            if (monogram[i] != 0) {fprintf(data, "m%c%c%c %d\n", i, ' ', ' ', monogram[i]);}
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
                    fscanf(data, "%d", &monogram[a]);
                    fgetc(data);
                    break;
                case 'b':
                    a = fgetc(data);
                    b = fgetc(data);
                    fscanf(data, "%d", &bigram[a][b]);
                    fgetc(data);
                    break;
                case 't':
                    a = fgetc(data);
                    b = fgetc(data);
                    c = fgetc(data);
                    fscanf(data, "%d", &trigram[a][b][c]);
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

void read_layout(char *name, struct keyboard_layout **lt)
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
    //just in case this pointer isn't free
    if (*lt != NULL) {free(*lt);}
    *lt = calloc(1, sizeof(struct keyboard_layout));
    //read layout
    for (int  i = 0; i < ROW; i++)
    {
        for (int j = 0; j < COL; j++)
        {
            fscanf(data, " %c", &(*lt)->matrix[i][j]);
        }
    }
    free(layout);
    fclose(data);
}

//sft, bsft, lst, hrt, frt, red, onehands, rolls, sft by finger
void analyze_trigram(int row0, int col0, int row1, int col1, int row2, int col2,  int value, struct keyboard_layout *lt)
{
    if (value == 0) {return;}
    lt->tri_total += value;
    //sft
    if(same_finger(row0, col0, row1, col1) && same_finger(row1, col1, row2, col2)
        && !same_pos(row0, col0, row1, col1) && !same_pos(row1, col1, row2, col2))
    {
        lt->stats[SFT] += value;
        //bad sft
        if (distance(row0, col0, row1, col1) == 2 || distance(row1, col1, row2, col2) == 2)
        {
            lt->stats[BAD_SFT] += value;
        }
        //sft by finger
        switch(col0)
        {
            case 0:
                lt->stats[LPT] += value;
                break;
            case 1:
                lt->stats[LRT] += value;
                break;
            case 2:
                lt->stats[LMT] += value;
                break;
            case 4:
                lt->stats[LLT] += value;
            case 3:
                lt->stats[LIT] += value;
                break;
            case 5:
                lt->stats[RLT] += value;
            case 6:
                lt->stats[RIT] += value;
                break;
            case 7:
                lt->stats[RMT] += value;
                break;
            case 8:
                lt->stats[RRT] += value;
                break;
            case 9:
                lt->stats[RPT] += value;
                break;
        }
    }
    //lst
    if (is_lsb(row0, col0, row1, col1) && is_lsb(row1, col1, row2, col2))
    {
        lt->stats[LST] += value;
    }
    //russors -- needs fixing
    if (is_russor(row0, col0, row1, col1) && is_russor(row1, col1, row2, col2))
    {
        if (distance(row0, col0, row1, col1) == 1 && distance(row1, col1, row2, col2) == 1)
        {
            lt->stats[HRT] += value;
        }
        if (distance(row0, col0, row1, col1) == 2 && distance(row1, col1, row2, col2) == 2)
        {
            lt->stats[FRT] += value;
        }
    }
    //alt
    if (same_hand(row0, col0, row2, col2) && !same_hand(row0, col0, row1, col1))
    {
        lt->stats[ALT] += value;
    }

    //redirects and onehands
    if (same_hand(row0, col0, row1, col1) && same_hand(row1, col1, row2, col2)
        && !same_finger(row0, col0, row1, col1)
        && !same_finger(row1, col1, row2, col2)
        && !same_finger(row0, col0, row2, col2))
    {
        //one
        if (is_one(row0, col0, row1, col1, row2, col2))
        {
            int one_in = is_one_in(row0, col0, row1, col1, row2, col2);
            int one_sr = is_same_row_one(row0, col0, row1, col1, row2, col2);
            int one_af = is_adjacent_finger_one(row0, col0, row1, col1, row2, col2);
            lt->stats[ONE]     += value;
            lt->stats[ONE_IN]  += value * one_in;
            lt->stats[ONE_OUT] += value * !one_in;

            lt->stats[SR_ONE]     += value * one_sr;
            lt->stats[SR_ONE_IN]  += value * one_sr * one_in;
            lt->stats[SR_ONE_OUT] += value * one_sr * !one_in;

            lt->stats[AF_ONE]     += value * one_af;
            lt->stats[AF_ONE_IN]  += value * one_af * one_in;
            lt->stats[AF_ONE_OUT] += value * one_af * !one_in;

            lt->stats[SRAF_ONE]     += value * one_sr * one_af;
            lt->stats[SRAF_ONE_IN]  += value * one_sr * one_af * one_in;
            lt->stats[SRAF_ONE_OUT] += value * one_sr * one_af * !one_in;
        }
        //redirects
        else
        {
            lt->stats[RED] += value;
            if (is_bad_red(row0, col0, row1, col1, row2, col2))
            {
                lt->stats[BAD_RED] += value;
            }
        }
    }

    //rolls
    if (is_roll(row0, col0, row1, col1, row2, col2))
    {
        int roll_in = is_roll_in(row0, col0, row1, col1, row2, col2);
        int roll_sr = is_same_row_roll(row0, col0, row1, col1, row2, col2);
        int roll_af = is_adjacent_finger_roll(row0, col0, row1, col1, row2, col2);

        lt->stats[ROL]     += value;
        lt->stats[ROL_IN]  += value * roll_in;
        lt->stats[ROL_OUT] += value * !roll_in;

        lt->stats[SR_ROL]     += value * roll_sr;
        lt->stats[SR_ROL_IN]  += value * roll_sr * roll_in;
        lt->stats[SR_ROL_OUT] += value * roll_sr * !roll_in;

        lt->stats[AF_ROL]     += value * roll_af;
        lt->stats[AF_ROL_IN]  += value * roll_af * roll_in;
        lt->stats[AF_ROL_OUT] += value * roll_af * !roll_in;

        lt->stats[SRAF_ROL]     += value * roll_sr * roll_af;
        lt->stats[SRAF_ROL_IN]  += value * roll_sr * roll_af * roll_in;
        lt->stats[SRAF_ROL_OUT] += value * roll_sr * roll_af * !roll_in;
    }
}

//sfs, bsfs,  lss, hrs, frs, sfs by finger
void analyze_skipgram(int row0, int col0, int row2, int col2, int value, struct keyboard_layout *lt)
{
    if (value == 0) {return;}
    lt->ski_total += value;
    //sfs
    if(same_finger(row0, col0, row2, col2) && !same_pos(row0, col0, row2, col2))
    {
        lt->stats[SFS] += value;
        //bad sfs
        if (distance(row0, col0, row2, col2) == 2) {lt->stats[BAD_SFS] += value;}
        //sfs by finger
        switch(col0)
        {
            case 0:
                lt->stats[LPS] += value;
                break;
            case 1:
                lt->stats[LRS] += value;
                break;
            case 2:
                lt->stats[LMS] += value;
                break;
            case 4:
            case 3:
                lt->stats[LIS] += value;
                break;
            case 5:
            case 6:
                lt->stats[RIS] += value;
                break;
            case 7:
                lt->stats[RMS] += value;
                break;
            case 8:
                lt->stats[RRS] += value;
                break;
            case 9:
                lt->stats[RPS] += value;
                break;
        }
        if (col0 == 4 || col2 == 4)
        {
            lt->stats[LLS] += value;
        }
        if (col0 == 5 || col2 == 5)
        {
            lt->stats[RLS] += value;
        }
    }
    //lss
    if (is_lsb(row0, col0, row2, col2)) {lt->stats[LSS] += value;}
    //russors
    if (is_russor(row0, col0, row2, col2))
    {
        if (distance(row0, col0, row2, col2) == 1) {lt->stats[HRS] += value;}
        if (distance(row0, col0, row2, col2) == 2) {lt->stats[FRS] += value;}
    }
}

//sfb, bsfb, lsb, hrb, frb, alt, sfb by finger
void analyze_bigram(int row0, int col0, int row1, int col1, int value, struct keyboard_layout *lt)
{
    if (value == 0) {return;}
    lt->big_total += value;
    //sfb
    if(same_finger(row0, col0, row1, col1) && !same_pos(row0, col0, row1, col1))
    {
        lt->stats[SFB] += value;
        //bad sfb
        if (distance(row0, col0, row1, col1) == 2) {lt->stats[BAD_SFB] += value;}
        //sfb by finger
        switch(col0)
        {
            case 0:
                lt->stats[LPB] += value;
                break;
            case 1:
                lt->stats[LRB] += value;
                break;
            case 2:
                lt->stats[LMB] += value;
                break;
            case 4:
            case 3:
                lt->stats[LIB] += value;
                break;
            case 5:
            case 6:
                lt->stats[RIB] += value;
                break;
            case 7:
                lt->stats[RMB] += value;
                break;
            case 8:
                lt->stats[RRB] += value;
                break;
            case 9:
                lt->stats[RPB] += value;
                break;
        }
        if (col0 == 4 || col1 == 4)
        {
            lt->stats[LLB] += value;
        }
        if (col0 == 5 || col1 == 5)
        {
            lt->stats[RLB] += value;
        }
    }
    //lsb
    if (is_lsb(row0, col0, row1, col1)) {lt->stats[LSB] += value;}
    //russors
    if (is_russor(row0, col0, row1, col1))
    {
        if (distance(row0, col0, row1, col1) == 1) {lt->stats[HRB] += value;}
        if (distance(row0, col0, row1, col1) == 2) {lt->stats[FRB] += value;}
    }
}

//hand, row, and finger usage
void analyze_monogram(int row, int col, int value, struct keyboard_layout *lt)
{
    if (value == 0) {return;}
    lt->mon_total += value;
    //hand usage
    if (hand(row, col) == 'l') {lt->stats[LHU] += value;}
    else                       {lt->stats[RHU] += value;}
    //row usage
    if (row == 0)      {lt->stats[TRU] += value;}
    else if (row == 1) {lt->stats[HRU] += value;}
    else               {lt->stats[BRU] += value;}
    //finger usage
    switch(col)
    {
        case 0:
            lt->stats[LPU] += value;
            break;
        case 1:
            lt->stats[LRU] += value;
            break;
        case 2:
            lt->stats[LMU] += value;
            break;
        //FOR STRETCH FINGERS IT COUNTS FOR BOTH STRETCH AND INDEX USAGE
        case 4:
            lt->stats[LLU] += value;
        case 3:
            lt->stats[LIU] += value;
            break;
        //SAME HERE
        case 5:
            lt->stats[RLU] += value;
        case 6:
            lt->stats[RIU] += value;
            break;
        case 7:
            lt->stats[RMU] += value;
            break;
        case 8:
            lt->stats[RRU] += value;
            break;
        case 9:
            lt->stats[RPU] += value;
            break;
    }
}

void analyze_layout(struct keyboard_layout *lt)
{
    //first key
    for (int row0 = 0; row0 < ROW; row0++)
    {
        for (int col0 = 0; col0 < COL; col0++)
        {
            analyze_monogram(row0, col0, monogram[key(row0, col0, lt)], lt);

            //second key
            for (int row1 = 0; row1 < ROW; row1++)
            {
                for (int col1 = 0; col1 < COL; col1++)
                {
                    analyze_bigram(row0, col0, row1, col1, bigram[key(row0, col0, lt)][key(row1, col1, lt)], lt);
                    //skipgrams with all possible trigrams, not just on this layout
                    for (int i = 0; i < 128; i++)
                    {
                        analyze_skipgram(row0, col0, row1, col1, trigram[key(row0, col0, lt)][i][key(row1, col1, lt)], lt);
                    }
                    //third key
                    for (int row2 = 0; row2 < ROW; row2++)
                    {
                        for (int col2 = 0; col2 < COL; col2++)
                        {
                            analyze_trigram(row0, col0, row1, col1, row2, col2, trigram[key(row0, col0, lt)][key(row1, col1, lt)][key(row2, col2, lt)], lt);
                        }
                    }
                }
            }
        }
    }
    if (lt->stats[RHU] > lt->stats[LHU])
    {
        lt->stats[HAND_BALANCE] = lt->stats[RHU] - lt->stats[LHU];
    }
    else
    {
        lt->stats[HAND_BALANCE] = lt->stats[LHU] - lt->stats[RHU];
    }
}

void read_weights(char *name, struct stat_weights **wt)
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
    //just in case weights already exist
    if (*wt != NULL) {free(*wt);}
    *wt = calloc(1, sizeof(struct stat_weights));
    for (int i = 0; i < ROW; i++)
    {
        for (int j = 0; j < COL; j++)
        {
            fscanf(data, " %c", &temp);
            if (temp == '.') {(*wt)->pins[i][j] = 0;}
            else {(*wt)->pins[i][j] = 1;}
        }
    }
    for(enum patterns i = SFB; i < END; i++)
    {
        fscanf(data, " %*[^0-9.-]%lf", &(*wt)->multiplier[i]);
    }
    free(weight);
    fclose(data);
}

int total(enum patterns stat, struct keyboard_layout *lt)
{
    switch(totals[stat])
    {
        case('m'):
            return lt->mon_total;
            break;
        case('b'):
            return lt->big_total;
            break;
        case('s'):
            return lt->ski_total;
            break;
        default:
            return lt->tri_total;
            break;
    }
}

void get_score(struct keyboard_layout *lt, struct stat_weights *wt)
{
    for (enum patterns i = SFB; i < END; i++)
    {
        lt->score += ((double)lt->stats[i]/total(i, lt)*100) * wt->multiplier[i];
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

void print_layout(struct keyboard_layout *lt)
{
    printf("Score : %f\n", lt->score);
    puts("");
    for (int i = 0; i < ROW; i++)
    {
        for (int j = 0; j < COL; j++)
        {
            if (j == RIGHT_HAND) {printf(" ");}
            printf("%c ", lt->matrix[i][j]);
        }
        puts("");
    }
    puts("");
    for (enum patterns i = SFB; i < LPU; i++)
    {
        printf("%s: %06.3f%%", names[i], (double)lt->stats[i]/total(i, lt) * 100);
        if (print_new_line(i)) {puts("");}
        else {printf(" | ");}
    }
    printf("Finger Usage: \n[");
    for (enum patterns i = LPU; i < RPU; i++)
    {
        printf("%05.2f, ", (double)lt->stats[i]/total(i, lt) * 100);
    }
    printf("%05.2f]\nFinger Bigrams: \n[", (double)lt->stats[RPU]/total(RPU, lt) * 100);
    for (enum patterns i = LPB; i < RPB; i++)
    {
        printf("%05.2f, ", (double)lt->stats[i]/total(i, lt) * 100);
    }
    printf("%05.2f]\nFinger Skipgrams: \n[", (double)lt->stats[RPB]/total(RPB, lt) * 100);
    for (enum patterns i = LPS; i < RPS; i++)
    {
        printf("%05.2f, ", (double)lt->stats[i]/total(i, lt) * 100);
    }
    printf("%05.2f]\nFinger Trigrams: \n[", (double)lt->stats[RPS]/total(RPS, lt) * 100);
    for (enum patterns i = LPT; i < RPT; i++)
    {
        printf("%05.2f, ", (double)lt->stats[i]/total(i, lt) * 100);
    }
    printf("%05.2f]\n", (double)lt->stats[RPT]/total(RPT, lt) * 100);
}

void short_print(struct keyboard_layout *lt)
{
    for (int i = 0; i < ROW; i++)
    {
        for (int j = 0; j < COL; j++)
        {
            if (j == RIGHT_HAND) {printf(" ");}
            printf("%c ", lt->matrix[i][j]);
        }
        puts("");
    }
    puts("");
    for (enum patterns i = SFB; i < LPU; i++)
    {
        if (i == SR_ONE) {i = ROL;}
        printf("%s: %06.3f%%", names[i], (double)lt->stats[i]/total(i, lt) * 100);
        if (print_new_line(i)) {puts("");}
        else {printf(" | ");}
    }
    printf("Finger Usage: \n[");
    for (enum patterns i = LPU; i < RPU; i++)
    {
        printf("%05.2f, ", (double)lt->stats[i]/total(i, lt) * 100);
    }
    printf("%05.2f]\n", (double)lt->stats[RPU]/total(RPU, lt) * 100);
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

void rank_layouts(struct stat_weights *wt)
{
    DIR *dir;
    struct dirent *entry;
    dir = opendir("./layouts");
    if (dir == NULL)
    {
        error_out("No layouts found.");
        return;
    }

    struct keyboard_layout *lt;

    while ((entry = readdir(dir)) != NULL)
    {
        if (entry->d_type == DT_REG)
        {
            if (lt != NULL) {free(lt);}
            lt = malloc(sizeof(struct keyboard_layout));
            read_layout(entry->d_name, &lt);
            analyze_layout(lt);
            get_score(lt, wt);
            struct ranking *node = malloc(sizeof(struct ranking));
            node->name = (char*)malloc(strlen(entry->d_name) + 1);
            strcpy(node->name, entry->d_name);
            node->score = lt->score;
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
            free(lt);
            lt = NULL;
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

void swap (int row0, int col0, int row1, int col1, struct keyboard_layout *lt)
{
    char temp = lt->matrix[row0][col0];
    lt->matrix[row0][col0] = lt->matrix[row1][col1];
    lt->matrix[row1][col1] = temp;
}

void shuffle(struct keyboard_layout *lt)
{
    srand(time(NULL));
    for (int i = ROW - 1; i > 0; i--)
    {
        for (int j = COL - 1; j > 0; j--)
        {
            int row0 = i;
            int col0 = j;
            int row1 = rand() % (i + 1);
            int col1 = rand() % (j + 1);
            if (row0 != row1 || col0 != col1)
            {
                swap(row0, col0, row1, col1, lt);
            }
        }
    }
}

void copy_layout(struct keyboard_layout *src, struct keyboard_layout **dest)
{
    if (*dest != NULL) {free(*dest);}
    *dest = calloc(1, sizeof(struct keyboard_layout));
    for (int i = 0; i < ROW; i++)
    {
        for (int j = 0; j < COL; j++)
        {
            (*dest)->matrix[i][j] = src->matrix[i][j];
        }
    }
    for (enum patterns i = SFB; i < END; i++)
    {
        (*dest)->stats[i] = src->stats[i];
    }
    (*dest)->score = src->score;
    (*dest)->mon_total = src->mon_total;
    (*dest)->big_total = src->big_total;
    (*dest)->ski_total = src->ski_total;
    (*dest)->tri_total = src->tri_total;
}

void blank_layout(struct keyboard_layout *src, struct keyboard_layout **dest)
{
    if (*dest != NULL) {free(*dest);}
    *dest = calloc(1, sizeof(struct keyboard_layout));
    for (int i = 0; i < ROW; i++)
    {
        for (int j = 0; j < COL; j++)
        {
            (*dest)->matrix[i][j] = src->matrix[i][j];
        }
    }
}

void generate(int generation_quantity, struct keyboard_layout **lt, struct stat_weights *wt, int seed)
{
    if (seed == 0) {printf("Generating layouts...\n");}
    srand(time(NULL));
    for (int i = 0; i < seed; i++) {rand();}
    struct keyboard_layout *max = NULL;
    copy_layout(*lt, &max);
    int row0, col0, row1, col1;
    for (int i = generation_quantity; i > 0; i--)
    {
        printf("%d / %d\r", generation_quantity - i, generation_quantity);
        blank_layout(max, &(*lt));
        for (int j = (i / (generation_quantity/30)) + 1; j > 0; j--)
        {
            row0 = rand() % 3;
            col0 = rand() % 10;
            row1 = rand() % 3;
            col1 = rand() % 10;
            while(wt->pins[row0][col0])
            {
                row0 = rand() % 3;
                col0 = rand() % 10;
            }
            while(same_pos(row0, col0, row1, col1) || wt->pins[row1][col1])
            {
                row1 = rand() % 3;
                col1 = rand() % 10;
            }
            swap(row0, col0, row1, col1, *lt);
        }
        analyze_layout(*lt);
        get_score(*lt, wt);
        if ((*lt)->score > max->score)
        {
            free(max);
            max = NULL;
            copy_layout(*lt, &max);
        }
    }
    copy_layout(max, &(*lt));
    free(max);
}

void *thread_gen(void* arg)
{
    struct ThreadData* data = (struct ThreadData*)arg;
    //do the generation stuff
    generate(data->generation_quantity, &(data->lt), data->wt, data->thread_id);
    data->score = data->lt->score;
    pthread_exit(NULL);
}

void analyze_mode(char *corpus, char *layout, char *weight, char output)
{
    struct keyboard_layout *lt = NULL;
    struct stat_weights *wt = NULL;
    read_corpus(corpus);
    read_layout(layout, &lt);
    analyze_layout(lt);
    read_weights(weight, &wt);
    get_score(lt, wt);
    if (output == 'l') {print_layout(lt);}
    else {short_print(lt);}
    free(lt);
    free(wt);
}

void rank_mode(char *corpus, char* weight)
{
    struct stat_weights *wt = NULL;
    read_corpus(corpus);
    read_weights(weight, &wt);
    rank_layouts(wt);
    free(wt);
}

void generate_mode(char *corpus, char *layout, char *weight, int generation_quantity, int improve, char output)
{
    struct keyboard_layout *lt = NULL;
    struct stat_weights *wt = NULL;
    read_corpus(corpus);
    read_layout(layout, &lt);
    if (!improve)
    {
        for (int i = 0; i < 10; i++)
        {
            shuffle(lt);
        }
    }
    analyze_layout(lt);
    read_weights(weight, &wt);
    get_score(lt, wt);
    generate(generation_quantity, &lt, wt, 0);
    if (output == 'l') {print_layout(lt);}
    else {short_print(lt);}
    free(lt);
    free(wt);
}

void multi_mode(char *corpus, char *layout, char *weight, int generation_quantity, int improve, char output)
{
    struct keyboard_layout *lt = NULL;
    struct stat_weights *wt = NULL;
    read_corpus(corpus);
    read_layout(layout, &lt);
    if (!improve)
    {
        for (int i = 0; i < 10; i++)
        {
            shuffle(lt);
        }
    }
    analyze_layout(lt);
    read_weights(weight, &wt);
    get_score(lt, wt);
    //hmmm
    printf("Multithreaded mode\n");
    int numThread = sysconf(_SC_NPROCESSORS_CONF);
    pthread_t threads[numThread];
    printf("Identified %d cores: using %d threads\n", numThread, numThread);
    int genThread = generation_quantity / numThread;
    printf("Total generation quantity was %d -> %d per thread\n", generation_quantity, genThread);
    struct ThreadData data[numThread];
    for (int i = 0; i < numThread; i++)
    {
        data[i].thread_id = i;
        data[i].generation_quantity = genThread;
        data[i].lt = NULL;
        copy_layout(lt, &(data[i].lt));
        data[i].wt = wt;
        data[i].score = lt->score;

        pthread_create(&threads[i], NULL, thread_gen, (void*)&data[i]);
    }
    for (int i = 0; i < numThread; i++)
    {
        pthread_join(threads[i], NULL);
    }
    struct keyboard_layout *best_layout = NULL;
    int best_thread = 0;
    for (int i = 1; i < numThread; i++)
    {
        if (data[i].score > data[best_thread].score)
        {
            best_thread = i;
        }
    }
    copy_layout(data[best_thread].lt, &best_layout);
    for (int i = 0; i < numThread; i++)
    {
        free(data[i].lt);
    }
    printf("Thread %d was best\n", best_thread);
    if (output == 'l') {print_layout(best_layout);}
    else {short_print(best_layout);}
    free(best_layout);
    free(lt);
    free(wt);
}

int main(int argc, char **argv)
{
    //set defaults
    char *corpus = "shai";
    int corpus_malloc = 0;
    char *layout = "hiyou";
    int layout_malloc = 0;
    char *weight = "default";
    int weights_malloc = 0;
    char mode = 'a';
    int generation_quantity = 100;
    char output = 'l';
    int improve = 0;
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
                case 'i':
                    improve = 1;
                    break;
                case 'e':
                    error_out("Test Error");
                    break;
                case 'm':
                    mode = 'm';
                    if (i + 1 < argc && argv[i+1][0] != '-')
            		{
            			generation_quantity = atoi(argv[i+1]);
            		}
                    if (generation_quantity < 50) {error_out("Invalid generation quantity.");}
                    break;
            }
        }
    }

    //actually start doing stuff
    switch (mode)
    {
        case 'a':
            analyze_mode(corpus, layout, weight, output);
            break;
        case 'r':
            rank_mode(corpus, weight);
            break;
        case 'g':
            generate_mode(corpus, layout, weight, generation_quantity, improve, output);
            break;
        case 'm':
            multi_mode(corpus, layout, weight, generation_quantity, improve, output);
            break;
    }

    if(corpus_malloc){free(corpus);}
    if(layout_malloc){free(layout);}
    if(weights_malloc){free(weight);}
	return EXIT_SUCCESS;
}
