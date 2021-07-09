#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#define STRING_LENGTH 1026
#define TEXT_BASE_CAPACITY 2
#define TEXT_GROWTH 9
#define CMD_BASE_CAPACITY 2
#define CMD_GROWTH 9
#define STATE_BASE_CAPACITY 2
#define STATE_GROWTH 4
#define STATE_TO_SAVE 300

typedef struct {
    /* Type of the command, could be 'c' or 'd' */
    char type;
    /* Number of lines overwritten or deleted */
    long linesLost;
    /* Index in the 'pointers' array from which the lines have been overwritten */
    long indexLostStart;
    /* Array of pointers to the lines that have been lost */
    char **dataLost;
    /* Start and end indexes of the command */
    long startPosition, endPosition;
    /* Array of pointers to the new lines added/modified by a 'c', NULL if 'd' */
    char **newData;
} Command;

typedef struct {
    /* Number of commands queued for undo */
    long toUndo;
    /* Number of commands that can be redone */
    long alreadyUndone;

    /* Capacity of 'commands' array of pointers */
    size_t capacity;
    /* Current size of 'commands' array of pointers */
    size_t size;
    /* Array of pointers to commands in order of execution */
    Command **commands;

    /* Helper field that specifies how many elements are present in 'commands' */
    long length;

    /* How many Command objects are allocated but not currently used and can be
     * overwritten without allocating more memory */
    long toReclaim;
} CommandQueue;

typedef struct {
    /* Current size of 'pointers' array */
    size_t size;
    /* Array of pointers that represents the order of lines in the text */
    char **pointers;

    /* Helper field that specifies how many lines are present in 'pointers' */
    long length;
} State;

typedef struct {
    /* Capacity of 'states' array */
    size_t capacity;
    /* Current size of 'states' array */
    size_t size;
    /* Array of pointers to saved states */
    State **states;

    /* Helper field that specifies how many states are present in 'states' */
    long length;

    /* How many State objects are allocated but not currently used and can be
     * overwritten without allocating more memory */
    long toReclaim;
} StateQueue;

typedef struct {
    /* Capacity of 'pointers' array */
    size_t capacity;
    /* Current size of 'pointers' array */
    size_t size;
    /* Array of pointers that represents the order of lines in the text */
    char **pointers;

    /* Helper field that specifies how many lines are present in 'pointers' */
    long length;

    /* Pointer to the queue of commands saved for undo/redo */
    CommandQueue *cmdQueue;
    /* Pointer to the queue of saved states */
    StateQueue *stateQueue;
} Text;

/* Initialize and returns a new Text object */
Text *Empty();
/* Check if there are any commands to be undone/redone, if so executes the necessary undo/redo */
void applyUndoRedo(Text *text, short clearQueue);
/* Get a new Command object either by reclaiming one that is not used anymore, or by allocating a new one */
Command *newCommand(Text *text);
/* Push a command to the top of the commands queue */
void enqueueCommand(Text *text, Command *command);
/* Get a new State object either by reclaiming one that is not used anymore, or by allocating a new one */
State *newState(Text *text);
/* Save the current state of the text for faster undo/redo */
void saveState(Text *text);
/* Restore a previous state */
void restoreState(Text *text, long index);

void change(Text *text, long startPosition, long endPosition);
void delete(Text *text, long startPosition, long endPosition, int redoing);
void print(Text *text, long startPosition, long endPosition);
void undo(Text *text, long num);
void redo(Text *text, long num);

