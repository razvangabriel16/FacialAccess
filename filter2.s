.text
.global create_integral_matrix

create_integral_matrix:
    
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

    mov x0, x20    
    mul x0, x0, x21    
    lsl x0, x0, #2   
    add x0, x0, x21, lsl #3 

    bl malloc
    cbz x0, alloc_fail 
    mov x23, x0  
    str x23, [x22]  
    add x24, x23, x21, lsl #3  
    mov x25, #0 
init_rows:
    cmp x25, x21
    b.ge init_done
    mov x0, x20
    mul x0, x0, x25      
    add x0, x24, x0, lsl #2  
    str x0, [x23, x25, lsl #3] 

    add x25, x25, #1
    b init_rows

init_done:
    ldr x26, [x23]     
    ldr x27, [x19]    
    mov x28, #0    
    mov x9, #0      
first_row_loop:
    cmp x9, x20
    b.ge first_row_done

    ldr w10, [x27, x9, lsl #2]
    add x28, x28, x10    
    str w28, [x26, x9, lsl #2] 

    add x9, x9, #1
    b first_row_loop

first_row_done:
    mov x25, #1        
row_loop:
    cmp x25, x21
    b.ge done

    ldr x26, [x23, x25, lsl #3]      
    ldr x27, [x19, x25, lsl #3]     
    sub x0, x25, #1
    ldr x28, [x23, x0, lsl #3]     

    ldr w10, [x27]        
    ldr w11, [x28]          
    add w10, w10, w11
    str w10, [x26]                  

    mov x9, #1         
col_loop:
    cmp x9, x20
    b.ge next_row

    ldr w10, [x27, x9, lsl #2]
    ldr w11, [x28, x9, lsl #2]     
    sub x0, x9, #1
    ldr w12, [x26, x0, lsl #2]     
    ldr w13, [x28, x0, lsl #2]      

    add w10, w10, w11
    add w10, w10, w12
    sub w10, w10, w13
    str w10, [x26, x9, lsl #2]     

    add x9, x9, #1
    b col_loop

next_row:
    add x25, x25, #1
    b row_loop

alloc_fail:
    mov x0, #0
    str x0, [x22]
done:
    ldp x27, x28, [sp, #80]
    ldp x25, x26, [sp, #64]
    ldp x23, x24, [sp, #48]
    ldp x21, x22, [sp, #32]
    ldp x19, x20, [sp, #16]
    ldp x29, x30, [sp], #96
    ret