hubo-sine-test: src/hubo-sine-test.c
	gcc -Wall -o hubo-sine-test src/hubo-sine-test.c -lach -lm -lrt

clean: 
	rm -f hubo-sine-test

.PHONY: clean
