//.section .text
//.global invert_pixels

//invert_pixels:
//    stp x29, x30, [sp, #-96]!
//    mov x29, sp 
//    stp x19, x20, [sp, #16]
//    stp x21, x22, [sp, #32]
//    stp x23, x24, [sp, #48]
//    stp x25, x26, [sp, #64]
//    stp x27, x28, [sp, #80]

//   mov x20, x0 
//    mov x21, x1
//    mov x22, x3

//    mov x23, #0
//for1:
//    cmp x23, x21
//    b.ge done
    
//    mov x24, #0
//for2:
//    cmp x24, x22
//    b.ge skip

//    mul x25, x23, x22
//    add x25, x25, x24
//    add x26, x20, x25

//    ldrb w27, [x26]
//    eor w27, w27, #0xFF
//    strb w27, [x26]

//    add x24, x24, #1
//    b for2
//skip:
//    add x23, x23, #1
//    b for1

//done:
//    ldp x27, x28, [sp, #80]
//    ldp x25, x26, [sp, #64]
//    ldp x23, x24, [sp, #48]
//    ldp x21, x22, [sp, #32]
//    ldp x19, x20, [sp, #16]
//    ldp x29, x30, [sp], #96
//    ret


.section .text
.global invert_pixels

invert_pixels:
    stp x29, x30, [sp, #-80]!
    mov x29, sp 
    stp x19, x20, [sp, #16]
    stp x21, x22, [sp, #32]
    stp x23, x24, [sp, #48]
    stp x25, x26, [sp, #64]

    mov x20, x0   
    mov x21, x1   
    mov x22, x3    

    movi v1.16b, #0xFF

    mov x23, #0
for1:
    cmp x23, x21
    b.ge done

    mov x24, #0 
for2:
    cmp x24, x22
    b.ge skip

    mul x25, x23, x22
    add x25, x25, x24
    add x26, x20, x25

    ld1 {v0.16b}, [x26]
    eor v0.16b, v0.16b, v1.16b
    st1 {v0.16b}, [x26]

    add x24, x24, #16
    b for2

skip:
    add x23, x23, #1
    b for1

done:
    ldp x25, x26, [sp, #64]
    ldp x23, x24, [sp, #48]
    ldp x21, x22, [sp, #32]
    ldp x19, x20, [sp, #16]
    ldp x29, x30, [sp], #80
    ret
