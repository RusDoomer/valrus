/*
 * File: main.c
 * Author: RusDoomer
 * Purpose: yeah this is the only file it does the whole thing.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>

#define UNUSED(x) (void)(x)

// some useful constants
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

// global arrays for holding ngram data
static int monogram[128];
static int bigram[128][128];
static int trigram[128][128][128];

// ngram patterns
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

// ngram pattern names as strings
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

/*
 * defines whether each ngram pattern is for monograms, bigrams, trigrams,
 * or skipgrams to associate them with the right total values for finding
 * percentages for layout stats
 */
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

// linked list for the rank command
struct ranking
{
    char *name;
    double score;
    struct ranking *next;
};

struct ranking *head = NULL;
struct ranking *ptr = NULL;

//the layout, includes keys, score, totals for each type of ngram, and stats
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

// pins are keys that won't be moved when improving the layout
struct stat_weights
{
    int pins[3][10];
    double multiplier[END];
};

// data for each thread
struct ThreadData
{
    int thread_id;
    int generation_quantity;
    struct keyboard_layout *lt;
    struct stat_weights *wt;
    double score;
};

// in case something goes wrong, displays message and frees the rank linked list
void error_out(char *message)
{
    printf("Error: %s\n", message);
    printf("Freeing, and exiting.\n");

    // free rank linked list if it was allocated to heap
    while (head != NULL)
    {
        ptr = head;
        head = head->next;
        free(ptr->name);
        free(ptr);
    }

    exit(1);
}

/*
 * converts any char into one suitable for comparing layouts
 * takes shifted pairs of common symbols and all letters and lowercases them
 * converts all unknown/weird characters into spaces
 */
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

/*
 * returns the key that is found at that position in the layout
 * row, col is the position on the 3x10 grid, lt is the layout
 */
char key(int row, int col, struct keyboard_layout *lt)
{
    return lt->matrix[row][col];
}

// returns whether the position given by row, col is on the left or right hand
char hand(int row, int col)
{
    UNUSED(row);

    if (col <= LEFT_HAND) {return 'l';}
    else {return 'r';}
}

// returns which physical finger the position at row, col is to be pressed by
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

// returns whether the keys at row0, col0 and row1, col1 are on the same finger
int same_finger(int row0, int col0, int row1, int col1)
{
    return finger(row0, col0) == finger (row1, col1);
}

// returns whether the keys at row0, col0 and row1, col1 are on the same row
int same_row(int row0, int col0, int row1, int col1)
{
    UNUSED(col0);
    UNUSED(col1);

    return row0 == row1;
}

// returns whether the keys at row0, col0 and row1, col1 are on the position
int same_pos(int row0, int col0, int row1, int col1)
{
    return row0 == row1 && col0 == col1;
}

// returns whether the keys at row0, col0 and row1, col1 are on the same hand
int same_hand(int row0, int col0, int row1, int col1)
{
    return hand(row0, col0) == hand(row1, col1);
}

// returns whether the keys at row0, col0 and row1, col1 are on adjacent fingers
int adjacent_finger(int row0, int col0, int row1, int col1)
{
    return same_hand(row0, col0, row1, col1)
        && (finger(row0, col0) - finger(row1, col1) == 1
        || finger(row0, col0) - finger(row1, col1) == -1);
}

// returns the vertical distance between the keys at row0, col0 and row1, col1
int distance(int row0, int col0, int row1, int col1)
{
    UNUSED(col0);
    UNUSED(col1);

    int dist = row0 - row1;
    if (dist >= 0) {return dist;}
    else {return -dist;}
}

// returns whether the key at row0, col0 is on one of the stretch columns
int is_stretch(int row0, int col0)
{
    UNUSED(row0);

	return col0 == 4 || col0 == 5;
}

/*
 * returns whether the key pair at row0, col0 and row1, col1 forms an LSB
 * LSB defined as a key on the middle finger and a key on the stretch index
 * positions of the same hand
 */
int is_lsb(int row0, int col0, int row1, int col1)
{
    UNUSED(row0);
    UNUSED(row1);

    return ((col0 == 2 && col1 == 4) || (col0 == 4 && col1 == 2)
        || (col0 == 5 && col1 == 7) || (col0 == 7 && col1 == 5));
}

/*
 * returns whether the key pair at row0, col0 and row1, col1 fit the adjacent
 * finger stat, defined as being on adjacent fingers, and not being an LSB
 */
int adjacent_finger_stat(int row0, int col0, int row1, int col1)
{
    return adjacent_finger(row0, col0, row1, col1)
        && !is_lsb(row0, col0, row1, col1);
}

/*
 * returns whether the key pair at row0, col0 and row1, col1 form a russor
 * a russor is defined by two keys on different fingers on the same hand
 * with one key being on the ring or middle finger, when this is used later you
 * will need to define whether it is a full or half russor by the distance
 * between the keys, 2 for full, 1 for half
 */
int is_russor(int row0, int col0, int row1, int col1)
{
    return (same_hand(row0, col0, row1, col1)
        && !same_finger(row0, col0, row1, col1)
        && (col0 == 1 || col0 == 2 || col0 == 7 || col0 == 8
            || col1 == 1 || col1 == 2 || col1 == 7 || col1 == 8));
}

/*
 * returns whether the keys at row0, col0,  row1, col1,  row2, col2
 * form a onehand
 * a onehand is defined by three keys on different fingers on the same hand
 * all "moving" in one direction, either towards or away from the index
 */
