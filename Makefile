all: spi+pwm

spi+pwm: spi+pwm.c spi+pwm.h
	msp430-gcc -Os -mmcu=msp430g2553 -o spi+pwm.elf spi+pwm.c

clean:
	rm -f *.o
	rm -f *.elf
