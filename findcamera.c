#include "findcamera.h"

static inline int min(int a, int b) { return a < b ? a : b; }
static inline int max(int a, int b) { return a > b ? a : b; }

int is_camera_device(const char* path) {
    struct v4l2_capability cap;
    int file_descriptor = open(path, O_RDWR);
    //DIE(file_descriptor < 0, "cannot oepn device_path");
    if(file_descriptor < 0) return 0;
    int analisys = ioctl(file_descriptor, VIDIOC_QUERYCAP, &cap);
    close(file_descriptor);
    if (analisys < 0) return 0;
    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) return 0;
    /* capabilitatile sunt definite ca setting-uri ale anumitor biti
    #define V4L2_CAP_VIDEO_CAPTURE     0x00000001  // bit 0
    #define V4L2_CAP_VIDEO_OUTPUT      0x00000002  // bit 1  
    #define V4L2_CAP_VIDEO_OVERLAY     0x00000004  // bit 2
    #define V4L2_CAP_STREAMING         0x04000000  // bit 26
    cap.capabilities poate avea multipli biti setati insemnand ca are multiple capabilities
    */

    //Aceasta actualizare pt a evita loopback-uri
    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        close(file_descriptor);
        return 0;
    }
    const char* bus_info = (char*) cap.bus_info;
    int is_real_camera = 0;
    if (strstr(bus_info, "usb-") != NULL) is_real_camera = 1;
    if (strstr(bus_info, "PCI") != NULL) is_real_camera = 1;
    if (strstr(bus_info, "platform:") != NULL && strstr(bus_info, "v4l2loopback") == NULL) is_real_camera = 1;
    return is_real_camera;
}

int list_cameras(camera_device_t* cameras){
    struct dirent* de;
    DIR* dr = opendir("/dev");
    DIE(!dr, "couldnt open directory");
    int camera_index = 0;
    while((de = readdir(dr))){
        if (camera_index >= MAX_CAMERAS) break;
        if(!strncmp(de->d_name, "video", 5)){
            char device_path[MAX_DEVICE_PATH];
            snprintf(device_path, sizeof(device_path), "/dev/%s", de->d_name);
            if(is_camera_device(device_path)){
                int fd = open(device_path, O_RDWR);
                DIE(fd < 0, "cannot open device");
                
                cameras[camera_index] = camera_specs(fd, device_path);
                close(fd);
                
                camera_index++;
            }
        }
    }
    //TODO free(camera_names)
    closedir(dr);
    return camera_index;
}
camera_device_t camera_specs(int fd, const char* device_path) {
    struct v4l2_fmtdesc fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    
    camera_device_t current_camera;
    memset(&current_camera, 0, sizeof(camera_device_t));
    
    current_camera.formats = (camera_format_t*)malloc(sizeof(camera_format_t) * MAX_CAMERA_FORMATS);
    DIE(!current_camera.formats, "camera_formats crash");
    
    strncpy(current_camera.device_path, device_path, sizeof(current_camera.device_path) - 1);
    current_camera.device_path[sizeof(current_camera.device_path) - 1] = '\0';
    
    /*
    Facem asta pentru ca un v4l2 device suporta mai multe tipuri de stream-uri : video-capture,
    video-output, overlay, metadata streams etc.
    Facem loop pentru ca unele camere suporta mai multe formate de pixeli: MJPEG(compressed), YUYV(uncompressed), H264 etc.
    si vreau sa le parcurg pe toate
    */
    int format_index = 0;
    while(!ioctl(fd, VIDIOC_ENUM_FMT, &fmt) && format_index < MAX_CAMERA_FORMATS) {
        current_camera.formats[format_index].pixelformat = fmt.pixelformat;

        struct v4l2_frmsizeenum frmsize;
        memset(&frmsize, 0, sizeof(frmsize));
        frmsize.pixel_format = fmt.pixelformat;

        camera_format_t* current_format = &current_camera.formats[format_index];
        
        current_format->resolutions = (resolution_t*)malloc(sizeof(resolution_t) * MAX_CAMERA_RESOLUTIONS);
        DIE(!current_format->resolutions, "resolutions alloc failed");

        int resolution_index = 0;
        while (ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frmsize) == 0 && resolution_index < MAX_CAMERA_RESOLUTIONS) {
            if (frmsize.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
                current_format->resolutions[resolution_index].width = frmsize.discrete.width;
                current_format->resolutions[resolution_index].height = frmsize.discrete.height;
                printf("\tResolution: %ux%u\n", frmsize.discrete.width, frmsize.discrete.height);
                resolution_index++;
            }
            frmsize.index++;
        }

        current_format->resolutions_number = resolution_index;
        format_index++;
        fmt.index++;
    }
    current_camera.num_formats = format_index;
    
    return current_camera;
    //TODO free pentru resolutions si pentru formats
}

