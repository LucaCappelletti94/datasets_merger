//
// Created by Luca Cappelletti on 2019-03-03.
//

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "column_alignment.h"
#include "load_csv.h"
#include "int.h"
#include "threshold.h"
#include "double.h"
#include "assignment.h"
#include "tarjan.h"
#include "json-c/json.h"
#include "vector.h"

#define verbose_alignment (0)

bool self_loading_rule(int i, int j){
    return i==j;
}

bool other_loading_rule(int i, int j){
    return i<j;
}

char* format_path(char* path, int metric, int df1, int df2){
    char* str_metric = int_to_str(metric);
    char* str_df1 = int_to_str(df1);
    char* str_df2 = int_to_str(df2);
    size_t size = strlen(path)+strlen(str_metric)+strlen(str_df1)+strlen(str_df2)+8;
    char* formatted_path = (char*)malloc(size* sizeof(char));
    snprintf(formatted_path, size, "%s/%s/%s-%s.csv", path, str_metric, str_df1, str_df2);
    free(str_metric);
    free(str_df1);
    free(str_df2);
    return formatted_path;
}

char* format_output_path(char* path, char* filename){
    size_t size = strlen(path)+strlen(filename)+7;
    char* formatted_path = (char*)malloc(size* sizeof(char));
    snprintf(formatted_path, size, "%s/%s.json", path, filename);
    return formatted_path;
}

int matrices_number(int datasets, bool (*loading_rule)(int, int)){
    int matrices = 0;
    for (int i=0; i<datasets; i++){
        for (int j=0; j<datasets; j++){
            if (loading_rule(i,j)){
                matrices++;
            }
        }
    }
    return  matrices;
}

int self_matrices_number(int datasets){
    return matrices_number(datasets, &self_loading_rule);
}

int other_matrices_number(int datasets){
    return matrices_number(datasets, &other_loading_rule);
}

Matrix** load_groups(char* path, int metrics, int datasets, int matrices, bool (*loading_rule)(int, int)){
    Matrix** groups = (Matrix**)malloc(metrics* sizeof(Matrix*));
    for (int metric=0; metric<metrics; metric++){
        groups[metric] = (Matrix*)malloc(matrices* sizeof(Matrix));
        for (int df1=0, matrix=0; df1<datasets; df1++){
            for (int df2=0; df2<datasets; df2++){
                if (loading_rule(df1, df2)){
                    char* formatted_path = format_path(path, metric, df1, df2);
                    groups[metric][matrix++] = load_csv(formatted_path);
                    free(formatted_path);
                }
            }
        }
    }
    return groups;
}

Matrix** load_self_groups(char* path, int metrics, int datasets, int matrices){
    return load_groups(path, metrics, datasets, matrices, &self_loading_rule);
}

Matrix** load_other_groups(char* path, int metrics, int datasets, int matrices){
    return load_groups(path, metrics, datasets, matrices, &other_loading_rule);
}

void free_groups(Matrix** groups, int metrics, int datasets, bool (*loading_rule)(int, int)){
    for (int metric=0; metric<metrics; metric++){
        for (int df1=0, matrix=0; df1<datasets; df1++){
            for (int df2=0; df2<datasets; df2++){
                if (loading_rule(df1, df2)){
                    free_matrix(groups[metric][matrix++]);
                }
            }
        }
        free(groups[metric]);
    }
    free(groups);
}

void free_self_groups(Matrix ** groups, int metrics, int datasets){
    return free_groups(groups, metrics, datasets, &self_loading_rule);
}

void free_other_groups(Matrix ** groups, int metrics, int datasets){
    return free_groups(groups, metrics, datasets, &other_loading_rule);
}

Matrix* groups_nan_composition(Matrix** groups, double const* weights, int metrics, int matrices){
    Matrix* groups_composition = (Matrix *)malloc(matrices* sizeof(Matrix));
    for (int i=0; i<matrices; i++){
        groups_composition[i]=init_matrix_like(groups[0][i]);
        for (int j = 0; j < groups_composition[i].h; j++) {
            for (int k = 0; k < groups_composition[i].w; k++) {
                int any_assignment = 0;
                groups_composition[i].M[j][k] = 0;
                for (int metric = 0; metric < metrics; metric++) {
                    if(is_not_nan(groups[metric][i].M[j][k])){
                        any_assignment = 1;
                        groups_composition[i].M[j][k] += weights[i]*groups[metric][i].M[j][k];
                    }
                }
                if (!any_assignment){
                    groups_composition[i].M[j][k] = NAN;
                }
            }
        }
    }
    return groups_composition;
}

