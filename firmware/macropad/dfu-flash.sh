#!/bin/bash

dfu-util -d 1209:1253 -a 0 -D $1
