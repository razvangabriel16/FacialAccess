//#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <dirent.h>
#include <linux/types.h>
#include <limits.h>
#include <stdint.h>
#include <sys/mman.h>
#include <math.h>

#define DIE(assertion, call_description)				\
        do {								\
                if (assertion) {					\
                        fprintf(stderr, "(%s, %d): ",			\
                                        __FILE__, __LINE__);		\
                        perror(call_description);			\
                        exit(errno);					\
                }							\
        } while (0)


#ifndef FINDCAMERA_H
#define FINDCAMERA_H

#define THRESH 64
#define MAX_CAMERAS 100
#define MAX_CAMERA_NAME 100
#define MAX_DEVICE_PATH 256
#define MAX_CAMERA_FORMATS 100
#define MAX_CAMERA_RESOLUTIONS 32

typedef enum {
    CAPTURE_SUCCESS = 0,
    CAPTURE_NO_CAMERA = -1,
    CAPTURE_NO_FSWEBCAM = -2,
    CAPTURE_MEMORY_ERROR = -3,
    CAPTURE_WRITE_ERROR = -4,
    CAPTURE_UNKNOWN_ERROR = -5,
    CAPTURE_NO_PERM = -6,
    CAPTURE_SETTING_ERROR = -7
} capture_result_t;

typedef struct {
    __u32 width;
    __u32 height;
} resolution_t;

typedef struct {
    resolution_t* resolutions;
    __u32 pixelformat;
    /* direct din v4l2_fmtdesc
    kernelul are un typedef de tipul typedef unsigned int __u32; pentru ca
    pe unele arhitecturi int poate fi 16, 32 sau 64 de biti si vrem portabilitate
    intre arhitecturi x86, ARM, MPIS etc.
    */
    int resolutions_number;
} camera_format_t;

typedef struct {
    char device_path[256];
    //camera_format_t formats[100]; //ulimit -s
    camera_format_t* formats;
    //e important sa vad care e dimensiunea stack-ului per thread pe arhitectura sa nu dea crash
    int num_formats;
} camera_device_t;

typedef struct __attribute__((packed)){
    float weight; //aliniere
    __u16 x;
    __u16 y;
    __u16 width;
    __u16 height;
}feature_haar_t;

typedef struct {
    int x_left, y_left, x_right, y_right;
} coordinates_t;

static inline int min(int a, int b);
static inline int min(int a, int b);
int is_camera_device(const char* path);
int list_cameras(camera_device_t* cameras);
camera_device_t camera_specs(int fd, const char* device_path);
long long camera_score(const camera_device_t camera, resolution_t* result);
int find_best_camera(camera_device_t* cameras, int num_cameras, resolution_t* result);
const char* get_error_message(capture_result_t result);
capture_result_t capture_image_yuyv(camera_device_t* cameras, const char* image_name, int index, resolution_t max_resolution);
capture_result_t capture_image_jpeg(camera_device_t* cameras, const char* image_name, int index, resolution_t max_resolution);
capture_result_t capture_image2(const char* image_name, resolution_t max_resolution);
void** malloc_matrix(int width, int height, int data_size);
void put_coordinates(coordinates_t *** tiles, int oriz_tiles, int vert_tiles, int tile_size);
void free_matrix_int(int **matrix, const int dim);
int *** malloc_cubic_int_matrixes(int width, int height, int depth);
void put_histograms(int **** histograms, coordinates_t ** tiles, int oriz_tiles, int vert_tiles, int tile_size,
                    const uint8_t * const *grayscale_photo, int width, int height);
void clip(int **histogram, int cliplimit, int tile_size);
void create_lut(int **histogram, int **lut, int tile_size);
void create_luts(int *** histograms, int **** luts, int oriz_tiles, int vert_tiles, int tile_size);
int interpolate_pixel(uint8_t pixel_value, int r, int c, int *** luts, int oriz_tiles, int vert_tiles, 
                     int tile_size, int width, int height);
void apply_clahe(const uint8_t * const *grayscale_photo, int *** equalized_image, int *** luts,
                int oriz_tiles, int vert_tiles, int tile_size, int width, int height);                     
void free_cubic_matrix(int *** matrix, int height, int width);
void clahe(const uint8_t * const *grayscale_photo, int width, int height, const int cliplimit, 
          const int tile_size, int ***equalized_image);
void blend_with_original(int **equalized, const uint8_t * const *original, 
                        int width, int height, float clahe_strength);
#endif