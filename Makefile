all: dce
dce: dce.cpp
	g++ dce.cpp -L./lib -lqbe -I ./include -D DEBUG -o dce
test: dce
	bash run.sh
clean:
	rm -rf dce
	rm -R outputs
run: dce.cpp
	g++ dce.cpp -L./lib -lqbe -I ./include -o dce
	./dce