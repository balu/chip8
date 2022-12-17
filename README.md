A CHIP8 emulator that works on your Linux terminal.

Change your terminals setting so that each pixel is a square. Find the widthxheight ratio of cursor in settings. Without this, the graphics will be skewed.

To test:

    ./chip8_run.sh octojam1title.ch8

This should display an animated logo. To exit, open another terminal and do:

    killall -9 chip8

To run mini-lights-out.ch8:

    ./chip8_run.sh mini-lights-out.ch8

Type 'e' at the splash screen to enter the game, use:

    1 2 3 4
    q w e r
    a s d f
    z x c v

to switch of lights.

To run outlaw.ch8:

    ./chip8_run.sh outlaw.ch8 300

The keys are wasd for movement and e to shoot.

The second parameter sets the frame rate. There may be some bug in frame rate setting.

# References

 - https://johnearnest.github.io/chip8Archive/?sort=platform
 - https://johnearnest.github.io/Octo/
 - https://github.com/mattmikolay/chip-8/wiki/CHIP%E2%80%908-Technical-Reference
 - https://en.wikipedia.org/wiki/ANSI_escape_code#CSI_(Control_Sequence_Introducer)_sequences