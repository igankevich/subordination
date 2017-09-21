# Bscheduler: A framework for developing distributed applications

# Build

	mkdir build
	meson . build
	ninja -C build

# Run tests and benchmarks

	cd build
	ninja test
	ninja benchmark
