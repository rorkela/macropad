#!/bin/bash

dfu-util -d 1209:1253 -a 1 -D $1
