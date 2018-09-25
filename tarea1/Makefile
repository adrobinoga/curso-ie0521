CXXFLAGS = -std=c++11

cache: cache.cpp
	g++ $(CXXFLAGS) cache.cpp -o cache

run: cache
	gunzip -c art.trace.gz | ./cache -t 32 -a 8 -l 64

clean:
	rm -f cache