void save_results(char const* path, int*** compositions, int groups, int const * compositions_number, int ** compositions_elements_number){
    json_object *results = json_object_new_array();
    json_object *outer, *inner;
    json_object **outers = (json_object**)malloc(groups*sizeof(json_object*));
    json_object ***inners = (json_object***)malloc(groups*sizeof(json_object**));
    for(int i=0; i<groups; i++){
        outer = json_object_new_array();
        outers[i] = outer;
        inners[i] = (json_object**)malloc(compositions_number[i]*sizeof(json_object*));
        json_object_array_add(results, outer);
        for(int j=0; j<compositions_number[i]; j++){
            inner = json_object_new_array();
            inners[i][j] = inner;
            json_object_array_add(outer, inner);
            for(int k=0; k<compositions_elements_number[i][j]; k++){
                int64_t value = compositions[i][j][k];
                json_object* json_value = json_object_new_int64(value);
                json_object_array_add(inner, json_value);
            }
        }
    }
    // Saving object to file
    json_object_to_file_ext(path, results, JSON_C_TO_STRING_PRETTY);
    // Clearing memory
    for (int i=0; i<groups; i++){
        for(int j=0; j<compositions_number[i]; j++){
            json_object_array_del_idx(inners[i][j], 0, (size_t)compositions_elements_number[i][j]);
        }
        free(inners[i]);
        json_object_array_del_idx(outers[i], 0, (size_t)compositions_number[i]);
    }
    free(inners);
    free(outers);
    json_object_array_del_idx(results, 0, (size_t)groups);
    free(results);
}

void free_2d_int_array(int ** array, int size){
    for(int i=0; i<size; i++){
        free(array[i]);
    }
    free(array);
}


void column_alignment(char* path, char* output, int datasets, int metrics, double const* known_negatives_percentages,  double const* weights){
    int self_matrices = self_matrices_number(datasets);
    int other_matrices = other_matrices_number(datasets);
    assert(self_matrices>0);
    assert(other_matrices>0);
    assert(metrics>0);
    assert(datasets>0);
    if verbose_alignment
        printf("Loading matrices from csvs...\n");
    Matrix ** self_matrices_groups = load_self_groups(path, metrics, datasets, self_matrices);
    Matrix ** other_matrices_groups = load_other_groups(path, metrics, datasets, other_matrices);
    if verbose_alignment
        printf("Applying thresholds to matrices...\n");
    // Values above thresholds are set to NAN, then matrices are max_min normalized in a NAN-aware fashion.
    threshold(self_matrices_groups, self_matrices, other_matrices_groups, other_matrices, metrics, known_negatives_percentages);
    if verbose_alignment
        printf("Build weighted matrices...\n");
    Matrix * groups_composition = groups_nan_composition(other_matrices_groups, weights, metrics, other_matrices);
    if verbose_alignment
        printf("Solving assignment problems using hungarian algorithm...\n");
    int**** composition_assignment = solve_assignment_problems(&groups_composition, 1, other_matrices);
    int**** other_assignment = solve_assignment_problems(other_matrices_groups, metrics, other_matrices);
    if verbose_alignment
        printf("Building weighted adjacency matrices...\n");
    Matrix* composition_adjacency_matrix = groups_to_adjacency_matrix(&groups_composition, composition_assignment, 1, other_matrices);
    Matrix* other_adjacency_matrix = groups_to_adjacency_matrix(other_matrices_groups, other_assignment, metrics, other_matrices);
    if verbose_alignment
        printf("Freeing assignments...\n");
    free_assignemnt_groups(composition_assignment, &groups_composition, 1, other_matrices);
    free_assignemnt_groups(other_assignment, other_matrices_groups, metrics, other_matrices);
    if verbose_alignment
        printf("Running Tarjan algorithm on the weighted adjacency matrices...\n");
    int* composition_components_number = (int*)malloc(sizeof(int));
    int* other_components_number = (int*)malloc(metrics*sizeof(int));
    int** composition_components_elements_number = (int**)malloc(sizeof(int*));
    int** other_components_elements_number = (int**)malloc(metrics*sizeof(int*));
    int*** composition_connected_components = determine_group_connected_components(composition_adjacency_matrix, 1, composition_components_number, composition_components_elements_number);
    int*** other_connected_components = determine_group_connected_components(other_adjacency_matrix, metrics, other_components_number, other_components_elements_number);
    if verbose_alignment
        printf("Freeing groups...\n");
    free_self_groups(self_matrices_groups, metrics, datasets);
    free_other_groups(other_matrices_groups, metrics, datasets);
    if verbose_alignment
        printf("Saving results to json...\n");
    char* composition_output = format_output_path(output, "composition");
    char* other_output = format_output_path(output, "other");
    save_results(composition_output, composition_connected_components, 1, composition_components_number, composition_components_elements_number);
    save_results(other_output, other_connected_components, metrics, other_components_number, other_components_elements_number);
    if verbose_alignment
        printf("Freeing connected components...\n");
    free_group_connected_components(composition_connected_components, 1, composition_components_number);
    free_group_connected_components(other_connected_components, metrics, other_components_number);
    if verbose_alignment
        printf("Freeing various helper arrays...\n");
    free(composition_components_number);
    free(other_components_number);
    free_2d_int_array(composition_components_elements_number, 1);
    free_2d_int_array(other_components_elements_number, metrics);
}