long long camera_score(const camera_device_t camera, resolution_t* result){
    long long max_score = 0;
    for(int i = 0; i < camera.num_formats; ++i){
        const camera_format_t current_format = camera.formats[i];
        for(int j = 0; j < current_format.resolutions_number; ++j){
            __u32 width = current_format.resolutions[j].width;
            __u32 height = current_format.resolutions[j].height;
            long long score = (long long) width * height;
            if(score > max_score){ max_score = score; *result = current_format.resolutions[j]; } //shalow copy
        }
    }
    return max_score;
}

int find_best_camera(camera_device_t* cameras, int num_cameras, resolution_t* result) {
    if (num_cameras <= 0) return -1;
    
    int best_camera_index = 0;
    long long best_score = camera_score(cameras[0], result);
    
    printf("Camera scores:\n");
    for(int i = 0; i < num_cameras; i++) {
        long long score = camera_score(cameras[i], result);
        printf("Camera %s: %lld pixels\n", cameras[i].device_path, score);
        
        if(score > best_score) {
            best_score = score;
            best_camera_index = i;
        }
    }
    
    printf("\nBest camera: %s with %lld pixels\n", 
           cameras[best_camera_index].device_path, best_score);
    
    return best_camera_index;
}


const char* get_error_message(capture_result_t result) {
    switch(result) {
        case CAPTURE_SUCCESS: return "Succes";
        case CAPTURE_NO_CAMERA: return "Camera nu a fost găsită";
        case CAPTURE_NO_FSWEBCAM: return "fswebcam nu este instalat";
        case CAPTURE_MEMORY_ERROR: return "Eroare de memorie camera";
        case CAPTURE_WRITE_ERROR: return "Nu pot scrie imaginea";
        case CAPTURE_NO_PERM: return "Nu am suficiente permisiuni";
        case CAPTURE_SETTING_ERROR: return "Eroare la setarile granulare cu v4l2";
        default: return "Eroare necunoscută";
    }
}


capture_result_t capture_image_yuyv(camera_device_t* cameras, const char* image_name, int index, resolution_t max_resolution){
    char* camera_path = cameras[index].device_path;
    int file_descriptor = open(camera_path, O_RDWR);
    if(file_descriptor < 0) return CAPTURE_UNKNOWN_ERROR; //enoent sau eacces 
    struct v4l2_format fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ioctl(file_descriptor, VIDIOC_S_FMT, &fmt);
    fmt.fmt.pix.height = 480;//max_resolution.height;
    fmt.fmt.pix.width = 640;//max_resolution.width;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV; //sau pot sa pun MJPEG
    fmt.fmt.pix.field = V4L2_FIELD_NONE;
    if(ioctl(file_descriptor, VIDIOC_S_FMT, &fmt) == -1) {
        close(file_descriptor);
        return CAPTURE_SETTING_ERROR;
    }
    const size_t expected_size = fmt.fmt.pix.width * fmt.fmt.pix.height * 2;
    DIE(fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_YUYV, "Camera doesn't support YUYV\n");

    struct v4l2_requestbuffers req = {0};
    req.count = 1;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if(ioctl(file_descriptor, VIDIOC_REQBUFS, &req) == -1) {
        close(file_descriptor);
        return CAPTURE_SETTING_ERROR;
    }
    struct v4l2_buffer buf = {0};
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = 0;
    if(ioctl(file_descriptor, VIDIOC_QUERYBUF, &buf) == -1){
        close(file_descriptor);
        return CAPTURE_SETTING_ERROR;
    }
    uint8_t* buffer = (uint8_t*)mmap (NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, file_descriptor, buf.m.offset);
    if(buffer == MAP_FAILED) {
        close(file_descriptor);
        return CAPTURE_SETTING_ERROR;
    }
    if(ioctl(file_descriptor, VIDIOC_QBUF, &buf) == -1) {
        munmap(buffer, buf.length);
        close(file_descriptor);
        return CAPTURE_SETTING_ERROR;
    }
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(ioctl(file_descriptor, VIDIOC_STREAMON, &type) == -1) {
        munmap(buffer, buf.length);
        close(file_descriptor);
        return CAPTURE_SETTING_ERROR;
    }
    if(ioctl(file_descriptor, VIDIOC_DQBUF, &buf) == -1) {
        ioctl(file_descriptor, VIDIOC_STREAMOFF, &type);
        munmap(buffer, buf.length);
        close(file_descriptor);
        return CAPTURE_SETTING_ERROR;
    }
    ioctl(file_descriptor, VIDIOC_STREAMOFF, &type);

    FILE* output_file = fopen(image_name, "wb");
    if(!output_file) {
        perror("Failed to create file");
        munmap(buffer, buf.length);
        close(file_descriptor);
        return CAPTURE_SETTING_ERROR;
    }
    /*
    fprintf(output_file, "P7\nWIDTH %d\nHEIGHT %d\nDEPTH 2\nMAXVAL 255\nTUPLTYPE YUV422\nENDHDR\n",
            fmt.fmt.pix.width, fmt.fmt.pix.height);*/

    size_t bytes_written = fwrite(buffer, 1, buf.bytesused, output_file);
    fclose(output_file);
    munmap(buffer, buf.length);
    close(file_descriptor);
    DIE(abs((long)bytes_written - (long)expected_size) > THRESH, "size mismatch");
    struct stat st;
    if (stat(image_name, &st) == 0 && st.st_size > 0) {
        printf("Imagine salvată: %s (%ld bytes)\n", image_name, st.st_size);
        return CAPTURE_SUCCESS;
    }
    return CAPTURE_UNKNOWN_ERROR;
} 