int main() {
    char command[STRING_LENGTH];
    int running = 1;

    char *partial_command;
    Text *text = Empty();

    u_int commands = 0;

    while(running) {
        fgets(command, STRING_LENGTH, stdin);
        if(command[0] == 'q') {
            running = 0;
        } else {
            char id = command[strlen(command) - 2];
            switch (id) {
                case 'c': {
                    applyUndoRedo(text,1);

                    long start, end;
                    start = strtol(command, &partial_command, 10);
                    end = strtol(partial_command+1, &partial_command, 10);

                    change(text, start, end);

                    commands++;
                    if(commands == STATE_TO_SAVE) {
                        commands = 0;
                        saveState(text);
                    }
                    break;
                }
                case 'd': {
                    applyUndoRedo(text,1);

                    long start, end;
                    start = strtol(command, &partial_command, 10);
                    end = strtol(partial_command + 1, &partial_command, 10);

                    delete(text, start, end,0);

                    commands++;
                    if(commands == STATE_TO_SAVE) {
                        commands = 0;
                        saveState(text);
                    }
                    break;
                }
                case 'p': {
                    applyUndoRedo(text,0);

                    long start, end;
                    start = strtol(command, &partial_command, 10);
                    end = strtol(partial_command+1, &partial_command, 10);

                    print(text, start, end);
                    break;
                }
                case 'u': {
                    long num;
                    num = strtol(command, &partial_command, 10);

                    text->cmdQueue->toUndo += num;
                    if(text->cmdQueue->toUndo > text->cmdQueue->length) {
                        text->cmdQueue->toUndo = text->cmdQueue->length;
                    }
                    break;
                }
                case 'r': {
                    long num;
                    num = strtol(command, &partial_command, 10);

                    text->cmdQueue->toUndo -= num;
                    if(text->cmdQueue->toUndo < 0) {
                        if(text->cmdQueue->alreadyUndone > 0) {
                            if(-text->cmdQueue->toUndo > text->cmdQueue->alreadyUndone) {
                                text->cmdQueue->toUndo = -text->cmdQueue->alreadyUndone;
                            }
                        } else {
                            text->cmdQueue->toUndo = 0;
                        }
                    }
                    break;
                }
                default:
                    break;
            }
        }
    }
    return 0;
}

Text *Empty() {
    Text *text = malloc(sizeof(Text));

    text->size = 0;
    text->capacity = sizeof(char*) * TEXT_BASE_CAPACITY;
    text->pointers = malloc(text->capacity);

    text->length = 0;

    text->cmdQueue = malloc(sizeof(CommandQueue));
    text->cmdQueue->toUndo = 0;
    text->cmdQueue->alreadyUndone = 0;
    text->cmdQueue->size = 0;
    text->cmdQueue->capacity = sizeof(Command*) * CMD_BASE_CAPACITY;
    text->cmdQueue->commands = malloc(text->cmdQueue->capacity);
    text->cmdQueue->length = 0;
    text->cmdQueue->toReclaim = 0;

    text->stateQueue = malloc(sizeof(StateQueue));
    text->stateQueue->size = 0;
    text->stateQueue->capacity = sizeof(State*) * STATE_BASE_CAPACITY;
    text->stateQueue->states = malloc(text->stateQueue->capacity);
    text->stateQueue->length = 0;
    text->stateQueue->toReclaim = 0;
    return text;
}

void applyUndoRedo(Text *text, short clearQueue) {
    double n = (double) (text->cmdQueue->length - text->cmdQueue->toUndo);

    if(text->cmdQueue->toUndo > 0) {
        long stateJumps = ceil(n / STATE_TO_SAVE);
        long remainingUndo = STATE_TO_SAVE * stateJumps - n;

        if(text->cmdQueue->toUndo == text->cmdQueue->length) {
            text->length = 0;
            text->size = 0;

            text->cmdQueue->size = 0;
            text->cmdQueue->length = 0;
        } else if(text->cmdQueue->toUndo > STATE_TO_SAVE && stateJumps <= text->stateQueue->length) {
            restoreState(text, stateJumps - 1);

            text->cmdQueue->size -= (text->cmdQueue->toUndo - remainingUndo) * sizeof(Command*);
            text->cmdQueue->length -= (text->cmdQueue->toUndo - remainingUndo);

            if(remainingUndo > 0) undo(text, remainingUndo);
        } else {
            undo(text, text->cmdQueue->toUndo);
        }

        text->cmdQueue->alreadyUndone += text->cmdQueue->toUndo;
    } else if(text->cmdQueue->toUndo < 0) {
        long stateJumps = floor(n / STATE_TO_SAVE);
        long remainingRedo = n - (STATE_TO_SAVE * stateJumps);
        if(-text->cmdQueue->toUndo > STATE_TO_SAVE  && stateJumps <= text->stateQueue->length) {
            if(stateJumps == 0) {
                text->length = 0;
                text->size = 0;

                text->cmdQueue->size = 0;
                text->cmdQueue->length = 0;
            } else {
                restoreState(text, stateJumps - 1);

                text->cmdQueue->size += (-text->cmdQueue->toUndo - remainingRedo) * sizeof(Command*);
                text->cmdQueue->length += (-text->cmdQueue->toUndo - remainingRedo);
            }

            if(remainingRedo > 0) redo(text, remainingRedo);
        } else {
            redo(text, -text->cmdQueue->toUndo);
        }

        text->cmdQueue->alreadyUndone += text->cmdQueue->toUndo;
    }
    text->cmdQueue->toUndo = 0;

    if(clearQueue) {
        text->cmdQueue->toReclaim += text->cmdQueue->alreadyUndone;
        double n1 = (double) text->cmdQueue->length;
        long stateJumpsToRedo = text->stateQueue->length - floor(n1 / STATE_TO_SAVE);
        if(stateJumpsToRedo > 0) {
            text->stateQueue->length -= stateJumpsToRedo;
            text->stateQueue->size -= stateJumpsToRedo * sizeof(State*);
            text->stateQueue->toReclaim += stateJumpsToRedo;
        }
        text->cmdQueue->alreadyUndone = 0;
    }
}

