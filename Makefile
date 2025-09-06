all: git datamatrix

update:
	git submodule update --remote --merge

datamatrix: iec16022.c Image/image.o Reedsol/reedsol.o iec16022ecc200.o
	cc -g -O -o $@ $< Image/image.o Reedsol/reedsol.o iec16022ecc200.o -lpopt -lz -IImage -IReedsol

iec16022.o: iec16022.c
	cc -g -O -c -o $@ $< -IReedsol -IAXL -DLIB

iec16022ecc200.o: iec16022ecc200.c
	cc -g -O -c -o $@ $< -IReedsol -IAXL -DLIB

Image/image.o: Image/image.c
	make -C Image

Reedsol/reedsol.o: Reedsol/reedsol.c
	make -C Reedsol

git:
	git submodule update --init
