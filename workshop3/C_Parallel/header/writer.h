#ifndef WRITER_H
#define WRITER_H

#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>

#include "globals.h"
#include "structs.h"
#include "utilities.h"

/* Save fields data to files */
void dump_data(struct Grid2D *g, struct FieldPointers *f, int rank, int nprocs);

#endif /* WRITER_H */
