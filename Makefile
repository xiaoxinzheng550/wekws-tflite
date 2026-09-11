.PHONY: all stream clean

all:
	./build.sh

stream:
	./build.sh -DWEKWS_BUILD_STREAM=ON

clean:
	cmake -E remove_directory build
