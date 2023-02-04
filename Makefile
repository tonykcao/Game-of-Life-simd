C = gcc
C++ = g++
# CFLAGS = -g -Wall -Wvla -Werror -Wno-error=unused-variable
FASTCFLAGS = -O3 -Wno-unused-result # system("clear") triggers this
# FASTCFLAGS = -g -Wall -Wvla -Werror -Wno-error=unused-variable

SIMDFLAGS = -mavx -mavx2 -msse4.1 -msse2
# SIMDFLAGS = -march=native
# GPROFFLAGS = -pg

OPTIONS = -fPIC

FASTSIMD=fastsimd
JOINSIMD=joinsimd
BARRSIMD=barrsimd


all: $(FASTPROG) $(JOINSIMD) $(BARRSIMD)

fastsimd:
$(FASTSIMD):
	$(C++) $(FASTCFLAGS) $(OPTIONS) $(GPROFFLAGS) $(SIMDFLAGS) $(FASTSIMD).c -o $(FASTSIMD)

joinsimd:
$(JOINSIMD):
	$(C++) $(FASTCFLAGS) $(OPTIONS) $(GPROFFLAGS) $(SIMDFLAGS) $(JOINSIMD).c -o $(JOINSIMD)

barrsimd:
$(BARRSIMD):
	$(C++) $(FASTCFLAGS) $(OPTIONS) $(GPROFFLAGS) $(SIMDFLAGS) $(BARRSIMD).c -o $(BARRSIMD)

clean:
	$(RM) $(MAINPROG) $(FASTPROG) $(FASTSIMD) $(JOINSIMD) $(BARRSIMD) *.o gmon.out my_output
