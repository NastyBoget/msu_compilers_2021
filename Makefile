all: dce
dce: dce.cpp
	g++ dce.cpp -L./lib -lqbe -I ./include -o dce
test:
	bash run.sh
clean:
	rm -rf dce
	rm -R outputs
run: dce
	./dce