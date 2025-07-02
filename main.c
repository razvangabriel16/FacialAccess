#include "findcamera.h"

extern void yuyv_to_grayscale(const unsigned char *buffer, int width, int height, int ***grayscale);
extern void create_integral_matrix(const int **grayscale, const int width, int height, int ***integral_image);
extern void clahe_process(const unsigned char* input, 
                   unsigned char* output, 
                   int width, 
                   int height,
                   unsigned int* hist_buffer);

int main(void) {
    camera_device_t cameras[MAX_CAMERAS];
    int num_cameras = list_cameras(cameras);
    DIE(!num_cameras, "No cameras found!\n");
    
    printf("Found %d camera(s)\n\n", num_cameras);
    resolution_t max_resolution;
    
    int best_camera_idx = find_best_camera(cameras, num_cameras, &max_resolution);
    
    if (best_camera_idx >= 0)
        printf("Use camera: %s\n", cameras[best_camera_idx].device_path);
    else
        return 1;
    
    printf("%d %d\n", max_resolution.height, max_resolution.width);
    capture_result_t result = capture_image_yuyv(cameras, "/home/pi/project2/output.yuv", best_camera_idx, max_resolution);
    
    FILE* captured_photo = fopen("/home/pi/project2/output.yuv", "rb");
    DIE(!captured_photo, "No photo available");
    
    unsigned char *buffer = (unsigned char *)malloc(640 * 480 * 2);
    DIE(!buffer, "malloc failed");
    
    for (int i = 0; i < 640 * 480 * 2; ++i) {
        unsigned char byte;
        size_t temp = fread(&byte, sizeof(unsigned char), 1, captured_photo);
        DIE(temp != 1, "fread failed");
        buffer[i] = byte;
    }
    fclose(captured_photo);

    uint8_t **grayscale_photo;
    yuyv_to_grayscale(buffer, 640, 480, (void***)&grayscale_photo);

    FILE *f = fopen("imagine.pgm", "wb");
    fprintf(f, "P5\n%d %d\n255\n", 640, 480);
    for (int y = 0; y < 480; ++y) {
        fwrite(grayscale_photo[y], 1, 640, f); 
    }
    fclose(f);
    

    int **equalized = (int **)malloc_matrix(640, 480, sizeof(int));
    DIE(!equalized, "malloc failed");
    clahe(grayscale_photo, 640, 480, 3 , 64, &equalized);
    blend_with_original(equalized, grayscale_photo, 640, 480, 0.2f);
    FILE* g = fopen("equalized.pgm", "wb");
    fprintf(g, "P5\n%d %d\n255\n", 640, 480);
    for(int i = 0; i < 480; ++i) {
        uint8_t *row = malloc(640);
        for(int j = 0; j < 640; ++j) {
            // Clamp the value to 0-255 range
            int val = equalized[i][j];
            if(val < 0) val = 0;
            if(val > 255) val = 255;
            row[j] = (uint8_t)val;
    }
    fwrite(row, 1, 640, g);
    free(row);
}
fclose(g);
     //haar cascades part
    //system("chmod +x /home/pi/project2/haar_select.py");
    int ret = system("python3 /home/pi/project2/haar_select.py /home/pi/project2/equalized.pgm");
    DIE(ret == -1, "python script failed");
    

    
    
/*
     unsigned char *flat_grayscale = ( unsigned char*)malloc(640 * 480);
    DIE(!flat_grayscale, "malloc failed");
    
    for (int y = 0; y < 480; ++y) {
        for (int x = 0; x < 640; ++x) {
            flat_grayscale[y * 640 + x] = ( unsigned char)grayscale_photo[y][x];
        }
    }

    unsigned char* output_image = ( unsigned char*)malloc(640 * 480);
    DIE(!output_image, "malloc failed");

    int* hist_buf1 = (int *)malloc(256 * sizeof(int));
   
    DIE(!hist_buf1, "n");


    memset(hist_buf1, 0, 256 * sizeof(int));
    clahe_process(flat_grayscale,output_image, 640, 480, hist_buf1); 

    for(int i =0; i < 50; ++i)
        printf("%d ", hist_buf1[i]);
    printf("\n");

    f = fopen("equalized.pgm", "wb");
    fprintf(f, "P5\n%d %d\n255\n", 640, 480);
    fwrite(output_image, 1, 640 * 480, f);
    fclose(f);
            printf("***");
    for(int i = 0; i < 100; ++i){
        printf("%d", output_image[i]);
    }

    free(buffer);
    free(flat_grayscale);
    free(output_image);
    
    for (int y = 0; y < 480; ++y) {
        free(grayscale_photo[y]);
    }
    free(grayscale_photo);
 */   
    
    return 0;
}
