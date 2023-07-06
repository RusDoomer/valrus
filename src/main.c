#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define ROW 3
#define COL 10

#define LEFT_HAND 4
#define RIGHT_HAND 5

static long unsigned int monogram[128];
static long unsigned int bigram[128][128];
static long unsigned int trigram[128][128][128];
long unsigned int total;

struct patterns
{
    long unsigned int sfb; //same finger bigram
    long unsigned int sfs; //same finger skipgram
    long unsigned int sft; //same finger trigram

    long unsigned int bsfb;//bad same finger bigram
    long unsigned int bsfs;//bad same finger skipgram
    long unsigned int bsft;//bad same finger trigram

    long unsigned int lsb; //lateral stretch bigram
    long unsigned int lss; //lateral stretch skipgram
    long unsigned int lst; //lateral stretch trigram

    long unsigned int hrb; //half russor bigram
    long unsigned int hrs; //half russor skipgram
    long unsigned int hrt; //half russor trigram

    long unsigned int frb; //full russor bigram
    long unsigned int frs; //full russor skipgram
    long unsigned int frt; //full russor trigram

    long unsigned int alt; //alternation
    long unsigned int red; //redirect
    long unsigned int brd; //bad redirect

    long unsigned int one; //onehand
    long unsigned int oni; //onehand in
    long unsigned int ono; //onehand out
    long unsigned int sron;//same row onehand
    long unsigned int soi; //same row onehand in
    long unsigned int soo; //same row onehand out
    long unsigned int afon;//adjacent finger onehand
    long unsigned int aoi; //adjacent finger onehand in
    long unsigned int aoo; //adjacent finger onehand out
    long unsigned int saon;//same row adjacent finger onehand
    long unsigned int saoi;//same row adjacent finger onehand in
    long unsigned int saoo;//same row adjacent finger onehand out

    long unsigned int rol; //roll
    long unsigned int irl; //inroll
    long unsigned int orl; //outroll
    long unsigned int srr; //same row roll
    long unsigned int sri; //same row inroll
    long unsigned int sro; //same row outroll
    long unsigned int afr; //adjacent finger roll
    long unsigned int afi; //adjacent finger inroll
    long unsigned int afo; //adjacent finger outroll
    long unsigned int sar; //same row adjacent finger roll
    long unsigned int sai; //same row adjacent finger inroll
    long unsigned int sao; //same row adjacent finger outroll

    long unsigned int lhu; //left  hand usage
    long unsigned int rhu; //right hand usage

    long unsigned int tru; //top    row usage
    long unsigned int hru; //home   row usage
    long unsigned int bru; //bottom row usage

    long unsigned int lpu; //left  pinky    usage
    long unsigned int lru; //left  ring     usage
    long unsigned int lmu; //left  middle   usage
    long unsigned int liu; //left  index    usage
    long unsigned int lsu; //left  stretch  usage

    long unsigned int rsu; //right stretch  usage
    long unsigned int riu; //right index    usage
    long unsigned int rmu; //right middle   usage
    long unsigned int rru; //right ring     usage
    long unsigned int rpu; //right pinky    usage


    long unsigned int lpb; //left pinky     sfb
    long unsigned int lmb; //left ring      sfb
    long unsigned int lrb; //left middle    sfb
    long unsigned int lib; //left index     sfb
    long unsigned int lssb;//left stretch   sfb

    long unsigned int rssb;//right stretch  sfb
    long unsigned int rib; //right index    sfb
    long unsigned int rmb; //right middle   sfb
    long unsigned int rrb; //right ring     sfb
    long unsigned int rpb; //right pinky    sfb


    long unsigned int lps; //left pinky     sfs
    long unsigned int lrs; //left ring      sfs
    long unsigned int lms; //left middle    sfs
    long unsigned int lis; //left index     sfs
    long unsigned int lsss;//left stretch   sfs

    long unsigned int rsss;//right stretch  sfs
    long unsigned int ris; //right index    sfs
    long unsigned int rms; //right middle   sfs
    long unsigned int rrs; //right ring     sfs
    long unsigned int rps; //right pinky    sfs