capture_result_t capture_image2(const char* image_name, resolution_t max_resolution){
    char command[512];
    char buffer[512];
    int camera_opened = 0;
    int image_written = 0;
    snprintf(command, sizeof(command), 
             "fswebcam -d /dev/video0 -r %dx4%d --skip 5 --jpeg 90 --no-banner %s 2>&1", max_resolution.width, max_resolution.height,
             image_name);
    FILE *fdes = popen(command, "r");
    if (!fdes) {
        return CAPTURE_NO_FSWEBCAM;
    }
    while(fgets(buffer, sizeof(buffer), fdes)){
        if (strstr(buffer, "--- Opening") || strstr(buffer, "/dev/video"))
            camera_opened = 1;
        if (strstr(buffer, "Writing JPEG image") || strstr(buffer, "Capturing frame"))
            image_written = 1;
        if (strstr(buffer, "Unable to open") || strstr(buffer, "No such device")) {
            pclose(fdes);
            return CAPTURE_NO_CAMERA;
        }
         if (strstr(buffer, "command not found") || strstr(buffer, "fswebcam: not found")) {
            pclose(fdes);
            return CAPTURE_NO_FSWEBCAM;
        }
        
        if (strstr(buffer, "mmap") || strstr(buffer, "VIDIOC_")) {
            pclose(fdes);
            return CAPTURE_MEMORY_ERROR;
        }
        if (strstr(buffer, "Permission denied")) {
            pclose(fdes);
            return CAPTURE_NO_PERM;
        }
    }
    int exit_code = pclose(fdes);
    struct stat st;
    if (stat(image_name, &st) == 0 && st.st_size > 0) {
        printf("Imagine salvată: %s (%ld bytes)\n", image_name, st.st_size);
        return CAPTURE_SUCCESS;
    }
    if (exit_code != 0) {
        if (!camera_opened) {
            return CAPTURE_NO_CAMERA;
        }
        if (camera_opened && !image_written) {
            return CAPTURE_WRITE_ERROR;
        }
    }
    
    return CAPTURE_UNKNOWN_ERROR;
}