int is_one(int row0, int col0, int row1, int col1, int row2, int col2)
{
    return (finger(row0, col0) < finger(row1, col1)
        && finger(row1, col1) < finger(row2, col2))
        ||
        (finger(row0, col0) > finger(row1, col1)
        && finger(row1, col1) > finger(row2, col2));
}

/*
 * returns whether the onehand formed by row0, col0,  row1, col1,  row2, col2
 * moves inwards towards the index
 */
int is_one_in(int row0, int col0, int row1, int col1, int row2, int col2)
{
    UNUSED(row1);
    UNUSED(row2);

    return (hand(row0, col0) == 'l' && col0 < col1)
        || (hand(row0, col0) == 'r' && col1 > col2);
}

/*
 * returns whether the onehand formed by row0, col0,  row1, col1,  row2, col2
 * has all three keys on the same row, and none on the stretch index position
 */
int is_same_row_one(int row0, int col0, int row1, int col1, int row2, int col2)
{
    return same_row(row0, col0, row1, col1)
        && same_row(row1, col1, row2, col2)
        && !is_stretch(row0, col0)
        && !is_stretch(row1, col1)
        && !is_stretch(row2, col2);
}

/*
 * returns whether the onehand formed by row0, col0,  row1, col1,  row2, col2
 * fits the adjacent finger stat, meaning that all three fingers are adjacent
 * and do not form an LSB
 */
int is_adjacent_finger_one(int row0, int col0, int row1, int col1, int row2, int col2)
{
    return adjacent_finger_stat(row0, col0, row1, col1)
        && adjacent_finger_stat(row1, col1, row2, col2);
}

/*
 * returns whether the redirect formed by row0, col0,  row1, col1,  row2, col2
 * is a bad redirect, meaning that none of the keys fall on the index
 */
int is_bad_red(int row0, int col0, int row1, int col1, int row2, int col2)
{
    UNUSED(row0);
    UNUSED(row1);
    UNUSED(row2);

    return finger(row0, col0) != LI && finger(row0, col0) != RI
        && finger(row1, col1) != LI && finger(row1, col1) != RI
        && finger(row2, col2) != LI && finger(row2, col2) != RI;
}

/*
 * returns whether the keys at row0, col0,  row1, col1,  row2, col2 form a roll
 * a roll being a trigram where two consecutive keys are on different fingers on
 * one hand and the third key is on the other hand
 */
int is_roll(int row0, int col0, int row1, int col1, int row2, int col2)
{
    return same_hand(row0, col0, row1, col1) != same_hand(row1, col1, row2, col2)
        && finger(row0, col0) != finger(row1, col1)
        && finger(row1, col1) != finger(row2, col2);
}

/*
 * returns whether the roll with keys at row0, col0,  row1, col1,  row2, col2
 * has the two keys on the same hand moving inwards towards the index
 */
int is_roll_in(int row0, int col0, int row1, int col1, int row2, int col2)
{
    return (same_hand(row0, col0, row1, col1) && hand(row1, col1) == 'l' && col0 < col1)
        || (same_hand(row0, col0, row1, col1) && hand(row1, col1) == 'r' && col0 > col1)
        || (same_hand(row1, col1, row2, col2) && hand(row1, col1) == 'l' && col1 < col2)
        || (same_hand(row1, col1, row2, col2) && hand(row1, col1) == 'r' && col1 > col2);
}

/*
 * returns whether the roll with keys at row0, col0,  row1, col1,  row2, col2
 * has the two keys on the hand on the same row
 */
int is_same_row_roll(int row0, int col0, int row1, int col1, int row2, int col2)
{
    return !is_stretch(row0, col0) && !is_stretch(row1, col1) && !is_stretch(row2, col2)
   		&& ((same_hand(row0, col0, row1, col1) && same_row(row0, col0, row1, col1))
            || (same_hand(row1, col1, row2, col2) && same_row(row1, col1, row2, col2)));
}

/*
 * returns whether the roll with keys at row0, col0,  row1, col1,  row2, col2
 * matches the adjacent finger stat, meaning the two keys on the same hand
 * are on adjacent fingers and do not form an LSB
 */
int is_adjacent_finger_roll(int row0, int col0, int row1, int col1, int row2, int col2)
{
    return (same_hand(row0, col0, row1, col1) && adjacent_finger_stat(row0, col0, row1, col1))
        || (same_hand(row1, col1, row2, col2) && adjacent_finger_stat(row1, col1, row2, col2));
}

/*
 * takes corpus and fills ngram arrays with data from the corpus
 * name is the file to be read, either from ./ngram or ./raw
 */
