.PHONY: docker-build docker-pull docker-run docker-run-backend transform build gradelab1 gradelab2 gradelab3 gradelab4 gradelab5 gradelab6 gradeall clean register format

docker-build:
	docker build -t cs-ttb/tigerlabs_env .

docker-pull:
	docker pull yuexizou/tiger-compiler-env:tiger

docker-run:
	docker run -it --platform linux/amd64 --privileged -p 2222:22 \
		-v $(shell pwd):/home/stu/tiger-compiler yuexizou/tiger-compiler-env:tiger

docker-run-backend:
	docker run -dt --cap-add=SYS_PTRACE --security-opt seccomp=unconfined --security-opt apparmor=unconfined -p 2222:22 \
		-v $(shell pwd):/home/stu/tiger-compiler yuexizou/tiger-compiler-env:tiger

build:
	mkdir -p build && cd build && cmake -DCMAKE_BUILD_TYPE=Release .. && make

build-debug:
	mkdir -p build && cd build && cmake -DCMAKE_BUILD_TYPE=Debug .. && make

gradelab1:
	bash scripts/grade.sh lab1

gradelab2:
	bash scripts/grade.sh lab2

gradelab3:
	bash scripts/grade.sh lab3

gradelab4:
	bash scripts/grade.sh lab4

gradelab5:
	bash scripts/grade.sh lab5

gradelab6:
	bash scripts/grade.sh lab6

gradeall:
	bash scripts/grade.sh all

ziplab1:
	zip -j lab1-answer.zip src/straightline/slp.*

ziplab2:
	zip -j lab2-answer.zip src/tiger/lex/tiger.lex src/tiger/lex/scanner.h

ziplab3:
	zip -j lab3-answer.zip src/tiger/parse/tiger.y

ziplab4:
	zip -j lab4-answer.zip src/tiger/semant/semant.cc

ziplab5-1:
	zip -r lab5-1_answer.zip src/

ziplab5-2:
	zip -r lab5-2_answer.zip src/tiger/frame/* src/tiger/translate/* src/tiger/runtime/* src/tiger/env/* src/tiger/escape/*

ziplab6:
	zip -r lab6_answer.zip src/

zipclean:
	rm -rf *.zip

clean:
	rm -rf build/ src/tiger/lex/scannerbase.h src/tiger/lex/lex.cc \
		src/tiger/parse/parserbase.h src/tiger/parse/parse.cc

register:
	python3 scripts/register.py

format:
	find . \( -name "*.h" -o -iname "*.cc" \) | xargs clang-format -i -style=file

