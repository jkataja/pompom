QMAKE_CXXFLAGS += -std=c++0x \
	-march=native \
	-finline-functions \
	-funroll-loops \
	-funswitch-loops \
	-ftree-vectorize \
	-fpredictive-commoning \
	-fgcse-after-reload \
	-Wno-ignored-qualifiers

# ports
macx {
	QMAKE_CC = gcc-mp-4.6
	QMAKE_CXX = g++-mp-4.6
	QMAKE_LINK = g++-mp-4.6
	INCLUDEPATH += /opt/local/include
	LIBS += -L/opt/local/lib 
}