capture_result_t capture_image_jpeg(camera_device_t* cameras, const char* image_name, int index, resolution_t max_resolution){
    char* camera_path = cameras[index].device_path;
    int file_descriptor = open(camera_path, O_RDWR);
    if(file_descriptor < 0) return CAPTURE_UNKNOWN_ERROR; //enoent sau eacces 
    struct v4l2_format fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ioctl(file_descriptor, VIDIOC_S_FMT, &fmt);
    fmt.fmt.pix.height = 480;//max_resolution.height;
    fmt.fmt.pix.width = 640;//max_resolution.width;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;
    if(ioctl(file_descriptor, VIDIOC_S_FMT, &fmt) == -1) {
        close(file_descriptor);
        return CAPTURE_SETTING_ERROR;
    }
    struct v4l2_requestbuffers req = {0};
    req.count = 1;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if(ioctl(file_descriptor, VIDIOC_REQBUFS, &req) == -1) {
        close(file_descriptor);
        return CAPTURE_SETTING_ERROR;
    }
    struct v4l2_buffer buf = {0};
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = 0;
    if(ioctl(file_descriptor, VIDIOC_QUERYBUF, &buf) == -1){
        close(file_descriptor);
        return CAPTURE_SETTING_ERROR;
    }
    uint8_t* buffer = (uint8_t*)mmap (NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, file_descriptor, buf.m.offset);
    if(buffer == MAP_FAILED) {
        close(file_descriptor);
        return CAPTURE_SETTING_ERROR;
    }
    if(ioctl(file_descriptor, VIDIOC_QBUF, &buf) == -1) {
        munmap(buffer, buf.length);
        close(file_descriptor);
        return CAPTURE_SETTING_ERROR;
    }
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(ioctl(file_descriptor, VIDIOC_STREAMON, &type) == -1) {
        munmap(buffer, buf.length);
        close(file_descriptor);
        return CAPTURE_SETTING_ERROR;
    }
    if(ioctl(file_descriptor, VIDIOC_DQBUF, &buf) == -1) {
        ioctl(file_descriptor, VIDIOC_STREAMOFF, &type);
        munmap(buffer, buf.length);
        close(file_descriptor);
        return CAPTURE_SETTING_ERROR;
    }
    ioctl(file_descriptor, VIDIOC_STREAMOFF, &type);

    FILE* output_file = fopen(image_name, "wb");
    if(!output_file) {
        perror("Failed to create file");
        munmap(buffer, buf.length);
        close(file_descriptor);
        return CAPTURE_SETTING_ERROR;
    }

    size_t bytes_written = fwrite(buffer, 1, buf.bytesused, output_file);
    fclose(output_file);
    munmap(buffer, buf.length);
    close(file_descriptor);
     /*
    st_size mai variaza putin, asta e explicatia
    # Simplified JPEG compression variability
    def compress_frame(frame):
        quality = random.uniform(88, 92)  //Auto-adjusting quality
        tables = choose_quant_table()     //May differ per frame
        return jpeg_compress(frame, quality, tables)  //Output size varies
        
        Capture Scenario	Expected Size Variance
        Static scene	±1-3% variation
        Slight lighting changes	±5% variation
        Active auto-focus/exposure	±10% variation

    */
    struct stat st;
    if (stat(image_name, &st) == 0 && st.st_size > 0) {
        printf("Imagine salvată: %s (%ld bytes)\n", image_name, st.st_size);
        return CAPTURE_SUCCESS;
    }
    return CAPTURE_UNKNOWN_ERROR;
}

void** malloc_matrix(int width, int height, int data_size){
    void** result = malloc(height * sizeof(void*));
    DIE(!result, "malloc failed");
    for(int i = 0; i < height; ++i){
        *(result + i) = malloc(width * data_size);
        if(!result[i]){
            for(int j = 0; j < i; ++j)
                free(result[j]);
            free(result);
            return NULL;
        }
        memset(result[i], 0, width * data_size);
    }
    return result;
}

void put_coordinates(coordinates_t *** tiles, int oriz_tiles, int vert_tiles, int tile_size) {
    for(int i = 0; i < vert_tiles; ++i){
        for(int j = 0; j < oriz_tiles; ++j){
            (*tiles)[i][j].x_left = j * tile_size;
            (*tiles)[i][j].y_left = i * tile_size;
            (*tiles)[i][j].x_right = min((j + 1) * tile_size, 640);
            (*tiles)[i][j].y_right = min((i + 1) * tile_size, 480);
        }
    }
}

void free_matrix_int(int **matrix, const int dim){
    if(!matrix || !dim)
        return;
    for(int i = 0; i < dim; ++i)
            free(matrix[i]);
    free(matrix);
}

void free_matrix(void** matrix, const int dim){
     if(!matrix || !dim)
        return;
    for(int i = 0; i < dim; ++i)
            free(matrix[i]);
    free(matrix);
}

int *** malloc_cubic_int_matrixes(int width, int height, int depth){
    int*** result = (int ***)malloc(sizeof(int**) * height); 
    if(!result)
        return NULL;
    for(int i = 0; i < height; ++i){
        result[i] = (int**)malloc_matrix(width, depth, sizeof(int));
        if(!result[i]){
            for(int j = 0; j < i; ++j){
                free_matrix_int(result[j], width);
            }
            free(result);
            return NULL;
        }
    }
    return result;
}

