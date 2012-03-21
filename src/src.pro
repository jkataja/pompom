TEMPLATE = app
CONFIG = console warn_on release
SOURCES = main.cpp pompom.cpp model.cpp encoder.cpp decoder.cpp
TARGET = pompom
DESTDIR = ../bin

QMAKE_CXXFLAGS += -std=c++0x

LIBS = -lpthread -lm -lboost_system -lboost_program_options

# ports
macx {
	QMAKE_CC = gcc-mp-4.6
	QMAKE_CXX = g++-mp-4.6
	QMAKE_LINK = g++-mp-4.6
	INCLUDEPATH += /opt/local/include
	LIBS += -L/opt/local/lib 
}
