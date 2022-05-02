#!/bin/bash

echo "This Shell Script will install dependencies for adxe" 
echo -n "Are you continue? (y/n) "
read answer
if echo "$answer" | grep -iq "^y" ;then
    echo "It will take few minutes"
else
    exit
fi



DEPENDS+="pkg-config "
DEPENDS+="fontconfig "
DEPENDS+="freetype2 "
DEPENDS+="glew "
DEPENDS+="glfw "
DEPENDS+="glibc "
DEPENDS+="libgl "
DEPENDS+="libpng "
DEPENDS+="libx11 "
DEPENDS+="zlib "

sudo pacman -S -y $DEPENDS
