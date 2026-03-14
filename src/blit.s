        .section .text, "ax"

radar_data:
.incbin "bin/radar.bin"

logo_data:
.incbin "bin/space-chase.bin"

        // 128 x 32 = 4096 bytes
        // r0 = screen start pointer
        .global BlitRadar
BlitRadar:
        STMFD sp!, {r4-r11}

        LDR r0, =ScreenStart
        LDR r0, [r0] // Load the actual screen start pointer
        // Add (320 x 200) + 96 to the screen start pointer to get to the radar area
        // 320 x 200 = 64000 bytes for the main screen
        LDR r1, RadarScreenOffset
        ADD r0, r0, r1

        // 32 quads per line
        LDR r1, =radar_data // Source pointer (radar bitmap)
        MOV r12, #32 // Height of the radar

1:
        LDMIA r1!,{r2-r9} // 32 pixels at a time
        STMEA r0!,{r2-r9}
        LDMIA r1!,{r2-r9} // 32 pixels at a time
        STMEA r0!,{r2-r9}
        LDMIA r1!,{r2-r9} // 32 pixels at a time
        STMEA r0!,{r2-r9}
        LDMIA r1!,{r2-r9} // 32 pixels at a time
        STMEA r0!,{r2-r9}

        // Add 320 - 128 = 192 to the destination pointer to move to the next line
        ADD r0, r0, #192

        SUBS r12, r12, #1 // Decrement the height counter
        BNE 1b

        LDMFD sp!, {r4-r11}
        MOV pc, lr

        // 320 x 48
        // r0 = screen start pointer
        .global BlitLogo
BlitLogo:
        STMFD sp!, {r4-r11}

        // 32 quads per line
        LDR r0, =ScreenStart
        LDR r0, [r0] // Load the actual screen start pointer
        LDR r1, =logo_data // Source pointer (logo bitmap)
        MOV r12, #48 // Height of the logo

1:
        LDMIA r1!,{r2-r11} // 40 pixels at a time
        STMEA r0!,{r2-r11}
        LDMIA r1!,{r2-r11} // 40 pixels at a time
        STMEA r0!,{r2-r11}
        LDMIA r1!,{r2-r11} // 40 pixels at a time
        STMEA r0!,{r2-r11}
        LDMIA r1!,{r2-r11} // 40 pixels at a time
        STMEA r0!,{r2-r11}
        LDMIA r1!,{r2-r11} // 40 pixels at a time
        STMEA r0!,{r2-r11}
        LDMIA r1!,{r2-r11} // 40 pixels at a time
        STMEA r0!,{r2-r11}
        LDMIA r1!,{r2-r11} // 40 pixels at a time
        STMEA r0!,{r2-r11}
        LDMIA r1!,{r2-r11} // 40 pixels at a time
        STMEA r0!,{r2-r11}

        SUBS r12, r12, #1 // Decrement the height counter
        BNE 1b

        LDMFD sp!, {r4-r11}
        MOV pc, lr

        // r0 = pointer to V3D struct with radar position
        .global AddSignature
AddSignature:
        STMFD sp!, {r4}

        // Load the radar position from the V3D struct
        LDMFD r0, {r1-r3} // Load x, y, z from the V3D struct
        // X
        MOV r1, r1, ASR #17
        ADD r1, r1, #158 // - 2 as writing 4 bytes on quad line
        CMP r1, #128
        MOVLT r1, #128
        CMP r1, #192
        MOVGT r1, #192

        // Z
        MOV r3, r3, ASR #19
        RSB r3, r3, #216

        // Y (for now, just the delta, whether the enemy is above or below the player)
        MOVS r2, r2, ASR #19

        // If delta is -, then the stick will push below the Z marker
        SUBLT r2, r3, r2 // r2 = r3 -(-r2) = r3 + r2
        MOVLT r4, #16 // Darker, as they are underneath

        // If delta is +, then r3 needs pulling up
        SUBGE r3, r3, r2 // Pull r3 up
        ADDGE r2, r3, r2 // And add r2 to it's new position
        MOVGE r4, #20 // Lighter as they are above

        ORR r4, r4, LSL #8
        ORR r4, r4, LSL #16

        // r3 < r2 at this point
        // Limit the Y values so we don't go outside valid screen buffer lines
        CMP r3, #0
        MOVLT r3, #0
        CMP r3, #255
        MOVGT r3, #255
        CMP r2, #0
        MOVLT r2, #0
        CMP r2, #255
        MOVGT r2, #255

        SUB r2, r2, r3

        // Multiply the y coordinate by 320 to get the offset for the screen buffer
        ADD r3, r3, r3, LSL #2
        MOV r3, r3, LSL #6 // r2 = y * 320

        LDR r0, =ScreenStart
        LDR r0, [r0] // Load the actual screen start pointer
        ADD r0, r0, r3 // Move to the correct line
        ADD r0, r0, r1 // Move to the correct x position

1:
        STRB r4, [r0], #1 // Write the pixel color to the screen buffer
        STRB r4, [r0], #1 // Write the pixel color to the screen buffer
        STRB r4, [r0], #1 // Write the pixel color to the screen buffer
        STRB r4, [r0], #317 // Write the pixel color to the screen buffer
        SUBS r2, r2, #1
        BGT 1b

        LDMFD sp!, {r4}
        MOV pc, lr
        
        .global BlitCrosshair
BlitCrosshair:
        LDR r0, =ScreenStart
        LDR r0, [r0]

        @ Build color word: 242 (white) replicated to all 4 bytes
        MOV r1, #242
        ORR r1, r1, r1, LSL #8
        ORR r1, r1, r1, LSL #16

        @ Compute center pixel address: screen + 100*320 + 160
        MOV r2, #100
        ADD r2, r2, r2, LSL #2     @ r2 = 500
        MOV r2, r2, LSL #6         @ r2 = 32000
        ADD r0, r0, r2
        ADD r0, r0, #160           @ r0 = center pixel

        @ --- Horizontal: 8 pixels each side = 16 pixels = 4 words ---
        SUB r2, r0, #16
        MOV r3, r1
        STMIA r2, {r1, r3}
        ADD r2, r0, #8
        STMIA r2, {r1, r3}

        @ --- Vertical: 8 above + 8 below center, unrolled ---
        SUB r2, r0, #2560          @ Start 8 lines above center
        STRB r1, [r2], #-320
        STRB r1, [r2], #-320
        STRB r1, [r2], #-320
        STRB r1, [r2], #-320
        STRB r1, [r2], #-320
        STRB r1, [r2], #-320
        STRB r1, [r2], #-320
        STRB r1, [r2], #-320
        ADD r2, r0, #2560           @ Skip center (drawn by horizontal)
        STRB r1, [r2], #320
        STRB r1, [r2], #320
        STRB r1, [r2], #320
        STRB r1, [r2], #320
        STRB r1, [r2], #320
        STRB r1, [r2], #320
        STRB r1, [r2], #320
        STRB r1, [r2]

        MOV pc, lr

RadarScreenOffset:
        .word 64096




