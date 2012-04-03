TEMPLATE = app
CONFIG = console warn_on release
SOURCES = main.cpp pompom.cpp encoder.cpp decoder.cpp
TARGET = pompom
DESTDIR = ../bin
LIBS = -lpthread -lm -lboost_system -lboost_program_options

include(../common.pri)