void put_histograms(int **** histograms, coordinates_t ** tiles, int oriz_tiles, int vert_tiles, int tile_size, const uint8_t * const *grayscale_photo, int width, int height){
    *histograms = malloc_cubic_int_matrixes(oriz_tiles, vert_tiles, 256);
    //*histograms = malloc_cubic_int_matrixes(vert_tiles, oriz_tiles, 256);
    if(!*histograms)
        return;
    for(int i = 0; i < vert_tiles; ++i)
        for(int j = 0; j < oriz_tiles; ++j)
            for(int k = 0; k < 256; ++k)
                (*histograms)[i][j][k] = 0;
    for(int i = 0; i < vert_tiles; ++i){
        for(int j = 0; j < oriz_tiles; ++j){
            int x_left = tiles[i][j].x_left;
            int x_right = tiles[i][j].x_right;
            int y_left = tiles[i][j].y_left;
            int y_right = tiles[i][j].y_right;
            for(int ii = y_left; ii < y_right; ++ii)
                for(int jj = x_left; jj < x_right; ++jj)
                    {
                        uint8_t pixel_val = grayscale_photo[ii][jj];
if(pixel_val >= 0 && pixel_val < 256) {
    (*histograms)[i][j][pixel_val]++;
}
                    }
        }
    }
}

void clip(int **histogram, int cliplimit, int tile_size){
    int absolute_clip = round(cliplimit * tile_size * tile_size / 256);
    if(absolute_clip < 1)
        absolute_clip = 1;
    int excess = 0;
    for(int i = 0; i < 256; ++i)
        if((*histogram)[i] > absolute_clip){
            excess += ((*histogram)[i] - absolute_clip);
            (*histogram)[i] = absolute_clip; 
        }
    if(excess > 0){
        int newstep = excess / 256;
        int reziduu = excess % 256;
        for(int i = 0; i < 256; ++i){
            (*histogram)[i] += newstep;
            if(i < reziduu)
                (*histogram)[i]++;
        }
    }
}

void create_lut(int **histogram, int **lut, int tile_size){
    int cumulative = 0;
    int total_pixels = tile_size * tile_size;
    
    for(int i = 0; i < 256; ++i){
        cumulative += (*histogram)[i];
        (*lut)[i] = (cumulative * 255) / total_pixels;
        if((*lut)[i] > 255) (*lut)[i] = 255;
        if((*lut)[i] < 0) (*lut)[i] = 0;
    }
}

void create_luts(int *** histograms, int **** luts, int oriz_tiles, int vert_tiles, int tile_size){
    *luts = malloc_cubic_int_matrixes(oriz_tiles, vert_tiles, 256);
    if(!*luts)
        return;
    
    for(int i = 0; i < vert_tiles; ++i)
        for(int j = 0; j < oriz_tiles; ++j)
            create_lut(&histograms[i][j], &(*luts)[i][j], tile_size);
}

int interpolate_pixel(uint8_t pixel_value, int r, int c, int *** luts, int oriz_tiles, int vert_tiles, 
                     int tile_size, int width, int height){
    float exact_tile_row = (float)(r + 0.5) * vert_tiles / height - 0.5;
    float exact_tile_col = (float)(c + 0.5) * oriz_tiles / width - 0.5;
    
    int tile_row1 = (int)floor(exact_tile_row);
    int tile_row2 = tile_row1 + 1;
    int tile_col1 = (int)floor(exact_tile_col);
    int tile_col2 = tile_col1 + 1;
    
    if(tile_row1 < 0) tile_row1 = 0;
    if(tile_row2 >= vert_tiles) tile_row2 = vert_tiles - 1;
    if(tile_col1 < 0) tile_col1 = 0;
    if(tile_col2 >= oriz_tiles) tile_col2 = oriz_tiles - 1;
    
    if(tile_row1 == tile_row2 && tile_col1 == tile_col2){
        return luts[tile_row1][tile_col1][pixel_value];
    }
    
    if(tile_row1 == tile_row2){
        float weight_col = exact_tile_col - floor(exact_tile_col);
        int val1 = luts[tile_row1][tile_col1][pixel_value];
        int val2 = luts[tile_row1][tile_col2][pixel_value];
        return (int)(val1 * (1.0 - weight_col) + val2 * weight_col);
    }
    
    if(tile_col1 == tile_col2){
        float weight_row = exact_tile_row - floor(exact_tile_row);
        int val1 = luts[tile_row1][tile_col1][pixel_value];
        int val2 = luts[tile_row2][tile_col1][pixel_value];
        return (int)(val1 * (1.0 - weight_row) + val2 * weight_row);
    }
    float weight_row = exact_tile_row - floor(exact_tile_row);
    float weight_col = exact_tile_col - floor(exact_tile_col);
    
    int val11 = luts[tile_row1][tile_col1][pixel_value];
    int val12 = luts[tile_row1][tile_col2][pixel_value];
    int val21 = luts[tile_row2][tile_col1][pixel_value];
    int val22 = luts[tile_row2][tile_col2][pixel_value];
    
    float interp_row1 = val11 * (1.0 - weight_col) + val12 * weight_col;
    float interp_row2 = val21 * (1.0 - weight_col) + val22 * weight_col;
    float final_value = interp_row1 * (1.0 - weight_row) + interp_row2 * weight_row;
    
    int result = (int)round(final_value);
    if(result < 0) result = 0;
    if(result > 255) result = 255;
    
    return result;
}

