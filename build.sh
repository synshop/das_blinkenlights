#!/bin/bash

gcc bl_siguser1.c -o bl_siguser1

g++ blinkenlights.cpp -o blinkenlights -lwiringPi -lyaml