Command *newCommand(Text *text) {
    Command *cmd;
    if(text->cmdQueue->toReclaim > 0) {
        text->cmdQueue->toReclaim--;

        cmd = text->cmdQueue->commands[text->cmdQueue->length];
    } else {
        cmd = malloc(sizeof(Command));
        cmd->dataLost = NULL;
        cmd->newData = NULL;
    }
    return cmd;
}

void enqueueCommand(Text *text, Command *command) {
    size_t newSize = text->cmdQueue->size + (sizeof(Command*));
    if(text->cmdQueue->capacity < newSize) {
        size_t newCapacity = newSize + (sizeof(Command*) * CMD_GROWTH);
        text->cmdQueue->commands = realloc(text->cmdQueue->commands, newCapacity);
        text->cmdQueue->capacity = newCapacity;
    }
    text->cmdQueue->size = newSize;

    text->cmdQueue->commands[text->cmdQueue->length] = command;

    text->cmdQueue->length++;
}

State *newState(Text *text) {
    State *state;
    if(text->stateQueue->toReclaim > 0) {
        text->stateQueue->toReclaim--;

        state = text->stateQueue->states[text->stateQueue->length];
    } else {
        state = malloc(sizeof(State));
        state->pointers = NULL;
        state->length = 0;
        state->size = 0;
    }
    return state;
}

void saveState(Text *text) {
    State *state = newState(text);

    state->length = text->length;
    state->size = text->size;
    if(state->pointers == NULL) {
        state->pointers = malloc(state->size);
    } else {
        state->pointers = realloc(state->pointers, state->size);
    }

    for(long i = 0; i < text->length; i++) {
        state->pointers[i] = text->pointers[i];
    }

    size_t newSize = text->stateQueue->size + (sizeof(State*));
    if(text->stateQueue->capacity < newSize) {
        size_t newCapacity = newSize + (sizeof(State*) * STATE_GROWTH);
        text->stateQueue->states = realloc(text->stateQueue->states, newCapacity);
        text->stateQueue->capacity = newCapacity;
    }
    text->stateQueue->size = newSize;

    text->stateQueue->states[text->stateQueue->length] = state;

    text->stateQueue->length++;
}

void restoreState(Text *text, long index) {
    State *state = text->stateQueue->states[index];

    text->length = state->length;
    text->size = state->size;

    for(long i = 0; i < state->length; i++) {
        text->pointers[i] = state->pointers[i];
    }
}

void change(Text *text, long startPosition, long endPosition) {
    long linesToModify = endPosition - startPosition + 1;
    long linesToAdd = 0;

    long linesOverwritten = linesToModify;
    if(endPosition > text->length) {
        linesToAdd = endPosition - text->length;
        linesOverwritten = linesToModify - linesToAdd;
    }

    Command *cmd = newCommand(text);

    cmd->type = 'c';
    cmd->linesLost = linesOverwritten;
    if(linesOverwritten > 0) {
        cmd->indexLostStart = startPosition - 1;
        if(cmd->dataLost == NULL) {
            cmd->dataLost = malloc(sizeof(char*) * linesOverwritten);
        } else {
            cmd->dataLost = realloc(cmd->dataLost, sizeof(char*) * linesOverwritten);
        }
    }
    cmd->startPosition = startPosition;
    cmd->endPosition = endPosition;
    if(cmd->newData == NULL) {
        cmd->newData = malloc(sizeof(char*) * linesToModify);
    } else {
        cmd->newData = realloc(cmd->newData, sizeof(char*) * linesToModify);
    }

    if(linesToAdd > 0) {
        size_t newSize =  text->size + (sizeof(char*) * linesToAdd);
        if(text->capacity < newSize) {
            size_t newCapacity = newSize + (sizeof(char*) * TEXT_GROWTH);
            text->pointers = realloc(text->pointers, newCapacity);
            text->capacity = newCapacity;
        }
        text->size = newSize;
    }

    char line[STRING_LENGTH];
    int j = 0;
    for(int i = 0; i <= linesToModify; i++) {
        fgets(line, STRING_LENGTH, stdin);
        if(i == linesToModify && line[0] == '.') {
            break;
        }

        size_t length = strlen(line);
        char *ptr = malloc(sizeof(char) * (length + 1));
        strcpy(ptr, line);

        long index = startPosition - 1 + i;
        if(index < text->length) {
            cmd->dataLost[j] = text->pointers[index];
            j++;
        }

        cmd->newData[i] = ptr;

        text->pointers[index] = ptr;
    }

    if(linesToAdd > 0) {
        text->length += linesToAdd;
    }

    enqueueCommand(text, cmd);
}

