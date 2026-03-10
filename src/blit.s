        .section .text, "ax"

radar_data:
.incbin "bin/radar.bin"

        // 128 x 32 = 4096 bytes
        // r0 = screen start pointer
        .global BlitRadar
BlitRadar:
        STMFD sp!, {r4-r11}

        // Add (320 x 200) + 96 to the screen start pointer to get to the radar area
        // 320 x 200 = 64000 bytes for the main screen
        LDR r1, RadarScreenOffset
        ADD r0, r0, r1

        // 32 quads per line
        LDR r1, =radar_data // Source pointer (radar bitmap)
        MOV r12, #32 // Height of the radar

BitmapLoop:
        LDMIA r1!,{r2-r9} // 8 pixels at a time
        STMEA r0!,{r2-r9}
        LDMIA r1!,{r2-r9} // 8 pixels at a time
        STMEA r0!,{r2-r9}
        LDMIA r1!,{r2-r9} // 8 pixels at a time
        STMEA r0!,{r2-r9}
        LDMIA r1!,{r2-r9} // 8 pixels at a time
        STMEA r0!,{r2-r9}

        // Add 320 - 128 = 192 to the destination pointer to move to the next line
        ADD r0, r0, #192

        SUBS r12, r12, #1 // Decrement the height counter
        BNE BitmapLoop

        LDMFD sp!, {r4-r11}
        MOV pc, lr

RadarScreenOffset:
        .word 64096




