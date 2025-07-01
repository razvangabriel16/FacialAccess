.section .rodata
.align 3
clip_limit: .float 4.0
tile_size: .word 8
bin_count: .word 256
float_255: .float 255.0

.section .text
.global clahe_process

clahe_process:
    stp x29, x30, [sp, #-64]!
    mov x29, sp
    stp x19, x20, [sp, #16]
    stp x21, x22, [sp, #32]
    stp x23, x24, [sp, #48]
    
    mov x19, x0
    mov x20, x1
    mov w21, w2
    mov w22, w3
    mov x23, x4
    
    mov w0, #0
y_loop:
    cmp w0, w22
    b.ge y_end
    
    mov w1, #0
x_loop:
    cmp w1, w21
    b.ge x_end
    
    mov w2, w1
    add w3, w1, #8
    cmp w3, w21
    csel w3, w3, w21, le
    
    mov w4, w0
    add w5, w0, #8
    cmp w5, w22
    csel w5, w5, w22, le
    
    mov x6, x19
    mov x7, x23
    mov w8, w21
    bl calculate_histogram
    
    mov x0, x23
    adr x1, clip_limit
    bl clip_histogram_and_cdf
    
    mov w6, w4
tile_y_loop:
    cmp w6, w5
    b.ge tile_y_end
    
    mov w7, w2
tile_x_loop:
    cmp w7, w3
    b.ge tile_x_end
    
    sxtw x25, w6
    sxtw x26, w21
    sxtw x27, w7
    madd x8, x25, x26, x27
    ldrb w9, [x19, x8]
    
    sxtw x28, w9
    ldr w10, [x23, x28, lsl #2]
    ldr w11, [x23, #1020]
    
    cmp w11, #0
    b.eq skip_transform
    
    ucvtf s0, w10
    ucvtf s1, w11
    adr x12, float_255
    ldr s2, [x12]
    fdiv s1, s2, s1
    fmul s0, s0, s1
    
    fcvtzs w13, s0
    cmp w13, #0
    csel w13, w13, wzr, ge
    cmp w13, #255
    mov w14, #255
    csel w13, w14, w13, lt
    
    strb w13, [x20, x8]
    b next_pixel
    
skip_transform:
    strb w9, [x20, x8]
    
next_pixel:
    add w7, w7, #1
    b tile_x_loop
    
tile_x_end:
    add w6, w6, #1
    b tile_y_loop
    
tile_y_end:
    add w1, w1, #8
    b x_loop
    
x_end:
    add w0, w0, #8
    b y_loop
    
y_end:
    ldp x23, x24, [sp, #48]
    ldp x21, x22, [sp, #32]
    ldp x19, x20, [sp, #16]
    ldp x29, x30, [sp], #64
    ret

calculate_histogram:
    stp x29, x30, [sp, #-32]!
    mov x29, sp
    stp x19, x20, [sp, #16]
    
    mov x19, #0
clear_hist:
    str wzr, [x7, x19, lsl #2]
    add x19, x19, #1
    cmp x19, #256
    b.lt clear_hist
    
    mov w19, w4
y_loop_hist:
    cmp w19, w5
    b.ge y_end_hist
    
    mov w20, w2
x_loop_hist:
    cmp w20, w3
    b.ge x_end_hist
    
    sxtw x25, w19
    sxtw x26, w8
    sxtw x27, w20
    madd x21, x25, x26, x27
    ldrb w22, [x6, x21]
    sxtw x28, w22
    ldr w23, [x7, x28, lsl #2]
    add w23, w23, #1
    str w23, [x7, x28, lsl #2]
    
    add w20, w20, #1
    b x_loop_hist
    
x_end_hist:
    add w19, w19, #1
    b y_loop_hist
    
y_end_hist:
    ldp x19, x20, [sp, #16]
    ldp x29, x30, [sp], #32
    ret

clip_histogram_and_cdf:
    stp x29, x30, [sp, #-48]!
    mov x29, sp
    stp x19, x20, [sp, #16]
    stp x21, x22, [sp, #32]
    
    ldr s0, [x1]
    mov w19, #0
    mov w20, #0
    mov w21, #256
    
clip_loop:
    cmp w20, w21
    b.ge clip_end
    
    sxtw x24, w20
    ldr w22, [x0, x24, lsl #2]
    scvtf s1, w22
    fcmp s1, s0
    b.le no_clip
    
    fsub s1, s1, s0
    fcvtzs w23, s1
    add w19, w19, w23
    fcvtzs w22, s0
    str w22, [x0, x24, lsl #2]
    
no_clip:
    add w20, w20, #1
    b clip_loop

clip_end:
    cbz w19, calc_cdf
    
    udiv w22, w19, w21
    msub w23, w22, w21, w19
    
    mov w24, #0
redist_loop:
    cmp w24, w21
    b.ge redist_end
    
    sxtw x25, w24
    ldr w26, [x0, x25, lsl #2]
    add w26, w26, w22
    cmp w24, w23
    b.ge no_extra
    add w26, w26, #1
no_extra:
    str w26, [x0, x25, lsl #2]
    add w24, w24, #1
    b redist_loop

redist_end:
calc_cdf:
    mov w19, #0
    mov w20, #0
cdf_loop:
    cmp w20, w21
    b.ge cdf_end
    
    sxtw x24, w20
    ldr w22, [x0, x24, lsl #2]
    add w19, w19, w22
    str w19, [x0, x24, lsl #2]
    add w20, w20, #1
    b cdf_loop

cdf_end:
    ldp x21, x22, [sp, #32]
    ldp x19, x20, [sp, #16]
    ldp x29, x30, [sp], #48
    ret