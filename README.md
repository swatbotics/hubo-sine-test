hubo-sine-test
==============

Very short test for hubo responsiveness

Instructions
============

First, download and compile the code:

    git clone https://github.com/swatbotics/hubo-sine-test.git
    cd hubo-sine-test
    make
    
It should produce a single binary named `hubo-sine-test`.

Turn on the robot motors. Start the hubo daemon using

    hubo-ach start drc
    
The console will appear. Verify that the robot is free from obstruction
and type 

    homeAll
    
to home the motors. Now press Ctrl+C to leave the hubo-ach console. Next,
with the robot still suspended above the ground, run the command 

    sudo ./hubo-sine-test LHP
    
It will run the left hip pitch motor with a sinusoidal pattern for 8 
seconds and write a log file named `sine_test_LAP_0000.txt`, After it
finishes, run the command

    sudo ./hubo-sine-test LSP
    
to do the same thing with the left shoulder pitch joint. Now we want to run 
the program again, after initializing sensors.  Type

    hubo-ach console
    
to bring up the console again, and enter the command

    iniSensors
    
to zero all of the senors.  Exit the console by pressing Ctrl+C. Now run the
two sine test commands from before:

    sudo ./hubo-sine-test LHP
    sudo ./hubo-sine-test LSP
    
At the end of the four commands, you will have four log files: 

    sine_test_LHP_0000.txt
    sine_test_LSP_0000.txt
    sine_test_LHP_0001.txt
    sine_test_LSP_0001.txt

Now you may stop the hubo daemon:

    hubo-ach stop
    
and turn off the motors afterwards.
