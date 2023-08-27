CXX			= g++

CXXFLAGS	= -O3 -w -std=c++11 -fopenmp

SRC			= 111062684_dfm_final.cpp

RM			= rm

EXE			= ./Fill_Insertion

VER			= ./verifier

OUT			= ./output/*.txt

all :: opt
opt: $(SRC)
	make -s clean && $(CXX) $(CXXFLAGS) $(SRC) -o $(EXE)
clean:
	$(RM) -rf $(EXE)
test: opt
	@read -p "Which testcase to run? (3 ~ 5): " CASE; \
	echo "Running testcase $$CASE ..."; \
	$(EXE) ./input/$$CASE.txt ./output/$$CASE.txt; \
	echo "Running verification on $$CASE ..."; \
	$(VER) ./input/$$CASE.txt ./output/$$CASE.txt
out: opt
	echo "Running testcase 3 ..."; \
	$(EXE) ./input/3.txt ./output/3.txt; \
	echo "Running testcase 4 ..."; \
	$(EXE) ./input/4.txt ./output/4.txt; \
	echo "Running testcase 5 ..."; \
	$(EXE) ./input/5.txt ./output/5.txt; \
	echo "Running verification on 3 ..."; \
	$(VER) ./input/3.txt ./output/3.txt; \
	echo "Running verification on 4 ..."; \
	$(VER) ./input/4.txt ./output/4.txt; \
	echo "Running verification on 5 ..."; \
	$(VER) ./input/5.txt ./output/5.txt;