    long unsigned int lpt; //left pinky    sft
    long unsigned int lrt; //left ring     sft
    long unsigned int lmt; //left middle   sft
    long unsigned int lit; //left index    sft
    long unsigned int lsst;//left stretch  sft

    long unsigned int rsst;//right stretch  sft
    long unsigned int rit; //right index    sft
    long unsigned int rmt; //right middle   sft
    long unsigned int rrt; //right ring     sft
    long unsigned int rpt; //right pinky    sft
};

struct keyboard_layout
{
    char matrix[ROW][COL];
    double score;
    long unsigned int layout_total;
    struct patterns *stats;
};

struct keyboard_layout *current;
struct keyboard_layout *max;

//lower case all letters, and symbols that are within the "alpha" area, leave the rest alone, also trim non-ascii stuff
char convert_char(char c)
{
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
    if (col <= LEFT_HAND)
    {
        return 'l';
    }
    else
    {
        return 'r';
    }
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

int same_hand(int col0, int col1)
{
    return (col0 <= LEFT_HAND && col1 <= LEFT_HAND) || (col0 >= RIGHT_HAND && col1 >= RIGHT_HAND);
}

int adjacent_fingers(int col0, int col1)
{
    return (same_hand(col0, col1) && (finger(col0) - finger(col1) == 1 || finger(col0) - finger(col1) == -1) && (col0 - col1 == 1 || col0 - col1 == -1));
}

int distance(int row0, int row1)
{
    int dist = row0 - row1;
    if (dist >= 0)
    {
        return dist;
    }
    else
    {
        return -dist;
    }
}

int is_lsb(int col0, int col1)
{
    return ((col0 == 2 && col1 == 4) || (col0 == 4 && col1 == 2) || (col0 == 5 && col1 == 7) || (col0 == 7 && col1 == 5));
}

int is_russor(int col0, int col1)
{
    return (same_hand(col0, col1) && !same_finger(col0, col1) && (col0 == 1 || col0 == 2 || col0 == 7 || col0 == 8 || col1 == 1 || col1 == 2 || col1 == 7 || col1 == 8));
}

int is_one(int col0, int col1, int col2)
{
    return (finger(col0) < finger(col1) && finger(col1) < finger(col2)) || (finger(col0) > finger(col1) && finger(col1) > finger(col2));
}

int is_one_in(int col0, int col1, int col2)
{
    return (hand(col0) == 'l' && col0 < col1) || (hand(col0) == 'r' && col0 > col1);
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

void read_corpus(char *name)
{
    printf("Reading corpus...\n");
    //define variables
    FILE *data;
    char *ngram, *raw;
    char a, b, c;
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
            printf("Raw data does not exist, no longer my problem...\n");
            free(raw);
            free(ngram);
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
            total = 2;
        }
        while ((c = fgetc(data)) != EOF)
        {
            c = convert_char(c);
            total++;
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
                    total += monogram[a];
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
    printf("Reading layout...\n");
    //find layout file
    FILE *data;
    char *layout = (char*)malloc(strlen("./layouts/") + strlen(name) + 1);
    strcpy(layout, "./layouts/");
    strcat(layout, name);
    data = fopen(layout, "r");
    //check if layout exists
    if (data == NULL)
    {
        free(layout);
        printf("No such layout was fount!\n");
        return;
    }
    current = calloc(1, sizeof(struct keyboard_layout));
    current->stats = calloc(1, sizeof(struct patterns));
    //read layout
    for (int  i = 0; i < 3; i++)
    {
        for (int j = 0; j < 10; j++)
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
    //sft
    if(same_finger(col0, col1) && same_finger(col1, col2) && !same_row(row0, row1) && !same_row(row1, row2))
    {
        current->stats->sft += value;
        //bad sft
        if (distance(row0, row1) == 2 || distance(row1, row2) == 2)
        {
            current->stats->bsft += value;
        }
        //sft by finger
        switch(col0)
        {
            case 0:
                current->stats->lpt += value;
                break;
            case 1:
                current->stats->lrt += value;
                break;
            case 2:
                current->stats->lmt += value;
                break;
            case 4:
                current->stats->lsst += value;
            case 3:
                current->stats->lit += value;
                break;
            case 5:
                current->stats->rsst += value;
            case 6:
                current->stats->rit += value;
                break;
            case 7:
                current->stats->rmt += value;
                break;
            case 8:
                current->stats->rrt += value;
                break;
            case 9:
                current->stats->rpt += value;
                break;
        }
    }
    //lst
    if (is_lsb(col0, col1) && is_lsb(col1, col2))
    {
        current->stats->lst += value;
    }
    //russors -- needs fixing
    if (is_russor(col0, col1) && is_russor(col1, col2))
    {
        if (distance(col0, col1) == 1 && distance(col1, col2) == 1)
        {
            current->stats->hrt += value;
        }
        if (distance(col0, col1) == 2 && distance(col1, col2) == 2)
        {
            current->stats->frt += value;
        }
    }
    //alt
    if (same_hand(col0, col2) && !same_hand(col0, col1))
    {
        current->stats->alt += value;
    }

    //redirects and onehands -- needs fixing, redirects account for sfs (but not repeats)
    if (same_hand(col0, col1) && same_hand(col1, col2) && !same_finger(col0, col1) && !same_finger(col1, col2) && !same_finger(col0, col2))
    {
        //one
        if (is_one(col0, col1, col2))
        {
            current->stats->one += value;
            //in
            if (is_one_in(col0, col1, col2))
            {
                current->stats->oni += value;
                if (same_row(row0, row1) && same_row(row1, row2))
                {
                    current->stats->sron += value;
                    current->stats->soi  += value;
                }
                if (adjacent_fingers(col0, col1) && adjacent_fingers(col1, col2))
                {
                    current->stats->afon += value;
                    current->stats->aoi += value;
                    if (same_row(row0, row1) && same_row(row1, row2))
                    {
                        current->stats->saon += value;
                        current->stats->saoi  += value;
                    }
                }
            }
            //out
            else
            {
                current->stats->ono += value;
                if (same_row(row0, row1) && same_row(row1, row2))
                {
                    current->stats->sron += value;
                    current->stats->soo  += value;
                }
                if (adjacent_fingers(col0, col1) && adjacent_fingers(col1, col2))
                {
                    current->stats->afon += value;
                    current->stats->aoi += value;
                    if (same_row(row0, row1) && same_row(row1, row2))
                    {
                        current->stats->saon += value;
                        current->stats->saoo  += value;
                    }
                }
            }
        }
        //redirects
        else
        {
            current->stats->red += value;
            if (is_bad_red(col0, col1, col2))
            {
                current->stats->brd += value;
            }
        }
    }

    //rolls
    if (is_roll(col0, col1, col2))
    {
        current->stats->rol += value;
        if(is_roll_in(col0, col1, col2))
        {
            current->stats->irl += value;
            if ((same_hand(col0, col1) && same_row(row0, row1)) || (same_hand(col1, col2) && same_row(col1, col2)))
            {
                current->stats->srr += value;
                current->stats->sri += value;
            }
            if ((same_hand(col0, col1) && adjacent_fingers(row0, row1)) || (same_hand(col1, col2) && adjacent_fingers(col1, col2)))
            {
                current->stats->afr += value;
                current->stats->afi += value;
                if ((same_hand(col0, col1) && same_row(row0, row1)) || (same_hand(col1, col2) && same_row(row1, row2)))
                {
                    current->stats->sar += value;
                    current->stats->sai += value;
                }
            }
        }
        else
        {
            current->stats->orl += value;
            if ((same_hand(col0, col1) && same_row(row0, row1)) || (same_hand(col1, col2) && same_row(col1, col2)))
            {
                current->stats->srr += value;
                current->stats->sro += value;
            }
            if ((same_hand(col0, col1) && adjacent_fingers(row0, row1)) || (same_hand(col1, col2) && adjacent_fingers(col1, col2)))
            {
                current->stats->afr += value;
                current->stats->afo += value;
                if ((same_hand(col0, col1) && same_row(row0, row1)) || (same_hand(col1, col2) && same_row(row1, row2)))
                {
                    current->stats->sar += value;
                    current->stats->sao += value;
                }
            }
        }
    }
}

//sfs, bsfs,  lss, hrs, frs, sfs by finger
void analyze_skipgram(int row0, int col0, int row2, int col2, long unsigned int value)
{
    //sfs
    if(same_finger(col0, col2) && !same_row(row0, row2))
    {
        current->stats->sfs += value;
        //bad sfs
        if (distance(row0, row2) == 2)
        {
            current->stats->bsfs += value;
        }
        //sfs by finger
        switch(col0)
        {
            case 0:
                current->stats->lps += value;
                break;
            case 1:
                current->stats->lrs += value;
                break;
            case 2:
                current->stats->lms += value;
                break;
            case 4:
                current->stats->lsss += value;
            case 3:
                current->stats->lis += value;
                break;
            case 5:
                current->stats->rsss += value;
            case 6:
                current->stats->ris += value;
                break;
            case 7:
                current->stats->rms += value;
                break;
            case 8:
                current->stats->rrs += value;
                break;
            case 9:
                current->stats->rps += value;
                break;
        }
    }
    //lss
    if (is_lsb(col0, col2))
    {
        current->stats->lss += value;
    }
    //russors
    if (is_russor(col0, col2))
    {
        if (distance(row0, row2) == 1)
        {
            current->stats->hrs += value;
        }
        if (distance(row0, row2) == 2)
        {
            current->stats->frs += value;
        }
    }
}

//sfb, bsfb, lsb, hrb, frb, alt, sfb by finger
void analyze_bigram(int row0, int col0, int row1, int col1, long unsigned int value)
{
    //sfb
    if(same_finger(col0, col1) && !same_row(row0, row1))
    {
        current->stats->sfb += value;
        //bad sfb
        if (distance(row0, row1) == 2)
        {
            current->stats->bsfb += value;
        }
        //sfb by finger
        switch(col0)
        {
            case 0:
                current->stats->lpb += value;
                break;
            case 1:
                current->stats->lrb += value;
                break;
            case 2:
                current->stats->lmb += value;
                break;
            case 4:
                current->stats->lssb += value;
            case 3:
                current->stats->lib += value;
                break;
            case 5:
                current->stats->rssb += value;
            case 6:
                current->stats->rib += value;
                break;
            case 7:
                current->stats->rmb += value;
                break;
            case 8:
                current->stats->rrb += value;
                break;
            case 9:
                current->stats->rpb += value;
                break;
        }
    }
    //lsb
    if (is_lsb(col0, col1))
    {
        current->stats->lsb += value;
    }
    //russors
    if (is_russor(col0, col1))
    {
        if (distance(row0, row1) == 1)
        {
            current->stats->hrb += value;
        }
        if (distance(row0, row1) == 2)
        {
            current->stats->frb += value;
        }
    }
}

//hand, row, and finger usage
void analyze_monogram(int row, int col, long unsigned int value)
{
    current->layout_total += value;
    //hand usage
    if (hand(col) == 'l')
    {
        current->stats->lhu += value;
    }
    else
    {
        current->stats->rhu += value;
    }
    //row usage
    if (row == 0)
    {
        current->stats->tru += value;
    }
    else if (row == 1)
    {
        current->stats->hru += value;
    }
    else
    {
        current->stats->bru += value;
    }
    //finger usage
    switch(col)
    {
        case 0:
            current->stats->lpu += value;
            break;
        case 1:
            current->stats->lru += value;
            break;
        case 2:
            current->stats->lmu += value;
            break;
        //FOR STRETCH FINGERS IT COUNTS FOR BOTH STRETCH AND INDEX USAGE
        case 4:
            current->stats->lsu += value;
        case 3:
            current->stats->liu += value;
            break;
        //SAME HERE
        case 5:
            current->stats->rsu += value;
        case 6:
            current->stats->riu += value;
            break;
        case 7:
            current->stats->rmu += value;
            break;
        case 8:
            current->stats->rru += value;
            break;
        case 9:
            current->stats->rpu += value;
            break;
    }
}

void analyze_layout()
{
    printf("Analyzing layout...\n");
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
                        analyze_skipgram(r1,c1, r2, c2, trigram[key(r1, c1)][i][key(r2, c2)]);
                    }
                    //third key
                    for (int r3 = 0; r3 < ROW; r3++)
                    {
                        for (int c3 = 0; c3 < COL; c3++)
                        {
                            //for only running skipgram analysis on trigrams possible with this layout
                            //analyze_skipgram(r1, c1, r3, c3, trigram[key(r1, c1)][key(r2, c2)][key(r3, c3)]);
                            analyze_trigram(r1, c1, r2, c2, r3, c3, trigram[key(r1, c1)][key(r2, c2)][key(r3, c3)]);
                        }
                    }
                    // run skipgram analysis with space as the in between keypress
                    //analyze_skipgram(r1,c1, r2, c2, trigram[key(r1, c1)][' '][key(r2, c2)]);
                }
            }
        }
    }
}

