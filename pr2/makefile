compile:
	UPCXX_GASNET_CONDUIT=udp upcxx -O -std=c++17 main.cpp -o main

run:
	upcxx-run -n 8 $$(upcxx-nodes nodes) main  2>/dev/null

clean:
	rm -f main
