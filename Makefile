.PHONY: all kws stream clean

all:
	./build.sh

kws:
	./build.sh kws

stream:
	./build.sh kws_stream

clean:
	./build.sh clean
