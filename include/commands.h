#ifndef COMMANDS_H
#define COMMANDS_H

#include <stdbool.h>
#include "fat16.h"

// Declaração da função ls
struct fat_dir *ls(FILE *fp, struct fat_bpb *bpb);

// Declarações das outras funções
void mv(FILE *fp, char *source, char *dest, struct fat_bpb *bpb);
void rm(FILE *fp, char *filename, struct fat_bpb *bpb);
void cp(FILE *fp, char *source, char *dest, struct fat_bpb *bpb);
void cat(FILE *fp, char *filename, struct fat_bpb *bpb);

#endif
