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

RadarScreenOffset:
        .word 64096