void apply_clahe(const uint8_t * const *grayscale_photo, int *** equalized_image, int *** luts,
                int oriz_tiles, int vert_tiles, int tile_size, int width, int height){
    *equalized_image = (int**)malloc_matrix(width, height, sizeof(int));
    if(!*equalized_image)
        return;
    
    for(int r = 0; r < height; ++r){
        for(int c = 0; c < width; ++c){
            uint8_t pixel_value = grayscale_photo[r][c];
            (*equalized_image)[r][c] = interpolate_pixel(pixel_value, r, c, luts, 
                                                       oriz_tiles, vert_tiles, tile_size, width, height);
        }
    }
}

void free_cubic_matrix(int *** matrix, int height, int width){
    if(!matrix) return;
    for(int i = 0; i < height; ++i){
        if(matrix[i]){
            for(int j = 0; j < width; ++j){
                free(matrix[i][j]);
            }
            free(matrix[i]);
        }
    }
    free(matrix);
}

void clahe(const uint8_t * const *grayscale_photo, int width, int height, const int cliplimit, 
          const int tile_size, int ***equalized_image){
    if(!grayscale_photo || !tile_size || cliplimit < 2)
        return;
    
    int oriz_tiles = width / tile_size;
    int vert_tiles = height / tile_size;
    
    coordinates_t **tiles = (coordinates_t **)malloc_matrix(oriz_tiles, vert_tiles, sizeof(coordinates_t));
    DIE(!tiles, "malloc failed");
    put_coordinates(&tiles, oriz_tiles, vert_tiles, tile_size);
    
    int *** histograms;
    put_histograms(&histograms, tiles, oriz_tiles, vert_tiles, tile_size, grayscale_photo, width, height);
    if(!histograms){
        free(tiles);
        return;
    }
    
    for(int i = 0; i < vert_tiles; ++i)
        for(int j = 0; j < oriz_tiles; ++j)
            clip(&histograms[i][j], cliplimit, tile_size);
    
    int *** luts;
    create_luts(histograms, &luts, oriz_tiles, vert_tiles, tile_size);
    if(!luts){
        free_cubic_matrix(histograms, vert_tiles, oriz_tiles);
        free(tiles);
        return;
    }
    
    apply_clahe(grayscale_photo, equalized_image, luts, oriz_tiles, vert_tiles, tile_size, width, height);
    
    //free_cubic_matrix(histograms, vert_tiles, oriz_tiles);
    //free_cubic_matrix(luts, vert_tiles, oriz_tiles);
    //free(tiles);
}

void blend_with_original(int **equalized, const uint8_t * const *original, 
                        int width, int height, float clahe_strength) {
    for(int i = 0; i < height; i++) {
        for(int j = 0; j < width; j++) {
            float blended = original[i][j] * (1.0f - clahe_strength) + 
                           equalized[i][j] * clahe_strength;
            equalized[i][j] = (int)blended;
        }
    }
}


double** createGaussianKernel(double sigma, int* size_out) {
    int radius = (int)ceil(3 * sigma);
    int size = 2 * radius + 1;
    *size_out = size;
    
    double** kernel = (double **)malloc_matrix(size, size, sizeof(double));
    DIE(!kernel, "kernel alloc failed");
    
    double sum = 0;
    double sigmasq = sigma * sigma;
    
    for(int i = -radius; i <= radius; ++i){
        for(int j = -radius; j <= radius; ++j){
            double val = exp(-(j*j + i*i) / (2 * sigmasq));
            kernel[i + radius][j + radius] = val;
            sum += val;
        }
    }
    
    for(int i = 0; i < size; ++i){
        for(int j = 0; j < size; ++j)   
            kernel[i][j] /= sum;
    }
    
    return kernel;
}

