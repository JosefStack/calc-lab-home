// Joseph Johnson, StudentID: 251ADB113
// (optional) https://github.com/JosefStack/calc-lab-home
// Compile with: gcc -O2 -Wall -Wextra -std=c17 -o calc calc.c -lm
// Resources used: ChatGPT (https://chatgpt.com/share/6ae19574-affc-83eb-b71e-e2486aa4db18)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <sys/stat.h>

#define STUDENT "joseph_johnson_251ADB113"
enum { END = 256, NUMBER, POWER, INVALID };

// AI (ChatGPT), prompt: "how would i handle floats"
// AI (ChatGPT), prompt: "64 bit integers in C??"
// Link: https://chatgpt.com/share/6ae19574-affc-83eb-b71e-e2486aa4db18
// the float reply explains why i need the type too..  7 / 2 gives 3 but 7 / 2.0 gives 3.5
// even if 2.0 prints as 2 it still needs to stay a float
typedef struct {
    long long integer;
    double real;
    int is_float;
} Value;

// AI (ChatGPT), prompt: "maybe i could combine peek and consume into one fn. maybe a fn called next_token that reads the next token and stores it in the Parser?"
// Link: https://chatgpt.com/share/6ae19574-affc-83eb-b71e-e2486aa4db18
// this reply shows keeping the token in the parser instead of separate peek and consume functions
// i can look at this token without moving  next_token moves to the next one
// positions start at 1 so error = 0 means no error yet
typedef struct {
    char *text;
    size_t length, cursor, position, error;
    int token;
    Value number;
} Parser;

static void fatal(const char *message) {
    fprintf(stderr, "calc: %s\n", message);
    exit(EXIT_FAILURE);
}

static void *resize(void *memory, size_t count, size_t size) {
    // i check the size first otherwise the multiplication can overflow and i get too little memory
    if (count > (size_t)-1 / size) fatal("Input too large");
    void *result = realloc(memory, count * size);
    if (result == NULL) fatal("Out of memory");
    return result;
}

// AI (ChatGPT), prompt: "okey, so how do i open a file without actually knowing its size?? i know some basic file handing in C (fopen, fgets, fclose).."
// Link: https://chatgpt.com/share/6ae19574-affc-83eb-b71e-e2486aa4db18
// the responsee goes through growing the buffer with realloc and doubling the size
// i dont know the size so start small and double the space when its full
// this way i dont have to resize and copy the whole thing  for every character
static void *grow(void *memory, size_t *capacity, size_t size) {
    if (*capacity > (size_t)-1 / 2) fatal("Input too large");
    *capacity = *capacity == 0 ? 16 : *capacity * 2;
    return resize(memory, *capacity, size);
}

// AI (ChatGPT), prompt: "maybe i could combine peek and consume into one fn. maybe a fn called next_token that reads the next token and stores it in the Parser?"
// AI (ChatGPT), prompt: "how would i handle floats"
// AI (ChatGPT), prompt: "errno?"
// Link: https://chatgpt.com/share/6ae19574-affc-83eb-b71e-e2486aa4db18
// these replies explain storing one token at a time  recognising floats and what errno is for
static void next_token(Parser *p) {
    char *s = p->text;
    while (p->cursor < p->length &&
           (s[p->cursor] == ' ' || s[p->cursor] == '\t' || s[p->cursor] == '\r'))
        p->cursor++;
    // count from the start of the file  spaces newlines and comments count too
    // add 1 because the array starts at 0 but error positions starts at 1
    p->position = p->cursor + 1;
    if (p->cursor == p->length) {
        // if a value is missing at the end point one place after the last character
        p->token = END;
        return;
    }
    unsigned char c = (unsigned char)s[p->cursor++];
    if (isdigit(c)) {
        size_t start = p->cursor - 1;
        while (isdigit((unsigned char)s[p->cursor])) p->cursor++;
        int is_float = s[p->cursor] == '.';
        if (is_float) {
            size_t dot = p->cursor++;
            if (!isdigit((unsigned char)s[p->cursor])) {
                p->position = dot + 1;
                p->token = INVALID;
                return;
            }
            while (isdigit((unsigned char)s[p->cursor])) p->cursor++;
        }
        // strtod accepts 1.5e3 which isnt allowed here  i put '\0' where the
        // number ends so it stops there and then put the original character back
        char saved = s[p->cursor];
        s[p->cursor] = '\0';
        errno = 0;
        p->number = (Value){0};
        p->number.is_float = is_float;
        if (is_float) {
            p->number.real = strtod(s + start, NULL);
            p->token = isfinite(p->number.real) ? NUMBER : INVALID;
        } else {
            p->number.integer = strtoll(s + start, NULL, 10);
            p->token = errno == ERANGE ? INVALID : NUMBER;
        }
        s[p->cursor] = saved;
    } else if (c == '*' && s[p->cursor] == '*') {
        p->cursor++;
        p->token = POWER;
    } else {
        p->token = c == 0 ? INVALID : c;
    }
}