void read_weights()
{
    printf("Reading weights...\n");
    return;
}

void get_score()
{
    printf("Calculating score...\n");

    puts("");
    printf("Score : %f\n", current->score);
    puts("");
    return;
}

void print_layout()
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
    puts("USAGE:");
    //puts("");
    printf("Left Hand: %06.3f%% | ", (double)current->stats->lhu/current->layout_total*100);
    printf("Right Hand: %06.3f%%\n", (double)current->stats->rhu/current->layout_total*100);
    //puts("");
    printf("Top Row: %06.3f%% | ",   (double)current->stats->tru/current->layout_total*100);
    printf("Home Row %06.3f%% | ",   (double)current->stats->hru/current->layout_total*100);
    printf("Bottom Row: %06.3f%%\n", (double)current->stats->bru/current->layout_total*100);
    //puts("");
    printf("Left : ");
    printf("Pinky: %06.3f%% | ",  (double)current->stats->lpu/current->layout_total*100);
    printf("Ring: %06.3f%% | ",   (double)current->stats->lru/current->layout_total*100);
    printf("Middle: %06.3f%% | ", (double)current->stats->lmu/current->layout_total*100);
    printf("Index: %06.3f%% | ",  (double)current->stats->liu/current->layout_total*100);
    printf("Stretch: %06.3f%%\n", (double)current->stats->lsu/current->layout_total*100);
    printf("Right: ");
    printf("Pinky: %06.3f%% | ",  (double)current->stats->rpu/current->layout_total*100);
    printf("Ring: %06.3f%% | ",   (double)current->stats->rru/current->layout_total*100);
    printf("Middle: %06.3f%% | ", (double)current->stats->rmu/current->layout_total*100);
    printf("Index: %06.3f%% | ",  (double)current->stats->riu/current->layout_total*100);
    printf("Stretch: %06.3f%%\n", (double)current->stats->rsu/current->layout_total*100);
    puts("");
    printf("SFB: %06.3f%% | ",  (double)current->stats->sfb /current->layout_total*100);
    printf("Bad: %06.3f%%\n",   (double)current->stats->bsfb/current->layout_total*100);
    printf("Left : ");
    printf("Pinky: %06.3f%% | ",  (double)current->stats->lpb /current->layout_total*100);
    printf("Ring: %06.3f%% | ",   (double)current->stats->lrb /current->layout_total*100);
    printf("Middle: %06.3f%% | ", (double)current->stats->lmb /current->layout_total*100);
    printf("Index: %06.3f%% | ",  (double)current->stats->lib /current->layout_total*100);
    printf("Stretch: %06.3f%%\n", (double)current->stats->lssb/current->layout_total*100);
    printf("Right: ");
    printf("Pinky: %06.3f%% | ",  (double)current->stats->rpb /current->layout_total*100);
    printf("Ring: %06.3f%% | ",   (double)current->stats->rrb /current->layout_total*100);
    printf("Middle: %06.3f%% | ", (double)current->stats->rmb /current->layout_total*100);
    printf("Index: %06.3f%% | ",  (double)current->stats->rib /current->layout_total*100);
    printf("Stretch: %06.3f%%\n", (double)current->stats->rssb/current->layout_total*100);
    puts("");
    printf("SFS: %06.3f%% | ", (double)current->stats->sfs /current->layout_total*100);
    printf("Bad: %06.3f%%\n",  (double)current->stats->bsfs/current->layout_total*100);
    printf("Left : ");
    printf("Pinky: %06.3f%% | ",  (double)current->stats->lps /current->layout_total*100);
    printf("Ring: %06.3f%% | ",   (double)current->stats->lrs /current->layout_total*100);
    printf("Middle: %06.3f%% | ", (double)current->stats->lms /current->layout_total*100);
    printf("Index: %06.3f%% | ",  (double)current->stats->lis /current->layout_total*100);
    printf("Stretch: %06.3f%%\n", (double)current->stats->lsss/current->layout_total*100);
    printf("Right: ");
    printf("Pinky: %06.3f%% | ",  (double)current->stats->rps /current->layout_total*100);
    printf("Ring: %06.3f%% | ",   (double)current->stats->rrs /current->layout_total*100);
    printf("Middle: %06.3f%% | ", (double)current->stats->rms /current->layout_total*100);
    printf("Index: %06.3f%% | ",  (double)current->stats->ris /current->layout_total*100);
    printf("Stretch: %06.3f%%\n", (double)current->stats->rsss/current->layout_total*100);
    puts("");
    printf("SFT: %06.3f%% | ", (double)current->stats->sft /current->layout_total*100);
    printf("Bad: %06.3f%%\n",  (double)current->stats->bsft/current->layout_total*100);
    printf("Left : ");
    printf("Pinky: %06.3f%% | ",  (double)current->stats->lpt /current->layout_total*100);
    printf("Ring: %06.3f%% | ",   (double)current->stats->lrt /current->layout_total*100);
    printf("Middle: %06.3f%% | ", (double)current->stats->lmt /current->layout_total*100);
    printf("Index: %06.3f%% | ",  (double)current->stats->lit /current->layout_total*100);
    printf("Stretch: %06.3f%%\n", (double)current->stats->lsst/current->layout_total*100);
    printf("Right: ");
    printf("Pinky: %06.3f%% | ",  (double)current->stats->rpt /current->layout_total*100);
    printf("Ring: %06.3f%% | ",   (double)current->stats->rrt /current->layout_total*100);
    printf("Middle: %06.3f%% | ", (double)current->stats->rmt /current->layout_total*100);
    printf("Index: %06.3f%% | ",  (double)current->stats->rit /current->layout_total*100);
    printf("Stretch: %06.3f%%\n", (double)current->stats->rsst/current->layout_total*100);
    puts("");
    printf("LSB: %06.3f%% | ", (double)current->stats->lsb/current->layout_total*100);
    printf("LSS: %06.3f%% | ", (double)current->stats->lss/current->layout_total*100);
    printf("LST: %06.3f%%\n",  (double)current->stats->lst/current->layout_total*100);
    printf("HRB: %06.3f%% | ", (double)current->stats->hrb/current->layout_total*100);
    printf("HRS: %06.3f%% | ", (double)current->stats->hrs/current->layout_total*100);
    printf("HRT: %06.3f%%\n",  (double)current->stats->hrt/current->layout_total*100);
    printf("FRB: %06.3f%% | ", (double)current->stats->frb/current->layout_total*100);
    printf("FRS: %06.3f%% | ", (double)current->stats->frs/current->layout_total*100);
    printf("FRT: %06.3f%%\n",  (double)current->stats->frt/current->layout_total*100);
    puts("");
    printf("Alt: %06.3f%%\n",  (double)current->stats->alt/current->layout_total*100);
    printf("Red: %06.3f%% | ", (double)current->stats->red/current->layout_total*100);
    printf("Bad: %06.3f%%\n",  (double)current->stats->brd/current->layout_total*100);
    puts("");
    printf("One : %06.3f%% | ", (double)current->stats->one /current->layout_total*100);
    printf("In: %06.3f%% | ",   (double)current->stats->oni /current->layout_total*100);
    printf("Out: %06.3f%%\n",   (double)current->stats->ono /current->layout_total*100);
    printf("SRON: %06.3f%% | ", (double)current->stats->sron/current->layout_total*100);
    printf("In: %06.3f%% | ",   (double)current->stats->soi /current->layout_total*100);
    printf("Out: %06.3f%%\n",   (double)current->stats->soo /current->layout_total*100);
    printf("AFON: %06.3f%% | ", (double)current->stats->afon/current->layout_total*100);
    printf("In: %06.3f%% | ",   (double)current->stats->aoi /current->layout_total*100);
    printf("Out: %06.3f%%\n",   (double)current->stats->aoo /current->layout_total*100);
    printf("SAON: %06.3f%% | ", (double)current->stats->saon/current->layout_total*100);
    printf("In: %06.3f%% | ",   (double)current->stats->saoi/current->layout_total*100);
    printf("Out: %06.3f%%\n",   (double)current->stats->saoo/current->layout_total*100);
    puts("");
    printf("Rol: %06.3f%% | ", (double)current->stats->rol /current->layout_total*100);
    printf("In: %06.3f%% | ",  (double)current->stats->irl /current->layout_total*100);
    printf("Out: %06.3f%%\n",  (double)current->stats->orl /current->layout_total*100);
    printf("SRR: %06.3f%% | ", (double)current->stats->srr /current->layout_total*100);
    printf("In: %06.3f%% | ",  (double)current->stats->sri /current->layout_total*100);
    printf("Out: %06.3f%%\n",  (double)current->stats->sro /current->layout_total*100);
    printf("AFR: %06.3f%% | ", (double)current->stats->afr /current->layout_total*100);
    printf("In: %06.3f%% | ",  (double)current->stats->afi /current->layout_total*100);
    printf("Out: %06.3f%%\n",  (double)current->stats->afo /current->layout_total*100);
    printf("SAR: %06.3f%% | ", (double)current->stats->sar /current->layout_total*100);
    printf("In: %06.3f%% | ",  (double)current->stats->sai /current->layout_total*100);
    printf("Out: %06.3f%%\n",  (double)current->stats->sao /current->layout_total*100);
}

