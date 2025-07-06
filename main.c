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
            int val = equalized[i][j];
            if(val < 0) val = 0;
            if(val > 255) val = 255;
            row[j] = (uint8_t)val;
        }
        fwrite(row, 1, 640, g);
        free(row);
    }
    fclose(g);
    //system("chmod +x /home/pi/project2/haar_select.py");
    int ret = system("python3 /home/pi/project2/haar_select.py /home/pi/project2/equalized.pgm");
    DIE(ret == -1, "python script failed");
    
    f = fopen("/home/pi/project2/faces/equalized_face_1.pgm", "rb");
    DIE(!f, "couldn't open the image file");

    char magic[3];
    int face_width, face_height, max_val;
    int items_read;

    items_read = fscanf(f, "%2s %d %d %d", magic, &face_width, &face_height, &max_val);
    DIE(items_read != 4, "failed to read PGM header");
    DIE(strcmp(magic, "P5") != 0, "not a valid P5 PGM file");
    char c;
    fread(&c, 1, 1, f);
    
    int **face_image = (int **)malloc_matrix(face_width, face_height, sizeof(int));
    DIE(!face_image, "malloc failed");
    for(int i = 0; i < face_height; ++i){
        for(int j = 0; j < face_width; ++j){
            uint8_t val;
            fread(&val, sizeof(uint8_t), 1, f);
            face_image[i][j] = (int)val;
        }
    }
    fclose(f);
    double** phi_final;
/*geodesic_level_set_contour(face_image, &phi_final, face_width, face_height,
                          1.0,
                         1,
                          0.1,
                         100); */
/*geodesic_level_set_contour(face_image, &phi_final, face_width, face_height,
                          1.0,
                          1,
                          0.02,
                          300);*/
    geodesic_level_set_contour(face_image, &phi_final, face_width, face_height, 
                                   1.0,//sigma pentru smoothing
                                   0.5,//niu - baloon force; pozitiv pt extindere exterior, negativ interior
                                   0.06,//dt, cat mai mic cu atat mai stabil
                                   1500);
                                   //2000); N
                     
   f = fopen("imagine_contour.pgm", "wb");
    DIE(!f, "couldn't create output file");

    fprintf(f, "P5\n%d %d\n255\n", face_width, face_height);

    for (int y = 0; y < face_height; ++y) {
        for(int x = 0; x < face_width; ++x) {
            double phi_val = phi_final[y][x];
            unsigned char pixel_val;
            if (phi_val > 0)
                pixel_val = 255;
            else
                pixel_val = 0;
            fwrite(&pixel_val, sizeof(unsigned char), 1, f);
        }
    }
    fclose(f);
    return 0;
}