static double as_double(Value value) {
    return value.is_float ? value.real : (double)value.integer;
}

// AI (ChatGPT), prompt: "how would i handle floats"
// AI (ChatGPT), prompt: "and how do i keep track of the first error and exactly where it happened in the file"
// Link: https://chatgpt.com/share/6ae19574-affc-83eb-b71e-e2486aa4db18
// the replies cover mixing ints with floats and which position to use when a calculation fails
static Value calculate(Parser *p, int op, Value left, Value right,
                       size_t operator_position, size_t right_position) {
    // if something already failed stop here  keep that first error
    if (p->error) return left;
    // for 8 / (4 - 4) the error points to '('
    // i saved that position before calculating it because now the parser already moved past it
    if (op == '/' && as_double(right) == 0) {
        p->error = right_position;
        return left;
    }
    Value result = {0};
    result.is_float = left.is_float || right.is_float;
    // if one is float use doubles  and if both are integers keep integer arithmetic
    if (result.is_float) {
        double a = as_double(left), b = as_double(right);
        if (op == '+') result.real = a + b;
        else if (op == '-') result.real = a - b;
        else if (op == '*') result.real = a * b;
        else if (op == '/') result.real = a / b;
        else result.real = pow(a, b);
        // if the result is infinity or nan point to the operator that caused it
        // for 10.0 ** 400  that is **
        if (!isfinite(result.real)) p->error = operator_position;
    } else {
        long long a = left.integer, b = right.integer;
        if (op == '+') result.integer = a + b;
        else if (op == '-') result.integer = a - b;
        else if (op == '*') result.integer = a * b;
        else if (op == '/') result.integer = a / b;
        // a negative integer exponent is an error so point to where the exponent starts
        else if (b < 0) p->error = right_position;
        else {
            // multiplying b times can take too long so square a and halve b instead
            // if b is odd multiply the result by a  for the part left over
            result.integer = 1;
            while (b > 0) {
                if (b % 2) result.integer *= a;
                b /= 2;
                // i dont square again once its done  that extra square could overflow
                if (b > 0) a *= a;
            }
        }
    }
    return result;
}

static Value expression(Parser *p);

// AI (ChatGPT), prompt: "how can i handle precendence of operators?"
// AI (ChatGPT), prompt: "and how do i keep track of the first error and exactly where it happened in the file"
// Link: https://chatgpt.com/share/6ae19574-affc-83eb-b71e-e2486aa4db18
// parentheses and where to report a missing value or ')' are explained in these replies
static Value primary(Parser *p) {
    Value value = {0};
    if (p->token == NUMBER) {
        value = p->number;
        next_token(p);
    } else if (p->token == '(') {
        next_token(p);
        // calculate whats inside the parentheses together before using the result outside
        value = expression(p);
        if (!p->error) {
            if (p->token == ')') next_token(p);
            // missing ')' so i point to where it got stuck even if its the newline or file end
            else p->error = p->position;
        }
    } else {
        p->error = p->position;
    }
    return value;
}

// AI (ChatGPT), prompt: "this is probably not something the code would have to handle, but right now we use recursion to handle powers, what if the file had several thousand ** ?"
// Link: https://chatgpt.com/share/6ae19574-affc-83eb-b71e-e2486aa4db18
// this is the array approach shown in the reply:  save the values and work out powers backwards
static Value power(Parser *p) {
    typedef struct {
        Value base;
        size_t operator_position, exponent_position;
    } PendingPower;
    PendingPower *pending = NULL;
    size_t count = 0, capacity = 0;
    Value value = primary(p);
    while (!p->error && p->token == POWER) {
        if (count == capacity) pending = grow(pending, &capacity, sizeof(*pending));
        size_t operator_position = p->position;
        next_token(p);
        pending[count++] = (PendingPower){value, operator_position, p->position};
        value = primary(p);
    }

    // go backwards  2**3**2 becomes 3**2 = 9 then 2**9 = 512
    // keep the waiting calculations in the array so a long chain doesnt need lots of recursive calls
    while (count && !p->error) {
        PendingPower item = pending[--count];
        value = calculate(p, POWER, item.base, value,
                          item.operator_position, item.exponent_position);
    }
    free(pending);
    return value;
}

// AI (ChatGPT), prompt: "how can i handle precendence of operators?"
// Link: https://chatgpt.com/share/6ae19574-affc-83eb-b71e-e2486aa4db18
// the precedence reply explains this level for * and /
// i finish powers first then do * and / from left to right.. 20/2/5 gives 10 then 2
static Value term(Parser *p) {
    Value value = power(p);
    while (!p->error && (p->token == '*' || p->token == '/')) {
        int op = p->token;
        size_t operator_position = p->position;
        next_token(p);
        size_t right_position = p->position;
        Value right = power(p);
        value = calculate(p, op, value, right, operator_position, right_position);
    }
    return value;
}

