
# Vectorizer: converts a black and white video into triangulations (WIP)
# Bruno Levy, April 2023
# License: BSD 3 clauses

VIDEOSOURCE=https://ia802905.us.archive.org/19/items/TouhouBadApple/Touhou%20-%20Bad%20Apple.mp4
FPS=5

####################################################################

# fig2obj: converts an xfig file with paths into an alias|wavefront file
# usage: fig2obj input.fig output.obj
fig2obj() {
   awk < $1 '
      BEGIN { 
         v_offset=1; 
         nb_in_polyline=0;
         nb_remaining=0;
      } { 
         if($1 == 3 && NF== 14) {
           nb_in_polyline = $14
           nb_remaining = $14
         } else if(nb_remaining != 0) {
           printf("v %d %d\n", $1, -$2)
           --nb_remaining
           if(nb_remaining == 0) {
              for(i=0; i<nb_in_polyline; ++i) {
                 printf("l %d %d\n", v_offset+i, v_offset+((i+1)%nb_in_polyline))
              }
              v_offset += nb_in_polyline
           }
         }
      }
   '
} > $2

####################################################################

# Step 1: download video
echo "$0: [1] downloading video..."
if [ ! -f VIDEO/video.mp4 ]; then
  wget $VIDEOSOURCE -O VIDEO/video.mp4
else
  echo "   Using cached VIDEO/video.mp4"
fi

# Step 2: extract frames
echo "$0: [2] extracting frames..."
rm -f FRAMES/*
ffmpeg -i VIDEO/video.mp4 -vf fps=$FPS FRAMES/frame%04d.pgm

# Step 3: trace frames
echo "$0: [3] tracing frames..."
for frame in `ls FRAMES/*.pgm`
do
   BASEFRAME=`basename $frame .pgm`
   FIGFRAME=PATHS/$BASEFRAME.fig
   OBJFRAME=PATHS/$BASEFRAME.obj
   potrace -b xfig -a 0 -O 20.0 -r 128x128 $frame -o $FIGFRAME
   fig2obj $FIGFRAME $OBJFRAME
done
