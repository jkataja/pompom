TEMPLATE = app
CONFIG = console warn_on release
SOURCES = main.cpp pompom.cpp model.cpp encoder.cpp decoder.cpp
TARGET = pompom

INCLUDEPATH += /opt/local/include
LIBS = -lpthread -lm \
	-L/opt/local/lib -lboost_system -lboost_program_options -ldatrie 
