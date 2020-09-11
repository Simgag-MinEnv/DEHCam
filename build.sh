mkdir src

rm `basename $1`.bin
rm src/*.*
cp $1/*.* src

cp libraries2/*.h src
cp libraries2/*.cpp src
cp libraries2/*.c src

particle compile argon src --target 0.8.0-rc.27 --saveTo `basename $1`.bin
