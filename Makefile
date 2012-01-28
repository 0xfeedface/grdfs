SRC=src

all: grdfs

grdfs:
	make -C $(SRC)

.PHONY: clean
clean:
	make clean -C $(SRC)
