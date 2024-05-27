cmake_minimum_required(VERSION 3.27)
project(Shell_SO C)

set(CMAKE_C_STANDARD 23)

add_executable(a Shell_project.c
        job_control.c
        parse_redir.h)