// AI (ChatGPT), prompt: "how can i handle precendence of operators?"
// Link: https://chatgpt.com/share/6ae19574-affc-83eb-b71e-e2486aa4db18
// the reply uses 2+3*4 to explain this  term gives 12 before i add 2
// then do + and - from left to right  10-3-2 gives 7 then 5
static Value expression(Parser *p) {
    Value value = term(p);
    while (!p->error && (p->token == '+' || p->token == '-')) {
        int op = p->token;
        size_t operator_position = p->position;
        next_token(p);
        size_t right_position = p->position;
        Value right = term(p);
        value = calculate(p, op, value, right, operator_position, right_position);
    }
    return value;
}

// AI (ChatGPT), prompt: "okey, so how do i open a file without actually knowing its size?? i know some basic file handing in C (fopen, fgets, fclose).."
// Link: https://chatgpt.com/share/6ae19574-affc-83eb-b71e-e2486aa4db18
// the reply reads one line at a time  here i read the whole file into the growing buffer
static char *read_file(const char *path, size_t *length) {
    // text mode can change \r\n into \n on windows so use binary mode
    // this keeps the actual file bytes  otherwise the error positions could be wrong
    FILE *input = fopen(path, "rb");
    if (input == NULL) fatal("Cannot open input file");
    size_t capacity = 0;
    char *text = grow(NULL, &capacity, 1);
    *length = 0;
    // use int so eof stays separate from all the possible byte values
    int ch;
    while ((ch = fgetc(input)) != EOF) {
        // i leave one byte for '\0' so i dont write past the end of the buffer
        if (*length == capacity - 1) text = grow(text, &capacity, 1);
        text[(*length)++] = (char)ch;
    }
    if (ferror(input)) {
        free(text);
        fclose(input);
        fatal("Cannot read input file");
    }
    text[*length] = '\0';
    if (fclose(input) != 0) {
        free(text);
        fatal("Cannot close input file");
    }
    return text;
}

static char *output_path(const char *input, const char *directory) {
    // for labs/task1.txt use just task1 in the output name  dont copy its folders too
    const char *base = strrchr(input, '/');
    base = base == NULL ? input : base + 1;
    size_t length = strlen(base);
    if (length >= 4 && strcmp(base + length - 4, ".txt") == 0) length -= 4;
    char *name = resize(NULL, length + 1, 1);
    memcpy(name, base, length);
    name[length] = '\0';
    size_t size = strlen(directory) + length + sizeof("/_" STUDENT ".txt");
    char *path = resize(NULL, size, 1);
    snprintf(path, size, "%s/%s_" STUDENT ".txt", directory, name);
    free(name);
    return path;
}

// AI (ChatGPT), prompt: "argc, argc and structs are fine. go through string comparison and parsing."
// Link: https://chatgpt.com/share/6ae19574-affc-83eb-b71e-e2486aa4db18
// strcmp and reading the arguments are explained here  i only use the single file -o form
int main(int argc, char **argv) {
    if (argc != 4 || strcmp(argv[1], "-o") != 0)
        fatal("Usage: calc -o OUTDIR input.txt");
    Parser p = {0};
    p.text = read_file(argv[3], &p.length);
    struct stat info;
    // the folder can exist already but a normal file with that name wont work so check it
    if (mkdir(argv[2], 0777) != 0 &&
        (errno != EEXIST || stat(argv[2], &info) != 0 || !S_ISDIR(info.st_mode)))
        fatal("Cannot create output directory");
    char *path = output_path(argv[3], argv[2]);
    FILE *output = fopen(path, "wb");
    if (output == NULL) fatal("Cannot open output file");

    next_token(&p);
    while (p.token != END && !p.error) {
        // only skip # before the expression starts  a # after an expression is an error
        if (p.token == '\n' || p.token == '#') {
            if (p.token == '#') {
                while (p.cursor < p.length && p.text[p.cursor] != '\n') p.cursor++;
            }
            next_token(&p);
            continue;
        }
        Value value = expression(&p);
        if (!p.error && p.token != '\n' && p.token != END) p.error = p.position;
        if (!p.error) {
            int written = value.is_float ? fprintf(output, "%.15g\n", value.real)
                                         : fprintf(output, "%lld\n", value.integer);
            if (written < 0) fatal("Cannot write result");
        }
    }
    // AI (ChatGPT), prompt: "and how do i keep track of the first error and exactly where it happened in the file"
    // Link: https://chatgpt.com/share/6ae19574-affc-83eb-b71e-e2486aa4db18
    // the reply explains why earlier answers has to go if a later line has an error
    // i clear the file with wb instead of using a temporary file then write just the error position
    if (p.error) {
        if (fflush(output) != 0) fatal("Cannot flush output");
        output = freopen(path, "wb", output);
        if (output == NULL || fprintf(output, "ERROR:%zu\n", p.error) < 0)
            fatal("Cannot write error result");
    }
    if (fclose(output) != 0) fatal("Cannot finish output");
    free(path);
    free(p.text);
    return EXIT_SUCCESS;
}
