#include "findcamera.h"

extern void yuyv_to_grayscale(const unsigned char *buffer, int width, int height, int ***grayscale);
extern void create_integral_matrix(const int **grayscale, const int width, int height, int ***integral_image);
extern void clahe_process(const unsigned char* input, 
                   unsigned char* output, 
                   int width, 
                   int height,
                   unsigned int* hist_buffer);

typedef struct {
    char* command;
    char** args;
    char* output_file;
    int interval;
} monitor_config_t;

void deep_copy(void* arg_void, monitor_config_t** out_new_config) {
    monitor_config_t* arg = (monitor_config_t*)arg_void;

    *out_new_config = malloc(sizeof(monitor_config_t));
    DIE(!*out_new_config, "malloc failed");
    (*out_new_config)->command = strdup(arg->command);
    DIE(!(*out_new_config)->command, "strdup failed");
    (*out_new_config)->output_file = strdup(arg->output_file);
    DIE(!(*out_new_config)->output_file, "strdup failed\n");
    (*out_new_config)->interval = arg->interval;
    int argc = 0;
    while (arg->args[argc]) argc++;
    (*out_new_config)->args = malloc((argc + 1) * sizeof(char*));
    DIE(!(*out_new_config)->args, "malloc failed for args");

    for (int i = 0; i < argc; i++) {
        (*out_new_config)->args[i] = strdup(arg->args[i]);
        DIE(!(*out_new_config)->args[i], "strdup failed for args[i]");
    }
    (*out_new_config)->args[argc] = NULL;
}


void* monitor_thread(void* arg){
   monitor_config_t* config;
   deep_copy(arg, &config);
    //monitor_config_t* config = (monitor_config_t*)arg;
    while(1){
        pid_t pid = fork();
        DIE(pid == -1, "fork err\n");
        if(!pid){
            int fd = open(config->output_file, O_WRONLY | O_CREAT | O_APPEND, 0644);
            DIE(fd == -1, "open file failed");
            DIE(dup2(fd, STDOUT_FILENO) == -1, "dup2 failed");
            close(fd);
            execvp(config->command, config->args);
            perror("execvp failed");
            exit(1);
        }else{
            int status;
            wait(&status);
            //DIE(WIFEXITED(status) && WEXITSTATUS(status) != 0, "failed\n");
            if(WIFEXITED(status) && WEXITSTATUS(status) == 0){
                int fd = open(config->output_file, O_WRONLY | O_APPEND, 0644);
                if(fd != -1){
                    write(fd, "\n", 1);
                    close(fd);
                }
            }
        }
        sleep(config->interval);
    }
    return NULL;
}

void multithread_stats(void){
    static char* temp_args[] = {"vcgencmd", "get_temp", NULL};
    static char* throttle_args[] = {"vcgencmd", "get_throttled", NULL};
    static char* cpu_args[] = {"cat", "/proc/loadavg", NULL};
    static char* memory_args[] = {"free", "-h", NULL};
    
    monitor_config_t temp_config = {
        .command = "vcgencmd",
        .args = temp_args,
        .output_file = "/home/pi/project2/stats/temp",
        .interval = 5
    };  
    monitor_config_t throttle_config = {
        .command = "vcgencmd",
        .args = throttle_args,
        .output_file = "/home/pi/project2/stats/throttled",
        .interval = 10
    };  
    monitor_config_t cpu_config = {
        .command = "cat",
        .args = cpu_args,
        .output_file = "/home/pi/project2/stats/cpu_load",
        .interval = 5
    };  
    monitor_config_t memory_config = {
        .command = "free",
        .args = memory_args,
        .output_file = "/home/pi/project2/stats/memory",
        .interval = 10
    };
    pthread_t temp_thread, throttle_thread, cpu_thread, memory_thread;
    pthread_create(&temp_thread, NULL, monitor_thread, &temp_config);
    pthread_create(&throttle_thread, NULL, monitor_thread, &throttle_config);
    pthread_create(&cpu_thread, NULL, monitor_thread, &cpu_config);
    pthread_create(&memory_thread, NULL, monitor_thread, &memory_config);
    //pthread_join(temp_thread, NULL);
    //pthread_join(throttle_thread, NULL);
    //pthread_join(cpu_thread, NULL);
    //pthread_join(memory_thread, NULL);
    pthread_detach(temp_thread);
    pthread_detach(throttle_thread);
    pthread_detach(cpu_thread);
    pthread_detach(memory_thread);
}

int main(void) {
    multithread_stats();
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
    DIE(!captured_photo, "no photo available");
    
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
                                   1200);
                                   //1500);
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
    //chmod +x interpolate.py
    //int ret2 = system("python3 /home/pi/project2/interpolate.py");
    //DIE(ret2 == -1, "python script2 err\n");
    return 0;
}
