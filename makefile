APP=nodeview

all: $(APP)

$(APP): main.c
	gcc -g -O0 -o $@ $< `pkg-config gtk+-3.0 --cflags --libs`
