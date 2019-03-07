//
// Created by Luca Cappelletti on 2019-03-04.
//

#include "threshold.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>

int comp (const void * elem1, const void * elem2){
    double f = *((double*)elem1);
    double s = *((double*)elem2);
    if (f > s) return  1;
    if (f < s) return -1;
    return 0;
}

void sort(double* x, size_t size){
    qsort(x, size, sizeof(*x), comp);
}

double determine_threshold(Matrix const* matrices, size_t const matrices_number, double const known_negatives_percentage){
    size_t vector_size;
    double* flatten = flatten_matrices(matrices, matrices_number, &vector_size);
    qsort(flatten, vector_size, sizeof(*flatten), comp);
    double position = vector_size*(1-known_negatives_percentage);
    int lower=(int)floor(position), upper=(int)ceil(position);
    double threshold = (flatten[lower] + flatten[upper])/2;
    free(flatten);
    return threshold;
}

double* determine_thresholds(Matrix const ** matrices_groups, size_t const matrices_number, size_t const groups_number, double const* known_negatives_percentages){
    double* thresholds = (double *)malloc(groups_number * sizeof(double));
    for(int i=0; i<groups_number; i++){
        thresholds[i] = determine_threshold(matrices_groups[i], matrices_number, known_negatives_percentages[i]);
    }
    return thresholds;
}

void apply_threshold(Matrix** matrices_groups, size_t const matrices_number, size_t const groups_number, double const* thresholds){
    for (int i=0; i<groups_number; i++){
        for (int j=0; j<matrices_number; j++){
            fill_above_matrix(&matrices_groups[i][j], NAN, thresholds[i], true);
            min_max_matrix_norm(&matrices_groups[i][j], true);
        }
    }
}

void threshold(Matrix const** self_matrices_groups, size_t  const self_matrices_number, Matrix** other_matrices_groups, size_t const other_matrices_number, size_t const groups_number, double const* known_negatives_percentages){
    double* thresholds = determine_thresholds(self_matrices_groups, self_matrices_number, groups_number, known_negatives_percentages);
    apply_threshold(other_matrices_groups, other_matrices_number, groups_number, thresholds);
    free(thresholds);
}