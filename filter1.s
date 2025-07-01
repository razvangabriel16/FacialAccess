.text
.global yuyv_to_grayscale
.extern malloc

yuyv_to_grayscale:
    stp x29, x30, [sp, #-96]!
    mov x29, sp 
    stp x19, x20, [sp, #16]
    stp x21, x22, [sp, #32]
    stp x23, x24, [sp, #48]
    stp x25, x26, [sp, #64]
    stp x27, x28, [sp, #80]

    mov x19, x0    
    mov x20, x1         
    mov x21, x2     
    mov x22, x3         
    mul x0, x20, x21    
    ;lsl x0, x0, #2     
    add x0, x0, x21, lsl #3  

    bl malloc
    cbz x0, alloc_failed 
    mov x23, x0         
    str x23, [x22]

    add x24, x23, x21, lsl #3  
    mov x25, #0        
    mov x26, x24       
init_row_pointers:
    cmp x25, x21
    b.ge init_done

    str x26, [x23, x25, lsl #3]

    add x26, x26, x20, lsl #2

    add x25, x25, #1
    b init_row_pointers

init_done:

    mov x25, #0          
row_loop:
    cmp x25, x21
    b.ge conversion_done

    ldr x26, [x23, x25, lsl #3]

    mov x27, #0          
col_loop:
    cmp x27, x20
    b.ge next_row

    mov x28, x25        
    mul x28, x28, x20    
    add x28, x28, x27  
    lsr x28, x28, #1    
    lsl x28, x28, #2     
    add x28, x19, x28    

    tst x27, #1
    b.ne odd_col

even_col:
    ldrb w0, [x28]       
    b store_pixel

odd_col:
    ldrb w0, [x28, #2]   

store_pixel:
    ;str w0, [x26, x27, lsl #2]
    strb w0, [x26, x27]
    add x27, x27, #1
    b col_loop

next_row:
    add x25, x25, #1
    b row_loop

conversion_done:
    ldp x27, x28, [sp, #80]
    ldp x25, x26, [sp, #64]
    ldp x23, x24, [sp, #48]
    ldp x21, x22, [sp, #32]
    ldp x19, x20, [sp, #16]
    ldp x29, x30, [sp], #96
    ret

alloc_failed:
    mov x0, #0
    str x0, [x22]
    b conversion_done