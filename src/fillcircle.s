        .text
        .align  2
        .global FillCircleClipped
        .extern ScreenStart

@ ----------------------------------------------------------------------
@ FillCircleClipped
@
@ in:
@   r0 = x centre
@   r1 = y centre
@   r2 = radius
@   r3 = color
@
@ draws a filled circle in colour 242, clipped to:
@   x = 0..319
@   y = 0..255
@
@ destroys:
@   r0-r12, lr
@ ----------------------------------------------------------------------

FillCircleClipped:
        STMFD   sp!, {r4-r11,lr}

        MOV     r10, r0                 @ xc
        MOV     r11, r1                 @ yc

        LDR     r5, =ScreenStart
        LDR     r5, [r5]                @ screen base

        MOV     r6, r3                  @ color
        ORR     r6, r6, r6, LSL #8
        ORR     r6, r6, r6, LSL #16     @ r6 = color color color color
        MOV     r12, r6                 @ second fill word for STMIA

        MOV     r7, #255
        ADD     r7, r7, #64             @ r7 = 319

        MOV     r8, #0                  @ y = 0
        MOV     r9, r2                  @ x = radius
        RSB     r4, r2, #1              @ d = 1 - radius

1:      CMP     r8, r9
        BGT     9f

@ ---- rows yc +/- y, span xc-x .. xc+x ----

        ADD     r0, r11, r8             @ row = yc + y
        SUB     r1, r10, r9             @ left = xc - x
        ADD     r2, r10, r9             @ right = xc + x
        BL      DrawSpanClipped

        CMP     r8, #0
        BEQ     2f                      @ avoid duplicate when y == 0

        SUB     r0, r11, r8             @ row = yc - y
        SUB     r1, r10, r9
        ADD     r2, r10, r9
        BL      DrawSpanClipped

2:
@ ---- rows yc +/- x, span xc-y .. xc+y ----

        CMP     r9, r8
        BEQ     3f                      @ avoid duplicate when x == y

        ADD     r0, r11, r9             @ row = yc + x
        SUB     r1, r10, r8             @ left = xc - y
        ADD     r2, r10, r8             @ right = xc + y
        BL      DrawSpanClipped

        SUB     r0, r11, r9             @ row = yc - x
        SUB     r1, r10, r8
        ADD     r2, r10, r8
        BL      DrawSpanClipped

3:      ADD     r8, r8, #1              @ y++

        CMP     r4, #0
        BLT     4f

        SUB     r9, r9, #1              @ x--
        SUB     r0, r8, r9
        ADD     r4, r4, r0, LSL #1
        ADD     r4, r4, #1              @ d += 2*(y-x)+1
        B       1b

4:      ADD     r4, r4, r8, LSL #1
        ADD     r4, r4, #1              @ d += 2*y+1
        B       1b

9:      LDMFD   sp!, {r4-r11,pc}


@ ----------------------------------------------------------------------
@ DrawSpanClipped
@
@ in:
@   r0 = y
@   r1 = left x
@   r2 = right x
@
@ uses:
@   r5  = screen base
@   r6  = 0xF2F2F2F2
@   r7  = 319
@   r12 = 0xF2F2F2F2
@
@ destroys:
@   r0-r3
@ ----------------------------------------------------------------------

DrawSpanClipped:
@ ---- clip Y ----
        CMP     r0, #0
        MOVLT   pc, lr
        CMP     r0, #255
        MOVGT   pc, lr

@ ---- clip X ----
        CMP     r1, #0
        MOVLT   r1, #0

        CMP     r2, r7
        MOVGT   r2, r7

        CMP     r1, r2
        MOVGT   pc, lr

@ ---- len = right-left+1 ----
        SUB     r3, r2, r1
        ADD     r3, r3, #1

@ ---- dst = screen + y*320 + left ----
        ADD     r2, r5, r0, LSL #8      @ y * 256
        ADD     r2, r2, r0, LSL #6      @ y * 320
        ADD     r2, r2, r1              @ dst

@ ---- align to word ----
5:      TST     r2, #3
        BEQ     6f
        STRB    r6, [r2], #1
        SUBS    r3, r3, #1
        MOVEQ   pc, lr
        B       5b

@ ---- 8 bytes at a time ----
6:      CMP     r3, #8
        BLT     7f
        STMIA   r2!, {r6,r12}
        SUB     r3, r3, #8
        B       6b

@ ---- 4 bytes at a time ----
7:      CMP     r3, #4
        BLT     8f
        STR     r6, [r2], #4
        SUB     r3, r3, #4
        B       7b

@ ---- tail bytes ----
8:      CMP     r3, #0
        MOVEQ   pc, lr
        STRB    r6, [r2], #1
        SUBS    r3, r3, #1
        BNE     8b
        MOV     pc, lr
