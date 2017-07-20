TS_SOURCE = /mnt/hgfs/WorkShared/trafficserver
INC=-I $(TS_SOURCE)/lib
TARGET=url_analysis
SRC=$(MAIN)

MAIN=-c url_analysis.c -c ats_common.c


all: $(TARGET)_install
clean:
		rm *.lo *~ $(TARGET).so
$(TARGET):
		tsxs -o $@.so $(SRC) $(INC)
$(TARGET)_install: $(TARGET)
		tsxs -o $(TARGET).so -i
