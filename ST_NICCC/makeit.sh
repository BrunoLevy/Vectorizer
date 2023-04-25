
# The ST-NICCC Demo
# http://leonard.oxg.free.fr/stniccc/stniccc.html
# http://www.pouet.net/topic.php?which=11559&page=1
#
# Data file and information:
#    http://arsantica-online.com/st-niccc-competition/


# Download ST_NICCC data file
if [ ! -f scene1.bin ]; then
    wget http://www.evilshirt.com/_ataristuff/scene_data_file.zip
    unzip scene_data_file.zip
    rm scene_data_file.zip
else
  echo "   Using cached scene1.bin"
fi

CFLAGS="-Wall -Wpedantic"

gcc $CFLAGS -DGFX_BACKEND_GLFW ST_NICCC.c graphics.c io.c -lglfw -lGL -o ST_NICCC_glfw
gcc $CFLAGS -DGFX_BACKEND_ANSI ST_NICCC.c graphics.c io.c -o ST_NICCC_console
gcc $CFLAGS test_gen_anim.c io.c -lm -o test_gen_anim
gcc $CFLAGS test_ST_NICCC.c -o test_ST_NICCC