void delete(Text *text, long startPosition, long endPosition, int redoing) {
    long linesRemoved = 0;

    Command *cmd;

    if(!redoing) {
        cmd = newCommand(text);
        cmd->type = 'd';
        cmd->startPosition = startPosition;
        cmd->endPosition = endPosition;
    }

    if(startPosition <= text->length && endPosition > 0) {
        linesRemoved = endPosition - startPosition + 1;
        if(startPosition < 1) {
            linesRemoved += startPosition - 1;
        }
        if(endPosition > text->length) {
            linesRemoved -= endPosition - text->length;
        }
        if(linesRemoved < 0){
            linesRemoved = 0;
        }

        if(linesRemoved > 0) {
            if(!redoing) {
                if(cmd->dataLost == NULL) {
                    cmd->dataLost = malloc(sizeof(char*) * linesRemoved);
                } else {
                    cmd->dataLost = realloc(cmd->dataLost, sizeof(char*) * linesRemoved);
                }
                cmd->indexLostStart = startPosition < 1 ? 0 : startPosition - 1;

                long j = 0;
                for(long i = cmd->indexLostStart; i < (cmd->indexLostStart + linesRemoved); i++) {
                    cmd->dataLost[j] = text->pointers[i];
                    j++;
                }
            }

            for(long i = endPosition; i < text->length; i++) {
                text->pointers[i-linesRemoved] = text->pointers[i];
            }

            text->length -= linesRemoved;
            text->size -= linesRemoved * sizeof(char*);
        }
    }

    if(!redoing) {
        cmd->linesLost = linesRemoved;
        enqueueCommand(text, cmd);
    }
}

void print(Text *text, long startPosition, long endPosition) {
    for(long i = startPosition-1; i < endPosition; i++) {
        if(i < 0 || i >= text->length) {
            fputs(".\n", stdout);
        } else {
            char *string = text->pointers[i];
            fputs(string, stdout);
        }
    }
}

void undo(Text *text, long num) {
    Command *cmd;
    for(long i = 0; i < num; i++) {
        cmd = text->cmdQueue->commands[text->cmdQueue->length - 1 - i];

        if(cmd->type == 'c') {
            long linesToRemove = cmd->endPosition - cmd->startPosition + 1;
            if(cmd->linesLost > 0) {
                linesToRemove -= cmd->linesLost;

                long k = 0;
                for(long j = cmd->indexLostStart; j < cmd->indexLostStart + cmd->linesLost; j++) {
                    text->pointers[j] = cmd->dataLost[k];
                    k++;
                }
            }

            text->length -= linesToRemove;
            text->size -= linesToRemove * sizeof(char*);
        } else {
            if(cmd->linesLost > 0) {
                for(long j = text->length - 1; j >= cmd->indexLostStart; j--) {
                    text->pointers[j + cmd->linesLost] = text->pointers[j];
                }

                long k = 0;
                for(long j = cmd->indexLostStart; j < cmd->indexLostStart + cmd->linesLost; j++) {
                    text->pointers[j] = cmd->dataLost[k];
                    k++;
                }

                text->length += cmd->linesLost;
                text->size += cmd->linesLost * sizeof(char*);
            }
        }
    }

    text->cmdQueue->size -= sizeof(Command*) * num;
    text->cmdQueue->length -= num;
}

void redo(Text *text, long num) {
    Command *cmd;
    for(long i = 0; i < num; i++) {
        cmd = text->cmdQueue->commands[text->cmdQueue->length + i];

        if(cmd->type == 'c') {
            for(long j = 0; j < (cmd->endPosition - cmd->startPosition + 1); j++) {
                text->pointers[cmd->startPosition - 1 + j] = cmd->newData[j];
            }

            if(cmd->endPosition > text->length) {
                text->size += (sizeof(char*) * (cmd->endPosition - text->length));
                text->length = cmd->endPosition;
            }
        } else {
            delete(text, cmd->startPosition, cmd->endPosition, 1);
        }
    }

    text->cmdQueue->size += sizeof(Command*) * num;
    text->cmdQueue->length += num;
}
