all: dce
dce: dce.cpp
	g++ dce.cpp -L/usr/bin/lib -lqbe -o dce
test:
	bash run.sh
clean:
	rm -rf dce
	rm -R outputs
run: dce
	./dce