void convolve(int width, int height, double **image, double ***convoluted, int kernel_size_half, double **kernel){
    *convoluted = (double **)malloc_matrix(width, height, sizeof(double));
    DIE(!(*convoluted), "malloc failed convoluted");
    
    for(int i = 0; i < height; ++i) {
        for(int j = 0; j < width; ++j) {
            if (i - kernel_size_half < 0 || i + kernel_size_half >= height ||
                j - kernel_size_half < 0 || j + kernel_size_half >= width) {
                (*convoluted)[i][j] = image[i][j];
                continue;
            }

            double new_val = 0.0;
            for(int ii = -kernel_size_half; ii <= kernel_size_half; ++ii) {
                for(int jj = -kernel_size_half; jj <= kernel_size_half; ++jj) {
                    new_val += kernel[ii + kernel_size_half][jj + kernel_size_half] * image[i + ii][j + jj];
                }
            }
            (*convoluted)[i][j] = new_val;
        }
    }
}

void gradient_double(double **image, double ***grad_x, double ***grad_y, int width, int height) {
    *grad_x = (double **)malloc_matrix(width, height, sizeof(double));
    *grad_y = (double **)malloc_matrix(width, height, sizeof(double));
    DIE(!(*grad_x), "malloc failed gradient_x");
    DIE(!(*grad_y), "malloc failed gradient_y");

    for (int i = 0; i < width; ++i) {
        (*grad_x)[0][i] = 0.0;
        (*grad_y)[0][i] = 0.0;
        (*grad_x)[height - 1][i] = 0.0;
        (*grad_y)[height - 1][i] = 0.0;
    }

    for (int i = 0; i < height; ++i) {
        (*grad_x)[i][0] = 0.0;
        (*grad_y)[i][0] = 0.0;
        (*grad_x)[i][width - 1] = 0.0;
        (*grad_y)[i][width - 1] = 0.0;
    }

    for (int y = 1; y < height - 1; y++) {
        for (int x = 1; x < width - 1; x++) {
            (*grad_x)[y][x] = (image[y][x + 1] - image[y][x - 1]) / 2.0;
            (*grad_y)[y][x] = (image[y + 1][x] - image[y - 1][x]) / 2.0;
        }
    }
}

void gradient_magnitude_double(double **grad_x, double **grad_y, double ***magnitude, int width, int height) {
    *magnitude = (double **)malloc_matrix(width, height, sizeof(double));
    DIE(!(*magnitude), "magnitude malloc failed");
    
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            (*magnitude)[y][x] = sqrt(grad_x[y][x] * grad_x[y][x] + grad_y[y][x] * grad_y[y][x]);
        }
    }
}

void edge_stopping_function_double(double **mag, double ***g, int width, int height) {
    *g = (double **)malloc_matrix(width, height, sizeof(double));
    DIE(!(*g), "edge stopping function malloc failed");
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            double val = mag[y][x];
            (*g)[y][x] = 1.0 / (1.0 + val * val);
        }
    }
}

void compute_curvature(double **phi, double ***curvature, int width, int height) {
    *curvature = (double **)malloc_matrix(width, height, sizeof(double));
    DIE(!(*curvature), "curvature malloc failed");
    
    const double eps = 1e-8;
    
    for (int y = 1; y < height - 1; y++) {
        for (int x = 1; x < width - 1; x++) {
            double phi_x = (phi[y][x + 1] - phi[y][x - 1]) / 2.0;
            double phi_y = (phi[y + 1][x] - phi[y - 1][x]) / 2.0;
            
            double phi_xx = phi[y][x + 1] - 2.0 * phi[y][x] + phi[y][x - 1];
            double phi_yy = phi[y + 1][x] - 2.0 * phi[y][x] + phi[y - 1][x];
            double phi_xy = (phi[y + 1][x + 1] - phi[y + 1][x - 1] - 
                           phi[y - 1][x + 1] + phi[y - 1][x - 1]) / 4.0;
            
            //κ = div(∇φ/|∇φ|)
            double grad_mag_sq = phi_x * phi_x + phi_y * phi_y + eps;
            double grad_mag = sqrt(grad_mag_sq);
            
            (*curvature)[y][x] = (phi_xx * phi_y * phi_y - 2.0 * phi_x * phi_y * phi_xy + 
                                phi_yy * phi_x * phi_x) / (grad_mag_sq * grad_mag);
        }
    }
    for (int x = 0; x < width; x++) {
        (*curvature)[0][x] = 0.0;
        (*curvature)[height - 1][x] = 0.0;
    }
    for (int y = 0; y < height; y++) {
        (*curvature)[y][0] = 0.0;
        (*curvature)[y][width - 1] = 0.0;
    }
}