int main(int argc, char **argv)
{
    //set defaults
    char* corpus = "monkeyracer";
    int corpus_malloc = 0;
    char* layout = "alphabetical";
    int layout_malloc = 0;
    char* weights = "default";
    int weights_malloc = 0;
    char mode = 'n';
    int generation_quantity = 100;
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
                        weights = (char*)malloc(strlen(argv[i+1])+1);
                        strcpy(weights, argv[i+1]);
                        weights_malloc = 1;
            		}
            		break;
            	case 'a':
            		mode = 'a';
            		break;
            	case 'g':
            		mode = 'g';
            		if (i + 1 < argc && argv[i+1][0] != '-')
            		{
            			generation_quantity = atoi(argv[i+1]);
            		}
            }
        }
    }
    //print end results
    printf("corpus : %s\n", corpus);
    printf("layout : %s\n", layout);
    printf("weights : %s\n", weights);
    printf("mode : %c\n", mode);
    printf("generation quantity : %d\n\n", generation_quantity);

    //actually start doing stuff
    switch (mode)
    {
        case 'n':
            printf("No mode selected, exiting.\n");
            break;
        case 'a':
            printf("Starting analysis...\n");
            read_corpus(corpus);
            read_layout(layout);
            analyze_layout();
            //TODO
            read_weights();
            get_score();
            //
            print_layout();
            break;
        case 'g':
            printf("Starting generation...\n");
            read_corpus(corpus);
            read_layout(layout);
            break;
    }

    if(current != NULL)
    {
        free(current->stats);
        free(current);
    }
    if(max != NULL)
    {
        free(max->stats);
        free(max);
    }
    if(corpus_malloc){free(corpus);}
    if(layout_malloc){free(layout);}
    if(weights_malloc){free(weights);}
}
