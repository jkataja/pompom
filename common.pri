QMAKE_CXXFLAGS += -std=c++0x \
	-m64 \
	# nehalem and later
	-march=corei7 \
	-msse4.2 \
	# crc32 intrisic
	-mcrc32 -DBUILTIN_CRC \
	# optimizations
	-ffast-math \
	-fgcse-after-reload \
	-finline-functions \
	-fpredictive-commoning \
	-ftree-vectorize \
	-funroll-loops \
	-funswitch-loops \
	-fprefetch-loop-arrays \
	# less warnings of unsigned
	-Wno-ignored-qualifiers \
	# more verbose output
	-DVERBOSE \
	# less checks
	-DUNSAFE \
	# debugging
	#-DDEBUG \
	# bootstrap based on recent text when space is full
	-DBOOTSTRAP

#CONFIG += debug

# ports
macx {
	QMAKE_CC = gcc-mp-4.6
	QMAKE_CXX = g++-mp-4.6
	QMAKE_LINK = g++-mp-4.6
	INCLUDEPATH += /opt/local/include
	LIBS += -L/opt/local/lib 
}
