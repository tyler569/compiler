cmake_minimum_required(VERSION 3.25)
project(compiler C)

set(CMAKE_C_STANDARD 23)

add_executable(compiler main.c token.c token.h parse.c parse.h diag.c diag.h tu.c)