void read_corpus(char *name)
{
    // define variables
    FILE *data;
    char *ngram, *raw;
    int a, b, c;

    // get full filename for ngram data of layout
    ngram = (char*)malloc(strlen("./corpora/ngram/") + strlen(name) + 1);
    strcpy(ngram, "./corpora/ngram/");
    strcat(ngram, name);

    // get full filename for raw data of layout
    raw = (char*)malloc(strlen("./corpora/raw/") + strlen(name) + 1);
    strcpy(raw, "./corpora/raw/");
    strcat(raw, name);

    // check if ngram data exists, create if doesn't
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

        // read corpus from raw file and create ngram file
        a = fgetc(data);
        b = fgetc(data);

        // fill buffer by reading first two chars
        if (a != EOF && b != EOF)
        {
            a = convert_char(a);
            b = convert_char(b);
            monogram[a]++;
            monogram[b]++;
            bigram[a][b]++;
        }
        else {error_out("Corpus not long enough.");}

        // read corpus file character by character, filling ngram arrays
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

        // actually write ngram file
        data = fopen(ngram, "w");
        for (int i = 0; i < 128; i++)
        {
            for (int j = 0; j < 128; j++)
            {
                for (int k = 0; k < 128; k++)
                {
                    // print trigrams
                    if (trigram[i][j][k]) {fprintf(data, "t%c%c%c %d\n", i, j, k, trigram[i][j][k]);}
                }
                // print bigrams
                if (bigram[i][j] != 0) {fprintf(data, "b%c%c%c %d\n", i, j, ' ', bigram[i][j]);}
            }
            // print monograms
            if (monogram[i] != 0) {fprintf(data, "m%c%c%c %d\n", i, ' ', ' ', monogram[i]);}
        }
    }

    // use ngram data if it exists
    else
    {
        // read corpus from ngram file
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

/*
 * reads layout from file into the matrix of a layout struct
 * name is the name of the layout file in ./layouts
 * **lt is a pointer to an empty pointer for a layout
 * rus note: should be look something like read_layout(hiyou, &lt);
 */
void read_layout(char *name, struct keyboard_layout **lt)
{
    // find layout file
    FILE * data;
    char *layout = (char*)malloc(strlen("./layouts/") + strlen(name) + 1);
    strcpy(layout, "./layouts/");
    strcat(layout, name);
    data = fopen(layout, "r");

    // check if layout exists
    if (data == NULL)
    {
        free(layout);
        error_out("No such layout was found.");
        return;
    }

    // just in case this pointer isn't free
    if (*lt != NULL) {free(*lt);}
    *lt = calloc(1, sizeof(struct keyboard_layout));

    // read layout
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

/*
 * analyzes monogram stats - stats for a single key presses
 * row, col is the position of the key
 * value is the number of times this monogram appears in the corpus
 * lt is the layout that is being analyzed
 * the stats here are :
 * Hand Usage, Row Usage, Finger Usage
 */
void analyze_monogram(int row, int col, int value, struct keyboard_layout *lt)
{
    // throw away never seen monograms
    if (value == 0) {return;}
    lt->mon_total += value;

    //Hand Usage, the amount of the total corpus typed on each hand
    if (hand(row, col) == 'l') {lt->stats[LHU] += value;}
    else                       {lt->stats[RHU] += value;}

    //Row Usage, the amount of the total corpus typed on each row
    if (row == 0)      {lt->stats[TRU] += value;}
    else if (row == 1) {lt->stats[HRU] += value;}
    else               {lt->stats[BRU] += value;}

    //Finger Usage, the amount of the total corpus typed on each finger
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
        //For stretch positions it counts for both the index and stretch usage
        case 4:
            lt->stats[LLU] += value;
        case 3:
            lt->stats[LIU] += value;
            break;
        //For stretch positions it counts for both the index and stretch usage
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

/*
 * analyzes bigram stats - stats for sequences of 2 consecutive key presses
 * row*, col* are the positions of each key
 * value is the number of times this bigram appears in the corpus
 * lt is the layout that is being analyzed
 * the stats here are :
 * Same Finger Bigrams, Lateral Stretch Bigrams, and Russor Bigrams
 */
void analyze_bigram(int row0, int col0, int row1, int col1, int value, struct keyboard_layout *lt)
{
    // throw away never seen bigrams
    if (value == 0) {return;}
    lt->big_total += value;

    /*
     * check for same finger bigrams, the two keys are on the same finger,
     * ignoring repeats because they appear the same on all layouts
     */
    if(same_finger(row0, col0, row1, col1) && !same_pos(row0, col0, row1, col1))
    {
        lt->stats[SFB] += value;

        // bad same finger bigrams, where the distance between the keys is 2
        if (distance(row0, col0, row1, col1) == 2) {lt->stats[BAD_SFB] += value;}

        // same finger bigrams by finger
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

        // checks if either key is on the stretch position
        if (col0 == 4 || col1 == 4)
        {
            lt->stats[LLB] += value;
        }
        if (col0 == 5 || col1 == 5)
        {
            lt->stats[RLB] += value;
        }
    }

    /*
     * Lateral Stretch Bigrams, where two keys are on the same hand,
     * one on the middle finger, and one on the stretch position of the index
     */
    if (is_lsb(row0, col0, row1, col1)) {lt->stats[LSB] += value;}

    /*
     * russors, where two keys are on different fingers on the same hand with
     * one key being on ring or middle finger
     */
    if (is_russor(row0, col0, row1, col1))
    {
        // half russor if distance between the keys is 1
        if (distance(row0, col0, row1, col1) == 1) {lt->stats[HRB] += value;}

        // full russor if distance between the keys is 2
        if (distance(row0, col0, row1, col1) == 2) {lt->stats[FRB] += value;}
    }
}

/*
 * analyzes skipgram stats - stats for sequences of 2 disjoint key presses
 * row*, col* are the positions of each key
 * value is the number of times this skipgram appears in the corpus
 * lt is the layout that is being analyzed
 * the stats here are :
 * Same Finger Skipgrams, Lateral Stretch Skipgrams, and Russor Skipgrams
 */
void analyze_skipgram(int row0, int col0, int row2, int col2, int value, struct keyboard_layout *lt)
{
    // throw away never seen skipgrams
    if (value == 0) {return;}
    lt->ski_total += value;

    /*
     * check for same finger skipgrams, the two keys are on the same finger,
     * ignoring repeats because they appear the same on all layouts
     */
    if(same_finger(row0, col0, row2, col2) && !same_pos(row0, col0, row2, col2))
    {
        lt->stats[SFS] += value;

        // bad same finger skipgram, where the distance between the keys is 2
        if (distance(row0, col0, row2, col2) == 2) {lt->stats[BAD_SFS] += value;}

        // same finger skipgrams by finger
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

        // checks if either key is on the stretch position
        if (col0 == 4 || col2 == 4)
        {
            lt->stats[LLS] += value;
        }
        if (col0 == 5 || col2 == 5)
        {
            lt->stats[RLS] += value;
        }
    }
    // Lateral Stretch Skipgrams, where the skipgram is an LSB
    if (is_lsb(row0, col0, row2, col2)) {lt->stats[LSS] += value;}

    // russor skipgram - skipgram version of russor bigrams
    if (is_russor(row0, col0, row2, col2))
    {
        // half russor if distance between the keys is 1
        if (distance(row0, col0, row2, col2) == 1) {lt->stats[HRS] += value;}

        // full russor if distance between the keys is 2
        if (distance(row0, col0, row2, col2) == 2) {lt->stats[FRS] += value;}
    }
}

/*
 * analyzes trigram stats - stats for sequences of 3 consecutive key presses
 * row*, col* are the positions of each key
 * value is the number of times this trigram appears in the corpus
 * lt is the layout that is being analyzed
 * the stats here are :
 * Same Finger Trigrams, Lateral Stretch Trigrams, Russor Trigrams, One-Hands,
 * Redirects, and Rolls
 */
void analyze_trigram(int row0, int col0, int row1, int col1, int row2, int col2,  int value, struct keyboard_layout *lt)
{
    // throw away never seen trigrams
    if (value <= 0) {return;}
    lt->tri_total += value;

    /*
     * check for same finger trigrams, three keys on the same finger,
     * ignoring adjacent repeats, but counting skip repeats such as lol
     */
    if(same_finger(row0, col0, row1, col1) && same_finger(row1, col1, row2, col2)
        && !same_pos(row0, col0, row1, col1) && !same_pos(row1, col1, row2, col2))
    {
        lt->stats[SFT] += value;

        // bad same finger trigrams, where the distance between each pair is 2
        if (distance(row0, col0, row1, col1) == 2 || distance(row1, col1, row2, col2) == 2)
        {
            lt->stats[BAD_SFT] += value;
        }

        // same finger trigrams by finger
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
            case 3:
                lt->stats[LIT] += value;
                break;
            case 5:
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

        // checks if either key is on the stretch position
        if (col0 == 4 || col1 == 4 || col2 == 4)
        {
            lt->stats[LLT] += value;
        }
        if (col0 == 5 || col1 == 5 || col2 == 5)
        {
            lt->stats[RLT] += value;
        }
    }

    // Lateral Stretch Trigrams, where both bigrams within the trigram are LSBs
    if (is_lsb(row0, col0, row1, col1) && is_lsb(row1, col1, row2, col2))
    {
        lt->stats[LST] += value;
    }

    // russor trigrams, where both bigrams within the trigram are russors
    if (is_russor(row0, col0, row1, col1) && is_russor(row1, col1, row2, col2))
    {
        // half russors if the distance is 1 both times
        if (distance(row0, col0, row1, col1) == 1 && distance(row1, col1, row2, col2) == 1)
        {
            lt->stats[HRT] += value;
        }

        // full russors if the distance is 2 both times
        if (distance(row0, col0, row1, col1) == 2 && distance(row1, col1, row2, col2) == 2)
        {
            lt->stats[FRT] += value;
        }
    }

    /*
     * alternation, where you switch hands for the middle key in a trigram then
     * return to the first hand for the final key
     */
    if (same_hand(row0, col0, row2, col2) && !same_hand(row0, col0, row1, col1))
    {
        lt->stats[ALT] += value;
    }

    /*
     * redirects and onehands, where three keys are on one hand,
     * but different fingers
     */
    if (same_hand(row0, col0, row1, col1) && same_hand(row1, col1, row2, col2)
        && !same_finger(row0, col0, row1, col1)
        && !same_finger(row1, col1, row2, col2)
        && !same_finger(row0, col0, row2, col2))
    {
        /*
         * a onehand is when three keys are on the same hand, but different
         * fingers, and the keys are all "moving" in one direction, either
         * towards or away from the index
         */
        if (is_one(row0, col0, row1, col1, row2, col2))
        {
            // check if it is inwards, same-row, and/or adjacent fingers
            int one_in = is_one_in(row0, col0, row1, col1, row2, col2);
            int one_sr = is_same_row_one(row0, col0, row1, col1, row2, col2);
            int one_af = is_adjacent_finger_one(row0, col0, row1, col1, row2, col2);

            // apply to each stat it satisfies
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

        /*
         * a redirect is when three keys are on the same hand, but different
         * fingers, and the keys change directions and do not all "move"
         * towards or away from the index
         */
        else
        {
            lt->stats[RED] += value;

            // check if it is a bad redirect, i.e. no key is on the index
            if (is_bad_red(row0, col0, row1, col1, row2, col2))
            {
                lt->stats[BAD_RED] += value;
            }
        }
    }

    /*
     * Best stat!
     * rolls are when two consecutive keys are on different fingers on the
     * same hand, and the third key is on the other hand
     */
    if (is_roll(row0, col0, row1, col1, row2, col2))
    {
        // check if it is inwards, same-row, and/or adjacent fingers
        int roll_in = is_roll_in(row0, col0, row1, col1, row2, col2);
        int roll_sr = is_same_row_roll(row0, col0, row1, col1, row2, col2);
        int roll_af = is_adjacent_finger_roll(row0, col0, row1, col1, row2, col2);

        // apply to each stat it satisfies
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

// analyzes the ngrams of the layout provided by lt
void analyze_layout(struct keyboard_layout *lt)
{
    // ugly nested for loops to check every possible mono, bi, skip, and trigram
    for (int row0 = 0; row0 < ROW; row0++)
    {
        for (int col0 = 0; col0 < COL; col0++)
        {
            analyze_monogram(row0, col0, monogram[key(row0, col0, lt)], lt);

            // second key
            for (int row1 = 0; row1 < ROW; row1++)
            {
                for (int col1 = 0; col1 < COL; col1++)
                {
                    analyze_bigram(row0, col0, row1, col1, bigram[key(row0, col0, lt)][key(row1, col1, lt)], lt);

                    // skipgrams with all possible trigrams, not just on this layout
                    for (int i = 0; i < 128; i++)
                    {
                        analyze_skipgram(row0, col0, row1, col1, trigram[key(row0, col0, lt)][i][key(row1, col1, lt)], lt);
                    }

                    // third key
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

    // hand balance stat computed by subtracting hand usage from each other
    if (lt->stats[RHU] > lt->stats[LHU])
    {
        lt->stats[HAND_BALANCE] = lt->stats[RHU] - lt->stats[LHU];
    }
    else
    {
        lt->stats[HAND_BALANCE] = lt->stats[LHU] - lt->stats[RHU];
    }
}

/*
 * reads weight from file into the stat_weights struct
 * name is the name of the weight file in ./weights
 * **wt is a pointer to an empty pointer for a weight struct
 * rus note: should be look something like read_weights(default, &wt);
 */
void read_weights(char *name, struct stat_weights **wt)
{
    // find weights file
    char temp;
    FILE * data;
    char *weight = (char*)malloc(strlen("./weights/") + strlen(name) + 1);
    strcpy(weight, "./weights/");
    strcat(weight, name);
    data = fopen(weight, "r");

    // check if weight exists
    if (data == NULL)
    {
        free(weight);
        error_out("No such weights were found.");
        return;
    }

    // just in case this pointer isn't free
    if (*wt != NULL) {free(*wt);}
    *wt = calloc(1, sizeof(struct stat_weights));

    // read pins
    for (int i = 0; i < ROW; i++)
    {
        for (int j = 0; j < COL; j++)
        {
            fscanf(data, " %c", &temp);
            if (temp == '.') {(*wt)->pins[i][j] = 0;}
            else {(*wt)->pins[i][j] = 1;}
        }
    }

    // read weights
    for(enum patterns i = SFB; i < END; i++)
    {
        fscanf(data, " %*[^0-9.-]%lf", &(*wt)->multiplier[i]);
    }

    free(weight);
    fclose(data);
}

/*
 * allows you to check the total number of ngrams corresponding to a stat
 * takes in a stat named stat and a layout
 * returns the total for that ngram, i.e. SFB returns the number of bigrams
 */
int total(enum patterns stat, struct keyboard_layout *lt)
{
    switch(totals[stat])
    {
        //monograms
        case('m'):
            return lt->mon_total;
            break;

        //bigrams
        case('b'):
            return lt->big_total;
            break;

        //skipgrams
        case('s'):
            return lt->ski_total;
            break;

        //trigrams
        default:
            return lt->tri_total;
            break;
    }
}

/*
 * computes layout lt's score against weights wt
 * multiplies each stat by the corresponding value in the weights struct
 */
void get_score(struct keyboard_layout *lt, struct stat_weights *wt)
{
    for (enum patterns i = SFB; i < END; i++)
    {
        lt->score += ((double)lt->stats[i]/total(i, lt)*100) * wt->multiplier[i];
    }
}

/*
 * helper for the printing functions, stating whether a new line should be
 * placed after the stat
 */
int print_new_line(enum patterns stat)
{
    return stat == SFT || stat == BAD_SFT || stat == LST || stat == HRT
        || stat == FRT || stat == BAD_RED || stat == ONE_OUT
        || stat == SR_ONE_OUT || stat == AF_ONE_OUT || stat == SRAF_ONE_OUT
        || stat == ROL_OUT || stat == SR_ROL_OUT || stat == AF_ROL_OUT
        || stat == SRAF_ROL_OUT || stat == RHU || stat == BRU;
}

// prints out the layout lt, first its score, then the key matrix, then stats
void print_layout(struct keyboard_layout *lt)
{
    // print score
    printf("Score : %f\n\n", lt->score);

    // print key matrix
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

    // print all stats up to per finger stats
    for (enum patterns i = SFB; i < LPU; i++)
    {
        printf("%s: %06.3f%%", names[i], (double)lt->stats[i]/total(i, lt) * 100);
        if (print_new_line(i)) {puts("");}
        else {printf(" | ");}
    }

    // print finger usage
    printf("Finger Usage: \n[");
    for (enum patterns i = LPU; i < RPU; i++)
    {
        printf("%05.2f, ", (double)lt->stats[i]/total(i, lt) * 100);
    }
    printf("%05.2f]\nFinger Bigrams: \n[", (double)lt->stats[RPU]/total(RPU, lt) * 100);

    // print finger bigrams
    for (enum patterns i = LPB; i < RPB; i++)
    {
        printf("%05.2f, ", (double)lt->stats[i]/total(i, lt) * 100);
    }
    printf("%05.2f]\nFinger Skipgrams: \n[", (double)lt->stats[RPB]/total(RPB, lt) * 100);

    // print finger skipgrams
    for (enum patterns i = LPS; i < RPS; i++)
    {
        printf("%05.2f, ", (double)lt->stats[i]/total(i, lt) * 100);
    }
    printf("%05.2f]\nFinger Trigrams: \n[", (double)lt->stats[RPS]/total(RPS, lt) * 100);

    // print finger trigrams
    for (enum patterns i = LPT; i < RPT; i++)
    {
        printf("%05.2f, ", (double)lt->stats[i]/total(i, lt) * 100);
    }
    printf("%05.2f]\n", (double)lt->stats[RPT]/total(RPT, lt) * 100);
}

// shortened version of print_layout() removes score and most per finger stats
void short_print(struct keyboard_layout *lt)
{
    // print key matrix
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

    // print all stats up to per finger stats
    for (enum patterns i = SFB; i < LPU; i++)
    {
        if (i == SR_ONE) {i = ROL;}
        printf("%s: %06.3f%%", names[i], (double)lt->stats[i]/total(i, lt) * 100);
        if (print_new_line(i)) {puts("");}
        else {printf(" | ");}
    }

    // print finger usage
    printf("Finger Usage: \n[");
    for (enum patterns i = LPU; i < RPU; i++)
    {
        printf("%05.2f, ", (double)lt->stats[i]/total(i, lt) * 100);
    }
    printf("%05.2f]\n", (double)lt->stats[RPU]/total(RPU, lt) * 100);
}

// print the layouts ranked in order from best to worst performer
void print_rankings()
{
    ptr = head;

    // go through linked list printing each item
    while (ptr != NULL)
    {
        printf("%s -> %lf\n", ptr->name, ptr->score);
        ptr = ptr->next;
    }
}

// rank layouts based on score for weights wt
void rank_layouts(struct stat_weights *wt)
{
    // find all layouts in the ./layouts directory
    DIR *dir;
    struct dirent *entry;
    dir = opendir("./layouts");

    // exit if none found
    if (dir == NULL)
    {
        error_out("No layouts found.");
        return;
    }

    // create temporary keyboard struct for finding each layout's score
    struct keyboard_layout *lt = NULL;

    // for each layout in the directory
    while ((entry = readdir(dir)) != NULL)
    {
        if (entry->d_type == DT_REG)
        {
            // read the layout
            if (lt != NULL) {free(lt);}
            lt = malloc(sizeof(struct keyboard_layout));
            read_layout(entry->d_name, &lt);

            // get its score
            analyze_layout(lt);
            get_score(lt, wt);

            // create node in linked list for it
            struct ranking *node = malloc(sizeof(struct ranking));
            node->name = (char*)malloc(strlen(entry->d_name) + 1);
            strcpy(node->name, entry->d_name);
            node->score = lt->score;
            node->next = NULL;

            // put in correct place in linked list
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

            // free when done
            free(lt);
            lt = NULL;
        }
    }

    // self explanatory
    print_rankings();

    // free linked list when done
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

// swap the keys at row0, col0 and row1, col1 in layout lt
void swap (int row0, int col0, int row1, int col1, struct keyboard_layout *lt)
{
    char temp = lt->matrix[row0][col0];
    lt->matrix[row0][col0] = lt->matrix[row1][col1];
    lt->matrix[row1][col1] = temp;
}

// randomly shuffle the layout lt
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

/*
 * copy the layout from src to the pointer dest
 * this includes the key matrix, the score, and all stats
 */
void copy_layout(struct keyboard_layout *src, struct keyboard_layout **dest)
{
    // allocate the destination
    if (*dest != NULL) {free(*dest);}
    *dest = calloc(1, sizeof(struct keyboard_layout));

    // copy the key matrix
    for (int i = 0; i < ROW; i++)
    {
        for (int j = 0; j < COL; j++)
        {
            (*dest)->matrix[i][j] = src->matrix[i][j];
        }
    }

    // copy stats
    for (enum patterns i = SFB; i < END; i++)
    {
        (*dest)->stats[i] = src->stats[i];
    }

    // copy the rest
    (*dest)->score = src->score;
    (*dest)->mon_total = src->mon_total;
    (*dest)->big_total = src->big_total;
    (*dest)->ski_total = src->ski_total;
    (*dest)->tri_total = src->tri_total;
}

/*
 * copy the layout from src to the pointer dest
 * set everything to zero except the key matrix
 */
void blank_layout(struct keyboard_layout *src, struct keyboard_layout **dest)
{
    // allocate the destination
    if (*dest != NULL) {free(*dest);}
    *dest = calloc(1, sizeof(struct keyboard_layout));

    //copy the key matrix
    for (int i = 0; i < ROW; i++)
    {
        for (int j = 0; j < COL; j++)
        {
            (*dest)->matrix[i][j] = src->matrix[i][j];
        }
    }
}

/*
 * tries to impove the score of the layout lt with the weights wt by randomly
 * shuffling the keys and checking if the score has improved, if it has
 * lt is replaced with the new best layout, do this generation_quantity numbers
 * of times
 * the seed is only there so that when using multithreading all of the threads
 * don't end up making the same swaps
 */
void generate(int generation_quantity, struct keyboard_layout **lt, struct stat_weights *wt, int seed)
{
    // only print this if this is the first time this is being run
    if (seed == 0) {printf("Generating layouts...\n");}

    // get random numbers
    srand(time(NULL));
    for (int i = 0; i < seed; i++) {rand();}

    // set current layout to the max score
    struct keyboard_layout *max = NULL;
    copy_layout(*lt, &max);
    int row0, col0, row1, col1;

    //shuffle the keys generation_quantity numbers of times
    for (int i = generation_quantity; i > 0; i--)
    {
        printf("%d / %d\r", generation_quantity - i, generation_quantity);

        // clear old stats
        blank_layout(max, &(*lt));

        //shuffle the layout, doing more swaps the closer you get to finishing
        for (int j = (i / (generation_quantity/30)) + 1; j > 0; j--)
        {
            // get two random key positions
            row0 = rand() % 3;
            col0 = rand() % 10;
            row1 = rand() % 3;
            col1 = rand() % 10;

            // check that the positions aren't pinned or the same
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

            // swap the positions
            swap(row0, col0, row1, col1, *lt);
        }

        // get the new layout's score
        analyze_layout(*lt);
        get_score(*lt, wt);

        // if it is better replace the max layout with the new one
        if ((*lt)->score > max->score)
        {
            free(max);
            max = NULL;
            copy_layout(*lt, &max);
        }
    }

    // copy the best layout after generation back into lt
    copy_layout(max, &(*lt));
    free(max);
}

/*
 * generate command to be run by each thread in multithreaded mode,
 * the only real difference is setting the seed to the thread_id which ensures
 * that even if the threads are created at almost the exactly the same time
 * they don't end up having the same srand seed, and so create unique swaps
 */
void *thread_gen(void* arg)
{
    struct ThreadData* data = (struct ThreadData*)arg;

    // run generate with the thread's generation quantity, layout, weights
    generate(data->generation_quantity, &(data->lt), data->wt, data->thread_id);

    // place the new score into the thread's score
    data->score = data->lt->score;

    pthread_exit(NULL);
}

/*
 * running in analyze mode takes a single layout and compares it against the
 * corpus with respect to the weights, then prints using the output mode from
 * output
 */
void analyze_mode(char *corpus, char *layout, char *weight, char output)
{
    // initialize layout and weights
    struct keyboard_layout *lt = NULL;
    struct stat_weights *wt = NULL;

    // get corpus and layout
    read_corpus(corpus);
    read_layout(layout, &lt);

    // analyze the layout's stats on the corpus
    analyze_layout(lt);

    // get weights
    read_weights(weight, &wt);

    // score layout stats against weights
    get_score(lt, wt);

    // print depending on mode
    if (output == 'l') {print_layout(lt);}
    else {short_print(lt);}

    free(lt);
    free(wt);
}

/*
 * running in rank mode takes all of the layouts in the ./layouts directory and
 * scores them against weight according to the corpus, then prints the results
 */
void rank_mode(char *corpus, char* weight)
{
    // initialize weights
    struct stat_weights *wt = NULL;

    // read corpus and weights
    read_corpus(corpus);
    read_weights(weight, &wt);

    // score each layout and rank them, then print
    rank_layouts(wt);

    free(wt);
}

/*
 * running in generate mode tries to create the best layout for a certain weight
 * and corpus pair by taking an existing or random layout and shuffling it
 * keeping the shuffled layout with the best score, it shuffles
 * generation_quantity numbers of times, if improve is set to true then it will
 * use an existing layout as the base, otherwise it will create a random layout
 * and try to improve that, it then prints depending on the output mode
 */
void generate_mode(char *corpus, char *layout, char *weight, int generation_quantity, int improve, char output)
{
    // inititalize layout and weights
    struct keyboard_layout *lt = NULL;
    struct stat_weights *wt = NULL;

    // read corpus and layout
    read_corpus(corpus);
    read_layout(layout, &lt);

    // if not in improve mode shuffles the given layout randomly
    if (!improve)
    {
        for (int i = 0; i < 10; i++)
        {
            shuffle(lt);
        }
    }

    // gets initial stats and reads weights
    analyze_layout(lt);
    read_weights(weight, &wt);

    /*
     * if in improve mode the pins will be used to keep some keys in their
     * original positions, if not in improve mode they can be ignored
     */
    if (!improve)
    {
        for (int i = 0; i < ROW; i++)
        {
            for (int j = 0; j < COL; j++)
            {
                wt->pins[i][j] = 0;
            }
        }
    }

    // finds score of initial layout and begins improving
    get_score(lt, wt);
    generate(generation_quantity, &lt, wt, 0);

    // prints depending on output mode
    if (output == 'l') {print_layout(lt);}
    else {short_print(lt);}
    free(lt);
    free(wt);
}

/*
 * running in multi mode is a multithreaded version of the generate mode
 * the generation quantity is split among the threads, and if it is high enough
 * a second pass is done to improve the best layouts the threads create
 */
void multi_mode(char *corpus, char *layout, char *weight, int generation_quantity, int improve, char output)
{
    // inititalize layout and weights
    struct keyboard_layout *lt = NULL;
    struct stat_weights *wt = NULL;

    // read corpus and layout
    read_corpus(corpus);
    read_layout(layout, &lt);

    // if not in improve mode shuffles the given layout randomly
    if (!improve)
    {
        for (int i = 0; i < 10; i++)
        {
            shuffle(lt);
        }
    }

    // gets initial stats and reads weights
    analyze_layout(lt);
    read_weights(weight, &wt);

    /*
     * if in improve mode the pins will be used to keep some keys in their
     * original positions, if not in improve mode they can be ignored
     */
    if (!improve)
    {
        for (int i = 0; i < ROW; i++)
        {
            for (int j = 0; j < COL; j++)
            {
                wt->pins[i][j] = 0;
            }
        }
    }

    // finds score of initial layout and begins improving
    get_score(lt, wt);

    // lets user know it is in multithreaded mode
    printf("Multithreaded mode\n");

    // sets number of threads to the number of "processors" in the system
    int numThread = sysconf(_SC_NPROCESSORS_CONF);
    pthread_t threads[numThread];
    printf("Identified %d threads\n", numThread);

    // splits up generation quantity among threads
    int genThread = generation_quantity / numThread;
    printf("Total generation quantity was %d -> %d per thread\n", generation_quantity, genThread);

    // inititalizes each thread
    struct ThreadData data[numThread];
    for (int i = 0; i < numThread; i++)
    {
        // gives thread id, generation quantity
        data[i].thread_id = i;
        data[i].generation_quantity = genThread;

        // copies layout into thread
        data[i].lt = NULL;
        copy_layout(lt, &(data[i].lt));

        // copies weights and score into thread
        data[i].wt = wt;
        data[i].score = lt->score;

        // creates thread and tells it to generate new layouts
        pthread_create(&threads[i], NULL, thread_gen, (void*)&data[i]);
    }

    // wait for threads to finish
    for (int i = 0; i < numThread; i++)
    {
        pthread_join(threads[i], NULL);
    }

    // initialize variables to find best layout
    struct keyboard_layout *best_layout = NULL;
    int best_thread = 0;

    // check which thread found the best layout
    for (int i = 1; i < numThread; i++)
    {
        if (data[i].score > data[best_thread].score)
        {
            best_thread = i;
        }
    }

    // copy best layout from best thread
    copy_layout(data[best_thread].lt, &best_layout);

    // free thread layouts
    for (int i = 0; i < numThread; i++)
    {
        free(data[i].lt);
    }

    // print best score of stage 1
    printf("Thread %d was best: %lf\n", best_thread, best_layout->score);

    /*
     * if large enough generation quantity was chosen do a second pass
     * improving on the best layout of stage 1
     */
    if (genThread >= 1000)
    {
        // decimate the generation quantity to save time
        printf("Running second pass:\n");
        genThread /= 10;
        printf("Total generation quantity was %d -> %d per thread\n", generation_quantity, genThread);

        // start each thread with the best layout from stage 1
        copy_layout(best_layout, &lt);
        for (int i = 0; i < numThread; i++)
        {
            // gives thread id, generation quantity
            data[i].thread_id = i;
            data[i].generation_quantity = genThread;

            // copies layout into thread
            data[i].lt = NULL;
            copy_layout(lt, &(data[i].lt));

            // copies weights and score into thread
            data[i].wt = wt;
            data[i].score = lt->score;

            // creates thread and tells it to generate new layouts
            pthread_create(&threads[i], NULL, thread_gen, (void*)&data[i]);
        }

        // wait for threads to finish
        for (int i = 0; i < numThread; i++)
        {
            pthread_join(threads[i], NULL);
        }

        // find the best layout from the threads
        int best_thread = 0;
        for (int i = 1; i < numThread; i++)
        {
            if (data[i].score > data[best_thread].score)
            {
                best_thread = i;
            }
        }

        // copy best layout from best thread
        copy_layout(data[best_thread].lt, &best_layout);

        // free thread layouts
        for (int i = 0; i < numThread; i++)
        {
            free(data[i].lt);
        }

        // praise best thread
        printf("Thread %d was best: %lf\n", best_thread, best_layout->score);
    }

    // print the best generated layout
    if (output == 'l') {print_layout(best_layout);}
    else {short_print(best_layout);}

    free(best_layout);
    free(lt);
    free(wt);
}

int main(int argc, char **argv)
{
    // set defaults
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

    // read arguments
    for (int i = 1; i < argc; i++)
    {
        if (argv[i][0] == '-')
        {
            switch (argv[i][1])
            {
                // corpus argument
                case 'c':
                    if (i + 1 < argc)
                    {
                        corpus = (char*)malloc(strlen(argv[i+1])+1);
                        strcpy(corpus, argv[i+1]);
                        corpus_malloc = 1;
                    }
                    break;

                // layout argument
            	case 'l':
            		if (i + 1 < argc)
            		{
                        layout = (char*)malloc(strlen(argv[i+1])+1);
                        strcpy(layout, argv[i+1]);
                        layout_malloc = 1;
            		}
            		break;

                // weights argument
            	case 'w':
            		if (i + 1 < argc)
            		{
                        weight = (char*)malloc(strlen(argv[i+1])+1);
                        strcpy(weight, argv[i+1]);
                        weights_malloc = 1;
            		}
            		break;

                // analyzer mode
                case 'a':
                    mode = 'a';
                    break;

                // generation mode and quantity
            	case 'g':
                    if (mode != 'a') {error_out("Too many mode arguments.");}
            		mode = 'g';
            		if (i + 1 < argc && argv[i+1][0] != '-')
            		{
            			generation_quantity = atoi(argv[i+1]);
            		}
                    if (generation_quantity < 50) {error_out("Invalid generation quantity.");}
                    break;

                // multi mode
                case 'm':
                    mode = 'm';
                    if (i + 1 < argc && argv[i+1][0] != '-')
            		{
            			generation_quantity = atoi(argv[i+1]);
            		}
                    if (generation_quantity < 50*sysconf(_SC_NPROCESSORS_CONF)) {error_out("Invalid generation quantity.");}
                    break;

                // quite output
                case 'q':
                    output = 'q';
                    break;

                // rank mode
                case 'r':
                    if (mode != 'a') {error_out("Too many mode arguments.");}
                    mode = 'r';
                    break;

                // improve mode
                case 'i':
                    improve = 1;
                    break;

                // test error mode
                case 'e':
                    error_out("Test Error");
                    break;
            }
        }
    }

    // begin working based on mode
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

    // free corpus, layouts, and weights if they were allocated
    if(corpus_malloc){free(corpus);}
    if(layout_malloc){free(layout);}
    if(weights_malloc){free(weight);}

    // good job
	return EXIT_SUCCESS;
}