void reinitialize_level_set(double **phi, int width, int height) {
    const double dt = 0.1;
    const double eps = 1e-10;
    
    double **phi_grad_x, **phi_grad_y, **phi_grad_magnitude;
    gradient_double(phi, &phi_grad_x, &phi_grad_y, width, height);
    gradient_magnitude_double(phi_grad_x, phi_grad_y, &phi_grad_magnitude, width, height);
    
    double **phi_new = (double **)malloc_matrix(width, height,sizeof(double));
    DIE(!phi_new, "reinit malloc failed");
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            double sign_phi = phi[y][x] / sqrt(phi[y][x] * phi[y][x] + eps);
            
            //φ = φ - dt * sign(φ) * (|∇φ| - 1)
            phi_new[y][x] = phi[y][x] - dt * sign_phi * (phi_grad_magnitude[y][x] - 1.0);
        }
    }
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            phi[y][x] = phi_new[y][x];
        }
    }
    
    free_matrix((void**)phi_new, height);
    free_matrix((void**)phi_grad_x, height);
    free_matrix((void**)phi_grad_y, height);
    free_matrix((void**)phi_grad_magnitude, height);
}

void geodesic_level_set_contour(int **image, double ***phi_final, int width, int height, 
                               double sigma, double nu, double dt, int N) {
    double **image_double = (double **)malloc_matrix(width, height, sizeof(double));
    DIE(!image_double, "image_double malloc failed");
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            image_double[y][x] = (double)image[y][x];
        }
    }
    
    int kernel_size;
    double **gaussian_kernel = createGaussianKernel(sigma, &kernel_size);
    int kernel_size_half = kernel_size / 2;
    
    double **I_smooth;
    convolve(width, height, image_double, &I_smooth, kernel_size_half, gaussian_kernel);
    
    double **grad_x, **grad_y, **grad_magnitude;
    gradient_double(I_smooth, &grad_x, &grad_y, width, height);
    gradient_magnitude_double(grad_x, grad_y, &grad_magnitude, width, height);
    
    double **g;
    edge_stopping_function_double(grad_magnitude, &g, width, height);
    
    double **phi = (double **)malloc_matrix(height, width, sizeof(double));
    DIE(!phi, "phi malloc failed");
    
    double center_x = width / 2.0;
    double center_y = height / 2.0;
    double radius = fmin(width, height) / 6.0;
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            double dist = sqrt((x - center_x) * (x - center_x) + (y - center_y) * (y - center_y));
            phi[y][x] = dist - radius;
        }
    }
    
    for (int iter = 0; iter < N; iter++) {
        double **phi_grad_x, **phi_grad_y, **phi_grad_magnitude;
        gradient_double(phi, &phi_grad_x, &phi_grad_y, width, height);
        gradient_magnitude_double(phi_grad_x, phi_grad_y, &phi_grad_magnitude, width, height);
        
        double **curvature;
        compute_curvature(phi, &curvature, width, height);
        
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                double update = dt * phi_grad_magnitude[y][x] * g[y][x] * 
                              (curvature[y][x] + nu);
                phi[y][x] += update;
            }
        }
        
        if (iter % 5 == 0) {
            reinitialize_level_set(phi, width, height);
        }
        
        free_matrix((void**)phi_grad_x, height);
        free_matrix((void**)phi_grad_y, height);
        free_matrix((void**)phi_grad_magnitude, height);
        free_matrix((void**)curvature, height);
    }
    
    *phi_final = phi;
    
    free_matrix((void**)image_double, height);
    free_matrix((void**)gaussian_kernel, kernel_size);
    free_matrix((void**)I_smooth, height);
    free_matrix((void**)grad_x, height);
    free_matrix((void**)grad_y, height);
    free_matrix((void**)grad_magnitude, height);
    free_matrix((void**)g, height);
}
